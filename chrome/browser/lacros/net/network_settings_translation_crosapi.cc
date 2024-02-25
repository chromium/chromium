// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/network_settings_translation.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kAshProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("proxy_config_system", R"(
      semantics {
        sender: "Proxy Config"
        description:
          "Establishing a connection through a proxy server using system proxy "
          "settings."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "User cannot override system proxy settings, but can change them "
          "through the Chrome OS Network Settings UI."
        policy_exception_justification:
          "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' "
          "policies can set Chrome to use a specific proxy settings and avoid "
          "system proxy."
      })");

net::ProxyConfigWithAnnotation TranslatePacProxySettings(
    crosapi::mojom::ProxySettingsPacPtr proxy_settings) {
  if (!proxy_settings->pac_url.is_valid())
    return net::ProxyConfigWithAnnotation::CreateDirect();
  net::ProxyConfig proxy_config =
      net::ProxyConfig::CreateFromCustomPacURL(proxy_settings->pac_url);
  if (proxy_settings->pac_mandatory)
    proxy_config.set_pac_mandatory(proxy_settings->pac_mandatory);
  return net::ProxyConfigWithAnnotation(proxy_config,
                                        kAshProxyConfigTrafficAnnotation);
}

net::ProxyConfigWithAnnotation TranslateWpadProxySettings(
    crosapi::mojom::ProxySettingsWpadPtr proxy_settings) {
  // Ash-Chrome has it's own mechanisms for detecting the PAC URL when
  // configured via WPAD.
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateAutoDetect();
  return net::ProxyConfigWithAnnotation(proxy_config,
                                        kAshProxyConfigTrafficAnnotation);
}

net::ProxyChain ProxyToProxyChain(crosapi::mojom::ProxyLocation::Scheme in,
                                  net::HostPortPair host_port_pair) {
  switch (in) {
    // We map kUnknown to HTTP because this will make the proxy setting work for
    // most deployments with older ASH browser versions which do not support
    // sending the ProxyLocation::Scheme yet, as HTTP is the most commonly used
    // scheme for communicating with a HTTP proxy server.
    case crosapi::mojom::ProxyLocation::Scheme::kUnknown:
      return net::ProxyChain(net::ProxyServer::Scheme::SCHEME_HTTP,
                             host_port_pair);
    case crosapi::mojom::ProxyLocation::Scheme::kInvalid:
      return net::ProxyChain();
    case crosapi::mojom::ProxyLocation::Scheme::kDirect:
      return net::ProxyChain::Direct();
    case crosapi::mojom::ProxyLocation::Scheme::kHttp:
      return net::ProxyChain(net::ProxyServer::Scheme::SCHEME_HTTP,
                             host_port_pair);
    case crosapi::mojom::ProxyLocation::Scheme::kSocks4:
      return net::ProxyChain(net::ProxyServer::Scheme::SCHEME_SOCKS4,
                             host_port_pair);
    case crosapi::mojom::ProxyLocation::Scheme::kSocks5:
      return net::ProxyChain(net::ProxyServer::Scheme::SCHEME_SOCKS5,
                             host_port_pair);
    case crosapi::mojom::ProxyLocation::Scheme::kHttps:
      return net::ProxyChain(net::ProxyServer::Scheme::SCHEME_HTTPS,
                             host_port_pair);
    case crosapi::mojom::ProxyLocation::Scheme::kQuic:
      return net::ProxyChain(net::ProxyServer::Scheme::SCHEME_QUIC,
                             host_port_pair);
  }

  return net::ProxyChain();
}

net::ProxyConfigWithAnnotation TranslateManualProxySettings(
    crosapi::mojom::ProxySettingsManualPtr proxy_settings) {
  net::ProxyConfig proxy_config = net::ProxyConfig();
  proxy_config.proxy_rules().type =
      net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;

  for (auto const& proxy : proxy_settings->http_proxies) {
    proxy_config.proxy_rules().proxies_for_http.AddProxyChain(ProxyToProxyChain(
        proxy->scheme, net::HostPortPair(proxy->host, proxy->port)));
  }
  for (auto const& proxy : proxy_settings->secure_http_proxies) {
    proxy_config.proxy_rules().proxies_for_https.AddProxyChain(
        ProxyToProxyChain(proxy->scheme,
                          net::HostPortPair(proxy->host, proxy->port)));
  }
  for (auto const& proxy : proxy_settings->socks_proxies) {
    // See `net::ProxyServer::GetSchemeFromPacTypeInternal()`.
    proxy_config.proxy_rules().fallback_proxies.AddProxyChain(ProxyToProxyChain(
        proxy->scheme, net::HostPortPair(proxy->host, proxy->port)));
  }

  for (const auto& domains : proxy_settings->exclude_domains) {
    proxy_config.proxy_rules().bypass_rules.AddRuleFromString(domains);
  }
  return net::ProxyConfigWithAnnotation(proxy_config,
                                        kAshProxyConfigTrafficAnnotation);
}

}  // namespace

namespace chromeos {

net::ProxyConfigWithAnnotation CrosapiProxyToNetProxy(
    crosapi::mojom::ProxyConfigPtr crosapi_proxy) {
  if (!crosapi_proxy)
    return net::ProxyConfigWithAnnotation::CreateDirect();

  if (crosapi_proxy->proxy_settings->is_direct())
    return net::ProxyConfigWithAnnotation::CreateDirect();

  if (crosapi_proxy->proxy_settings->is_pac())
    return TranslatePacProxySettings(
        std::move(crosapi_proxy->proxy_settings->get_pac()));

  if (crosapi_proxy->proxy_settings->is_wpad())
    return TranslateWpadProxySettings(
        std::move(crosapi_proxy->proxy_settings->get_wpad()));

  if (crosapi_proxy->proxy_settings->is_manual())
    return TranslateManualProxySettings(
        std::move(crosapi_proxy->proxy_settings->get_manual()));

  return net::ProxyConfigWithAnnotation::CreateDirect();
}

}  // namespace chromeos
