// Copyright 2021 The Chromium Authors
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

// Some strings are not URL schemes, but they are given in the same spot in
// the proxy server specifier strings. So we return other strings in that
// case.
constexpr char kInvalidScheme[] = "invalid";
constexpr char kDirectScheme[] = "direct";
constexpr char kSocksScheme[] = "socks";
constexpr char kSocks5Scheme[] = "socks5";
constexpr char kQuicScheme[] = "quic";

// Format the proxy url. The request scheme is the scheme the request to the
// actual web server is using. The proxy scheme is the scheme the proxy is using
// to receive requests. For example a proxy receiving its request through http
// can forward requests going to an https server.
std::string FormatProxyUri(const char* request_scheme,
                           const crosapi::mojom::ProxyLocationPtr& proxy) {
  const char* proxy_scheme_string = url::kHttpScheme;

  switch (proxy->scheme) {
    // We map kUnknown to HTTP because this will make the proxy setting work for
    // most deployments with older ASH browser versions which do not support
    // sending the ProxyLocation::Scheme yet, as HTTP is the most commonly used
    // scheme for communicating with a HTTP proxy server.
    case crosapi::mojom::ProxyLocation::Scheme::kUnknown:
      proxy_scheme_string = url::kHttpScheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kInvalid:
      proxy_scheme_string = kInvalidScheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kDirect:
      proxy_scheme_string = kDirectScheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kHttp:
      proxy_scheme_string = url::kHttpScheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kSocks4:
      proxy_scheme_string = kSocksScheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kSocks5:
      proxy_scheme_string = kSocks5Scheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kHttps:
      proxy_scheme_string = url::kHttpsScheme;
      break;
    case crosapi::mojom::ProxyLocation::Scheme::kQuic:
      // Quic support on Chrome OS is experimental. Can be set by an extension
      // in the primary profile.
      proxy_scheme_string = kQuicScheme;
      break;
  }

  return base::StringPrintf("%s=%s://%s:%d", request_scheme,
                            proxy_scheme_string, proxy->host.c_str(),
                            proxy->port);
}

// See //net/docs/proxy.md and net::ProxyConfig::ProxyRules::ParseFromString()
// for translation rules.
base::Value::Dict TranslateManualProxySettings(
    crosapi::mojom::ProxySettingsManualPtr proxy_settings) {
  std::vector<std::string> proxy_server_specs;
  for (auto const& proxy : proxy_settings->http_proxies) {
    proxy_server_specs.push_back(FormatProxyUri(url::kHttpScheme, proxy));
  }
  for (auto const& proxy : proxy_settings->secure_http_proxies) {
    proxy_server_specs.push_back(FormatProxyUri(url::kHttpsScheme, proxy));
  }
  for (auto const& proxy : proxy_settings->socks_proxies) {
    proxy_server_specs.push_back(FormatProxyUri(kSocksScheme, proxy));
  }

  if (proxy_server_specs.empty()) {
    return ProxyConfigDictionary::CreateDirect();
  }
  return ProxyConfigDictionary::CreateFixedServers(
      base::JoinString(proxy_server_specs, ";"),
      base::JoinString(proxy_settings->exclude_domains, ";"));
}

base::Value::Dict TranslatePacProxySettings(
    crosapi::mojom::ProxySettingsPacPtr proxy_settings) {
  if (!proxy_settings->pac_url.is_valid())
    return ProxyConfigDictionary::CreateDirect();
  return ProxyConfigDictionary::CreatePacScript(proxy_settings->pac_url.spec(),
                                                proxy_settings->pac_mandatory);
}

base::Value::Dict TranslateWpadProxySettings(
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

  NOTREACHED_IN_MIGRATION() << "Invalid crosapi proxy settings";
  return DirectProxyConfig();
}

}  // namespace crosapi
