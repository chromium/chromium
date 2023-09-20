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

#include "ash/components/arc/mojom/net.mojom.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_service.pb.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace arc::net_utils {

// Translates a shill network state into a mojo NetworkConfigurationPtr.
// This get network properties from NetworkState and populating the
// corresponding fields defined in NetworkConfiguration in mojo.
arc::mojom::NetworkConfigurationPtr TranslateNetworkProperties(
    const ash::NetworkState* network_state,
    const base::Value::Dict* shill_dict);

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

// Translates a vector of NetworkState objects to a
// vector of mojo NetworkConfiguration objects.
std::vector<arc::mojom::NetworkConfigurationPtr> TranslateNetworkStates(
    const std::string& arc_vpn_path,
    const ash::NetworkStateHandler::NetworkStateList& network_states,
    const std::map<std::string, base::Value::Dict>& shill_network_properties,
    const std::vector<patchpanel::NetworkDevice>& devices);

// Convert a vector of subject name match list that containing ":" separated
// string in "Type:Value" format (like DNS:example.com, EMAIL:test@domain.com)
// to a base::Value::List format that is accepted by ONC.
base::Value::List TranslateSubjectNameMatchListToValue(
    const std::vector<std::string>& string_list);

// Translate a mojom socket connection event into a patchpanel socket connection
// event.
std::unique_ptr<patchpanel::SocketConnectionEvent>
TranslateSocketConnectionEvent(const mojom::SocketConnectionEventPtr& mojom);
}  // namespace arc::net_utils

#endif  // ASH_COMPONENTS_ARC_NET_ARC_NET_UTILS_H_
