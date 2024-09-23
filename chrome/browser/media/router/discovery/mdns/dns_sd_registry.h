// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"

namespace local_discovery {
class ServiceDiscoverySharedClient;
}

namespace media_router {

class DnsSdDeviceLister;
class ServiceTypeData;

// Registry class for keeping track of discovered network services over DNS-SD.
class DnsSdRegistry : public DnsSdDelegate {
 public:
  typedef std::vector<DnsSdService> DnsSdServiceList;

  class DnsSdObserver {
   public:
    virtual void OnDnsSdEvent(const std::string& service_type,
                              const DnsSdServiceList& services) = 0;
    virtual void OnDnsSdPermissionRejected() = 0;

   protected:
    virtual ~DnsSdObserver() {}
  };

  static DnsSdRegistry* GetInstance();

  DnsSdRegistry(const DnsSdRegistry&) = delete;
  DnsSdRegistry& operator=(const DnsSdRegistry&) = delete;

  // Publishes the current device list for |service_type| to event listeners
  // whose event filter matches the service type.
  virtual void Publish(const std::string& service_type);

  // Immediately issues a multicast DNS query for all registered service types.
  // Also resets the local cache from previous rounds of discovery.
  virtual void ResetAndDiscover();

  // Observer registration for parties interested in discovery events.
  virtual void AddObserver(DnsSdObserver* observer);
  virtual void RemoveObserver(DnsSdObserver* observer);

  // DNS-SD-related discovery functionality.
  virtual void RegisterDnsSdListener(const std::string& service_type);
  virtual void UnregisterDnsSdListener(const std::string& service_type);

  void ResetForTest();

 protected:
  // Data class for managing all the resources and information related to a
  // particular service type.
  class ServiceTypeData {
   public:
    explicit ServiceTypeData(std::unique_ptr<DnsSdDeviceLister> lister);

    ServiceTypeData(const ServiceTypeData&) = delete;
    ServiceTypeData& operator=(const ServiceTypeData&) = delete;

    virtual ~ServiceTypeData();

    // Notify the data class of listeners so that it can be reference counted.
    void ListenerAdded();
    // Returns true if the last listener was removed.
    bool ListenerRemoved();
    int GetListenerCount();

    // Immediately issues a multicast DNS query for the service type owned by
    // |this|. Also resets the cache from previous rounds of discovery.
    void ResetAndDiscover();

    // Methods for adding, updating or removing services for this service type.
    bool UpdateService(bool added, const DnsSdService& service);
    bool RemoveService(const std::string& service_name);

    // Called when services are flushed. Clears |service_list_| and requests
    // |lister_| to discover and return new services.
    bool ClearServices();

    const DnsSdRegistry::DnsSdServiceList& GetServiceList();

   private:
    int ref_count;
    std::unique_ptr<DnsSdDeviceLister> lister_;
    DnsSdRegistry::DnsSdServiceList service_list_;
  };

  virtual DnsSdDeviceLister* CreateDnsSdDeviceLister(
      DnsSdDelegate* delegate,
      const std::string& service_type,
      local_discovery::ServiceDiscoverySharedClient* discovery_client);

  // DnsSdDelegate implementation:
  void ServiceChanged(const std::string& service_type,
                      bool added,
                      const DnsSdService& service) override;
  void ServiceRemoved(const std::string& service_type,
                      const std::string& service_name) override;
  void ServicesFlushed(const std::string& service_type) override;
  void ServicesPermissionRejected() override;

  std::map<std::string, std::unique_ptr<ServiceTypeData>> service_data_map_;

 private:
  friend struct base::DefaultSingletonTraits<DnsSdRegistry>;
  friend class MockDnsSdRegistry;
  friend class TestDnsSdRegistry;

  DnsSdRegistry();
  explicit DnsSdRegistry(local_discovery::ServiceDiscoverySharedClient* client);
  virtual ~DnsSdRegistry();

  void DispatchApiEvent(const std::string& service_type);
  bool IsRegistered(const std::string& service_type);

  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  base::ObserverList<DnsSdObserver>::Unchecked observers_;
  base::ThreadChecker thread_checker_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_REGISTRY_H_
