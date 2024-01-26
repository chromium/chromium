// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_provider_api.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "chrome/common/extensions/api/vpn_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace {

namespace api_vpn = extensions::api::vpn_provider;

const char kCIDRSeperator[] = "/";

bool CheckIPCIDRSanity(const std::string& value, bool cidr, bool ipv6) {
  int dots = ipv6 ? 0 : 3;
  int sep = cidr ? 1 : 0;
  int colon = ipv6 ? 7 : 0;
  bool hex_allowed = ipv6;
  int counter = 0;

  for (const auto& elem : value) {
    if (base::IsAsciiDigit(elem)) {
      counter++;
      continue;
    }
    if (elem == '.') {
      if (!dots)
        return false;
      dots--;
    } else if (elem == kCIDRSeperator[0]) {
      if (!sep || dots || colon == 7 || !counter)
        return false;
      // Separator observed, no more dots and colons, only digits are allowed
      // after observing separator. So setting hex_allowed to false.
      sep--;
      counter = 0;
      colon = 0;
      hex_allowed = false;
    } else if (elem == ':') {
      if (!colon)
        return false;
      colon--;
    } else if (!hex_allowed || !base::IsHexDigit(elem)) {
      return false;
    } else {
      counter++;
    }
  }
  return !sep && !dots && (colon < 7) && counter;
}

bool CheckIPCIDRSanityList(const std::vector<std::string>& list,
                           bool cidr,
                           bool ipv6) {
  for (const auto& address : list) {
    if (!CheckIPCIDRSanity(address, cidr, ipv6)) {
      return false;
    }
  }
  return true;
}

void ConvertParameters(const api_vpn::Parameters& parameters,
                       base::Value::Dict* parameter_value,
                       std::string* error) {
  if (!CheckIPCIDRSanity(parameters.address, true /* CIDR */,
                         false /*IPV4 */)) {
    *error = "Address CIDR sanity check failed.";
    return;
  }

  if (!CheckIPCIDRSanityList(parameters.exclusion_list, true /* CIDR */,
                             false /*IPV4 */)) {
    *error = "Exclusion list CIDR sanity check failed.";
    return;
  }

  if (!CheckIPCIDRSanityList(parameters.inclusion_list, true /* CIDR */,
                             false /*IPV4 */)) {
    *error = "Inclusion list CIDR sanity check failed.";
    return;
  }

  if (!CheckIPCIDRSanityList(parameters.dns_servers, false /* Not CIDR */,
                             false /*IPV4 */)) {
    *error = "DNS server IP sanity check failed.";
    return;
  }

  std::vector<std::string> cidr_parts =
      base::SplitString(parameters.address, kCIDRSeperator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  CHECK_EQ(2u, cidr_parts.size());

  parameter_value->Set(shill::kAddressParameterThirdPartyVpn, cidr_parts[0]);

  parameter_value->Set(shill::kSubnetPrefixParameterThirdPartyVpn,
                       cidr_parts[1]);

  std::string ip_delimiter(1, shill::kIPDelimiter);
  parameter_value->Set(
      shill::kExclusionListParameterThirdPartyVpn,
      base::JoinString(parameters.exclusion_list, ip_delimiter));

  parameter_value->Set(
      shill::kInclusionListParameterThirdPartyVpn,
      base::JoinString(parameters.inclusion_list, ip_delimiter));

  if (parameters.mtu) {
    parameter_value->Set(shill::kMtuParameterThirdPartyVpn, *parameters.mtu);
  }

  if (parameters.broadcast_address) {
    parameter_value->Set(shill::kBroadcastAddressParameterThirdPartyVpn,
                         *parameters.broadcast_address);
  }

  std::string non_ip_delimiter(1, shill::kNonIPDelimiter);
  if (parameters.domain_search) {
    parameter_value->Set(
        shill::kDomainSearchParameterThirdPartyVpn,
        base::JoinString(*parameters.domain_search, non_ip_delimiter));
  }

  parameter_value->Set(shill::kDnsServersParameterThirdPartyVpn,
                       base::JoinString(parameters.dns_servers, ip_delimiter));

  if (parameters.reconnect) {
    parameter_value->Set(shill::kReconnectParameterThirdPartyVpn,
                         *parameters.reconnect);
  }

  return;
}

}  // namespace

VpnThreadExtensionFunction::~VpnThreadExtensionFunction() = default;

void VpnThreadExtensionFunction::SignalCallCompletionSuccess() {
  Respond(NoArguments());
}

void VpnThreadExtensionFunction::SignalCallCompletionSuccessWithId(
    const std::string& configuration_name) {
  Respond(WithArguments(configuration_name));
}

