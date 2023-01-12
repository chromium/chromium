// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/hats_bluetooth_revamp_trigger_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

namespace {
// Timeout used to wait before displaying survey.
constexpr const base::TimeDelta kHatsSurveyTimeout = base::Minutes(5);
}  // namespace

HatsBluetoothRevampTriggerImpl::HatsBluetoothRevampTriggerImpl() = default;

HatsBluetoothRevampTriggerImpl::~HatsBluetoothRevampTriggerImpl() = default;

// static
void HatsBluetoothRevampTriggerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::prefs::kUserPairedWithFastPair,
                                /*default_value=*/false);
}

void HatsBluetoothRevampTriggerImpl::TryToShowSurvey() {
  Profile* profile = GetActiveUserProfile();
  if (!profile) {
    HID_LOG(DEBUG) << "HaTS survey not shown. No available profile";
    return;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    HID_LOG(DEBUG) << "HaTS survey not shown, current sesssion is not active";
    return;
  }

  // The survey has already been shown and should not be shown again.
  if (hats_notification_controller_) {
    HID_LOG(DEBUG) << "HaTS survey has already been shown.";
    return;
  }

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  if (pref_service->GetBoolean(ash::prefs::kUserPairedWithFastPair)) {
    HID_LOG(DEBUG) << "HaTS survey not shown, user has visited Fast pair flow";
    return;
  }

  // Checks prefs to make sure a survey hasn't already been shown to the user
  // this survey cycle, and rolls a die to determine if the survey should be
  // shown.
  if (!HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsBluetoothRevampSurvey)) {
    HID_LOG(DEBUG) << "HaTS survey not shown, current profile not selected.";
    return;
  }

  if (hats_timer_.IsRunning()) {
    HID_LOG(DEBUG) << "HaTS survey not shown, timer is already running.";
    return;
  }

  HID_LOG(EVENT) << "HaTS survey timer started.";

  // Wait for `kHatsSurveyTimeout` before showing survey.
  hats_timer_.Start(FROM_HERE, kHatsSurveyTimeout,
                    base::BindOnce(&HatsBluetoothRevampTriggerImpl::ShowSurvey,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void HatsBluetoothRevampTriggerImpl::ShowSurvey() {
  HID_LOG(EVENT) << "HaTS survey is being shown.";
  hats_notification_controller_ =
      base::MakeRefCounted<ash::HatsNotificationController>(
          GetActiveUserProfile(), kHatsBluetoothRevampSurvey);
}

Profile* HatsBluetoothRevampTriggerImpl::GetActiveUserProfile() {
  if (did_set_profile_for_testing_) {
    return profile_for_testing_;
  }

  return ProfileManager::GetActiveUserProfile();
}

}  // namespace ash
