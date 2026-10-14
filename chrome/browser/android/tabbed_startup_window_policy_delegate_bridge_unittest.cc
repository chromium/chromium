// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tabbed_startup_window_policy_delegate_bridge.h"

#include <string>
#include <vector>

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome::android {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class TabbedStartupWindowPolicyDelegateBridgeTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(TabbedStartupWindowPolicyDelegateBridgeTest,
       GetSessionStartupUrls_NullPrefServiceReturnsEmpty) {
  EXPECT_THAT(GetSessionStartupUrls(nullptr), IsEmpty());
}

TEST_F(TabbedStartupWindowPolicyDelegateBridgeTest,
       SetSessionStartupUrls_NullPrefServiceIsNoOp) {
  SetSessionStartupUrls(nullptr, {"https://www.google.com/"});
  EXPECT_THAT(GetSessionStartupUrls(profile_.GetPrefs()), IsEmpty());
}

TEST_F(TabbedStartupWindowPolicyDelegateBridgeTest,
       SetAndGetSessionStartupUrls_FiltersInvalidAndUnsafeUrls) {
  PrefService* prefs = profile_.GetPrefs();
  SessionStartupPref initial_pref(SessionStartupPref::URLS);
  SessionStartupPref::SetStartupPref(prefs, initial_pref);

  SetSessionStartupUrls(prefs,
                        {"https://www.google.com/", "javascript:alert(1)",
                         "not a valid url", "http://www.wikipedia.org/"});

  EXPECT_THAT(
      GetSessionStartupUrls(prefs),
      ElementsAre("https://www.google.com/", "http://www.wikipedia.org/"));
  EXPECT_EQ(SessionStartupPref::URLS,
            SessionStartupPref::GetStartupPref(prefs).type);
}

TEST_F(TabbedStartupWindowPolicyDelegateBridgeTest,
       SetSessionStartupUrls_EmptyValidListResetsTypeToDefault) {
  PrefService* prefs = profile_.GetPrefs();
  SessionStartupPref initial_pref(SessionStartupPref::URLS);
  initial_pref.urls = {GURL("https://www.google.com/")};
  SessionStartupPref::SetStartupPref(prefs, initial_pref);

  SetSessionStartupUrls(prefs, {"javascript:void(0)"});

  EXPECT_THAT(GetSessionStartupUrls(prefs), IsEmpty());
  EXPECT_EQ(SessionStartupPref::DEFAULT,
            SessionStartupPref::GetStartupPref(prefs).type);
  EXPECT_EQ(SessionStartupPref::kPrefValueNewTab,
            prefs->GetInteger(prefs::kRestoreOnStartup));
}

}  // namespace
}  // namespace chrome::android
