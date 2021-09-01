// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_api_data_for_next_login_attempt_pref_cleaner.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"

namespace chromeos {

LoginApiDataForNextLoginAttemptPrefCleaner::
    LoginApiDataForNextLoginAttemptPrefCleaner() {
  session_observation_.Observe(session_manager::SessionManager::Get());
}

LoginApiDataForNextLoginAttemptPrefCleaner::
    ~LoginApiDataForNextLoginAttemptPrefCleaner() = default;

void LoginApiDataForNextLoginAttemptPrefCleaner::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->ClearPref(prefs::kLoginExtensionApiDataForNextLoginAttempt);
}

}  // namespace chromeos
