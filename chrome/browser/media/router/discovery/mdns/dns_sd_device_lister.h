// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_DEVICE_LISTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_DEVICE_LISTER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/common/buildflags.h"

namespace local_discovery {
class ServiceDiscoveryClient;
}  // local_discovery

namespace media_router {

class DnsSdDelegate;

// Manages a watcher for a specific MDNS/DNS-SD service type and notifies
// a delegate of changes to watched services.
class DnsSdDeviceLister
    : public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  DnsSdDeviceLister(
      local_discovery::ServiceDiscoveryClient* service_discovery_client,
      DnsSdDelegate* delegate,
      const std::string& service_type);
  virtual ~DnsSdDeviceLister();

  virtual void Discover();

  // Resets |device_lister_|.
  void Reset();

 protected:
  // local_discovery::ServiceDiscoveryDeviceLister::Delegate:
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;

 private:
  // The delegate to notify of changes to services.
  DnsSdDelegate* const delegate_;

  // Created when Discover() is called, if service discovery is enabled.
  std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister> device_lister_;

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  // The client and service type used to create |device_lister_|.
  local_discovery::ServiceDiscoveryClient* const service_discovery_client_;
  const std::string service_type_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DnsSdDeviceLister);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_DNS_SD_DEVICE_LISTER_H_
