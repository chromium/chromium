// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_api_data_for_next_login_attempt_pref_cleaner.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using LoginApiDataForNextLoginAttemptPrefCleanerBrowserTest =
    InProcessBrowserTest;

// Tests that the `kLoginExtensionApiDataForNextLoginAttempt` pref is cleared
// when the session becomes active.
IN_PROC_BROWSER_TEST_F(LoginApiDataForNextLoginAttemptPrefCleanerBrowserTest,
                       SessionStateChanged) {
  PrefService* local_state = g_browser_process->local_state();

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         "foo");

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  ASSERT_FALSE(local_state->HasPrefPath(
      prefs::kLoginExtensionApiDataForNextLoginAttempt));
}
