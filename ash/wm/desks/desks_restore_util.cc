// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_restore_util.h"

#include <tuple>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "base/auto_reset.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::desks_restore_util {

namespace {

// |kDesksMetricsList| stores a list of dictionaries with the following
// key value pairs (<key> : <entry>):
// |kCreationTimeKey| : an int which represents the number of minutes for
// base::Time::FromDeltaSinceWindowsEpoch().
// |kFirstDayVisitedKey| : an int which represents the number of days since
// local epoch (Jan 1, 2010).
// |kLastDayVisitedKey| : an int which represents the number of days since
// local epoch (Jan 1, 2010).
// |kInteractedWithThisWeekKey| : a boolean tracking whether this desk has been
// interacted with in the last week.
constexpr char kCreationTimeKey[] = "creation_time";
constexpr char kFirstDayVisitedKey[] = "first_day";
constexpr char kLastDayVisitedKey[] = "last_day";
constexpr char kInteractedWithThisWeekKey[] = "interacted_week";

// |kDesksWeeklyActiveDesksMetrics| stores a dictionary with the following key
// value pairs (<key> : <entry>):
// |kWeeklyActiveDesksKey| : an int representing the number of weekly active
// desks.
// |kReportTimeKey| : an int representing the time a user's weekly active desks
// metric is scheduled to go off at. The value is the time left on the
// scheduler + the user's current time stored as the number of minutes for
// base::Time::FromDeltaSinceWindowsEpoch().
constexpr char kWeeklyActiveDesksKey[] = "weekly_active_desks";
constexpr char kReportTimeKey[] = "report_time";

// While restore is in progress, changes are being made to the desks and their
// names. Those changes should not trigger an update to the prefs.
bool g_pause_desks_prefs_updates = false;

// A clock that can be overridden by tests. This is a global variable, reset it
// to nullptr when overridden is not needed anymore.
base::Clock* g_override_clock_ = nullptr;

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Check if the desk index is valid against a list of existing desks in
// DesksController.
bool IsValidDeskIndex(int desk_index) {
  return desk_index >= 0 &&
         desk_index < static_cast<int>(DesksController::Get()->desks().size());
}

// Returns Jan 1, 2010 00:00:00 as a base::Time object in the local timezone.
base::Time GetLocalEpoch() {
  static const base::Time local_epoch = [] {
    base::Time local_epoch;
    std::ignore = base::Time::FromLocalExploded({2010, 1, 5, 1, 0, 0, 0, 0},
                                                &local_epoch);
    return local_epoch;
  }();
  return local_epoch;
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  constexpr int kDefaultActiveDeskIndex = 0;
  registry->RegisterListPref(prefs::kDesksNamesList);
  registry->RegisterListPref(prefs::kDesksGuidsList);
  registry->RegisterListPref(prefs::kDesksLacrosProfileIdList);
  registry->RegisterListPref(prefs::kDesksMetricsList);
  registry->RegisterDictionaryPref(prefs::kDesksWeeklyActiveDesksMetrics);
  registry->RegisterIntegerPref(prefs::kDesksActiveDesk,
                                kDefaultActiveDeskIndex);
}

void RestorePrimaryUserDesks() {
  base::AutoReset<bool> in_progress(&g_pause_desks_prefs_updates, true);

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  const base::Value::List& desks_names_list =
      primary_user_prefs->GetList(prefs::kDesksNamesList);
  const base::Value::List& desks_guids_list =
      primary_user_prefs->GetList(prefs::kDesksGuidsList);
  const base::Value::List& desks_lacros_profile_ids_list =
      primary_user_prefs->GetList(prefs::kDesksLacrosProfileIdList);
  const base::Value::List& desks_metrics_list =
      primary_user_prefs->GetList(prefs::kDesksMetricsList);

  // First create the same number of desks.
  size_t restore_size = desks_names_list.size();

  // If we don't have any restore data, abort.
  if (restore_size == 0)
    return;

  // If we have more restore data than the *current* max, clamp it. This can
  // happen if the restore data was created when more desks were permitted.
  restore_size = std::min(restore_size, desks_util::GetMaxNumberOfDesks());

  auto* desks_controller = DesksController::Get();
  while (desks_controller->desks().size() < restore_size)
    desks_controller->NewDesk(DesksCreationRemovalSource::kDesksRestore);

  const size_t desks_metrics_list_size = desks_metrics_list.size();
  const auto now = base::Time::Now();
  for (size_t index = 0; index < restore_size; ++index) {
    const std::string& desk_name = desks_names_list[index].GetString();
    // Empty desks names are just place holders for desks whose names haven't
    // been modified by the user. Those don't need to be restored; they already
    // have the correct default names based on their positions in the desks
    // list.
    if (!desk_name.empty()) {
      desks_controller->RestoreNameOfDeskAtIndex(base::UTF8ToUTF16(desk_name),
                                                 index);
    }
    // It's possible that desks_guids_list is not yet populated.
    if (index < desks_guids_list.size()) {
      desks_controller->RestoreGuidOfDeskAtIndex(
          base::Uuid::ParseLowercase(desks_guids_list[index].GetString()),
          index);
    }

    if (index < desks_lacros_profile_ids_list.size()) {
      uint64_t lacros_profile_id = 0;
      if (base::StringToUint64(desks_lacros_profile_ids_list[index].GetString(),
                               &lacros_profile_id)) {
        desks_controller->GetDeskAtIndex(index)->SetLacrosProfileId(
            lacros_profile_id, /*source=*/std::nullopt,
            /*skip_prefs_update=*/true);
      }
    }

    // Only restore metrics if there is existing data.
    if (index >= desks_metrics_list_size)
      continue;

    const auto& desks_metrics_dict = desks_metrics_list[index].GetDict();

    // Restore creation time.
    const auto& creation_time_entry =
        desks_metrics_dict.FindInt(kCreationTimeKey);
    if (creation_time_entry.has_value()) {
      const auto creation_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Minutes(*creation_time_entry));
      if (!creation_time.is_null() && creation_time < now)
        desks_controller->RestoreCreationTimeOfDeskAtIndex(creation_time,
                                                           index);
    }

    // Restore consecutive daily metrics.
    const auto& first_day_visited_entry =
        desks_metrics_dict.FindInt(kFirstDayVisitedKey);
    const int first_day_visited = first_day_visited_entry.value_or(-1);

    const auto& last_day_visited_entry =
        desks_metrics_dict.FindInt(kLastDayVisitedKey);
    const int last_day_visited = last_day_visited_entry.value_or(-1);

    if (first_day_visited <= last_day_visited && first_day_visited != -1 &&
        last_day_visited != -1) {
      // Only restore the values if they haven't been corrupted.
      desks_controller->RestoreVisitedMetricsOfDeskAtIndex(
          first_day_visited, last_day_visited, index);
    }

    // Restore weekly active desks metrics.
    const auto& interacted_with_this_week_entry =
        desks_metrics_dict.FindBool(kInteractedWithThisWeekKey);
    const bool interacted_with_this_week =
        interacted_with_this_week_entry.value_or(false);
    if (interacted_with_this_week) {
      desks_controller->RestoreWeeklyInteractionMetricOfDeskAtIndex(
          interacted_with_this_week, index);
    }
  }

