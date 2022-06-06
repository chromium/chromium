// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NETWORKING_PRIVATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NETWORKING_PRIVATE_ASH_H_

#include "chromeos/crosapi/mojom/networking_private.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the NetworkingPrivate crosapi interface.
class NetworkingPrivateAsh : public mojom::NetworkingPrivate {
 public:
  NetworkingPrivateAsh();
  NetworkingPrivateAsh(const NetworkingPrivateAsh&) = delete;
  NetworkingPrivateAsh& operator=(const NetworkingPrivateAsh&) = delete;
  ~NetworkingPrivateAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::NetworkingPrivate> receiver);

  // crosapi::mojom::NetworkingPrivate:
  void GetProperties(const std::string& guid,
                     GetPropertiesCallback callback) override;
  void GetManagedProperties(const std::string& guid,
                            GetManagedPropertiesCallback callback) override;
  void GetState(const std::string& guid, GetStateCallback callback) override;
  void SetProperties(const std::string& guid,
                     base::Value properties,
                     bool allow_set_shared_config,
                     SetPropertiesCallback callback) override;
  void CreateNetwork(bool shared,
                     base::Value properties,
                     CreateNetworkCallback callback) override;
  void ForgetNetwork(const std::string& guid,
                     bool allow_forget_shared_config,
                     ForgetNetworkCallback callback) override;
  void GetNetworks(const std::string& network_type,
                   bool configured_only,
                   bool visible_only,
                   int limit,
                   GetNetworksCallback callback) override;
  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override;
  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override;
  void StartActivate(const std::string& guid,
                     const std::string& carrier,
                     StartActivateCallback callback) override;
  void GetCaptivePortalStatus(const std::string& guid,
                              GetCaptivePortalStatusCallback callback) override;
  void UnlockCellularSim(const std::string& guid,
                         const std::string& pin,
                         const std::string& puk,
                         UnlockCellularSimCallback callback) override;
  void SetCellularSimState(const std::string& guid,
                           bool require_pin,
                           const std::string& current_pin,
                           const std::string& new_pin,
                           SetCellularSimStateCallback callback) override;
  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      SelectCellularMobileNetworkCallback callback) override;
  void GetEnabledNetworkTypes(GetEnabledNetworkTypesCallback callback) override;
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void GetCertificateLists(GetCertificateListsCallback callback) override;
  void EnableNetworkType(const std::string& type,
                         EnableNetworkTypeCallback callback) override;
  void DisableNetworkType(const std::string& type,
                          DisableNetworkTypeCallback callback) override;
  void RequestScan(const std::string& type,
                   RequestScanCallback callback) override;

 private:
  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::NetworkingPrivate> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NETWORKING_PRIVATE_ASH_H_
