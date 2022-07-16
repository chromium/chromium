// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
#define ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_

#include "base/component_export.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "chromeos/dbus/fwupd/fwupd_device.h"
#include "dbus/message.h"

namespace ash {
// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(ASH_FIRMWARE_UPDATE_MANAGER) FirmwareUpdateManager
    : public chromeos::FwupdClient::Observer {
 public:
  // Query the fwupd DBus client for currently connected devices.
  void RequestDevices();

  // FwupdClient::Observer:
  // When the fwupd DBus client gets a response with devices from fwupd,
  // it calls this function and passes the response.
  void OnDeviceListResponse(chromeos::FwupdDeviceList* devices) override;

  FirmwareUpdateManager();
  FirmwareUpdateManager(const FirmwareUpdateManager&) = delete;
  FirmwareUpdateManager& operator=(const FirmwareUpdateManager&) = delete;
  ~FirmwareUpdateManager() override;

  // Gets the global instance pointer.
  static FirmwareUpdateManager* Get();

 protected:
  friend class FirmwareUpdateManagerTest;
  // Temporary auxiliary variable for testing.
  // TODO(swifton): Replace with mock observers.
  int on_device_list_response_count_for_testing_ = 0;
};
}  // namespace ash

#endif  // ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
