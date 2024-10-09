// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"

#include <fcntl.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace crostini {

// Currently, we are not supporting ethernet/mlan/usb port forwarding.
const char kDefaultInterfaceToForward[] = "wlan0";
const char kPortNumberKey[] = "port_number";
const char kPortProtocolKey[] = "protocol_type";
const char kPortLabelKey[] = "label";
const char kPortContainerIdKey[] = "container_id";

CrostiniPortForwarder::CrostiniPortForwarder(Profile* profile)
    : profile_(profile) {
  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  ash::NetworkStateHandler::NetworkStateList active_networks;

  // Get Physical networks only (so no Tether/VPN).
  network_state_handler->GetActiveNetworkListByType(
      ash::NetworkTypePattern::Physical(), &active_networks);

  if (active_networks.empty()) {
    current_interface_ = kDefaultInterfaceToForward;
    ip_address_ = "";
  } else {
    // Select the first active network for now.
    const ash::DeviceState* device = network_state_handler->GetDeviceState(
        active_networks[0]->device_path());
    if (device) {
      current_interface_ = device->interface();
      ip_address_ = device->GetIpAddressByType(shill::kTypeIPv4);
      if (ip_address_.empty()) {
        ip_address_ = device->GetIpAddressByType(shill::kTypeIPv6);
      }
    } else {
      ip_address_ = "";
      current_interface_ = kDefaultInterfaceToForward;
    }
  }
}

CrostiniPortForwarder::~CrostiniPortForwarder() = default;

void CrostiniPortForwarder::SignalActivePortsChanged() {
  for (auto& observer : observers_) {
    observer.OnActivePortsChanged(GetActivePorts());
  }
}

bool CrostiniPortForwarder::MatchPortRuleDict(const base::Value& dict,
                                              const PortRuleKey& key) {
  std::optional<int> port_number = dict.GetDict().FindInt(kPortNumberKey);
  std::optional<int> protocol_type = dict.GetDict().FindInt(kPortProtocolKey);
  return (port_number && port_number.value() == key.port_number) &&
         (protocol_type &&
          protocol_type.value() == static_cast<int>(key.protocol_type)) &&
         guest_os::GuestId(dict) == guest_os::GuestId(key.container_id);
}

bool CrostiniPortForwarder::MatchPortRuleContainerId(
    const base::Value& dict,
    const guest_os::GuestId& container_id) {
  return guest_os::GuestId(dict) == container_id;
}

void CrostiniPortForwarder::AddNewPortPreference(const PortRuleKey& key,
                                                 const std::string& label) {
  PrefService* pref_service = profile_->GetPrefs();
  ScopedListPrefUpdate update(pref_service,
                              crostini::prefs::kCrostiniPortForwarding);
  base::Value::List& all_ports = update.Get();
  base::Value::Dict new_port_metadata;
  new_port_metadata.Set(kPortNumberKey, key.port_number);
  new_port_metadata.Set(kPortProtocolKey, static_cast<int>(key.protocol_type));
  new_port_metadata.Set(kPortLabelKey, label);
  new_port_metadata.Set(guest_os::prefs::kVmNameKey, key.container_id.vm_name);
  new_port_metadata.Set(guest_os::prefs::kContainerNameKey,
                        key.container_id.container_name);
  all_ports.Append(std::move(new_port_metadata));
}

bool CrostiniPortForwarder::RemovePortPreference(const PortRuleKey& key) {
  PrefService* pref_service = profile_->GetPrefs();
  ScopedListPrefUpdate update(pref_service,
                              crostini::prefs::kCrostiniPortForwarding);
  base::Value::List& update_list = update.Get();
  auto it = base::ranges::find_if(update_list, [&key, this](const auto& dict) {
    return MatchPortRuleDict(dict, key);
  });
  if (it == update_list.end()) {
    return false;
  }
  update_list.erase(it);
  return true;
}

std::optional<base::Value> CrostiniPortForwarder::ReadPortPreference(
    const PortRuleKey& key) {
  PrefService* pref_service = profile_->GetPrefs();
  const base::Value::List& all_ports =
      pref_service->GetList(crostini::prefs::kCrostiniPortForwarding);
  auto it = base::ranges::find_if(all_ports, [&key, this](const auto& dict) {
    return MatchPortRuleDict(dict, key);
  });
  if (it == all_ports.end()) {
    return std::nullopt;
  }
  return std::optional<base::Value>(it->Clone());
}

