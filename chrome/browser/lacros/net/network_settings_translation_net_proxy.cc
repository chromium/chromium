// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/network_settings_translation.h"

#include "base/strings/string_split.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

crosapi::mojom::ProxyLocation::Scheme NetSchemeToCrosapiScheme(
    net::ProxyServer::Scheme in) {
  switch (in) {
    case net::ProxyServer::Scheme::SCHEME_INVALID:
      return crosapi::mojom::ProxyLocation::Scheme::kInvalid;
    case net::ProxyServer::Scheme::SCHEME_HTTP:
      return crosapi::mojom::ProxyLocation::Scheme::kHttp;
    case net::ProxyServer::Scheme::SCHEME_SOCKS4:
      return crosapi::mojom::ProxyLocation::Scheme::kSocks4;
    case net::ProxyServer::Scheme::SCHEME_SOCKS5:
      return crosapi::mojom::ProxyLocation::Scheme::kSocks5;
    case net::ProxyServer::Scheme::SCHEME_HTTPS:
      return crosapi::mojom::ProxyLocation::Scheme::kHttps;
    case net::ProxyServer::Scheme::SCHEME_QUIC:
      return crosapi::mojom::ProxyLocation::Scheme::kQuic;
  }

  return crosapi::mojom::ProxyLocation::Scheme::kUnknown;
}

std::vector<crosapi::mojom::ProxyLocationPtr> TranslateProxyLocations(
    const net::ProxyList& proxy_list) {
  std::vector<crosapi::mojom::ProxyLocationPtr> ptr_list;
  crosapi::mojom::ProxyLocationPtr ptr;
  for (const auto& proxy_chain : proxy_list.AllChains()) {
    // TODO(crbug.com/40284947): Remove single hop check when multi-hop proxy
    // chains are supported.
    CHECK(proxy_chain.is_single_proxy());
    net::ProxyServer proxy = proxy_chain.First();
    ptr = crosapi::mojom::ProxyLocation::New();
    ptr->host = proxy.host_port_pair().host();
    ptr->port = proxy.host_port_pair().port();
    ptr->scheme = NetSchemeToCrosapiScheme(proxy.scheme());
    ptr_list.push_back(std::move(ptr));
  }
  return ptr_list;
}

crosapi::mojom::ProxySettingsManualPtr TranslateManualProxySettings(
    const net::ProxyConfig::ProxyRules& rules) {
  crosapi::mojom::ProxySettingsManualPtr ptr =
      crosapi::mojom::ProxySettingsManual::New();
  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return nullptr;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      ptr->http_proxies = TranslateProxyLocations(rules.single_proxies);
      ptr->secure_http_proxies = TranslateProxyLocations(rules.single_proxies);
      ptr->socks_proxies = TranslateProxyLocations(rules.single_proxies);
      break;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      ptr->http_proxies = TranslateProxyLocations(rules.proxies_for_http);
      ptr->secure_http_proxies =
          TranslateProxyLocations(rules.proxies_for_https);
      ptr->socks_proxies = TranslateProxyLocations(rules.fallback_proxies);
      break;
  }

  for (const auto& domain : rules.bypass_rules.rules()) {
    ptr->exclude_domains.push_back(domain->ToString());
  }
  return ptr;
}

crosapi::mojom::ProxySettingsPtr NetProxyToProxySettings(
    const net::ProxyConfigWithAnnotation& net_proxy) {
  net::ProxyConfig proxy_config = net_proxy.value();
  if (proxy_config.proxy_rules().empty() &&
      !proxy_config.HasAutomaticSettings()) {
    return crosapi::mojom::ProxySettings::NewDirect(
        crosapi::mojom::ProxySettingsDirect::New());
  }

  if (proxy_config.has_pac_url()) {
    auto pac = crosapi::mojom::ProxySettingsPac::New();
    pac->pac_url = proxy_config.pac_url();
    pac->pac_mandatory = proxy_config.pac_mandatory();
    return crosapi::mojom::ProxySettings::NewPac(std::move(pac));
  }

  if (proxy_config.auto_detect()) {
    return crosapi::mojom::ProxySettings::NewWpad(
        crosapi::mojom::ProxySettingsWpad::New());
  }

  crosapi::mojom::ProxySettingsManualPtr manual =
      TranslateManualProxySettings(proxy_config.proxy_rules());
  if (!manual) {
    return crosapi::mojom::ProxySettings::NewDirect(
        crosapi::mojom::ProxySettingsDirect::New());
  }
  return crosapi::mojom::ProxySettings::NewManual(std::move(manual));
}

}  // namespace

namespace chromeos {

crosapi::mojom::ProxyConfigPtr NetProxyToCrosapiProxy(
    const net::ProxyConfigWithAnnotation& net_proxy) {
  auto proxy_config = crosapi::mojom::ProxyConfig::New();
  proxy_config->proxy_settings = NetProxyToProxySettings(net_proxy);
  return proxy_config;
}

}  // namespace chromeos
