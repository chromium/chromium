// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_NET_NETWORK_SETTINGS_TRANSLATION_H_
#define CHROME_BROWSER_LACROS_NET_NETWORK_SETTINGS_TRANSLATION_H_

#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace chromeos {

// Translates the proxy from a crosapi mojo representation to an internal //net
// proxy representation. Used when sending the proxy configuration set by an
// extension in the Lacros primary profile to Ash-Chrome.
net::ProxyConfigWithAnnotation CrosapiProxyToNetProxy(
    crosapi::mojom::ProxyConfigPtr crosapi_proxy);

// Translates the proxy from an internal //net proxy representation to a crosapi
// mojo representation.
crosapi::mojom::ProxyConfigPtr NetProxyToCrosapiProxy(
    const net::ProxyConfigWithAnnotation& proxy_config);

}  // namespace chromeos
#endif  // CHROME_BROWSER_LACROS_NET_NETWORK_SETTINGS_TRANSLATION_H_
