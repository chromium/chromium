// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include "base/check_op.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "dbus/message.h"

namespace ash {

namespace {

FirmwareUpdateManager* g_instance = nullptr;

}  // namespace

FirmwareUpdateManager::FirmwareUpdateManager() {
  DCHECK(chromeos::FwupdClient::Get());
  chromeos::FwupdClient::Get()->AddObserver(this);

  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
  g_instance->RequestDevices();
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

void FirmwareUpdateManager::RequestDevices() {
  chromeos::FwupdClient::Get()->RequestDevices();
}

void FirmwareUpdateManager::OnDeviceListResponse(
    chromeos::FwupdDeviceList* devices) {
  DCHECK(devices);
  // TODO(swifton): This is a stub implementation.
  ++on_device_list_response_count_for_testing_;
}

}  // namespace ash
