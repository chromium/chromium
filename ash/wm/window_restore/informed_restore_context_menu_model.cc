// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_context_menu_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/notreached.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

InformedRestoreContextMenuModel::InformedRestoreContextMenuModel()
    : ui::SimpleMenuModel(this) {
  const int group = 0;
  AddTitleWithStringId(IDS_ASH_INFORMED_RESTORE_DIALOG_TITLE);
  AddRadioItemWithStringId(
      static_cast<int>(full_restore::RestoreOption::kAskEveryTime),
      IDS_ASH_INFORMED_RESTORE_DIALOG_CONTEXT_MENU_ASK_OPTION, group);
  AddRadioItemWithStringId(
      static_cast<int>(full_restore::RestoreOption::kAlways),
      IDS_ASH_INFORMED_RESTORE_DIALOG_CONTEXT_MENU_ALWAYS_OPTION, group);
  AddRadioItemWithStringId(
      static_cast<int>(full_restore::RestoreOption::kDoNotRestore),
      IDS_ASH_INFORMED_RESTORE_DIALOG_CONTEXT_MENU_NEVER_OPTION, group);
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
}

InformedRestoreContextMenuModel::~InformedRestoreContextMenuModel() = default;

bool InformedRestoreContextMenuModel::IsCommandIdChecked(int command_id) const {
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

void InformedRestoreContextMenuModel::ExecuteCommand(int command_id,
                                                     int event_flags) {
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
