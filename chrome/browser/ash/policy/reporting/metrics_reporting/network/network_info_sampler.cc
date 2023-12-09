// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_info_sampler.h"

#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {
namespace {

std::optional<NetworkDeviceType> GetNetworkDeviceType(
    const ::ash::NetworkTypePattern& type) {
  if (type.Equals(::ash::NetworkTypePattern::Cellular())) {
    return NetworkDeviceType::CELLULAR_DEVICE;
  }
  if (type.MatchesPattern(::ash::NetworkTypePattern::EthernetOrEthernetEAP())) {
    return NetworkDeviceType::ETHERNET_DEVICE;
  }
  if (type.Equals(::ash::NetworkTypePattern::WiFi())) {
    return NetworkDeviceType::WIFI_DEVICE;
  }
  return std::nullopt;
}

}  // namespace

void NetworkInfoSampler::MaybeCollect(OptionalMetricCallback callback) {
  ::ash::NetworkStateHandler::DeviceStateList device_list;
  ::ash::NetworkStateHandler* network_state_handler =
      ::ash::NetworkHandler::Get()->network_state_handler();
  network_state_handler->GetDeviceList(&device_list);

  MetricData metric_data;
  auto* const networks_info =
      metric_data.mutable_info_data()->mutable_networks_info();
  for (const auto* device : device_list) {
    auto type = ::ash::NetworkTypePattern::Primitive(device->type());
    std::optional<NetworkDeviceType> device_type = GetNetworkDeviceType(type);
    if (!device_type.has_value()) {
      continue;
    }

    auto* const interface = networks_info->add_network_interfaces();
    interface->set_type(device_type.value());
    if (!device->mac_address().empty()) {
      interface->set_mac_address(device->mac_address());
    }
    if (!device->meid().empty()) {
      interface->set_meid(device->meid());
    }
    if (!device->imei().empty()) {
      interface->set_imei(device->imei());
    }
    if (!device->mdn().empty()) {
      interface->set_mdn(device->mdn());
    }
    if (!device->iccid().empty()) {
      interface->set_iccid(device->iccid());
    }
    if (!device->path().empty()) {
      interface->set_device_path(device->path());
    }

    // Report EIDs for cellular connections.
    if (type.Equals(::ash::NetworkTypePattern::Cellular())) {
      for (const auto& euicc_path :
           ::ash::HermesManagerClient::Get()->GetAvailableEuiccs()) {
        ::ash::HermesEuiccClient::Properties* properties =
            ::ash::HermesEuiccClient::Get()->GetProperties(euicc_path);
        interface->add_eids(properties->eid().value());
      }
    }
  }

  if (!networks_info->network_interfaces().empty()) {
    std::move(callback).Run(std::move(metric_data));
    return;
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace reporting
