// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

class DownloadBubblePrefsTest : public ::testing::Test {
 public:
  DownloadBubblePrefsTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  DownloadBubblePrefsTest(const DownloadBubblePrefsTest&) = delete;
  DownloadBubblePrefsTest& operator=(const DownloadBubblePrefsTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
  }

 protected:
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(DownloadBubblePrefsTest, IsPartialViewEnabled) {
  // Test default value.
  EXPECT_TRUE(IsDownloadBubblePartialViewEnabled(profile_));
  EXPECT_TRUE(IsDownloadBubblePartialViewEnabledDefaultPrefValue(profile_));

  // Set value.
  SetDownloadBubblePartialViewEnabled(profile_, false);
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabled(profile_));
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabledDefaultPrefValue(profile_));

  SetDownloadBubblePartialViewEnabled(profile_, true);
  EXPECT_TRUE(IsDownloadBubblePartialViewEnabled(profile_));
  // This should still be false because it has been set to an explicit value.
  EXPECT_FALSE(IsDownloadBubblePartialViewEnabledDefaultPrefValue(profile_));
}

TEST_F(DownloadBubblePrefsTest, PartialViewImpressions) {
  // Test default value.
  EXPECT_EQ(DownloadBubblePartialViewImpressions(profile_), 0);

  // Set value.
  SetDownloadBubblePartialViewImpressions(profile_, 1);
  EXPECT_EQ(DownloadBubblePartialViewImpressions(profile_), 1);
}

}  // namespace
}  // namespace download
