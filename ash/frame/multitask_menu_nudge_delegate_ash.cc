// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/multitask_menu_nudge_delegate_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

std::string GetShowCountPrefName(bool tablet_mode) {
  return tablet_mode ? prefs::kMultitaskMenuNudgeTabletShownCount
                     : prefs::kMultitaskMenuNudgeClamshellShownCount;
}

std::string GetLastShownPrefName(bool tablet_mode) {
  return tablet_mode ? prefs::kMultitaskMenuNudgeTabletLastShown
                     : prefs::kMultitaskMenuNudgeClamshellLastShown;
}

}  // namespace

MultitaskMenuNudgeDelegateAsh::MultitaskMenuNudgeDelegateAsh() = default;

MultitaskMenuNudgeDelegateAsh::~MultitaskMenuNudgeDelegateAsh() = default;

int MultitaskMenuNudgeDelegateAsh::GetTabletNudgeYOffset() const {
  return kTabletNudgeAdditionalYOffset +
         TabletModeMultitaskCueController::kCueHeight +
         TabletModeMultitaskCueController::kCueYOffset;
}

void MultitaskMenuNudgeDelegateAsh::GetNudgePreferences(
    bool tablet_mode,
    GetPreferencesCallback callback) {
  const int shown_count =
      GetPrefService()->GetInteger(GetShowCountPrefName(tablet_mode));
  const base::Time last_shown_time =
      GetPrefService()->GetTime(GetLastShownPrefName(tablet_mode));
  std::move(callback).Run(
      tablet_mode,
      chromeos::MultitaskMenuNudgeController::PrefValues{
          .show_count = shown_count, .last_shown_time = last_shown_time});
}

void MultitaskMenuNudgeDelegateAsh::SetNudgePreferences(bool tablet_mode,
                                                        int count,
                                                        base::Time time) {
  GetPrefService()->SetInteger(GetShowCountPrefName(tablet_mode), count);
  GetPrefService()->SetTime(GetLastShownPrefName(tablet_mode), time);
}

}  // namespace ash
