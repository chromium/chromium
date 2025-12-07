// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_networking_attributes/enterprise_networking_attributes_api.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/enterprise_networking_attributes.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "components/user_manager/user.h"

namespace extensions {
namespace {

constexpr char kErrorUserNotAffiliated[] =
    "Network attributes can only be read by an affiliated user.";
constexpr char kErrorNetworkNotConnected[] =
    "Device is not connected to a network.";

bool IsAccessAllowed(Profile* profile) {
  if (ash::IsSigninBrowserContext(profile)) {
    return true;
  }

  if (profile->IsOffTheRecord()) {
    return false;
  }

  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (!user) {
    return false;
  }
  return user->IsAffiliated();
}

}  // namespace

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    ~EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseNetworkingAttributesGetNetworkDetailsFunction::Run() {
  // TODO(crbug.com/354842935): Use Profile returned by browser_context().
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!IsAccessAllowed(profile)) {
    return RespondNow(Error(kErrorUserNotAffiliated));
  }

  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  if (!network) {
    // Not connected to a network.
    return RespondNow(Error(kErrorNetworkNotConnected));
  }
  const ash::DeviceState* device =
      network_state_handler->GetDeviceState(network->device_path());
  if (!device) {
    return RespondNow(Error(kErrorNetworkNotConnected));
  }

  api::enterprise_networking_attributes::NetworkDetails network_details;
  network_details.mac_address =
      ash::network_util::FormattedMacAddress(device->mac_address());
  if (std::string ipv4 = device->GetIpAddressByType(shill::kTypeIPv4);
      !ipv4.empty()) {
    network_details.ipv4 = std::move(ipv4);
  }
  if (std::string ipv6 = device->GetIpAddressByType(shill::kTypeIPv6);
      !ipv6.empty()) {
    network_details.ipv6 = std::move(ipv6);
  }

  return RespondNow(WithArguments(network_details.ToValue()));
}

}  // namespace extensions
