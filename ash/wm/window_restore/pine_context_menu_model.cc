// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_context_menu_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/notreached.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

PineContextMenuModel::PineContextMenuModel() : ui::SimpleMenuModel(this) {
  const int group = 0;
  AddRadioItem(static_cast<int>(full_restore::RestoreOption::kAskEveryTime),
               u"Ask every time", group);
  AddRadioItem(static_cast<int>(full_restore::RestoreOption::kAlways),
               u"Always restore", group);
  AddRadioItem(static_cast<int>(full_restore::RestoreOption::kDoNotRestore),
               u"Off", group);
}

PineContextMenuModel::~PineContextMenuModel() = default;

bool PineContextMenuModel::IsCommandIdChecked(int command_id) const {
  CHECK_GE(static_cast<int>(full_restore::RestoreOption::kMaxValue),
           command_id);

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(pref_service);

  // Get the current restore behavior from preferences and check it against the
  // command id.
  const int restore_behavior =
      pref_service->GetInteger(prefs::kRestoreAppsAndPagesPrefName);
  switch (static_cast<full_restore::RestoreOption>(restore_behavior)) {
    case full_restore::RestoreOption::kAskEveryTime:
    case full_restore::RestoreOption::kAlways:
    case full_restore::RestoreOption::kDoNotRestore:
      return command_id == restore_behavior;
  }
}

void PineContextMenuModel::ExecuteCommand(int command_id, int event_flags) {
  CHECK_GE(static_cast<int>(full_restore::RestoreOption::kMaxValue),
           command_id);

  switch (static_cast<full_restore::RestoreOption>(command_id)) {
    case full_restore::RestoreOption::kAskEveryTime:
    case full_restore::RestoreOption::kAlways:
    case full_restore::RestoreOption::kDoNotRestore:
      // Set the restore behavior in preferences.
      PrefService* pref_service =
          Shell::Get()->session_controller()->GetActivePrefService();
      CHECK(pref_service);
      pref_service->SetInteger(prefs::kRestoreAppsAndPagesPrefName, command_id);
      break;
  }
}

}  // namespace ash
