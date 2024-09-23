// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_MDNS_NEARBY_CONNECTIONS_MDNS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_MDNS_NEARBY_CONNECTIONS_MDNS_MANAGER_H_

#include <map>

#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace nearby::sharing {

class NearbyConnectionsMdnsManager
    : public ::sharing::mojom::MdnsManager,
      public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  NearbyConnectionsMdnsManager();
  ~NearbyConnectionsMdnsManager() override;
  void StartDiscoverySession(const std::string& service_type,
                             StartDiscoverySessionCallback callback) override;
  void StopDiscoverySession(const std::string& service_type,
                            StopDiscoverySessionCallback callback) override;
  void AddObserver(
      ::mojo::PendingRemote<::sharing::mojom::MdnsObserver> observer) override;

  // local_discovery::ServiceDiscoveryDeviceLister::Delegate
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;
  void OnPermissionRejected() override {}

  void SetDeviceListersForTesting(
      std::map<std::string,
               std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>*
          device_listers);

 private:
  mojo::RemoteSet<::sharing::mojom::MdnsObserver> observers_;
  // Map from service_type to associated lister (AKA discovery session).
  std::map<std::string,
           std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>
      device_listers_;
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      discovery_client_;
};

}  // namespace nearby::sharing

#endif  // CHROME_BROWSER_NEARBY_SHARING_MDNS_NEARBY_CONNECTIONS_MDNS_MANAGER_H_
