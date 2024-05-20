// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_service.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::on_device_controls {

// static
void AppControlsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kOnDeviceAppControlsPin, std::string());
  registry->RegisterBooleanPref(prefs::kOnDeviceAppControlsSetupCompleted,
                                false);
}

AppControlsService::AppControlsService() = default;

AppControlsService::~AppControlsService() = default;

}  // namespace ash::on_device_controls
