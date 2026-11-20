// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

class Profile;

namespace safe_browsing {

// This class manages a network context for Safe Browsing communications so
// cookies and socket pools are not shared between Safe Browsing and regular
// browsing. Off-the-record profiles use an isolated in-memory network context
// with no on-disk persistence.
class NetworkContextService : public KeyedService {
 public:
  // Creates a Safe Browsing network context service for `profile`, which must
  // not be null.
  explicit NetworkContextService(Profile* profile);
  ~NetworkContextService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Get the NetworkContext associated to this profile
  network::mojom::NetworkContext* GetNetworkContext();

  // Get the URLLoaderFactory associated to this profile
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Overrides the `SharedURLLoaderFactory` returned by `GetURLLoaderFactory()`
  // with `url_loader_factory_for_testing` (or clears the override when null).
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_for_testing);

  void FlushNetworkInterfaceForTesting();

 private:
  friend class NetworkContextServiceTest;

  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  // Whether this service belongs to an off-the-record profile.
  const bool is_off_the_record_;
  std::unique_ptr<ProxyConfigMonitor> proxy_config_monitor_;
  std::unique_ptr<SafeBrowsingNetworkContext> network_context_;

  // Optional test override returned by `GetURLLoaderFactory()` when non-null.
  // Always null in production.
  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NETWORK_CONTEXT_SERVICE_H_
