// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_DEVICE_LISTER_IMPL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_DEVICE_LISTER_IMPL_H_

#include <memory>
#include <string>

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister.h"

namespace local_discovery {
class ServiceDiscoveryClient;
}

namespace cloud_print {

class PrivetDeviceListerImpl
    : public PrivetDeviceLister,
      public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  PrivetDeviceListerImpl(
      local_discovery::ServiceDiscoveryClient* service_discovery_client,
      PrivetDeviceLister::Delegate* delegate);
  ~PrivetDeviceListerImpl() override;

  // PrivetDeviceLister:
  void Start() override;
  void DiscoverNewDevices() override;

 protected:
  // ServiceDiscoveryDeviceLister:
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;

 private:
  PrivetDeviceLister::Delegate* const delegate_;
  std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister> device_lister_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_DEVICE_LISTER_IMPL_H_
