// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/exit_type_service.h"

#include <string>

#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_service.h"

using ExitTypeServiceTest = BrowserWithTestWindowTest;

TEST_F(ExitTypeServiceTest, Defaults) {
  ExitTypeService* service = ExitTypeService::GetInstanceForProfile(profile());
  ASSERT_TRUE(service);
  PrefService* prefs = profile()->GetPrefs();
  // The initial state is crashed; store for later reference.
  std::string crash_value(prefs->GetString(prefs::kSessionExitType));

  // The first call to a type other than crashed should change the value.
  service->SetCurrentSessionExitType(ExitType::kForcedShutdown);
  std::string first_call_value(prefs->GetString(prefs::kSessionExitType));
  EXPECT_NE(crash_value, first_call_value);

  // Subsequent calls to a non-crash value should be ignored.
  service->SetCurrentSessionExitType(ExitType::kClean);
  std::string second_call_value(prefs->GetString(prefs::kSessionExitType));
  EXPECT_EQ(first_call_value, second_call_value);

  // Setting back to a crashed value should work.
  service->SetCurrentSessionExitType(ExitType::kCrashed);
  std::string final_value(prefs->GetString(prefs::kSessionExitType));
  EXPECT_EQ(crash_value, final_value);
}
