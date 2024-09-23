// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_nudge_controller.h"

#include <memory>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {
namespace {

// Flags to enable/disable the app list nudges for test.
bool g_reorder_nudge_disabled_for_test = false;
bool g_privacy_notice_accepted_for_test = false;

// Reorder nudge dictionary pref keys.
constexpr char kReorderNudgeShownCount[] = "shown_count";
constexpr char kReorderNudgeConfirmed[] = "confirmed";

// Privacy notice dictionary pref keys.
const char kPrivacyNoticeAcceptedKey[] = "accepted";
const char kPrivacyNoticeShownKey[] = "shown";

// Maximum number of times that the nudge is showing to users.
constexpr int kMaxShowCount = 3;

// Returns the last active user pref service.
PrefService* GetPrefs() {
  if (!Shell::HasInstance())
    return nullptr;

  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

// Returns the preference path string that corresponds to nudge type `type`.
std::string GetPrefPath(AppListNudgeController::NudgeType type) {
  switch (type) {
    case AppListNudgeController::NudgeType::kReorderNudge:
      return prefs::kAppListReorderNudge;
    default:
      NOTREACHED();
  }
}

// Returns true if the app list has been reordered before.
bool WasAppListReorderedPreviously(PrefService* prefs) {
  const base::Value::Dict& dictionary =
      prefs->GetDict(prefs::kAppListReorderNudge);
  return dictionary.FindBool(kReorderNudgeConfirmed).value_or(false);
}

}  // namespace

AppListNudgeController::AppListNudgeController() = default;

// static
void AppListNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAppListReorderNudge);
  registry->RegisterDictionaryPref(prefs::kLauncherFilesPrivacyNotice);
}

// static
void AppListNudgeController::ResetPrefsForNewUserSession(PrefService* prefs) {
  prefs->ClearPref(prefs::kAppListReorderNudge);
  prefs->ClearPref(prefs::kLauncherFilesPrivacyNotice);
}

// static
int AppListNudgeController::GetShownCount(PrefService* prefs, NudgeType type) {
  const base::Value::Dict& dictionary = prefs->GetDict(GetPrefPath(type));

  return dictionary.FindInt(kReorderNudgeShownCount).value_or(0);
}

// static
void AppListNudgeController::SetReorderNudgeDisabledForTest(bool is_disabled) {
  g_reorder_nudge_disabled_for_test = is_disabled;
}

// static
void AppListNudgeController::SetPrivacyNoticeAcceptedForTest(bool is_disabled) {
  g_privacy_notice_accepted_for_test = is_disabled;
}

bool AppListNudgeController::ShouldShowReorderNudge() const {
  if (g_reorder_nudge_disabled_for_test)
    return false;

  PrefService* prefs = GetPrefs();
  if (!prefs)
    return false;

  // Don't show the reorder nudge if the privacy notice is showing.
  if (current_nudge_ == NudgeType::kPrivacyNotice)
    return false;

  // Don't show the reorder nudge if the tutorial nudge is showing.
  if (current_nudge_ == NudgeType::kTutorialNudge) {
    return false;
  }

  if (GetShownCount(prefs, NudgeType::kReorderNudge) < kMaxShowCount &&
      !WasAppListReorderedPreviously(prefs)) {
    return true;
  }

  return false;
}

void AppListNudgeController::OnTemporarySortOrderChanged(
    const std::optional<AppListSortOrder>& new_order) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  // Record the reorder action so that the nudge view won't be showing anymore.
  ScopedDictPrefUpdate update(prefs, prefs::kAppListReorderNudge);
  update->Set(kReorderNudgeConfirmed, true);
}

void AppListNudgeController::SetPrivacyNoticeAcceptedPref(bool accepted) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  {
    ScopedDictPrefUpdate privacy_pref_update(
        prefs, prefs::kLauncherFilesPrivacyNotice);
    privacy_pref_update->Set(kPrivacyNoticeAcceptedKey, accepted);
  }
}

void AppListNudgeController::SetPrivacyNoticeShownPref(bool shown) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  ScopedDictPrefUpdate privacy_pref_update(prefs,
                                           prefs::kLauncherFilesPrivacyNotice);
  privacy_pref_update->Set(kPrivacyNoticeShownKey, shown);
}

bool AppListNudgeController::IsPrivacyNoticeAccepted() const {
  if (g_privacy_notice_accepted_for_test)
    return true;

  const PrefService* prefs = GetPrefs();
  if (!prefs)
    return false;

  return prefs->GetDict(prefs::kLauncherFilesPrivacyNotice)
      .FindBool(kPrivacyNoticeAcceptedKey)
      .value_or(false);
}

