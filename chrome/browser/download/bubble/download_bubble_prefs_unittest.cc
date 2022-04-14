// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

class DownloadBubblePrefsTest : public testing::Test {
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
  TestingProfile* profile_;
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(DownloadBubblePrefsTest, FeatureFlagEnabled) {
  feature_list_.InitAndEnableFeature(safe_browsing::kDownloadBubble);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_TRUE(IsDownloadBubbleEnabled(profile_));
}

TEST_F(DownloadBubblePrefsTest, FeatureFlagDisabled) {
  feature_list_.InitAndDisableFeature(safe_browsing::kDownloadBubble);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_FALSE(IsDownloadBubbleEnabled(profile_));
}

TEST_F(DownloadBubblePrefsTest, DownloadBubbleEnabledManaged) {
  feature_list_.InitAndEnableFeature(safe_browsing::kDownloadBubble);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kDownloadBubbleEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(IsDownloadBubbleEnabled(profile_));
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kDownloadBubbleEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(IsDownloadBubbleEnabled(profile_));
}

}  // namespace download