void VpnThreadExtensionFunction::SignalCallCompletionFailure(
    const std::string& error_name,
    const std::string& error_message) {
  if (!error_name.empty() && !error_message.empty()) {
    Respond(Error(error_name + ": " + error_message));
  } else if (!error_name.empty()) {
    Respond(Error(error_name));
  } else {
    Respond(Error(error_message));
  }
}

VpnProviderCreateConfigFunction::~VpnProviderCreateConfigFunction() = default;

ExtensionFunction::ResponseAction VpnProviderCreateConfigFunction::Run() {
  std::optional<api_vpn::CreateConfig::Params> params =
      api_vpn::CreateConfig::Params::Create(args());
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnServiceInterface* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->CreateConfiguration(
      extension_id(), params->name,
      base::BindOnce(
          &VpnProviderCreateConfigFunction::SignalCallCompletionSuccessWithId,
          this, params->name),
      base::BindOnce(&VpnProviderNotifyConnectionStateChangedFunction::
                         SignalCallCompletionFailure,
                     this));

  return RespondLater();
}

VpnProviderDestroyConfigFunction::~VpnProviderDestroyConfigFunction() = default;

ExtensionFunction::ResponseAction VpnProviderDestroyConfigFunction::Run() {
  std::optional<api_vpn::DestroyConfig::Params> params =
      api_vpn::DestroyConfig::Params::Create(args());
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnServiceInterface* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->DestroyConfiguration(
      extension_id(), params->id,
      base::BindOnce(
          &VpnProviderDestroyConfigFunction::SignalCallCompletionSuccess, this),
      base::BindOnce(&VpnProviderNotifyConnectionStateChangedFunction::
                         SignalCallCompletionFailure,
                     this));

  return RespondLater();
}

VpnProviderSetParametersFunction::~VpnProviderSetParametersFunction() = default;

ExtensionFunction::ResponseAction VpnProviderSetParametersFunction::Run() {
  std::optional<api_vpn::SetParameters::Params> params =
      api_vpn::SetParameters::Params::Create(args());
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnServiceInterface* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  base::Value::Dict parameter_value;
  std::string error;
  ConvertParameters(params->parameters, &parameter_value, &error);
  if (!error.empty()) {
    return RespondNow(Error(std::move(error)));
  }

  service->SetParameters(
      extension_id(), std::move(parameter_value),
      base::BindOnce(
          &VpnProviderSetParametersFunction::SignalCallCompletionSuccess, this),
      base::BindOnce(&VpnProviderNotifyConnectionStateChangedFunction::
                         SignalCallCompletionFailure,
                     this));

  return RespondLater();
}

VpnProviderSendPacketFunction::~VpnProviderSendPacketFunction() = default;

ExtensionFunction::ResponseAction VpnProviderSendPacketFunction::Run() {
  std::optional<api_vpn::SendPacket::Params> params =
      api_vpn::SendPacket::Params::Create(args());
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnServiceInterface* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  service->SendPacket(
      extension_id(),
      std::vector<char>(params->data.begin(), params->data.end()),
      base::BindOnce(
          &VpnProviderSendPacketFunction::SignalCallCompletionSuccess, this),
      base::BindOnce(&VpnProviderNotifyConnectionStateChangedFunction::
                         SignalCallCompletionFailure,
                     this));

  return RespondLater();
}

VpnProviderNotifyConnectionStateChangedFunction::
    ~VpnProviderNotifyConnectionStateChangedFunction() = default;

ExtensionFunction::ResponseAction
VpnProviderNotifyConnectionStateChangedFunction::Run() {
  std::optional<api_vpn::NotifyConnectionStateChanged::Params> params =
      api_vpn::NotifyConnectionStateChanged::Params::Create(args());
  if (!params) {
    return RespondNow(Error("Invalid arguments."));
  }

  chromeos::VpnServiceInterface* service =
      chromeos::VpnServiceFactory::GetForBrowserContext(browser_context());
  if (!service) {
    return RespondNow(Error("Invalid profile."));
  }

  // Cannot be VPN_CONNECTION_STATE_NONE at this point -- see !params guard
  // above.
  bool connection_success =
      params->state == api_vpn::VpnConnectionState::kConnected;
  service->NotifyConnectionStateChanged(
      extension_id(), connection_success,
      base::BindOnce(&VpnProviderNotifyConnectionStateChangedFunction::
                         SignalCallCompletionSuccess,
                     this),
      base::BindOnce(&VpnProviderNotifyConnectionStateChangedFunction::
                         SignalCallCompletionFailure,
                     this));

  return RespondLater();
}

}  // namespace extensions
