// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_CROS_NETWORK_CONFIG_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_CROS_NETWORK_CONFIG_BASE_H_

#include <string>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace policy {

// Fake implementation of CrosNetworkConfig that simply calls NOTREACHED()
// in every method.
//
// You can derive from this class in your unittests so you only need to
// override the methods that you know your code uses.
class FakeCrosNetworkConfigBase
    : public chromeos::network_config::mojom::CrosNetworkConfig {
 public:
  FakeCrosNetworkConfigBase() = default;
  FakeCrosNetworkConfigBase(const FakeCrosNetworkConfigBase&) = delete;
  FakeCrosNetworkConfigBase& operator=(const FakeCrosNetworkConfigBase&) =
      delete;
  ~FakeCrosNetworkConfigBase() override = default;

  // `chromeos::network_config::mojom::CrosNetworkConfig` implementation:
  void AddObserver(mojo::PendingRemote<
                   chromeos::network_config::mojom::CrosNetworkConfigObserver>
                       observer) override;
  void GetNetworkState(const std::string& guid,
                       GetNetworkStateCallback callback) override;
  void GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilterPtr filter,
      GetNetworkStateListCallback callback) override;
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;
  void GetManagedProperties(const std::string& guid,
                            GetManagedPropertiesCallback callback) override;
  void SetProperties(
      const std::string& guid,
      chromeos::network_config::mojom::ConfigPropertiesPtr properties,
      SetPropertiesCallback callback) override;
  void ConfigureNetwork(
      chromeos::network_config::mojom::ConfigPropertiesPtr properties,
      bool shared,
      ConfigureNetworkCallback callback) override;
  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override;
  void SetNetworkTypeEnabledState(
      chromeos::network_config::mojom::NetworkType type,
      bool enabled,
      SetNetworkTypeEnabledStateCallback callback) override;
  void SetCellularSimState(
      chromeos::network_config::mojom::CellularSimStatePtr state,
      SetCellularSimStateCallback callback) override;
  void SelectCellularMobileNetwork(
      const std::string& guid,
      const std::string& network_id,
      SelectCellularMobileNetworkCallback callback) override;
  void RequestNetworkScan(
      chromeos::network_config::mojom::NetworkType type) override;
  void GetGlobalPolicy(GetGlobalPolicyCallback callback) override;
  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override;
  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override;
  void SetVpnProviders(
      std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers)
      override;
  void GetVpnProviders(GetVpnProvidersCallback callback) override;
  void GetNetworkCertificates(GetNetworkCertificatesCallback callback) override;
  void GetAlwaysOnVpn(GetAlwaysOnVpnCallback callback) override;
  void SetAlwaysOnVpn(chromeos::network_config::mojom::AlwaysOnVpnPropertiesPtr
                          properties) override;
  void GetSupportedVpnTypes(GetSupportedVpnTypesCallback callback) override;
  void RequestTrafficCounters(const std::string& guid,
                              RequestTrafficCountersCallback callback) override;
  void ResetTrafficCounters(const std::string& guid) override;
  void SetTrafficCountersResetDay(
      const std::string& guid,
      chromeos::network_config::mojom::UInt32ValuePtr day,
      SetTrafficCountersResetDayCallback callback) override;
  void CreateCustomApn(const std::string& network_guid,
                       chromeos::network_config::mojom::ApnPropertiesPtr apn,
                       CreateCustomApnCallback callback) override;
  void CreateExclusivelyEnabledCustomApn(
      const std::string& network_guid,
      chromeos::network_config::mojom::ApnPropertiesPtr apn,
      CreateExclusivelyEnabledCustomApnCallback callback) override;
  void RemoveCustomApn(const std::string& network_guid,
                       const std::string& apn_id) override;
  void ModifyCustomApn(
      const std::string& network_guid,
      chromeos::network_config::mojom::ApnPropertiesPtr apn) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_CROS_NETWORK_CONFIG_BASE_H_
