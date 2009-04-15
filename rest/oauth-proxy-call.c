/*
 * librest - RESTful web services access
 * Copyright (c) 2008, 2009, Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
 *          Ross Burton <ross@linux.intel.com>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <string.h>
#include <libsoup/soup.h>
#include <rest/rest-proxy-call.h>
#include "oauth-proxy-call.h"
#include "oauth-proxy-private.h"
#include "rest-proxy-call-private.h"
#include "sha1.h"

G_DEFINE_TYPE (OAuthProxyCall, oauth_proxy_call, REST_TYPE_PROXY_CALL)

#define OAUTH_ENCODE_STRING(x_) soup_uri_encode( (x_), "!$&'()*+,;=@")

static char *
sign_plaintext (OAuthProxyPrivate *priv)
{
  char *cs;
  char *ts;
  char *rv;

  cs = OAUTH_ENCODE_STRING (priv->consumer_secret);
  ts = OAUTH_ENCODE_STRING (priv->token_secret ? priv->token_secret : "");
  rv = g_strconcat (cs, "&", ts, NULL);

  g_free (cs);
  g_free (ts);

  return rv;
}

static char *
encode_params (GHashTable *hash)
{
  GList *keys;
  GString *s;

  s = g_string_new (NULL);

  keys = g_hash_table_get_keys (hash);
  keys = g_list_sort (keys, (GCompareFunc)strcmp);

  while (keys) {
    const char *key;
    const char *value;
    char *k, *v;

    key = keys->data;
    value = g_hash_table_lookup (hash, key);

    k = OAUTH_ENCODE_STRING (key);
    v = OAUTH_ENCODE_STRING (value);

    if (s->len)
      g_string_append (s, "&");

    g_string_append_printf (s, "%s=%s", k, v);

    g_free (k);
    g_free (v);

    keys = keys->next;
  }

  return g_string_free (s, FALSE);
}

static char *
sign_hmac (OAuthProxy *proxy, RestProxyCall *call)
{
  OAuthProxyPrivate *priv;
  RestProxyCallPrivate *callpriv;
  char *key, *signature, *ep, *eep;
  GString *text;

  priv = PROXY_GET_PRIVATE (proxy);
  callpriv = call->priv;

  /* PLAINTEXT signature value is the HMAC-SHA1 key value */
  key = sign_plaintext (priv);

  text = g_string_new (NULL);
  g_string_append (text, rest_proxy_call_get_method (REST_PROXY_CALL (call)));
  g_string_append_c (text, '&');
  g_string_append_uri_escaped (text, callpriv->url, NULL, FALSE);
  g_string_append_c (text, '&');

  ep = encode_params (callpriv->params);
  eep = OAUTH_ENCODE_STRING (ep);
  g_string_append (text, eep);
  g_free (ep);
  g_free (eep);

  signature = hmac_sha1 (key, text->str);

  g_free (key);
  g_string_free (text, TRUE);

  return signature;
}

static gboolean
_prepare (RestProxyCall *call, GError **error)
{
  OAuthProxy *proxy = NULL;
  OAuthProxyPrivate *priv;
  char *s;

  g_object_get (call, "proxy", &proxy, NULL);
  priv = PROXY_GET_PRIVATE (proxy);

  rest_proxy_call_add_param (call, "oauth_version", "1.0");

  s = g_strdup_printf ("%lli", (long long int) time (NULL));
  rest_proxy_call_add_param (call, "oauth_timestamp", s);
  g_free (s);

  s = g_strdup_printf ("%u", g_random_int ());
  rest_proxy_call_add_param (call, "oauth_nonce", s);
  g_free (s);

  rest_proxy_call_add_param (call, "oauth_consumer_key", priv->consumer_key);

  if (priv->token)
    rest_proxy_call_add_param (call, "oauth_token", priv->token);

  switch (priv->method) {
  case PLAINTEXT:
    rest_proxy_call_add_param (call, "oauth_signature_method", "PLAINTEXT");
    s = sign_plaintext (priv);
    break;
  case HMAC_SHA1:
    rest_proxy_call_add_param (call, "oauth_signature_method", "HMAC-SHA1");
    s = sign_hmac (proxy, call);
    break;
  }
  rest_proxy_call_add_param (call, "oauth_signature", s);
  g_free (s);

  g_object_unref (proxy);

  return TRUE;
}

static void
oauth_proxy_call_class_init (OAuthProxyCallClass *klass)
{
  RestProxyCallClass *call_class = REST_PROXY_CALL_CLASS (klass);

  call_class->prepare = _prepare;
}

static void
oauth_proxy_call_init (OAuthProxyCall *self)
{
}
