// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config_base.h"
#include "base/notreached.h"

namespace policy {

void FakeCrosNetworkConfigBase::FakeCrosNetworkConfigBase::AddObserver(
    mojo::PendingRemote<
        chromeos::network_config::mojom::CrosNetworkConfigObserver> observer) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetNetworkState(
    const std::string& guid,
    GetNetworkStateCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetNetworkStateList(
    chromeos::network_config::mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetManagedProperties(
    const std::string& guid,
    GetManagedPropertiesCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SetProperties(
    const std::string& guid,
    chromeos::network_config::mojom::ConfigPropertiesPtr properties,
    SetPropertiesCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::ConfigureNetwork(
    chromeos::network_config::mojom::ConfigPropertiesPtr properties,
    bool shared,
    ConfigureNetworkCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::ForgetNetwork(const std::string& guid,
                                              ForgetNetworkCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SetNetworkTypeEnabledState(
    chromeos::network_config::mojom::NetworkType type,
    bool enabled,
    SetNetworkTypeEnabledStateCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SetCellularSimState(
    chromeos::network_config::mojom::CellularSimStatePtr state,
    SetCellularSimStateCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SelectCellularMobileNetwork(
    const std::string& guid,
    const std::string& network_id,
    SelectCellularMobileNetworkCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::RequestNetworkScan(
    chromeos::network_config::mojom::NetworkType type) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetGlobalPolicy(
    GetGlobalPolicyCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::StartConnect(const std::string& guid,
                                             StartConnectCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::StartDisconnect(
    const std::string& guid,
    StartDisconnectCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SetVpnProviders(
    std::vector<chromeos::network_config::mojom::VpnProviderPtr> providers) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetVpnProviders(
    GetVpnProvidersCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetNetworkCertificates(
    GetNetworkCertificatesCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetAlwaysOnVpn(
    GetAlwaysOnVpnCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SetAlwaysOnVpn(
    chromeos::network_config::mojom::AlwaysOnVpnPropertiesPtr properties) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::GetSupportedVpnTypes(
    GetSupportedVpnTypesCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::RequestTrafficCounters(
    const std::string& guid,
    RequestTrafficCountersCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::ResetTrafficCounters(const std::string& guid) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::SetTrafficCountersAutoReset(
    const std::string& guid,
    bool auto_reset,
    chromeos::network_config::mojom::UInt32ValuePtr day,
    SetTrafficCountersAutoResetCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::CreateCustomApn(
    const std::string& network_guid,
    chromeos::network_config::mojom::ApnPropertiesPtr apn,
    CreateCustomApnCallback callback) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::RemoveCustomApn(const std::string& network_guid,
                                                const std::string& apn_id) {
  NOTREACHED();
}

void FakeCrosNetworkConfigBase::ModifyCustomApn(
    const std::string& network_guid,
    chromeos::network_config::mojom::ApnPropertiesPtr apn) {
  NOTREACHED();
}

}  // namespace policy
