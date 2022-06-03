// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_translation.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "url/url_constants.h"

namespace {

// While "socks" is not a URL scheme, it is given in the same spot in the proxy
// server specifier strings.
constexpr char kSocksScheme[] = "socks";

// See //net/docs/proxy.md for translation rules.
base::Value TranslateManualProxySettings(
    crosapi::mojom::ProxySettingsManualPtr proxy_settings) {
  std::vector<std::string> proxy_server_specs;
  const std::string kProxyFormat = "%s=%s:%d";
  for (auto const& proxy : proxy_settings->http_proxies) {
    proxy_server_specs.push_back(
        base::StringPrintf(kProxyFormat.c_str(), url::kHttpScheme,
                           proxy->host.c_str(), proxy->port));
  }
  for (auto const& proxy : proxy_settings->secure_http_proxies) {
    proxy_server_specs.push_back(
        base::StringPrintf(kProxyFormat.c_str(), url::kHttpsScheme,
                           proxy->host.c_str(), proxy->port));
  }
  for (auto const& proxy : proxy_settings->socks_proxies) {
    proxy_server_specs.push_back(base::StringPrintf(
        kProxyFormat.c_str(), kSocksScheme, proxy->host.c_str(), proxy->port));
  }

  if (proxy_server_specs.empty()) {
    return ProxyConfigDictionary::CreateDirect();
  }
  return ProxyConfigDictionary::CreateFixedServers(
      base::JoinString(proxy_server_specs, ";"),
      base::JoinString(proxy_settings->exclude_domains, ";"));
}

base::Value TranslatePacProxySettings(
    crosapi::mojom::ProxySettingsPacPtr proxy_settings) {
  if (!proxy_settings->pac_url.is_valid())
    return ProxyConfigDictionary::CreateDirect();
  return ProxyConfigDictionary::CreatePacScript(proxy_settings->pac_url.spec(),
                                                proxy_settings->pac_mandatory);
}

base::Value TranslateWpadProxySettings(
    crosapi::mojom::ProxySettingsWpadPtr proxy_settings) {
  return ProxyConfigDictionary::CreateAutoDetect();
}

ProxyConfigDictionary DirectProxyConfig() {
  return ProxyConfigDictionary(ProxyConfigDictionary::CreateDirect());
}

}  // namespace

namespace crosapi {

ProxyConfigDictionary CrosapiProxyToProxyConfig(
    crosapi::mojom::ProxyConfigPtr crosapi_proxy) {
  if (!crosapi_proxy)
    return DirectProxyConfig();

  if (crosapi_proxy->proxy_settings->is_direct())
    return DirectProxyConfig();

  if (crosapi_proxy->proxy_settings->is_pac())
    return ProxyConfigDictionary(TranslatePacProxySettings(
        std::move(crosapi_proxy->proxy_settings->get_pac())));

  if (crosapi_proxy->proxy_settings->is_wpad())
    return ProxyConfigDictionary(TranslateWpadProxySettings(
        std::move(crosapi_proxy->proxy_settings->get_wpad())));

  if (crosapi_proxy->proxy_settings->is_manual())
    return ProxyConfigDictionary(TranslateManualProxySettings(
        std::move(crosapi_proxy->proxy_settings->get_manual())));

  NOTREACHED() << "Invalid crosapi proxy settings";
  return DirectProxyConfig();
}

}  // namespace crosapi
