// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"

#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "google_apis/google_api_keys.h"
#include "net/base/host_port_pair.h"
#include "net/proxy_resolution/proxy_config.h"
#include "url/gurl.h"

IsolatedPrerenderProxyConfigurator::IsolatedPrerenderProxyConfigurator() {
  connect_tunnel_headers_.SetHeader(IsolatedPrerenderProxyHeaderKey(),
                                    "key=" + google_apis::GetAPIKey());
}

IsolatedPrerenderProxyConfigurator::~IsolatedPrerenderProxyConfigurator() =
    default;

void IsolatedPrerenderProxyConfigurator::AddCustomProxyConfigClient(
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_config_clients_.Add(std::move(config_client));
  UpdateCustomProxyConfig();
}

void IsolatedPrerenderProxyConfigurator::UpdateCustomProxyConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsolatedPrerenderIsEnabled())
    return;

  network::mojom::CustomProxyConfigPtr config = CreateCustomProxyConfig();
  for (auto& client : proxy_config_clients_) {
    client->OnCustomProxyConfigUpdated(config->Clone());
  }
}

network::mojom::CustomProxyConfigPtr
IsolatedPrerenderProxyConfigurator::CreateCustomProxyConfig() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net::ProxyConfig::ProxyRules rules;
  DCHECK(rules.proxies_for_http.IsEmpty());
  DCHECK(rules.proxies_for_https.IsEmpty());

  auto config = network::mojom::CustomProxyConfig::New();
  config->rules.type =
      net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;

  // DIRECT is intentionally not added here because we want the proxy to always
  // be used in order to mask the user's IP address during the prerender.
  DCHECK(IsolatedPrerenderProxyHost().is_valid());
  config->rules.proxies_for_https.AddProxyServer(net::ProxyServer(
      net::ProxyServer::GetSchemeFromURI(IsolatedPrerenderProxyHost().scheme()),
      net::HostPortPair::FromURL(IsolatedPrerenderProxyHost())));

  // This ensures that the user's set proxy is honored, although we also disable
  // the feature is such cases.
  config->should_override_existing_config = false;
  config->allow_non_idempotent_methods = false;
  config->connect_tunnel_headers = connect_tunnel_headers_;
  return config;
}
