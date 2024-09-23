// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_translation.h"

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
  std::vector<crosapi::mojom::ProxyLocationPtr> proxy_ptr_list;
  for (const auto& proxy_chain : proxy_list.AllChains()) {
    // TODO(crbug.com/40284947): Remove single hop check when multi-hop proxy
    // chains are supported.
    CHECK(proxy_chain.is_single_proxy());
    net::ProxyServer proxy = proxy_chain.First();
    crosapi::mojom::ProxyLocationPtr proxy_ptr;
    proxy_ptr = crosapi::mojom::ProxyLocation::New();
    proxy_ptr->host = proxy.host_port_pair().host();
    proxy_ptr->port = proxy.host_port_pair().port();
    proxy_ptr->scheme = NetSchemeToCrosapiScheme(proxy.scheme());
    proxy_ptr_list.push_back(std::move(proxy_ptr));
  }
  return proxy_ptr_list;
}

crosapi::mojom::ProxySettingsManualPtr TranslateManualProxySettings(
    ProxyConfigDictionary* proxy_config) {
  crosapi::mojom::ProxySettingsManualPtr manual_proxy =
      crosapi::mojom::ProxySettingsManual::New();

  ProxyPrefs::ProxyMode mode;
  DCHECK(proxy_config->GetMode(&mode) &&
         mode == ProxyPrefs::MODE_FIXED_SERVERS);

  std::string proxy_servers;
  if (!proxy_config->GetProxyServer(&proxy_servers)) {
    LOG(ERROR) << "Missing manual proxy servers.";
    return nullptr;
  }

  net::ProxyConfig::ProxyRules rules;
  rules.ParseFromString(proxy_servers);

  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return nullptr;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      if (!rules.single_proxies.IsEmpty()) {
        manual_proxy->http_proxies =
            TranslateProxyLocations(rules.single_proxies);
        manual_proxy->secure_http_proxies =
            TranslateProxyLocations(rules.single_proxies);
        manual_proxy->socks_proxies =
            TranslateProxyLocations(rules.single_proxies);
      }
      break;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      if (!rules.proxies_for_http.IsEmpty()) {
        manual_proxy->http_proxies =
            TranslateProxyLocations(rules.proxies_for_http);
      }
      if (!rules.proxies_for_https.IsEmpty()) {
        manual_proxy->secure_http_proxies =
            TranslateProxyLocations(rules.proxies_for_https);
      }
      if (!rules.fallback_proxies.IsEmpty()) {
        manual_proxy->socks_proxies =
            TranslateProxyLocations(rules.fallback_proxies);
      }
      break;
  }

  std::string bypass_list;
  if (proxy_config->GetBypassList(&bypass_list) && !bypass_list.empty()) {
    manual_proxy->exclude_domains = base::SplitString(
        bypass_list, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  return manual_proxy;
}

}  // namespace

namespace crosapi {

// static
crosapi::mojom::ProxyConfigPtr ProxyConfigToCrosapiProxy(
    ProxyConfigDictionary* proxy_dict,
    GURL dhcp_wpad_url) {
  crosapi::mojom::ProxyConfigPtr proxy_config =
      crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsDirectPtr direct =
      crosapi::mojom::ProxySettingsDirect::New();

  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict || !proxy_dict->GetMode(&mode)) {
    proxy_config->proxy_settings =
        crosapi::mojom::ProxySettings::NewDirect(std::move(direct));
    return proxy_config;
  }
  switch (mode) {
    case ProxyPrefs::MODE_DIRECT:
      proxy_config->proxy_settings =
          crosapi::mojom::ProxySettings::NewDirect(std::move(direct));
      break;
    case ProxyPrefs::MODE_AUTO_DETECT: {
      crosapi::mojom::ProxySettingsWpadPtr wpad =
          crosapi::mojom::ProxySettingsWpad::New();
      // WPAD with DHCP has a higher priority than DNS.
      if (dhcp_wpad_url.is_valid()) {
        wpad->pac_url = std::move(dhcp_wpad_url);
      } else {
        // Fallback to WPAD via DNS.
        wpad->pac_url = GURL("http://wpad/wpad.dat");
      }
      proxy_config->proxy_settings =
          crosapi::mojom::ProxySettings::NewWpad(std::move(wpad));
      break;
    }
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url;
      if (!proxy_dict->GetPacUrl(&pac_url)) {
        proxy_config->proxy_settings =
            crosapi::mojom::ProxySettings::NewDirect(std::move(direct));
        LOG(ERROR) << "No pac URL for pac_script proxy mode.";
        break;
      }
      bool pac_mandatory = false;
      proxy_dict->GetPacMandatory(&pac_mandatory);

      crosapi::mojom::ProxySettingsPacPtr pac =
          crosapi::mojom::ProxySettingsPac::New();
      pac->pac_url = GURL(pac_url);
      pac->pac_mandatory = pac_mandatory;
      proxy_config->proxy_settings =
          crosapi::mojom::ProxySettings::NewPac(std::move(pac));
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      crosapi::mojom::ProxySettingsManualPtr manual =
          TranslateManualProxySettings(proxy_dict);
      proxy_config->proxy_settings =
          crosapi::mojom::ProxySettings::NewManual(std::move(manual));
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      // This mode means Chrome is getting the settings from the operating
      // system. On Chrome OS, ash-chrome is the source of truth for proxy
      // settings so this mode is never used.
      NOTREACHED_IN_MIGRATION()
          << "The system mode doesn't apply to Ash-Chrome";
      break;
    default:
      LOG(ERROR) << "Incorrect proxy mode.";
      proxy_config->proxy_settings =
          crosapi::mojom::ProxySettings::NewDirect(std::move(direct));
  }

  return proxy_config;
}

}  // namespace crosapi
