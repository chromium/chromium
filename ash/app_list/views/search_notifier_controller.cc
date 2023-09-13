// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_notifier_controller.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {
namespace {

// Image search privacy notice dictionary pref keys.
constexpr char kPrivacyNoticeShownCount[] = "shown_count";
constexpr char kPrivacyNoticeAccepted[] = "accepted";

// Maximum number of times that the notifier can be shown to users.
constexpr int kMaxShowCount = 3;

// Returns the last active user pref service.
PrefService* GetPrefs() {
  if (!Shell::HasInstance()) {
    return nullptr;
  }

  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

SearchNotifierController::SearchNotifierController() = default;

// static
void SearchNotifierController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kImageSearchPrivacyNotice);
}

// static
void SearchNotifierController::ResetPrefsForNewUserSession(PrefService* prefs) {
  prefs->ClearPref(prefs::kImageSearchPrivacyNotice);
}

// static
int SearchNotifierController::GetPrivacyNoticeShownCount(PrefService* prefs) {
  const base::Value::Dict& dictionary =
      prefs->GetDict(prefs::kImageSearchPrivacyNotice);

  return dictionary.FindInt(kPrivacyNoticeShownCount).value_or(0);
}

// static
bool SearchNotifierController::ShouldShowPrivacyNotice() {
  PrefService* prefs = GetPrefs();
  if (!prefs) {
    return false;
  }

  if (IsPrivacyNoticeAccepted()) {
    return false;
  }

  return GetPrivacyNoticeShownCount(prefs) <= kMaxShowCount;
}

// static
bool SearchNotifierController::IsPrivacyNoticeAccepted() {
  const PrefService* prefs = GetPrefs();
  if (!prefs) {
    return false;
  }

  return prefs->GetDict(prefs::kImageSearchPrivacyNotice)
      .FindBool(kPrivacyNoticeAccepted)
      .value_or(false);
}

void SearchNotifierController::EnableImageSearch() {
  PrefService* prefs = GetPrefs();
  if (!prefs) {
    return;
  }

  ScopedDictPrefUpdate update(prefs,
                              prefs::kLauncherSearchCategoryControlStatus);
  update->Set(
      GetAppListControlCategoryName(AppListSearchControlCategory::kImages),
      true);
}

void SearchNotifierController::SetPrivacyNoticeAcceptedPref() {
  PrefService* prefs = GetPrefs();
  if (!prefs) {
    return;
  }

  ScopedDictPrefUpdate privacy_pref_update(prefs,
                                           prefs::kImageSearchPrivacyNotice);
  privacy_pref_update->Set(kPrivacyNoticeAccepted, true);

  // Enable the image search as the privacy notice is accepted.
  EnableImageSearch();
}

void SearchNotifierController::UpdateNotifierVisibility(bool visible) {
  PrefService* prefs = GetPrefs();
  if (!prefs) {
    return;
  }

  bool is_visible_updated = visible != is_visible_;
  is_visible_ = visible;
  ScopedDictPrefUpdate update(prefs, prefs::kImageSearchPrivacyNotice);

  // Update the number of times that the notifier was shown to users if the
  // visibility updates.
  if (visible && is_visible_updated) {
    update->Set(kPrivacyNoticeShownCount,
                GetPrivacyNoticeShownCount(prefs) + 1);
  }
}

}  // namespace ash
