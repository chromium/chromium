// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_H_

#include <memory>

#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;

namespace safe_browsing {

// This class manages a network context for Safe Browsing communications. The
// primary reason for the use of a separate network context is to ensure that
// cookies are not shared between Safe Browsing functionality and regular
// browsing.
class NetworkContextService : public KeyedService {
 public:
  explicit NetworkContextService(Profile* profile);
  ~NetworkContextService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Get the NetworkContext associated to this profile
  network::mojom::NetworkContext* GetNetworkContext();

  // Get the URLLoaderFactory associated to this profile
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  void FlushNetworkInterfaceForTesting();

 private:
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  std::unique_ptr<SafeBrowsingNetworkContext> network_context_;
  std::unique_ptr<ProxyConfigMonitor> proxy_config_monitor_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_H_
