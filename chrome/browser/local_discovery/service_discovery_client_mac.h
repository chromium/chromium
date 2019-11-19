// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_

#import <Foundation/Foundation.h>
#include <memory>
#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class Thread;
}

namespace local_discovery {

template <class T>
class ServiceDiscoveryThreadDeleter {
 public:
  inline void operator()(T* t) { t->DeleteSoon(); }
};

// Implementation of ServiceDiscoveryClient that uses the Bonjour SDK.
// https://developer.apple.com/library/mac/documentation/Networking/Conceptual/
// NSNetServiceProgGuide/Articles/BrowsingForServices.html
class ServiceDiscoveryClientMac : public ServiceDiscoverySharedClient {
 public:
  ServiceDiscoveryClientMac();

 private:
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

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientMac);
};

class ServiceWatcherImplMac : public ServiceWatcher {
 public:
  class NetServiceBrowserContainer {
   public:
    NetServiceBrowserContainer(
        const std::string& service_type,
        ServiceWatcher::UpdatedCallback callback,
        scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner);
    ~NetServiceBrowserContainer();

    void Start();
    void DiscoverNewServices();

    void OnServicesUpdate(ServiceWatcher::UpdateType update,
                          const std::string& service);

    void DeleteSoon();

   private:
    void StartOnDiscoveryThread();
    void DiscoverOnDiscoveryThread();

    bool IsOnServiceDiscoveryThread() {
      return base::ThreadTaskRunnerHandle::Get() ==
             service_discovery_runner_.get();
    }

    std::string service_type_;
    ServiceWatcher::UpdatedCallback callback_;

    scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner_;

    base::scoped_nsobject<id> delegate_;
    base::scoped_nsobject<NSNetServiceBrowser> browser_;
    base::WeakPtrFactory<NetServiceBrowserContainer> weak_factory_;
  };

  ServiceWatcherImplMac(
      const std::string& service_type,
      ServiceWatcher::UpdatedCallback callback,
      scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner);

  ~ServiceWatcherImplMac() override;

  void OnServicesUpdate(ServiceWatcher::UpdateType update,
                        const std::string& service);

 private:
  void Start() override;
  void DiscoverNewServices() override;
  void SetActivelyRefreshServices(bool actively_refresh_services) override;
  std::string GetServiceType() const override;

  std::string service_type_;
  ServiceWatcher::UpdatedCallback callback_;
  bool started_;

  std::unique_ptr<NetServiceBrowserContainer,
                  ServiceDiscoveryThreadDeleter<NetServiceBrowserContainer>>
      container_;
  base::WeakPtrFactory<ServiceWatcherImplMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcherImplMac);
};

class ServiceResolverImplMac : public ServiceResolver {
 public:
  class NetServiceContainer {
   public:
    NetServiceContainer(
        const std::string& service_name,
        ServiceResolver::ResolveCompleteCallback callback,
        scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner);

    virtual ~NetServiceContainer();

    void StartResolving();

    void OnResolveUpdate(RequestStatus);

    void SetServiceForTesting(base::scoped_nsobject<NSNetService> service);

    void DeleteSoon();

   private:
    void StartResolvingOnDiscoveryThread();

    bool IsOnServiceDiscoveryThread() {
      return base::ThreadTaskRunnerHandle::Get() ==
             service_discovery_runner_.get();
    }

    const std::string service_name_;
    ServiceResolver::ResolveCompleteCallback callback_;

    scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner_;

    base::scoped_nsobject<id> delegate_;
    base::scoped_nsobject<NSNetService> service_;
    ServiceDescription service_description_;
    base::WeakPtrFactory<NetServiceContainer> weak_factory_;
  };

  ServiceResolverImplMac(
      const std::string& service_name,
      ServiceResolver::ResolveCompleteCallback callback,
      scoped_refptr<base::SingleThreadTaskRunner> service_discovery_runner);

  ~ServiceResolverImplMac() override;

  // Testing methods.
  NetServiceContainer* GetContainerForTesting();

 private:

  void StartResolving() override;
  std::string GetName() const override;

  void OnResolveComplete(RequestStatus status,
                         const ServiceDescription& description);

  const std::string service_name_;
  ServiceResolver::ResolveCompleteCallback callback_;
  bool has_resolved_;

  std::unique_ptr<NetServiceContainer,
                  ServiceDiscoveryThreadDeleter<NetServiceContainer>>
      container_;
  base::WeakPtrFactory<ServiceResolverImplMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceResolverImplMac);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
