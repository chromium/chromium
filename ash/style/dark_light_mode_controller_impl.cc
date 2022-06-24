// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dark_light_mode_controller_impl.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/style/dark_light_mode_nudge_controller.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

DarkLightModeControllerImpl* g_instance = nullptr;

}  // namespace

DarkLightModeControllerImpl::DarkLightModeControllerImpl()
    : ScheduledFeature(prefs::kDarkModeEnabled,
                       prefs::kDarkModeScheduleType,
                       std::string(),
                       std::string()),
      nudge_controller_(std::make_unique<DarkLightModeNudgeController>()) {
  DCHECK(!g_instance);
  g_instance = this;
}

DarkLightModeControllerImpl::~DarkLightModeControllerImpl() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
DarkLightModeControllerImpl* DarkLightModeControllerImpl::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void DarkLightModeControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDarkModeScheduleType,
      static_cast<int>(ScheduleType::kSunsetToSunrise));

  registry->RegisterIntegerPref(prefs::kDarkLightModeNudge,
                                kDarkLightModeNudgeMaxShownCount);
}

void DarkLightModeControllerImpl::SetAutoScheduleEnabled(bool enabled) {
  SetScheduleType(enabled ? ScheduleType::kSunsetToSunrise
                          : ScheduleType::kNone);
}

bool DarkLightModeControllerImpl::GetAutoScheduleEnabled() const {
  const ScheduleType type = GetScheduleType();
  // `DarkLightModeControllerImpl` does not support the custom scheduling.
  DCHECK_NE(type, ScheduleType::kCustom);
  return type == ScheduleType::kSunsetToSunrise;
}

void DarkLightModeControllerImpl::ToggledByUser() {
  nudge_controller_->ToggledByUser();
}

void DarkLightModeControllerImpl::SetShowNudgeForTesting(bool value) {
  nudge_controller_->set_show_nudge_for_testing(value);  // IN-TEST
}

void DarkLightModeControllerImpl::RefreshFeatureState() {}

void DarkLightModeControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state == session_manager::SessionState::ACTIVE)
    nudge_controller_->MaybeShowNudge();
}

const char* DarkLightModeControllerImpl::GetFeatureName() const {
  return "DarkLightModeControllerImpl";
}

}  // namespace ash
