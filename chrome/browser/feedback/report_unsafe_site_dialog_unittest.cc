// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site_dialog.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

// Tests for ReportUnsafeSiteDialog.
class ReportUnsafeSiteDialogTest : public testing::Test {
 public:
  ReportUnsafeSiteDialogTest() = default;
  ~ReportUnsafeSiteDialogTest() override = default;

 protected:
  void SetUp() override {
    PrefService* prefs = profile_.GetPrefs();
    prefs->SetBoolean(prefs::kUserFeedbackAllowed, true);
    safe_browsing::SetSafeBrowsingState(
        prefs, safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

class ReportUnsafeSiteDialogTest_FeatureEnabled
    : public ReportUnsafeSiteDialogTest {
 public:
  ReportUnsafeSiteDialogTest_FeatureEnabled() = default;
  ~ReportUnsafeSiteDialogTest_FeatureEnabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kReportUnsafeSite};
};

class ReportUnsafeSiteDialogTest_FeatureDisabled
    : public ReportUnsafeSiteDialogTest {
 public:
  ReportUnsafeSiteDialogTest_FeatureDisabled() {
    feature_list_.InitAndDisableFeature(features::kReportUnsafeSite);
  }
  ~ReportUnsafeSiteDialogTest_FeatureDisabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ReportUnsafeSiteDialogTest_FeatureDisabled, IsEnabled_FeatureDisabled) {
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogTest_FeatureEnabled, IsEnabled_Enabled) {
  EXPECT_TRUE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogTest_FeatureEnabled,
       IsEnabled_FeedbackDisallowed) {
  profile_.GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed, false);
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogTest_FeatureEnabled,
       IsEnabled_SafeBrowsingDisabled) {
  safe_browsing::SetSafeBrowsingState(
      profile_.GetPrefs(), safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

}  // namespace feedback