  // Restore an active desk for the primary user.
  const int active_desk_index =
      primary_user_prefs->GetInteger(prefs::kDesksActiveDesk);

  // A crash in between prefs::kDesksNamesList and prefs::kDesksActiveDesk
  // can cause an invalid active desk index.
  if (!IsValidDeskIndex(active_desk_index))
    return;

  desks_controller->RestorePrimaryUserActiveDeskIndex(active_desk_index);

  // Restore weekly active desks metrics.
  auto& weekly_active_desks_dict =
      primary_user_prefs->GetDict(prefs::kDesksWeeklyActiveDesksMetrics);
  const int report_time =
      weekly_active_desks_dict.FindInt(kReportTimeKey).value_or(-1);
  const int num_weekly_active_desks =
      weekly_active_desks_dict.FindInt(kWeeklyActiveDesksKey).value_or(-1);

  // Discard stored metrics if either are corrupted.
  if (report_time != -1 && num_weekly_active_desks != -1) {
    desks_controller->RestoreWeeklyActiveDesksMetrics(
        num_weekly_active_desks,
        base::Time::FromDeltaSinceWindowsEpoch(base::Minutes(report_time)));
  }
}

void UpdatePrimaryUserDeskNamesPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ScopedListPrefUpdate name_update(primary_user_prefs, prefs::kDesksNamesList);
  base::Value::List& name_pref_data = name_update.Get();
  name_pref_data.clear();

  const auto& desks = DesksController::Get()->desks();
  for (const auto& desk : desks) {
    // Desks whose names were not changed by the user, are stored as empty
    // strings. They're just place holders to restore the correct desks count.
    // RestorePrimaryUserDesks() restores only non-empty desks names.
    name_pref_data.Append(desk->is_name_set_by_user()
                              ? base::UTF16ToUTF8(desk->name())
                              : std::string());
  }

  DCHECK_EQ(name_pref_data.size(), desks.size());
}

