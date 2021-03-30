// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_networking_attributes/enterprise_networking_attributes_api.h"

#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_networking_attributes.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "components/user_manager/user.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace extensions {

namespace {

const char kErrorUserNotAffiliated[] =
    "Network attributes can only be read by an affiliated user.";
const char kErrorNetworkNotConnected[] =
    "Device is not connected to a network.";

// Checks for the current browser context if the user is affiliated or belongs
// to the sign-in profile.
bool CanGetNetworkAttributesForBrowserContext(
    content::BrowserContext* context) {
  const Profile* profile = Profile::FromBrowserContext(context);

  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return true;

  if (!profile->IsRegularProfile())
    return false;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user->IsAffiliated();
}

}  //  namespace

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

EnterpriseNetworkingAttributesGetNetworkDetailsFunction::
    ~EnterpriseNetworkingAttributesGetNetworkDetailsFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseNetworkingAttributesGetNetworkDetailsFunction::Run() {
  if (!CanGetNetworkAttributesForBrowserContext(browser_context())) {
    return RespondNow(Error(kErrorUserNotAffiliated));
  }

  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* network =
      network_state_handler->DefaultNetwork();
  if (!network) {
    // Not connected to a network.
    return RespondNow(Error(kErrorNetworkNotConnected));
  }
  const chromeos::DeviceState* device =
      network_state_handler->GetDeviceState(network->device_path());
  if (!device) {
    return RespondNow(Error(kErrorNetworkNotConnected));
  }

  const std::string mac_address =
      chromeos::network_util::FormattedMacAddress(device->mac_address());
  const std::string ipv4_address = device->GetIpAddressByType(shill::kTypeIPv4);
  const std::string ipv6_address = device->GetIpAddressByType(shill::kTypeIPv6);

  api::enterprise_networking_attributes::NetworkDetails details;
  details.mac_address = mac_address;
  if (!ipv4_address.empty()) {
    details.ipv4 = std::make_unique<std::string>(ipv4_address);
  }
  if (!ipv6_address.empty()) {
    details.ipv6 = std::make_unique<std::string>(ipv6_address);
  }

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(details.ToValue())));
}

}  // namespace extensions
