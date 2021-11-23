// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
#define ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "chromeos/dbus/fwupd/fwupd_device.h"
#include "chromeos/dbus/fwupd/fwupd_update.h"

namespace ash {
// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(ASH_FIRMWARE_UPDATE_MANAGER) FirmwareUpdateManager
    : public chromeos::FwupdClient::Observer {
 public:
  // TODO(zentaro): Replace this struct with mojo struct when implemented.
  struct FirmwareUpdate {
    FirmwareUpdate();
    FirmwareUpdate(FirmwareUpdate&& other);
    FirmwareUpdate& operator=(FirmwareUpdate&& other);
    ~FirmwareUpdate();

    std::string device_id;
    std::string device_name;
    std::string version;
    std::string description;
    uint32_t priority;
  };

  FirmwareUpdateManager();
  FirmwareUpdateManager(const FirmwareUpdateManager&) = delete;
  FirmwareUpdateManager& operator=(const FirmwareUpdateManager&) = delete;
  ~FirmwareUpdateManager() override;

  // Gets the global instance pointer.
  static FirmwareUpdateManager* Get();

  // FwupdClient::Observer:
  // When the fwupd DBus client gets a response with devices from fwupd,
  // it calls this function and passes the response.
  void OnDeviceListResponse(chromeos::FwupdDeviceList* devices) override;

  // When the fwupd DBus client gets a response with updates from fwupd,
  // it calls this function and passes the response.
  void OnUpdateListResponse(const std::string& device_id,
                            chromeos::FwupdUpdateList* updates) override;
  void OnInstallResponse(bool success) override;

  // Query all updates for all devices.
  void RequestAllUpdates();

  // Get the currently cached set of updates.
  // TODO(zentaro): Remove once mojo api fires observers.
  const std::vector<FirmwareUpdate>& GetCachedUpdatesForTesting();

 private:
  friend class FirmwareUpdateManagerTest;
  // Query the fwupd DBus client for currently connected devices.
  void RequestDevices();

  // Query the fwupd DBus client for updates for a certain device.
  void RequestUpdates(const std::string& device_id);

  // Query the fwupd DBus client to install an update for a certain device.
  void InstallUpdate(const std::string& device_id,
                     base::ScopedFD file_descriptor,
                     chromeos::FirmwareInstallOptions options);

  // Map of a device ID to `FwupdDevice` which is waiting for the list
  // of updates.
  base::flat_map<std::string, chromeos::FwupdDevice> devices_pending_update_;

  // List of all available updates. If `devices_pending_update_` is not
  // empty then this list is not yet complete.
  std::vector<FirmwareUpdate> updates_;

 protected:
  friend class FirmwareUpdateManagerTest;
  // Temporary auxiliary variables for testing.
  // TODO(swifton): Replace with mock observers.
  int on_device_list_response_count_for_testing_ = 0;
  int on_update_list_response_count_for_testing_ = 0;
  int on_install_update_response_count_for_testing_ = 0;
};
}  // namespace ash

#endif  // ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
