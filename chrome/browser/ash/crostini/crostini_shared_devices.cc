// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_shared_devices.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"

namespace crostini {

CrostiniSharedDevices::CrostiniSharedDevices(Profile* profile)
    : profile_(profile) {
  // There is a risk that after a Chrome crash and restart, we won't recognize
  // an already running container for which we have not yet applied user prefs.
  // In this case, there is little risk as CrostiniRecoveryView should have been
  // shown to tell the user they need to restart their container.
  guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
      ->AddContainerStartedObserver(this);
}

CrostiniSharedDevices::~CrostiniSharedDevices() = default;

bool CrostiniSharedDevices::IsVmDeviceShared(guest_os::GuestId container_id,
                                             const std::string& vm_device) {
  const base::Value* shared_devices = GetContainerPrefValue(
      profile_, container_id, guest_os::prefs::kContainerSharedVmDevicesKey);
  if (!shared_devices || !shared_devices->is_dict()) {
    VLOG(2) << "No shared_devices dict for " << container_id;
    return false;
  }
  auto opt_shared = shared_devices->GetDict().FindBool(vm_device);
  return opt_shared.has_value() && opt_shared.value();
}

void CrostiniSharedDevices::SetVmDeviceShared(guest_os::GuestId container_id,
                                              const std::string& vm_device,
                                              bool shared,
                                              ResultCallback callback) {
  const base::Value* prev_shared_devices = guest_os::GetContainerPrefValue(
      profile_, container_id, guest_os::prefs::kContainerSharedVmDevicesKey);
  base::Value::Dict shared_devices;
  if (prev_shared_devices && prev_shared_devices->is_dict()) {
    shared_devices = prev_shared_devices->GetDict().Clone();
  }
  shared_devices.Set(vm_device, shared);

  if (guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
          ->IsRunning(container_id)) {
    ApplySharingState(std::move(container_id), std::move(shared_devices),
                      std::move(callback));
  } else {
    VLOG(2) << "Not applying yet, container not running " << container_id;
    guest_os::MergeContainerPref(profile_, container_id,
                                 guest_os::prefs::kContainerSharedVmDevicesKey,
                                 std::move(shared_devices));
    std::move(callback).Run(false);
  }
}

void CrostiniSharedDevices::ApplySharingState(
    const guest_os::GuestId container_id,
    base::Value::Dict next_shared_devices,
    ResultCallback callback) {
  if (next_shared_devices.empty()) {
    VLOG(2) << "Nothing to apply";
    return;
  }
  vm_tools::cicerone::UpdateContainerDevicesRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);

  auto* updates = request.mutable_updates();
  for (auto it : next_shared_devices) {
    auto opt_shared = it.second.GetIfBool();
    vm_tools::cicerone::VmDeviceAction action =
        opt_shared.has_value() && opt_shared.value()
            ? vm_tools::cicerone::VmDeviceAction::ENABLE
            : vm_tools::cicerone::VmDeviceAction::DISABLE;
    (*updates)[it.first] = action;
  }

  ash::CiceroneClient::Get()->UpdateContainerDevices(
      request,
      base::BindOnce(&CrostiniSharedDevices::OnUpdateContainerDevices,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id),
                     std::move(next_shared_devices), std::move(callback)));
}

void CrostiniSharedDevices::OnUpdateContainerDevices(
    const guest_os::GuestId container_id,
    base::Value::Dict next_shared_devices,
    ResultCallback callback,
    std::optional<vm_tools::cicerone::UpdateContainerDevicesResponse>
        response) {
  bool success = true;
  if (!response.has_value()) {
    LOG(ERROR) << "No UpdateContainerDevicesResponse received.";
    success = false;
  } else if (response->status() !=
             vm_tools::cicerone::UpdateContainerDevicesResponse::OK) {
    LOG(ERROR)
        << "UpdateContainerDevices failed, error "
        << vm_tools::cicerone::UpdateContainerDevicesResponse_Status_Name(
               response->status());
    success = false;
  }

  if (success) {
    for (const auto& it : response->results()) {
      const base::Value* prev_shared_devices = guest_os::GetContainerPrefValue(
          profile_, container_id,
          guest_os::prefs::kContainerSharedVmDevicesKey);
      const auto& vm_device = it.first;
      auto result = it.second;
      switch (result) {
        case vm_tools::cicerone::UpdateContainerDevicesResponse::ACTION_FAILED:
          LOG(WARNING) << "trying to restore old pref value for " << it.first;
          if (prev_shared_devices && prev_shared_devices->is_dict()) {
            // Preserve/restore the old pref value.
            auto opt_shared =
                prev_shared_devices->GetDict().FindBool(vm_device);
            if (opt_shared.has_value()) {
              next_shared_devices.Set(vm_device, opt_shared.value());
            }
          } else {
            next_shared_devices.Remove(vm_device);
          }
          break;
        case vm_tools::cicerone::UpdateContainerDevicesResponse::
            NO_SUCH_VM_DEVICE:
          LOG(WARNING) << "VM has no such device " << it.first;
          next_shared_devices.Set(vm_device, false);
          break;
        case vm_tools::cicerone::UpdateContainerDevicesResponse::SUCCESS:
        default:
          VLOG(2) << "applied pref for " << it.first;
          break;
      }
    }
    guest_os::MergeContainerPref(profile_, container_id,
                                 guest_os::prefs::kContainerSharedVmDevicesKey,
                                 std::move(next_shared_devices));
  }
  std::move(callback).Run(success);
}

void CrostiniSharedDevices::OnContainerStarted(
    const guest_os::GuestId& container_id) {
  const base::Value* prev_shared_devices = guest_os::GetContainerPrefValue(
      profile_, container_id, guest_os::prefs::kContainerSharedVmDevicesKey);
  base::Value::Dict shared_devices;
  if (prev_shared_devices && prev_shared_devices->is_dict()) {
    shared_devices = prev_shared_devices->GetDict().Clone();
  }
  VLOG(2) << "Applying device sharing prefs for " << container_id;
  ApplySharingState(std::move(container_id), std::move(shared_devices),
                    base::DoNothing());
}

}  // namespace crostini
