// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

// Tests for ReportUnsafeSiteDialog.
class ReportUnsafeSiteDialogViewsTest : public testing::Test {
 public:
  ReportUnsafeSiteDialogViewsTest() = default;
  ~ReportUnsafeSiteDialogViewsTest() override = default;

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

class ReportUnsafeSiteDialogViewsTest_FeatureEnabled
    : public ReportUnsafeSiteDialogViewsTest {
 public:
  ReportUnsafeSiteDialogViewsTest_FeatureEnabled() = default;
  ~ReportUnsafeSiteDialogViewsTest_FeatureEnabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kReportUnsafeSite};
};

class ReportUnsafeSiteDialogViewsTest_FeatureDisabled
    : public ReportUnsafeSiteDialogViewsTest {
 public:
  ReportUnsafeSiteDialogViewsTest_FeatureDisabled() {
    feature_list_.InitAndDisableFeature(features::kReportUnsafeSite);
  }
  ~ReportUnsafeSiteDialogViewsTest_FeatureDisabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ReportUnsafeSiteDialogViewsTest_FeatureDisabled,
       IsEnabled_FeatureDisabled) {
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogViewsTest_FeatureEnabled, IsEnabled_Enabled) {
  EXPECT_TRUE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogViewsTest_FeatureEnabled,
       IsEnabled_FeedbackDisallowed) {
  EXPECT_TRUE(ReportUnsafeSiteDialog::IsEnabled(profile_));

  profile_.GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed, false);
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogViewsTest_FeatureEnabled,
       IsEnabled_SafeBrowsingDisabled) {
  EXPECT_TRUE(ReportUnsafeSiteDialog::IsEnabled(profile_));

  safe_browsing::SetSafeBrowsingState(
      profile_.GetPrefs(), safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(profile_));
}

TEST_F(ReportUnsafeSiteDialogViewsTest_FeatureEnabled, IsEnabled_Incognito) {
  EXPECT_TRUE(ReportUnsafeSiteDialog::IsEnabled(profile_));

  Profile* incognito_profile = profile_.GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUnique("Test::Foo"),
      /*create_if_needed=*/true);
  EXPECT_FALSE(ReportUnsafeSiteDialog::IsEnabled(*incognito_profile));
}

}  // namespace feedback
