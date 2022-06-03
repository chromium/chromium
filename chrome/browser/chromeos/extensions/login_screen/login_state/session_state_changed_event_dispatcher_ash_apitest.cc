// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using SessionStateChangedEventDispatcherApitest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(SessionStateChangedEventDispatcherApitest,
                       OnSessionStateDetailsChanged) {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  ASSERT_TRUE(RunExtensionTest(
      "login_screen_apis/login_state/on_session_state_changed"));

  // Test cases are rearranged as the event is not fired if the mapped session
  // state is the same.
  const struct {
    const session_manager::SessionState session_state;
    const char* expected;
  } kTestCases[] = {
      {session_manager::SessionState::OOBE, "IN_OOBE_SCREEN"},
      {session_manager::SessionState::LOGIN_PRIMARY, "IN_LOGIN_SCREEN"},
      {session_manager::SessionState::LOCKED, "IN_LOCK_SCREEN"},
      {session_manager::SessionState::LOGGED_IN_NOT_ACTIVE, "IN_LOGIN_SCREEN"},
      {session_manager::SessionState::ACTIVE, "IN_SESSION"},
      {session_manager::SessionState::LOGIN_SECONDARY, "IN_LOGIN_SCREEN"},
      {session_manager::SessionState::UNKNOWN, "UNKNOWN"},
  };

  for (const auto& test : kTestCases) {
    ExtensionTestMessageListener listener(test.expected, /*will_reply=*/false);
    session_manager->SetSessionState(test.session_state);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
}

}  // namespace extensions
