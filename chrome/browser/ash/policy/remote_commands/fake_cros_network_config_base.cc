// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config_base.h"
#include "base/notreached.h"

namespace policy {

void FakeCrosNetworkConfigBase::FakeCrosNetworkConfigBase::AddObserver(
    mojo::PendingRemote<
        chromeos::network_config::mojom::CrosNetworkConfigObserver> observer) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetNetworkState(
    const std::string& guid,
    GetNetworkStateCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetNetworkStateList(
    chromeos::network_config::mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetManagedProperties(
    const std::string& guid,
    GetManagedPropertiesCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SetProperties(
    const std::string& guid,
    chromeos::network_config::mojom::ConfigPropertiesPtr properties,
    SetPropertiesCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::ConfigureNetwork(
    chromeos::network_config::mojom::ConfigPropertiesPtr properties,
    bool shared,
    ConfigureNetworkCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::ForgetNetwork(const std::string& guid,
                                              ForgetNetworkCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SetNetworkTypeEnabledState(
    chromeos::network_config::mojom::NetworkType type,
    bool enabled,
    SetNetworkTypeEnabledStateCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SetCellularSimState(
    chromeos::network_config::mojom::CellularSimStatePtr state,
    SetCellularSimStateCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    SelectCellularMobileNetworkCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::RequestNetworkScan(
    chromeos::network_config::mojom::NetworkType type) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetGlobalPolicy(
    GetGlobalPolicyCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::StartConnect(const std::string& guid,
                                             StartConnectCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::StartDisconnect(
    const std::string& guid,
    StartDisconnectCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SetVpnProviders(
    std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetVpnProviders(
    GetVpnProvidersCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetNetworkCertificates(
    GetNetworkCertificatesCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetAlwaysOnVpn(
    GetAlwaysOnVpnCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SetAlwaysOnVpn(
    chromeos::network_config::mojom::AlwaysOnVpnPropertiesPtr properties) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::GetSupportedVpnTypes(
    GetSupportedVpnTypesCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::RequestTrafficCounters(
    const std::string& guid,
    RequestTrafficCountersCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::ResetTrafficCounters(const std::string& guid) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::SetTrafficCountersResetDay(
    const std::string& guid,
    chromeos::network_config::mojom::UInt32ValuePtr day,
    SetTrafficCountersResetDayCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::CreateCustomApn(
    const std::string& network_guid,
    chromeos::network_config::mojom::ApnPropertiesPtr apn,
    CreateCustomApnCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::CreateExclusivelyEnabledCustomApn(
    const std::string& network_guid,
    chromeos::network_config::mojom::ApnPropertiesPtr apn,
    CreateExclusivelyEnabledCustomApnCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::RemoveCustomApn(const std::string& network_guid,
                                                const std::string& apn_id) {
  NOTREACHED_IN_MIGRATION();
}

void FakeCrosNetworkConfigBase::ModifyCustomApn(
    const std::string& network_guid,
    chromeos::network_config::mojom::ApnPropertiesPtr apn) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace policy
