// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network_state_model.h"

#include <set>
#include <string>

#include "ash/public/cpp/network_config_service.h"
#include "ash/system/network/vpn_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

const int kUpdateFrequencyMs = 1000;

NetworkStatePropertiesPtr GetConnectingOrConnected(
    const NetworkStatePropertiesPtr* connecting_network,
    const NetworkStatePropertiesPtr* connected_network) {
  if (connecting_network &&
      (!connected_network || connecting_network->get()->connect_requested)) {
    // If connecting to a network, and there is either no connected network or
    // the connection was user requested, use the connecting network.
    return connecting_network->Clone();
  }
  if (connected_network)
    return connected_network->Clone();
  return nullptr;
}

}  // namespace

class TrayNetworkStateModel::Impl
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  explicit Impl(TrayNetworkStateModel* model) : model_(model) {
    GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());
    remote_cros_network_config_->AddObserver(
        cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl() override = default;

  void GetActiveNetworks() {
    DCHECK(remote_cros_network_config_);
    remote_cros_network_config_->GetNetworkStateList(
        NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                           /*limit=*/0),
        base::BindOnce(&TrayNetworkStateModel::Impl::OnActiveNetworksChanged,
                       base::Unretained(this)));
  }

  void GetVirtualNetworks() {
    DCHECK(remote_cros_network_config_);
    remote_cros_network_config_->GetNetworkStateList(
        NetworkFilter::New(FilterType::kConfigured, NetworkType::kVPN,
                           /*limit=*/0),
        base::BindOnce(&TrayNetworkStateModel::OnGetVirtualNetworks,
                       base::Unretained(model_)));
  }

  void GetDeviceStateList() {
    DCHECK(remote_cros_network_config_);
    remote_cros_network_config_->GetDeviceStateList(
        base::BindOnce(&TrayNetworkStateModel::OnGetDeviceStateList,
                       base::Unretained(model_)));
  }

  void GetGlobalPolicy() {
    DCHECK(remote_cros_network_config_);
    remote_cros_network_config_->GetGlobalPolicy(base::BindOnce(
        &TrayNetworkStateModel::OnGetGlobalPolicy, base::Unretained(model_)));
  }

  void SetNetworkTypeEnabledState(NetworkType type, bool enabled) {
    DCHECK(remote_cros_network_config_);
    remote_cros_network_config_->SetNetworkTypeEnabledState(type, enabled,
                                                            base::DoNothing());
  }

  chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config() {
    return remote_cros_network_config_.get();
  }

  void ConfigureRemoteForTesting(  // IN-TEST
      mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
          cros_network_config) {
    remote_cros_network_config_.reset();
    cros_network_config_observer_receiver_.reset();
    remote_cros_network_config_.Bind(std::move(cros_network_config));
    remote_cros_network_config_->AddObserver(
        cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  }

 private:
  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<NetworkStatePropertiesPtr> networks) override {
    model_->UpdateActiveNetworks(std::move(networks));
    model_->SendActiveNetworkStateChanged();
  }

  void OnNetworkStateListChanged() override {
    model_->NotifyNetworkListChanged();
    GetVirtualNetworks();
  }

  void OnDeviceStateListChanged() override { GetDeviceStateList(); }

  void OnVpnProvidersChanged() override { model_->NotifyVpnProvidersChanged(); }

  void OnPoliciesApplied(const std::string& userhash) override {
    GetGlobalPolicy();
  }

  raw_ptr<TrayNetworkStateModel> model_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
};

TrayNetworkStateModel::TrayNetworkStateModel()
    : update_frequency_(kUpdateFrequencyMs) {
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION) {
    update_frequency_ = 0;  // Send updates immediately for tests.
  }

  impl_ = std::make_unique<Impl>(this);
  vpn_list_ = std::make_unique<VpnList>(this);

  impl_->GetActiveNetworks();
  impl_->GetVirtualNetworks();
  impl_->GetDeviceStateList();
  impl_->GetGlobalPolicy();
}

TrayNetworkStateModel::~TrayNetworkStateModel() {
  vpn_list_.reset();
}

void TrayNetworkStateModel::ConfigureRemoteForTesting(
    mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
        cros_network_config) {
  impl_->ConfigureRemoteForTesting(std::move(cros_network_config));  // IN-TEST
}

