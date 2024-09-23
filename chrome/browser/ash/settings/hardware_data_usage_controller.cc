// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kPendingPref[] = "pending.cros.reven.enable_hw_data_usage";

}  // namespace

namespace ash {

static HWDataUsageController* g_hw_data_usage_controller = nullptr;

// static
void HWDataUsageController::Initialize(PrefService* local_state) {
  CHECK(!g_hw_data_usage_controller);
  g_hw_data_usage_controller = new HWDataUsageController(local_state);
}

// static
bool HWDataUsageController::IsInitialized() {
  return g_hw_data_usage_controller;
}

// static
void HWDataUsageController::Shutdown() {
  DCHECK(g_hw_data_usage_controller);
  delete g_hw_data_usage_controller;
  g_hw_data_usage_controller = nullptr;
}

// static
HWDataUsageController* HWDataUsageController::Get() {
  CHECK(g_hw_data_usage_controller);
  return g_hw_data_usage_controller;
}

// static
void HWDataUsageController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kPendingPref, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
}

HWDataUsageController::HWDataUsageController(PrefService* local_state)
    : OwnerPendingSettingController(kRevenEnableDeviceHWDataUsage,
                                    kPendingPref,
                                    local_state) {
  setting_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kRevenEnableDeviceHWDataUsage,
      base::BindRepeating(&HWDataUsageController::NotifyObservers,
                          this->as_weak_ptr()));
}

HWDataUsageController::~HWDataUsageController() {
  owner_settings_service_observation_.Reset();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace ash
