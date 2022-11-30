// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_TRANSLATION_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_TRANSLATION_H_

#include "base/values.h"

#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "url/gurl.h"

class ProxyConfigDictionary;

namespace crosapi {

// Translates the proxy from a ProxyConfigDictionary value to a crosapi mojo
// representation. If the `proxy_config` type is Web Proxy Auto-Detection
// (WPAD), `wpad_url` can specify a PAC URL (useful when the WPAD URL is
// configured via DHCP).
crosapi::mojom::ProxyConfigPtr ProxyConfigToCrosapiProxy(
    ProxyConfigDictionary* proxy_dict,
    GURL dhcp_wpad_url);

// Translates the proxy from a crosapi mojo representation to a
// ProxyConfigDictionary value. Used when the proxy is being set in the Lacros
// primary profile via an extension and then forwarded to Ash-Chrome via the
// mojo API.
ProxyConfigDictionary CrosapiProxyToProxyConfig(
    crosapi::mojom::ProxyConfigPtr crosapi_proxy);

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORK_SETTINGS_TRANSLATION_H_
