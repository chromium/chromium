// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_restore_util.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace desks_restore_util {

namespace {

// While restore is in progress, changes are being made to the desks and their
// names. Those changes should not trigger an update to the prefs.
bool g_pause_desks_prefs_updates = false;

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Check if the desk index is valid against a list of existing desks in
// DesksController.
bool IsValidDeskIndex(int desk_index) {
  return desk_index >= 0 &&
         desk_index < int{DesksController::Get()->desks().size()} &&
         desk_index < int{desks_util::GetMaxNumberOfDesks()};
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  constexpr int kDefaultActiveDeskIndex = 0;
  registry->RegisterListPref(prefs::kDesksNamesList);
  if (features::IsBentoEnabled()) {
    registry->RegisterIntegerPref(prefs::kDesksActiveDesk,
                                  kDefaultActiveDeskIndex);
  }
}

void RestorePrimaryUserDesks() {
  base::AutoReset<bool> in_progress(&g_pause_desks_prefs_updates, true);

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  const base::ListValue* desks_names_list =
      primary_user_prefs->GetList(prefs::kDesksNamesList);

  // First create the same number of desks.
  const size_t restore_size = desks_names_list->GetSize();

  // If we don't have any restore data, or the list is corrupt for some reason,
  // abort.
  if (!restore_size || restore_size > desks_util::GetMaxNumberOfDesks())
    return;

  auto* desks_controller = DesksController::Get();
  while (desks_controller->desks().size() < restore_size)
    desks_controller->NewDesk(DesksCreationRemovalSource::kDesksRestore);

  size_t index = 0;
  for (const auto& value : desks_names_list->GetList()) {
    const std::string& desk_name = value.GetString();
    // Empty desks names are just place holders for desks whose names haven't
    // been modified by the user. Those don't need to be restored; they already
    // have the correct default names based on their positions in the desks
    // list.
    if (!desk_name.empty()) {
      desks_controller->RestoreNameOfDeskAtIndex(base::UTF8ToUTF16(desk_name),
                                                 index);
    }
    ++index;
  }

  // Restore an active desk for the primary user.
  if (features::IsBentoEnabled()) {
    const int active_desk_index =
        primary_user_prefs->GetInteger(prefs::kDesksActiveDesk);

    // A crash in between prefs::kDesksNamesList and prefs::kDesksActiveDesk
    // can cause an invalid active desk index.
    if (!IsValidDeskIndex(active_desk_index))
      return;

    desks_controller->RestorePrimaryUserActiveDeskIndex(active_desk_index);
  }
}

void UpdatePrimaryUserDesksPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ListPrefUpdate update(primary_user_prefs, prefs::kDesksNamesList);
  base::ListValue* pref_data = update.Get();
  pref_data->Clear();
  const auto& desks = DesksController::Get()->desks();
  for (const auto& desk : desks) {
    // Desks whose names were not changed by the user, are stored as empty
    // strings. They're just place holders to restore the correct desks count.
    // RestorePrimaryUserDesks() restores only non-empty desks names.
    pref_data->Append(desk->is_name_set_by_user()
                          ? base::UTF16ToUTF8(desk->name())
                          : std::string());
  }

  DCHECK_EQ(pref_data->GetSize(), desks.size());
}

void UpdatePrimaryUserActiveDeskPrefs(int active_desk_index) {
  DCHECK(features::IsBentoEnabled());
  DCHECK(Shell::Get()->session_controller()->IsUserPrimary());
  DCHECK(IsValidDeskIndex(active_desk_index));
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  primary_user_prefs->SetInteger(prefs::kDesksActiveDesk, active_desk_index);
}

}  // namespace desks_restore_util

}  // namespace ash
