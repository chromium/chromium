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
  // TODO(b/311785210): Maybe implement category nudge.
}

// static
void SearchNotifierController::ResetPrefsForNewUserSession(PrefService* prefs) {
  // TODO(b/311785210): Maybe implement category nudge.
}

// static
int SearchNotifierController::GetPrivacyNoticeShownCount(PrefService* prefs) {
  // TODO(b/311785210): Maybe implement category nudge.
  return 0;
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
  // TODO(b/311785210): Maybe implement category nudge.
  return false;
}

void SearchNotifierController::SetPrivacyNoticeAcceptedPref() {
  // TODO(b/311785210): Maybe implement category nudge.
}

void SearchNotifierController::UpdateNotifierVisibility(bool visible) {
  // TODO(b/311785210): Maybe implement category nudge.
}

}  // namespace ash