void UpdatePrimaryUserDeskGuidsPrefs() {
  if (g_pause_desks_prefs_updates) {
    return;
  }

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ScopedListPrefUpdate guid_update(primary_user_prefs, prefs::kDesksGuidsList);
  base::Value::List& guid_pref_data = guid_update.Get();
  guid_pref_data.clear();

  const auto& desks = DesksController::Get()->desks();
  for (const auto& desk : desks) {
    guid_pref_data.Append(desk->uuid().AsLowercaseString());
  }

  DCHECK_EQ(guid_pref_data.size(), desks.size());
}

void UpdatePrimaryUserDeskLacrosProfileIdPrefs() {
  if (g_pause_desks_prefs_updates) {
    return;
  }

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  ScopedListPrefUpdate id_update(primary_user_prefs,
                                 prefs::kDesksLacrosProfileIdList);
  base::Value::List& id_pref_data = id_update.Get();
  id_pref_data.clear();

  for (const auto& desk : DesksController::Get()->desks()) {
    // Lacros profile IDs are 64-bit unsigned integers, which will fall outside
    // of the range of `int`. We therefore store them as strings.
    id_pref_data.Append(base::NumberToString(desk->lacros_profile_id()));
  }
}

void UpdatePrimaryUserDeskMetricsPrefs() {
  if (g_pause_desks_prefs_updates)
    return;

  PrefService* primary_user_prefs = GetPrimaryUserPrefService();
  if (!primary_user_prefs) {
    // Can be null in tests.
    return;
  }

  // Save per-desk metrics.
  ScopedListPrefUpdate metrics_update(primary_user_prefs,
                                      prefs::kDesksMetricsList);
  base::Value::List& metrics_pref_data = metrics_update.Get();
  metrics_pref_data.clear();

  auto* desks_controller = DesksController::Get();
  const auto& desks = desks_controller->desks();
  for (const auto& desk : desks) {
    base::Value::Dict metrics_dict =
        base::Value::Dict()
            .Set(kCreationTimeKey,
                 desk->creation_time().ToDeltaSinceWindowsEpoch().InMinutes())
            .Set(kFirstDayVisitedKey, desk->first_day_visited())
            .Set(kLastDayVisitedKey, desk->last_day_visited())
            .Set(kInteractedWithThisWeekKey, desk->interacted_with_this_week());
    metrics_pref_data.Append(std::move(metrics_dict));
  }

  DCHECK_EQ(metrics_pref_data.size(), desks.size());

  // Save weekly active report time.
  ScopedDictPrefUpdate weekly_active_desks_update(
      primary_user_prefs, prefs::kDesksWeeklyActiveDesksMetrics);
  weekly_active_desks_update->SetByDottedPath(
      kReportTimeKey, desks_controller->GetWeeklyActiveReportTime()
                          .ToDeltaSinceWindowsEpoch()
                          .InMinutes());
  weekly_active_desks_update->SetByDottedPath(kWeeklyActiveDesksKey,
                                              Desk::GetWeeklyActiveDesks());
}

void UpdatePrimaryUserActiveDeskPrefs(int active_desk_index) {
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

const base::Time GetTimeNow() {
  return g_override_clock_ ? g_override_clock_->Now() : base::Time::Now();
}

int GetDaysFromLocalEpoch() {
  return (GetTimeNow() - GetLocalEpoch()).InDays();
}

void OverrideClockForTesting(base::Clock* test_clock) {
  g_override_clock_ = test_clock;
}

}  // namespace ash::desks_restore_util
