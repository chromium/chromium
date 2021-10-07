// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/network_context_service.h"

#include "chrome/browser/browser_features.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace safe_browsing {

NetworkContextService::NetworkContextService(Profile* profile) {
  network_context_ = std::make_unique<SafeBrowsingNetworkContext>(
      profile->GetPath(), features::ShouldTriggerNetworkDataMigration(),
      base::BindRepeating(&NetworkContextService::CreateNetworkContextParams,
                          // This is safe because `this` owns
                          // `network_context_`.
                          base::Unretained(this)));
  proxy_config_monitor_ = std::make_unique<ProxyConfigMonitor>(profile);
}

NetworkContextService::~NetworkContextService() = default;

void NetworkContextService::Shutdown() {
  network_context_->ServiceShuttingDown();
}

network::mojom::NetworkContext* NetworkContextService::GetNetworkContext() {
  return network_context_->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
NetworkContextService::GetURLLoaderFactory() {
  return network_context_->GetURLLoaderFactory();
}

network::mojom::NetworkContextParamsPtr
NetworkContextService::CreateNetworkContextParams() {
  auto params = SystemNetworkContextManager::GetInstance()
                    ->CreateDefaultNetworkContextParams();
  proxy_config_monitor_->AddToNetworkContextParams(params.get());
  return params;
}

}  // namespace safe_browsing
