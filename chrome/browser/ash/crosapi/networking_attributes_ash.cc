// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/networking_attributes_ash.h"

#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/crosapi/mojom/networking_attributes.mojom.h"
#include "components/user_manager/user.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace crosapi {

namespace {

const char kErrorUserNotAffiliated[] =
    "Network attributes can only be read by an affiliated user.";
const char kErrorNetworkNotConnected[] =
    "Device is not connected to a network.";

}  // namespace

NetworkingAttributesAsh::NetworkingAttributesAsh() = default;
NetworkingAttributesAsh::~NetworkingAttributesAsh() = default;

void NetworkingAttributesAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NetworkingAttributes> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NetworkingAttributesAsh::GetNetworkDetails(
    GetNetworkDetailsCallback callback) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    std::move(callback).Run(Result::NewErrorMessage(kErrorUserNotAffiliated));
    return;
  }

  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  if (!network) {
    // Not connected to a network.
    std::move(callback).Run(Result::NewErrorMessage(kErrorNetworkNotConnected));
    return;
  }
  const ash::DeviceState* device =
      network_state_handler->GetDeviceState(network->device_path());
  if (!device) {
    std::move(callback).Run(Result::NewErrorMessage(kErrorNetworkNotConnected));
    return;
  }

  mojom::NetworkDetailsPtr details = mojom::NetworkDetails::New();
  details->mac_address =
      ash::network_util::FormattedMacAddress(device->mac_address());
  net::IPAddress ipv4_address;
  if (ipv4_address.AssignFromIPLiteral(
          device->GetIpAddressByType(shill::kTypeIPv4))) {
    details->ipv4_address = ipv4_address;
  }
  net::IPAddress ipv6_address;
  if (ipv6_address.AssignFromIPLiteral(
          device->GetIpAddressByType(shill::kTypeIPv6))) {
    details->ipv6_address = ipv6_address;
  }

  std::move(callback).Run(
      mojom::GetNetworkDetailsResult::NewNetworkDetails(std::move(details)));
}

}  // namespace crosapi
