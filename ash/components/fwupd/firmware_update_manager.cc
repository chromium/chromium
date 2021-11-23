// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "dbus/message.h"

namespace ash {

namespace {

FirmwareUpdateManager* g_instance = nullptr;

}  // namespace

FirmwareUpdateManager::FirmwareUpdate::FirmwareUpdate() = default;
FirmwareUpdateManager::FirmwareUpdate::FirmwareUpdate(FirmwareUpdate&& other) =
    default;
FirmwareUpdateManager::FirmwareUpdate&
FirmwareUpdateManager::FirmwareUpdate::operator=(FirmwareUpdate&& other) =
    default;
FirmwareUpdateManager::FirmwareUpdate::~FirmwareUpdate() = default;

FirmwareUpdateManager::FirmwareUpdateManager() {
  DCHECK(chromeos::FwupdClient::Get());
  chromeos::FwupdClient::Get()->AddObserver(this);

  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

FirmwareUpdateManager::~FirmwareUpdateManager() {
  DCHECK_EQ(this, g_instance);
  chromeos::FwupdClient::Get()->RemoveObserver(this);
  g_instance = nullptr;
}

// static
FirmwareUpdateManager* FirmwareUpdateManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// Query all updates for all devices.
void FirmwareUpdateManager::RequestAllUpdates() {
  DCHECK(devices_pending_update_.empty());
  RequestDevices();
}

void FirmwareUpdateManager::RequestDevices() {
  chromeos::FwupdClient::Get()->RequestDevices();
}

void FirmwareUpdateManager::RequestUpdates(const std::string& device_id) {
  chromeos::FwupdClient::Get()->RequestUpdates(device_id);
}

void FirmwareUpdateManager::InstallUpdate(
    const std::string& device_id,
    base::ScopedFD file_descriptor,
    chromeos::FirmwareInstallOptions options) {
  chromeos::FwupdClient::Get()->InstallUpdate(
      device_id, std::move(file_descriptor), options);
}

void FirmwareUpdateManager::OnDeviceListResponse(
    chromeos::FwupdDeviceList* devices) {
  DCHECK(devices);
  DCHECK(devices_pending_update_.empty());

  // TODO(zentaro): When mojo is implemented, fire the observer with an empty
  // list if there are no devices in the response.
  for (const auto& device : *devices) {
    devices_pending_update_[device.id] = device;
    RequestUpdates(device.id);
  }
}

void FirmwareUpdateManager::OnUpdateListResponse(
    const std::string& device_id,
    chromeos::FwupdUpdateList* updates) {
  DCHECK(updates);
  DCHECK(base::Contains(devices_pending_update_, device_id));

  // If there are updates, then choose the first one.
  if (!updates->empty()) {
    const chromeos::FwupdUpdate& update_details = updates->front();

    // Create a complete FirmwareUpdate and add to updates_.
    FirmwareUpdate update;
    update.device_id = device_id;
    update.device_name = devices_pending_update_[device_id].device_name;
    update.version = update_details.version;
    update.description = update_details.description;
    update.priority = update_details.priority;
    updates_.push_back(std::move(update));
  }

  // Remove the pending device.
  devices_pending_update_.erase(device_id);

  // TODO(zentaro): When mojo is implemented, fire the observer with `updates_`
  // if there are no more devices pending an update.
}

const std::vector<FirmwareUpdateManager::FirmwareUpdate>&
FirmwareUpdateManager::GetCachedUpdatesForTesting() {
  DCHECK(devices_pending_update_.empty());
  return updates_;
}

void FirmwareUpdateManager::OnInstallResponse(bool success) {
  ++on_install_update_response_count_for_testing_;
}

}  // namespace ash
