// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/content_features.h"
#include "net/http/http_auth_preferences.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

NetworkContextService::NetworkContextService(Profile* profile)
    : is_off_the_record_(profile->IsOffTheRecord()) {
  proxy_config_monitor_ = std::make_unique<ProxyConfigMonitor>(profile);
  network_context_ = SafeBrowsingNetworkContext::Create(
      is_off_the_record_, profile->GetPath(),
      base::FeatureList::IsEnabled(features::kTriggerNetworkDataMigration),
      base::BindRepeating(&NetworkContextService::CreateNetworkContextParams,
                          // This is safe because `this` owns
                          // `network_context_`, which clears this callback on
                          // `ServiceShuttingDown()` and destruction (before
                          // `proxy_config_monitor_` is destroyed).
                          base::Unretained(this)));
}

NetworkContextService::~NetworkContextService() = default;

void NetworkContextService::Shutdown() {
  network_context_->ServiceShuttingDown();
}

network::mojom::NetworkContext* NetworkContextService::GetNetworkContext() {
  // The SafeBrowsingNetworkContext cannot operate correctly without a
  // SystemNetworkContextManager instance, so return early if it does not exist.
  if (!SystemNetworkContextManager::GetInstance())
    return nullptr;
  return network_context_->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
NetworkContextService::GetURLLoaderFactory() {
  if (url_loader_factory_for_testing_) {
    return url_loader_factory_for_testing_;
  }
  return network_context_->GetURLLoaderFactory();
}

void NetworkContextService::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory>
        url_loader_factory_for_testing) {
  url_loader_factory_for_testing_ = std::move(url_loader_factory_for_testing);
}

void NetworkContextService::FlushNetworkInterfaceForTesting() {
  network_context_->FlushForTesting();
}

network::mojom::NetworkContextParamsPtr
NetworkContextService::CreateNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr params =
      SystemNetworkContextManager::GetInstance()
          ->CreateDefaultNetworkContextParams();
  proxy_config_monitor_->AddToNetworkContextParams(params.get());
  if (is_off_the_record_) {
    params->http_auth_static_network_context_params =
        network::mojom::HttpAuthStaticNetworkContextParams::New();
    params->http_auth_static_network_context_params->allow_default_credentials =
        net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS;
  }
  return params;
}

}  // namespace safe_browsing
