// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"

using ApplicationLifetimeTest = BrowserWithTestWindowTest;

TEST_F(ApplicationLifetimeTest, AttemptRestart) {
  ASSERT_TRUE(g_browser_process);
  TestingPrefServiceSimple* testing_pref_service =
      profile_manager()->local_state()->Get();

  EXPECT_FALSE(testing_pref_service->GetBoolean(prefs::kWasRestarted));
  chrome::AttemptRestart();
  EXPECT_TRUE(testing_pref_service->GetBoolean(prefs::kWasRestarted));

  // Cancel the effects of us calling chrome::AttemptRestart. Otherwise tests
  // ran after this one will fail.
  browser_shutdown::SetTryingToQuit(false);
}
