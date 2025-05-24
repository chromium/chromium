// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/network_info_provider.h"

#include <memory>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace ash::boca {
namespace {

namespace network_mojom = ::chromeos::network_config::mojom;

constexpr mojom::NetworkType GetNetworkType(
    const network_mojom::NetworkStatePropertiesPtr& network) {
  switch (network->type) {
    case network_mojom::NetworkType::kCellular:
      return mojom::NetworkType::kCellular;
    case network_mojom::NetworkType::kEthernet:
      return mojom::NetworkType::kEthernet;
    case network_mojom::NetworkType::kWiFi:
      return mojom::NetworkType::kWiFi;
    case network_mojom::NetworkType::kMobile:
    case network_mojom::NetworkType::kTether:
    case network_mojom::NetworkType::kVPN:
    case network_mojom::NetworkType::kAll:
    case network_mojom::NetworkType::kWireless:
      return mojom::NetworkType::kUnsupported;
  }
  return mojom::NetworkType::kUnsupported;
}

mojom::NetworkInfoPtr NetworkToNetworkInfo(
    const network_mojom::NetworkStatePropertiesPtr& network) {
  mojom::NetworkInfoPtr network_info = mojom::NetworkInfo::New();
  if (network) {
    network_info->state = network->connection_state;
    network_info->type = GetNetworkType(network);
    network_info->name = network->name;
    network_info->signal_strength =
        chromeos::network_config::GetWirelessSignalStrength(network.get());
  }
  return network_info;
}

}  // namespace

NetworkInfoProvider::NetworkInfoProvider(
    NetworkInfoCallback network_info_callback)
    : network_info_callback_(std::move(network_info_callback)) {
  GetNetworkConfigService(cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      cros_network_config_observer_.BindNewPipeAndPassRemote());
  FetchActiveNetworkList();
}

NetworkInfoProvider::~NetworkInfoProvider() = default;

void NetworkInfoProvider::OnActiveNetworksChanged(
    std::vector<network_mojom::NetworkStatePropertiesPtr> networks) {
  std::vector<mojom::NetworkInfoPtr> active_networks;
  for (const network_mojom::NetworkStatePropertiesPtr& network : networks) {
    active_networks.push_back(NetworkToNetworkInfo(network));
  }
  network_info_callback_.Run(std::move(active_networks));
}

void NetworkInfoProvider::FetchActiveNetworkList() {
  cros_network_config_->GetNetworkStateList(
      network_mojom::NetworkFilter::New(network_mojom::FilterType::kActive,
                                        network_mojom::NetworkType::kAll,
                                        network_mojom::kNoLimit),
      base::BindOnce(&NetworkInfoProvider::OnActiveNetworksChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash::boca
