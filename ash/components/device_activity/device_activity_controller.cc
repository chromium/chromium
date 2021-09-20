// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_controller.h"

#include "ash/components/device_activity/device_activity_client.h"
#include "base/check_op.h"

namespace ash {
namespace device_activity {

namespace {
DeviceActivityController* g_ash_device_activity_controller = nullptr;
}  // namespace

DeviceActivityController* DeviceActivityController::Get() {
  return g_ash_device_activity_controller;
}

DeviceActivityController::DeviceActivityController() {
  DCHECK(!g_ash_device_activity_controller);
  g_ash_device_activity_controller = this;
}

DeviceActivityController::~DeviceActivityController() {
  DCHECK_EQ(this, g_ash_device_activity_controller);
  Stop(Trigger::kNetwork);
  g_ash_device_activity_controller = nullptr;
}

void DeviceActivityController::Start(Trigger t) {
  if (t == Trigger::kNetwork) {
    da_client_network_ = std::make_unique<DeviceActivityClient>(t);
  }
}

void DeviceActivityController::Stop(Trigger t) {
  if (t == Trigger::kNetwork && da_client_network_) {
    da_client_network_.reset();
  }
}

}  // namespace device_activity
}  // namespace ash
