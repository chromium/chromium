// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_controller.h"

#include "ash/components/device_activity/device_activity_client.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {
namespace device_activity {

namespace {
DeviceActivityController* g_ash_device_activity_controller = nullptr;
}  // namespace

DeviceActivityController* DeviceActivityController::Get() {
  return g_ash_device_activity_controller;
}

// static
void DeviceActivityController::RegisterPrefs(PrefRegistrySimple* registry) {
  const base::Time unix_epoch = base::Time::UnixEpoch();
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownMonthlyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownAllTimePingTimestamp,
                             unix_epoch);
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
    da_client_network_ = std::make_unique<DeviceActivityClient>(
        chromeos::NetworkHandler::Get()->network_state_handler());
  }
}

void DeviceActivityController::Stop(Trigger t) {
  if (t == Trigger::kNetwork && da_client_network_) {
    da_client_network_.reset();
  }
}

}  // namespace device_activity
}  // namespace ash
