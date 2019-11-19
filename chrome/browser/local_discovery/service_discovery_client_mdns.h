// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "net/dns/mdns_client.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace local_discovery {

// Implementation of ServiceDiscoverySharedClient with the front-end on the
// UI thread and the networking code on the IO thread.
class ServiceDiscoveryClientMdns
    : public ServiceDiscoverySharedClient,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  class Proxy;

  ServiceDiscoveryClientMdns();

  // ServiceDiscoveryClient:
  std::unique_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      ServiceWatcher::UpdatedCallback callback) override;
  std::unique_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      ServiceResolver::ResolveCompleteCallback callback) override;
  std::unique_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      LocalDomainResolver::IPAddressCallback callback) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  ~ServiceDiscoveryClientMdns() override;

  void ScheduleStartNewClient();
  void StartNewClient();
  void OnInterfaceListReady(const net::InterfaceIndexFamilyList& interfaces);
  void OnMdnsInitialized(int net_error);
  void InvalidateWeakPtrs();
  void OnBeforeMdnsDestroy();
  void DestroyMdns();

  base::ObserverList<Proxy, true>::Unchecked proxies_;

  scoped_refptr<base::SequencedTaskRunner> mdns_runner_;

  // Access only on |mdns_runner_| thread.
  std::unique_ptr<net::MDnsClient> mdns_;

  // Access only on |mdns_runner_| thread.
  std::unique_ptr<ServiceDiscoveryClient> client_;

  // Counter of restart attempts we have made after network change.
  int restart_attempts_ = 0;

  // If false, delay tasks until initialization is posted to |mdns_runner_|.
  bool need_delay_mdns_tasks_ = true;

  base::WeakPtrFactory<ServiceDiscoveryClientMdns> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientMdns);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MDNS_H_
