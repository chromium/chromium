// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config.h"

#include "base/strings/stringprintf.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"

namespace policy::test {

namespace {

using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::NetworkTypeStateProperties;
using chromeos::network_config::mojom::NetworkTypeStatePropertiesPtr;
using chromeos::network_config::mojom::OncSource;

// Used to generate unique network GUIDs.
static int g_unique_network_id = 1;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// FakeCrosNetworkConfig
////////////////////////////////////////////////////////////////////////////////

FakeCrosNetworkConfig::FakeCrosNetworkConfig() = default;
FakeCrosNetworkConfig::~FakeCrosNetworkConfig() = default;

void FakeCrosNetworkConfig::SetActiveNetworks(
    std::vector<NetworkBuilder> networks) {
  active_networks_.clear();
  for (auto& builder : networks) {
    active_networks_.push_back(builder.Build());
  }
}

void FakeCrosNetworkConfig::AddActiveNetwork(NetworkBuilder builder) {
  active_networks_.push_back(builder.Build());
}

void FakeCrosNetworkConfig::ClearActiveNetworks() {
  active_networks_.clear();
}

void FakeCrosNetworkConfig::GetNetworkStateList(
    chromeos::network_config::mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  std::vector<NetworkStatePropertiesPtr> networks;
  for (const auto& network : active_networks_) {
    networks.push_back(network->Clone());
  }

  std::move(callback).Run(std::move(networks));
}

////////////////////////////////////////////////////////////////////////////////
/// NetworkBuilder
////////////////////////////////////////////////////////////////////////////////

NetworkBuilder::NetworkBuilder(NetworkBuilder&&) = default;
NetworkBuilder& NetworkBuilder::operator=(NetworkBuilder&&) = default;
NetworkBuilder::~NetworkBuilder() = default;
NetworkBuilder::NetworkBuilder(const NetworkBuilder& other)
    : network_(other.network_.Clone()) {}
void NetworkBuilder::operator=(const NetworkBuilder& other) {
  this->network_ = other.network_.Clone();
}

NetworkBuilder::NetworkBuilder(NetworkType type, const std::string& guid) {
  network_->type = type;
  if (guid.empty()) {
    network_->guid =
        base::StringPrintf("<network-guid-%i>", g_unique_network_id++);
  } else {
    network_->guid = guid;
  }
  network_->type_state = CreateTypeStateForType(type);
}

NetworkBuilder& NetworkBuilder::SetOncSource(OncSource source) {
  network_->source = source;
  return *this;
}

NetworkStatePropertiesPtr NetworkBuilder::Build() const {
  return network_.Clone();
}

NetworkTypeStatePropertiesPtr NetworkBuilder::CreateTypeStateForType(
    NetworkType type) {
  switch (type) {
    case NetworkType::kCellular:
      return NetworkTypeStateProperties::NewCellular(
          chromeos::network_config::mojom::CellularStateProperties::New());
    case NetworkType::kEthernet:
      return NetworkTypeStateProperties::NewEthernet(
          chromeos::network_config::mojom::EthernetStateProperties::New());
    case NetworkType::kTether:
      return NetworkTypeStateProperties::NewTether(
          chromeos::network_config::mojom::TetherStateProperties::New());
    case NetworkType::kVPN:
      return NetworkTypeStateProperties::NewVpn(
          chromeos::network_config::mojom::VPNStateProperties::New());
    case NetworkType::kWiFi:
      return NetworkTypeStateProperties::NewWifi(
          chromeos::network_config::mojom::WiFiStateProperties::New());
    case NetworkType::kAll:
    case NetworkType::kMobile:
    case NetworkType::kWireless:
      // These are not actual network types, but just shorthands used while
      // filtering.
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// ScopedFakeCrosNetworkConfig
////////////////////////////////////////////////////////////////////////////////

ScopedFakeCrosNetworkConfig::ScopedFakeCrosNetworkConfig() {
  ash::network_config::OverrideInProcessInstanceForTesting(this);
}

ScopedFakeCrosNetworkConfig::~ScopedFakeCrosNetworkConfig() {
  ash::network_config::OverrideInProcessInstanceForTesting(nullptr);
}

}  // namespace policy::test
