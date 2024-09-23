// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_ARC_NET_UTILS_H_
#define ASH_COMPONENTS_ARC_NET_ARC_NET_UTILS_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <map>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/arc_wifi.mojom.h"
#include "ash/components/arc/mojom/net.mojom.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_service.pb.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace arc::net_utils {

// Adds fields from |network_state| into |mojo| NetworkConfiguration.
void FillConfigurationsFromState(const ash::NetworkState* network_state,
                                 const base::Value::Dict* shill_dict,
                                 arc::mojom::NetworkConfiguration* mojo);

// Adds fields from patchpanel's virtual |device| into |mojo|
// NetworkConfiguration.
void FillConfigurationsFromDevice(const patchpanel::NetworkDevice& device,
                                  arc::mojom::NetworkConfiguration* mojo);

// Translates a mojo EapMethod into a shill EAP method.
std::string TranslateEapMethod(arc::mojom::EapMethod method);

// Translates a mojo EapPhase2Method into a shill EAP phase 2 auth type.
std::string TranslateEapPhase2Method(arc::mojom::EapPhase2Method method);

// Translates a mojo EapMethod into a ONC EAP method.
std::string TranslateEapMethodToOnc(arc::mojom::EapMethod method);

// Translates a mojo EapPhase2Method into a ONC EAP phase 2 auth type.
std::string TranslateEapPhase2MethodToOnc(arc::mojom::EapPhase2Method method);

// Translates a mojo KeyManagement into a shill kEapKeyMgmtProperty value.
std::string TranslateKeyManagement(mojom::KeyManagement management);

// Translates a mojo KeyManagement into a ONC value.
std::string TranslateKeyManagementToOnc(mojom::KeyManagement management);
// Translates a shill security class into a mojom SecurityType.
arc::mojom::SecurityType TranslateWiFiSecurity(
    const std::string& security_class);

// Translates a shill connection state into a mojo ConnectionStateType.
// This is effectively the inverse function of shill.Service::GetStateString
// defined in platform2/shill/service.cc, with in addition some of shill's
// connection states translated to the same ConnectionStateType value.
arc::mojom::ConnectionStateType TranslateConnectionState(
    const std::string& state);

// Translates a shill technology type into a mojom NetworkType.
arc::mojom::NetworkType TranslateNetworkType(const std::string& type);

// Translates a vector of NetworkStates to a vector of NetworkConfigurations.
// For each state, fill the fields in NetworkConfiguration.
std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkStates(
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& network_states,
    const std::map<std::string, base::Value::Dict>& shill_network_properties);

// Translates a vector of NetworkStates to a vector of ScanResults.
// For each state, fill the fields in ScanResult.
// TODO(b/329552433): Move this method to a separate util file for WiFi.
std::vector<arc::mojom::WifiScanResultPtr> TranslateScanResults(
    const ash::NetworkStateHandler::NetworkStateList& network_states);

// Translates a vector of NetworkDevices to a vector of NetworkConfigurations.
// For each device, fill the fields in NetworkConfiguration. For each active
// state, the corresponding fields of the associated device is added. A state
// without a device (e.g: host VPN) is also attached to the result.
std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkDevices(
    const std::vector<patchpanel::NetworkDevice>& devices,
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& active_network_states,
    const std::map<std::string, base::Value::Dict>& shill_network_properties);

// Convert a vector of subject name match list that containing ":" separated
// string in "Type:Value" format (like DNS:example.com, EMAIL:test@domain.com)
// to a base::Value::List format that is accepted by ONC.
base::Value::List TranslateSubjectNameMatchListToValue(
    const std::vector<std::string>& string_list);

// Translate a mojom socket connection event into a patchpanel socket connection
// event.
std::unique_ptr<patchpanel::SocketConnectionEvent>
TranslateSocketConnectionEvent(const mojom::SocketConnectionEventPtr& mojom);

// Duplicate of ARC's ArcNetworkUtils#areConfigurationsEquivalent. This is meant
// as a short-term solution to prevent spurious mojo calls to ARC if we know ARC
// is going to ignore the configuration update anyways. ARC's implementation
// should be the source of truth. See b/342973880 for more details.
bool AreConfigurationsEquivalent(
    std::vector<arc::mojom::NetworkConfigurationPtr>& latest_networks,
    std::vector<arc::mojom::NetworkConfigurationPtr>& cached_networks);
}  // namespace arc::net_utils

#endif  // ASH_COMPONENTS_ARC_NET_ARC_NET_UTILS_H_
