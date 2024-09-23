// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

namespace base {
class Thread;
}

@class NetServiceBrowser;
@class NetServiceResolver;

namespace local_discovery {

// Implementation of ServiceDiscoveryClient that uses the Bonjour SDK.
// https://developer.apple.com/library/mac/documentation/Networking/Conceptual/
// NSNetServiceProgGuide/Articles/BrowsingForServices.html
class ServiceDiscoveryClientMac : public ServiceDiscoverySharedClient {
 public:
  ServiceDiscoveryClientMac();

  ServiceDiscoveryClientMac(const ServiceDiscoveryClientMac&) = delete;
  ServiceDiscoveryClientMac& operator=(const ServiceDiscoveryClientMac&) =
      delete;

 private:
  friend class ServiceDiscoveryClientMacTest;

  ~ServiceDiscoveryClientMac() override;

  // ServiceDiscoveryClient implementation.
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

  void StartThreadIfNotStarted();

  std::unique_ptr<base::Thread> service_discovery_thread_;
};

class ServiceWatcherImplMac : public ServiceWatcher {
 public:
  ServiceWatcherImplMac(
      const std::string& service_type,
      ServiceWatcher::UpdatedCallback callback,
      scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner);

  ServiceWatcherImplMac(const ServiceWatcherImplMac&) = delete;
  ServiceWatcherImplMac& operator=(const ServiceWatcherImplMac&) = delete;

  ~ServiceWatcherImplMac() override;

  void OnServicesUpdate(ServiceWatcher::UpdateType update,
                        const std::string& service);
  void RecordPermissionState(bool permission_granted);

 private:
  void Start() override;
  void DiscoverNewServices() override;
  void SetActivelyRefreshServices(bool actively_refresh_services) override;
  std::string GetServiceType() const override;

  // These members should only be accessed on the object creator's sequence.
  const std::string service_type_;
  ServiceWatcher::UpdatedCallback callback_;
  bool started_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner_;

  // TODO(crbug.com/354231463): Remove usage of NetServiceBrowser once the
  // feature media_router::kUseNetworkFrameworkForLocalDiscovery is enabled by
  // default. `nw_browser_` and `browser_` lives on the
  // `service_discovery_runner_`, though they are initialized on the object
  // creator's sequence. They are cleaned up in `StopServiceBrowser()`.
  nw_browser_t __strong nw_browser_;
  NetServiceBrowser* __strong browser_;

  base::WeakPtrFactory<ServiceWatcherImplMac> weak_factory_{this};
};

class ServiceResolverImplMac : public ServiceResolver {
 public:
  ServiceResolverImplMac(
      const std::string& service_name,
      ServiceResolver::ResolveCompleteCallback callback,
      scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner);

  ServiceResolverImplMac(const ServiceResolverImplMac&) = delete;
  ServiceResolverImplMac& operator=(const ServiceResolverImplMac&) = delete;

  ~ServiceResolverImplMac() override;

 private:
  void StartResolving() override;
  std::string GetName() const override;

  void OnResolveComplete(RequestStatus status,
                         const ServiceDescription& description);

  void StopResolving();

  // These members should only be accessed on the object creator's sequence.
  const std::string service_name_;
  ServiceResolver::ResolveCompleteCallback callback_;
  bool has_resolved_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner_;
  // |resolver_| lives on the |service_discovery_runner_|, though it is
  // initialized on the object creator's sequence. It is released in
  // StopResolving().
  NetServiceResolver* __strong resolver_;

  base::WeakPtrFactory<ServiceResolverImplMac> weak_factory_{this};
};

// Parses the data out of the |service|, updating the |description| with the
// results.
void ParseNetService(NSNetService* service, ServiceDescription& description);

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