void TrayNetworkStateModel::AddObserver(TrayNetworkStateObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TrayNetworkStateModel::RemoveObserver(TrayNetworkStateObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

const DeviceStateProperties* TrayNetworkStateModel::GetDevice(
    NetworkType type) const {
  auto iter = devices_.find(type);
  if (iter == devices_.end())
    return nullptr;
  return iter->second.get();
}

DeviceStateType TrayNetworkStateModel::GetDeviceState(NetworkType type) const {
  const DeviceStateProperties* device = GetDevice(type);
  return device ? device->device_state : DeviceStateType::kUnavailable;
}

void TrayNetworkStateModel::SetNetworkTypeEnabledState(NetworkType type,
                                                       bool enabled) {
  impl_->SetNetworkTypeEnabledState(type, enabled);
}

bool TrayNetworkStateModel::IsBuiltinVpnProhibited() const {
  return TrayNetworkStateModel::GetDeviceState(NetworkType::kVPN) ==
         DeviceStateType::kProhibited;
}

chromeos::network_config::mojom::CrosNetworkConfig*
TrayNetworkStateModel::cros_network_config() {
  return impl_->cros_network_config();
}

void TrayNetworkStateModel::OnGetDeviceStateList(
    std::vector<DeviceStatePropertiesPtr> devices) {
  devices_.clear();
  for (auto& device : devices) {
    NetworkType type = device->type;
    if (base::Contains(devices_, type))
      continue;  // Ignore multiple entries with the same type.
    devices_.emplace(std::make_pair(type, std::move(device)));
  }

  impl_->GetActiveNetworks();  // Will trigger an observer event.
  SendDeviceStateListChanged();
}

void TrayNetworkStateModel::UpdateActiveNetworks(
    std::vector<NetworkStatePropertiesPtr> networks) {
  active_cellular_.reset();
  active_vpn_.reset();

  const NetworkStatePropertiesPtr* connected_network = nullptr;
  const NetworkStatePropertiesPtr* connected_non_cellular = nullptr;
  const NetworkStatePropertiesPtr* connecting_network = nullptr;
  const NetworkStatePropertiesPtr* connecting_non_cellular = nullptr;
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network->type == NetworkType::kVPN) {
      if (!active_vpn_)
        active_vpn_ = network.Clone();
      continue;
    }
    if (network->type == NetworkType::kCellular) {
      if (!active_cellular_)
        active_cellular_ = network.Clone();
    }
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      if (!connected_network)
        connected_network = &network;
      if (!connected_non_cellular && network->type != NetworkType::kCellular) {
        connected_non_cellular = &network;
      }
      continue;
    }
    // Active non connected networks are connecting.
    if (chromeos::network_config::NetworkStateMatchesType(
            network.get(), NetworkType::kWireless)) {
      if (!connecting_network)
        connecting_network = &network;
      if (!connecting_non_cellular && network->type != NetworkType::kCellular) {
        connecting_non_cellular = &network;
      }
    }
  }

  VLOG_IF(2, connected_network)
      << __func__ << ": Connected network: " << connected_network->get()->name
      << " State: " << connected_network->get()->connection_state;
  VLOG_IF(2, connecting_network)
      << __func__ << ": Connecting network: " << connecting_network->get()->name
      << " State: " << connecting_network->get()->connection_state;

  default_network_ =
      GetConnectingOrConnected(connecting_network, connected_network);
  VLOG_IF(2, default_network_)
      << __func__ << ": Default network: " << default_network_->name;

  active_non_cellular_ =
      GetConnectingOrConnected(connecting_non_cellular, connected_non_cellular);
}

void TrayNetworkStateModel::OnGetVirtualNetworks(
    std::vector<NetworkStatePropertiesPtr> networks) {
  has_vpn_ = !networks.empty();
}

void TrayNetworkStateModel::OnGetGlobalPolicy(
    chromeos::network_config::mojom::GlobalPolicyPtr global_policy) {
  global_policy_ = std::move(global_policy);
  NotifyGlobalPolicyChanged();
}

void TrayNetworkStateModel::NotifyNetworkListChanged() {
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::Milliseconds(update_frequency_),
               base::BindOnce(&TrayNetworkStateModel::SendNetworkListChanged,
                              base::Unretained(this)));
}

void TrayNetworkStateModel::NotifyGlobalPolicyChanged() {
  for (auto& observer : observer_list_)
    observer.GlobalPolicyChanged();
}

void TrayNetworkStateModel::NotifyVpnProvidersChanged() {
  for (auto& observer : observer_list_)
    observer.VpnProvidersChanged();
}

void TrayNetworkStateModel::SendActiveNetworkStateChanged() {
  for (auto& observer : observer_list_)
    observer.ActiveNetworkStateChanged();
}

void TrayNetworkStateModel::SendNetworkListChanged() {
  for (auto& observer : observer_list_)
    observer.NetworkListChanged();
}

void TrayNetworkStateModel::SendDeviceStateListChanged() {
  for (auto& observer : observer_list_)
    observer.DeviceStateListChanged();
}

}  // namespace ash
