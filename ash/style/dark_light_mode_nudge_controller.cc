// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dark_light_mode_nudge_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/dark_light_mode_nudge.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

PrefService* GetActiveUserPrefService() {
  return DarkLightModeControllerImpl::Get()->active_user_pref_service();
}

void SetRemainingShownCount(int count) {
  PrefService* prefs = GetActiveUserPrefService();
  if (prefs)
    prefs->SetInteger(prefs::kDarkLightModeNudgeLeftToShowCount, count);
}

}  // namespace

DarkLightModeNudgeController::DarkLightModeNudgeController() = default;

DarkLightModeNudgeController::~DarkLightModeNudgeController() = default;

// static
int DarkLightModeNudgeController::GetRemainingShownCount() {
  const PrefService* prefs = GetActiveUserPrefService();
  return prefs ? prefs->GetInteger(prefs::kDarkLightModeNudgeLeftToShowCount)
               : 0;
}

void DarkLightModeNudgeController::MaybeShowNudge() {
  if (!ShouldShowNudge())
    return;

  const int shown_count = GetRemainingShownCount();
  ShowNudge();
  SetRemainingShownCount(shown_count - 1);
}

void DarkLightModeNudgeController::ToggledByUser() {
  SetRemainingShownCount(0);
}

std::unique_ptr<SystemNudge> DarkLightModeNudgeController::CreateSystemNudge() {
  return std::make_unique<DarkLightModeNudge>();
}

bool DarkLightModeNudgeController::ShouldShowNudge() const {
  if (!chromeos::features::IsDarkLightModeEnabled())
    return false;

  // Do not show the nudge if it is set to be hidden in the tests.
  if (hide_nudge_for_testing_)
    return false;

  // Do not show if the command line flag to hide nudges is set.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshNoNudges))
    return false;

  auto* session_controller = Shell::Get()->session_controller();
  if (!session_controller->IsActiveUserSessionStarted())
    return false;

  absl::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  // Must have a `user_type` because of the active user session check above.
  DCHECK(user_type);
  switch (*user_type) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
      // We only allow regular and child accounts to see the nudge.
      break;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::NUM_USER_TYPES:
      return false;
  }

  return GetRemainingShownCount() > 0;
}

}  // namespace ash