void CrostiniPortForwarder::OnActivatePortCompleted(
    ResultCallback result_callback,
    PortRuleKey key,
    bool success) {
  if (!success) {
    forwarded_ports_.erase(key);
  }
  std::move(result_callback).Run(success);
  SignalActivePortsChanged();
}

void CrostiniPortForwarder::OnRemoveOrDeactivatePortCompleted(
    ResultCallback result_callback,
    PortRuleKey key,
    bool success) {
  forwarded_ports_.erase(key);
  std::move(result_callback).Run(success);
  SignalActivePortsChanged();
}

void CrostiniPortForwarder::TryActivatePort(
    const PortRuleKey& key,
    const guest_os::GuestId& container_id,
    base::OnceCallback<void(bool)> result_callback) {
  auto info =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)->GetInfo(
          container_id);
  if (!info) {
    LOG(ERROR) << "Inactive container to make port rules for.";
    std::move(result_callback).Run(false);
    return;
  }

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    std::move(result_callback).Run(false);
    return;
  }

  int lifeline[2] = {-1, -1};
  if (pipe(lifeline) < 0) {
    LOG(ERROR) << "Failed to create a lifeline pipe";
    std::move(result_callback).Run(false);
    return;
  }

  base::ScopedFD lifeline_local(lifeline[0]);
  base::ScopedFD lifeline_remote(lifeline[1]);

  forwarded_ports_[key] = std::move(lifeline_local);

  // TODO(matterchen): Determining how to request all interfaces dynamically.
  switch (key.protocol_type) {
    case Protocol::TCP:
      client->RequestTcpPortForward(
          key.port_number, current_interface_, info->ipv4_address,
          key.port_number, lifeline_remote.get(), std::move(result_callback));
      break;
    case Protocol::UDP:
      client->RequestUdpPortForward(
          key.port_number, current_interface_, info->ipv4_address,
          key.port_number, lifeline_remote.get(), std::move(result_callback));
      break;
  }
}

void CrostiniPortForwarder::TryDeactivatePort(
    const PortRuleKey& key,
    const guest_os::GuestId& container_id,
    base::OnceCallback<void(bool)> result_callback) {
  bool running = guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
                     ->IsRunning(container_id);
  if (!running) {
    LOG(ERROR) << "Inactive container to make port rules for.";
    std::move(result_callback).Run(false);
    return;
  }

  if (forwarded_ports_.find(key) == forwarded_ports_.end()) {
    LOG(ERROR) << "Port is already inactive.";
    std::move(result_callback).Run(false);
    return;
  }

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    std::move(result_callback).Run(false);
    return;
  }

  // TODO(matterchen): Determining how to release all interfaces.
  switch (key.protocol_type) {
    case Protocol::TCP:
      client->ReleaseTcpPortForward(key.port_number, current_interface_,
                                    std::move(result_callback));
      break;
    case Protocol::UDP:
      client->ReleaseUdpPortForward(key.port_number, current_interface_,
                                    std::move(result_callback));
      break;
  }
}

void CrostiniPortForwarder::AddPort(const guest_os::GuestId& container_id,
                                    uint16_t port_number,
                                    const Protocol& protocol_type,
                                    const std::string& label,
                                    ResultCallback result_callback) {
  PortRuleKey new_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .container_id = container_id,
  };

  if (ReadPortPreference(new_port_key)) {
    LOG(ERROR) << "Trying to add port which already exists.";
    std::move(result_callback).Run(false);
    return;
  }
  AddNewPortPreference(new_port_key, label);
  ActivatePort(container_id, port_number, protocol_type,
               std::move(result_callback));
}

void CrostiniPortForwarder::ActivatePort(const guest_os::GuestId& container_id,
                                         uint16_t port_number,
                                         const Protocol& protocol_type,
                                         ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .container_id = container_id,
  };

  if (!ReadPortPreference(existing_port_key)) {
    LOG(ERROR) << "Trying to activate port not found in preferences.";
    std::move(result_callback).Run(false);
    return;
  }
  if (forwarded_ports_.find(existing_port_key) != forwarded_ports_.end()) {
    LOG(ERROR) << "Trying to activate already active port.";
    std::move(result_callback).Run(false);
    return;
  }

  base::OnceCallback<void(bool)> on_activate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnActivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryActivatePort(existing_port_key, container_id,
                                         std::move(on_activate_port_completed));
}