bool AppListNudgeController::WasPrivacyNoticeShown() const {
  const PrefService* prefs = GetPrefs();
  if (!prefs)
    return false;

  return prefs->GetDict(prefs::kLauncherFilesPrivacyNotice)
      .FindBool(kPrivacyNoticeShownKey)
      .value_or(false);
}

void AppListNudgeController::SetPrivacyNoticeShown(bool shown) {
  DCHECK(current_nudge_ != NudgeType::kReorderNudge);

  current_nudge_ = shown ? NudgeType::kPrivacyNotice : NudgeType::kNone;
}

void AppListNudgeController::SetNudgeVisible(bool is_nudge_visible,
                                             NudgeType type) {
  // Do not update the state and prefs if it didn't change.
  if (is_visible_ == is_nudge_visible && is_active_ == is_nudge_visible &&
      current_nudge_ == type) {
    return;
  }

  // All NudgeType transition must start from or end to kNone to make sure the
  // prefs is correctly recorded.
  DCHECK(current_nudge_ == NudgeType::kNone || type == NudgeType::kNone ||
         current_nudge_ == type);

  const bool is_visible_updated = is_visible_ != is_nudge_visible;
  const bool is_active_updated = is_active_ != is_nudge_visible;
  is_visible_ = is_nudge_visible;
  // `is_active_` should be updated along with `is_visible_` if visibility
  // updates.
  is_active_ = is_visible_;
  current_nudge_ = type;

  UpdateCurrentNudgeStateInPrefs(is_visible_updated, is_active_updated);
}

void AppListNudgeController::SetNudgeActive(bool is_nudge_active,
                                            NudgeType type) {
  // Do not update the state and prefs if it didn't change.
  if (is_active_ == is_nudge_active && current_nudge_ == type)
    return;

  // All NudgeType transition must start from or end to kNone to make sure the
  // prefs is correctly recorded.
  DCHECK(current_nudge_ == NudgeType::kNone || type == NudgeType::kNone ||
         current_nudge_ == type);

  // The nudge must be visible to change its active state.
  DCHECK(is_visible_);

  const bool is_active_updated = is_active_ != is_nudge_active;
  current_nudge_ = type;
  is_active_ = is_nudge_active;

  UpdateCurrentNudgeStateInPrefs(false, is_active_updated);
}

void AppListNudgeController::OnReorderNudgeConfirmed() {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  // Record the nudge as confirmed so that it will not show up again.
  ScopedDictPrefUpdate update(prefs, prefs::kAppListReorderNudge);
  update->Set(kReorderNudgeConfirmed, true);
}

void AppListNudgeController::UpdateCurrentNudgeStateInPrefs(
    bool is_visible_updated,
    bool is_active_updated) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  // Handle the case where the nudge is active to the users.
  if (is_active_) {
    switch (current_nudge_) {
      case NudgeType::kReorderNudge: {
        // Only reset the timer if the `is_active_` state is updated.
        if (is_active_updated)
          current_nudge_show_timestamp_ = base::Time::Now();
        break;
      }
      case NudgeType::kPrivacyNotice:
      case NudgeType::kTutorialNudge:
      case NudgeType::kNone:
        break;
    }
    return;
  }

  // Handle the case where the nudge is not active to the users.
  switch (current_nudge_) {
    case NudgeType::kReorderNudge: {
      ScopedDictPrefUpdate update(prefs, prefs::kAppListReorderNudge);
      base::TimeDelta shown_duration =
          base::Time::Now() - current_nudge_show_timestamp_;

      // Caches that the nudge is considered as shown if:
      // 1. the time threshold is skipped; or
      // 2. the time delta of showing the nudge is long enough.
      if (ash::switches::IsSkipRecorderNudgeShowThresholdDurationEnabled() ||
          shown_duration >= base::Seconds(1)) {
        is_nudge_considered_as_shown_ = true;
      }

      // Update the number of times that the reorder nudge was
      // shown to users if the visibility updates.
      if (is_visible_updated) {
        MaybeIncrementShownCountInPrefs(update, shown_duration);
        is_nudge_considered_as_shown_ = false;
      }
    } break;
    case NudgeType::kPrivacyNotice:
    case NudgeType::kTutorialNudge:
    case NudgeType::kNone:
      break;
  }
}

void AppListNudgeController::MaybeIncrementShownCountInPrefs(
    ScopedDictPrefUpdate& update,
    base::TimeDelta duration) {
  // Only increment the shown count if the nudge changed to invisible state and
  // the nudge was shown long enough to the user before the nudge became
  // invisible. Note that if the nudge is inactive but visible, it doesn't count
  // as showing once to the user.
  if (!is_visible_ && is_nudge_considered_as_shown_) {
    update->Set(kReorderNudgeShownCount,
                GetShownCount(GetPrefs(), NudgeType::kReorderNudge) + 1);
  }
}

}  // namespace ash
