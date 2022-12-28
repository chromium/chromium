// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/fake_cros_network_config.h"

#include <memory>

#include "base/run_loop.h"

namespace ash {

namespace {
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using ::chromeos::network_config::mojom::DeviceStateType;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::GlobalPolicyPtr;
using ::chromeos::network_config::mojom::InhibitReason;
using ::chromeos::network_config::mojom::ManagedPropertiesPtr;
using ::chromeos::network_config::mojom::ManagedString;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::PolicySource;
using ::chromeos::network_config::mojom::SIMInfoPtr;
using ::chromeos::network_config::mojom::VpnProviderPtr;
}  // namespace

FakeCrosNetworkConfig::FakeCrosNetworkConfig() = default;
FakeCrosNetworkConfig::~FakeCrosNetworkConfig() = default;

void FakeCrosNetworkConfig::AddObserver(
    mojo::PendingRemote<
        chromeos::network_config::mojom::CrosNetworkConfigObserver> observer) {
  observers_.Add(std::move(observer));
}

void FakeCrosNetworkConfig::GetNetworkStateList(
    chromeos::network_config::mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  std::move(callback).Run(
      GetFilteredNetworkList(filter->network_type, filter->filter));
}

void FakeCrosNetworkConfig::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  std::move(callback).Run(mojo::Clone(device_properties_));
}

void FakeCrosNetworkConfig::GetManagedProperties(
    const std::string& guid,
    GetManagedPropertiesCallback callback) {
  auto it = guid_to_managed_properties_.find(guid);
  if (it != guid_to_managed_properties_.end()) {
    std::move(callback).Run(it->second.Clone());
    return;
  }
  std::move(callback).Run(nullptr);
}

void FakeCrosNetworkConfig::RequestNetworkScan(NetworkType type) {
  scan_count_[type]++;
}

void FakeCrosNetworkConfig::GetGlobalPolicy(GetGlobalPolicyCallback callback) {
  if (!global_policy_) {
    global_policy_ = chromeos::network_config::mojom::GlobalPolicy::New();
  }

  std::move(callback).Run(global_policy_.Clone());
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::GetVpnProviders(GetVpnProvidersCallback callback) {
  std::vector<VpnProviderPtr> providers;
  std::move(callback).Run(std::move(providers));
}

void FakeCrosNetworkConfig::SetDeviceProperties(
    DeviceStatePropertiesPtr device_properties) {
  AddOrReplaceDevice(std::move(device_properties));
  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::SetGlobalPolicy(
    bool allow_only_policy_cellular_networks) {
  global_policy_ = chromeos::network_config::mojom::GlobalPolicy::New();
  global_policy_->allow_only_policy_cellular_networks =
      allow_only_policy_cellular_networks;
  for (auto& observer : observers_) {
    observer->OnPoliciesApplied(/*userhash=*/std::string());
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::SetNetworkState(
    const std::string& guid,
    ConnectionStateType connection_state_type) {
  for (auto& network : visible_networks_) {
    if (network->guid == guid) {
      network->connection_state = connection_state_type;
      break;
    }
  }
  for (auto& observer : observers_) {
    observer->OnActiveNetworksChanged(
        GetFilteredNetworkList(NetworkType::kAll, FilterType::kActive));
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::AddNetworkAndDevice(
    NetworkStatePropertiesPtr network) {
  auto device_properties =
      chromeos::network_config::mojom::DeviceStateProperties::New();
  device_properties->type = network->type;
  device_properties->device_state = DeviceStateType::kEnabled;

  visible_networks_.push_back(std::move(network));
  AddOrReplaceDevice(std::move(device_properties));

  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
    observer->OnActiveNetworksChanged(
        GetFilteredNetworkList(NetworkType::kAll, FilterType::kActive));
  }
  base::RunLoop().RunUntilIdle();
}

void FakeCrosNetworkConfig::AddManagedProperties(
    const std::string& guid,
    ManagedPropertiesPtr managed_properties) {
  guid_to_managed_properties_[guid] = std::move(managed_properties);
}

void FakeCrosNetworkConfig::ClearNetworksAndDevices() {
  visible_networks_.clear();
  device_properties_.clear();
  for (auto& observer : observers_) {
    observer->OnDeviceStateListChanged();
    observer->OnActiveNetworksChanged({});
  }
  base::RunLoop().RunUntilIdle();
}

int FakeCrosNetworkConfig::GetScanCount(NetworkType type) {
  return scan_count_[type];
}

mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
FakeCrosNetworkConfig::GetPendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeCrosNetworkConfig::AddOrReplaceDevice(
    DeviceStatePropertiesPtr device_properties) {
  auto it =
      std::find_if(device_properties_.begin(), device_properties_.end(),
                   [&device_properties](const DeviceStatePropertiesPtr& p) {
                     return p->type == device_properties->type;
                   });
  if (it != device_properties_.end()) {
    (*it).Swap(&device_properties);
  } else {
    device_properties_.insert(device_properties_.begin(),
                              std::move(device_properties));
  }
}

std::vector<NetworkStatePropertiesPtr>
FakeCrosNetworkConfig::GetFilteredNetworkList(NetworkType network_type,
                                              FilterType filter_type) {
  std::vector<NetworkStatePropertiesPtr> result;
  for (const auto& network : visible_networks_) {
    if (network_type != NetworkType::kAll && network_type != network->type) {
      continue;
    }
    if (filter_type == FilterType::kActive &&
        network->connection_state == ConnectionStateType::kNotConnected) {
      continue;
    }
    result.push_back(network.Clone());
  }
  return result;
}

}  // namespace ash
