// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_DEVICE_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_DEVICE_LISTER_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/local_discovery/service_discovery_client.h"

namespace local_discovery {

// ServiceDiscoveryDeviceLister provides a way to get and maintain a list of all
// discoverable devices of a given service type (e.g. _ipp._tcp).
class ServiceDiscoveryDeviceLister {
 public:
  // Delegate interface to be implemented by recipients of list updates.
  class Delegate {
   public:
    virtual void OnDeviceChanged(
        const std::string& service_type,
        bool added,
        const ServiceDescription& service_description) = 0;
    // Not guaranteed to be called after OnDeviceChanged.
    virtual void OnDeviceRemoved(const std::string& service_type,
                                 const std::string& service_name) = 0;
    virtual void OnDeviceCacheFlushed(const std::string& service_type) = 0;
    virtual void OnPermissionRejected() = 0;
  };

  virtual ~ServiceDiscoveryDeviceLister() = default;

  virtual void Start() = 0;
  virtual void DiscoverNewDevices() = 0;

  virtual const std::string& service_type() const = 0;

  // Factory function, create and return a default-implementation DeviceLister
  // that lists devices of type |service_type|.  |delegate| will receive
  // callbacks, and |service_discovery_client| will be used as the source of the
  // the underlying discovery traffic.
  //
  // Typically, the ServiceDiscoverySharedClient is used as the client.
  static std::unique_ptr<ServiceDiscoveryDeviceLister> Create(
      Delegate* delegate,
      ServiceDiscoveryClient* service_discovery_client,
      const std::string& service_type);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_DEVICE_LISTER_H_
