// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multitask_menu_nudge_delegate_ash.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

std::string GetShowCountPrefName(bool tablet_mode) {
  return tablet_mode
             ? chromeos::MultitaskMenuNudgeController::kTabletShownCountPrefName
             : chromeos::MultitaskMenuNudgeController::
                   kClamshellShownCountPrefName;
}

std::string GetLastShownPrefName(bool tablet_mode) {
  return tablet_mode
             ? chromeos::MultitaskMenuNudgeController::kTabletLastShownPrefName
             : chromeos::MultitaskMenuNudgeController::
                   kClamshellLastShownPrefName;
}

}  // namespace

MultitaskMenuNudgeDelegateAsh::MultitaskMenuNudgeDelegateAsh() = default;

MultitaskMenuNudgeDelegateAsh::~MultitaskMenuNudgeDelegateAsh() = default;

bool MultitaskMenuNudgeDelegateAsh::IsRegularUser() const {
  auto* session_controller = Shell::Get()->session_controller();
  const absl::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  return user_type == user_manager::USER_TYPE_REGULAR;
}

int MultitaskMenuNudgeDelegateAsh::GetShowCount(bool tablet_mode) const {
  return GetPrefService()->GetInteger(GetShowCountPrefName(tablet_mode));
}

void MultitaskMenuNudgeDelegateAsh::SetShowCount(int count, bool tablet_mode) {
  GetPrefService()->SetInteger(GetShowCountPrefName(tablet_mode), count);
}

base::Time MultitaskMenuNudgeDelegateAsh::GetLastShownTime(
    bool tablet_mode) const {
  return GetPrefService()->GetTime(GetLastShownPrefName(tablet_mode));
}

void MultitaskMenuNudgeDelegateAsh::SetLastShownTime(base::Time time,
                                                     bool tablet_mode) {
  GetPrefService()->SetTime(GetLastShownPrefName(tablet_mode), time);
}

}  // namespace ash