void CrostiniPortForwarder::DeactivatePort(
    const guest_os::GuestId& container_id,
    uint16_t port_number,
    const Protocol& protocol_type,
    ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .container_id = container_id,
  };

  if (!ReadPortPreference(existing_port_key)) {
    LOG(ERROR) << "Trying to deactivate port not found in preferences.";
    std::move(result_callback).Run(false);
    return;
  }
  base::OnceCallback<void(bool)> on_deactivate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnRemoveOrDeactivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryDeactivatePort(
      existing_port_key, container_id, std::move(on_deactivate_port_completed));
}

void CrostiniPortForwarder::RemovePort(const guest_os::GuestId& container_id,
                                       uint16_t port_number,
                                       const Protocol& protocol_type,
                                       ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .container_id = container_id,
  };

  if (!RemovePortPreference(existing_port_key)) {
    LOG(ERROR) << "Trying to remove port not found in preferences.";
    std::move(result_callback).Run(false);
    return;
  }
  base::OnceCallback<void(bool)> on_remove_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnRemoveOrDeactivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryDeactivatePort(existing_port_key, container_id,
                                           std::move(on_remove_port_completed));
}

void CrostiniPortForwarder::DeactivateAllActivePorts(
    const guest_os::GuestId& container_id) {
  auto it = forwarded_ports_.begin();
  while (it != forwarded_ports_.end()) {
    if (it->first.container_id == container_id) {
      TryDeactivatePort(it->first, container_id, base::DoNothing());
      it = forwarded_ports_.erase(it);
    } else {
      ++it;
    }
  }
  SignalActivePortsChanged();
}

void CrostiniPortForwarder::RemoveAllPorts(
    const guest_os::GuestId& container_id) {
  PrefService* pref_service = profile_->GetPrefs();
  ScopedListPrefUpdate update(pref_service,
                              crostini::prefs::kCrostiniPortForwarding);
  update->EraseIf([&container_id, this](const auto& dict) {
    return MatchPortRuleContainerId(dict, container_id);
  });

  DeactivateAllActivePorts(container_id);
}

base::Value::List CrostiniPortForwarder::GetActivePorts() {
  base::Value::List forwarded_ports_list;
  for (const auto& port : forwarded_ports_) {
    base::Value::Dict port_info;
    port_info.Set(kPortNumberKey, port.first.port_number);
    port_info.Set(kPortProtocolKey, static_cast<int>(port.first.protocol_type));
    port_info.Set(kPortContainerIdKey, port.first.container_id.ToDictValue());
    forwarded_ports_list.Append(std::move(port_info));
  }
  return forwarded_ports_list;
}

base::Value::List CrostiniPortForwarder::GetActiveNetworkInfo() {
  base::Value::List network_info;
  network_info.Append(base::Value(current_interface_));
  network_info.Append(base::Value(ip_address_));
  return network_info;
}

size_t CrostiniPortForwarder::GetNumberOfForwardedPortsForTesting() {
  return forwarded_ports_.size();
}

std::optional<base::Value> CrostiniPortForwarder::ReadPortPreferenceForTesting(
    const PortRuleKey& key) {
  return ReadPortPreference(key);
}

void CrostiniPortForwarder::UpdateActivePortInterfaces() {
  for (auto& port : forwarded_ports_) {
    // Note that this process erases the current lifeline attached to the port
    // rule and implicitly causes the current port rule to be revoked.
    TryActivatePort(port.first, port.first.container_id, base::DoNothing());
  }
}

void CrostiniPortForwarder::ActiveNetworksChanged(
    const std::string& interface,
    const std::string& ip_address) {
  if (interface.empty()) {
    return;
  }
  if (interface == current_interface_) {
    return;
  }
  current_interface_ = interface;
  ip_address_ = ip_address;
  UpdateActivePortInterfaces();

  for (auto& observer : observers_) {
    observer.OnActiveNetworkChanged(base::Value(interface),
                                    base::Value(ip_address));
  }
}

}  // namespace crostini
