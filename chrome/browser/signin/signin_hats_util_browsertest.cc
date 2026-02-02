// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_hats_util.h"

#include <map>
#include <string>

#include "base/check_deref.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Not;
using ::testing::Values;

class SigninHatsUtilBaseBrowserTest : public SigninBrowserTestBase {
 public:
  explicit SigninHatsUtilBaseBrowserTest(
      const std::vector<base::test::FeatureRef> enabled_features) {
    feature_list_.InitWithFeatures(enabled_features, /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            CHECK_DEREF(browser()).profile(),
            base::BindRepeating(&BuildMockHatsService)));
  }

  void TearDownOnMainThread() override {
    SigninBrowserTestBase::TearDownOnMainThread();
    mock_hats_service_ = nullptr;
  }

 protected:
  MockHatsService& mock_hats_service() {
    return CHECK_DEREF(mock_hats_service_);
  }

  void MakeAccountAvailable() {
    signin::MakeAccountAvailable(
        identity_manager(),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            // Sign-in with the access point that `ChromeSigninClient` doesn't
            // listen to, to avoid triggering the 'automatic' trigger of HaTS
            // survey on sign-in.
            .WithAccessPoint(signin_metrics::AccessPoint::kStartPage)
            .Build("alice@example.com"));
  }

 private:
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
};

class SigninHatsUtilBrowserTest : public SigninHatsUtilBaseBrowserTest {
 public:
  SigninHatsUtilBrowserTest()
      : SigninHatsUtilBaseBrowserTest(
            /*enabled_features=*/{
                switches::
                    kChromeIdentitySurveySwitchProfileFromProfilePicker}) {}

 protected:
  std::string trigger() const {
    return kHatsSurveyTriggerIdentitySwitchProfileFromProfilePicker;
  }
};

IN_PROC_BROWSER_TEST_F(SigninHatsUtilBrowserTest, LaunchHatsSurveyForProfile) {
  MakeAccountAvailable();

  const std::map<std::string, std::string> survey_data = {
      {"Channel", "unknown"},
      {"Chrome Version", version_info::GetVersion().GetString()},
      {"Number of Chrome Profiles", "1"},
      {"Number of Google Accounts", "1"},
      {"Sign-in Status", "Signed In"}};

  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(trigger(), _, _, Eq(survey_data)));
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(Not(trigger()), _, _, _))
      .Times(0);

  signin::LaunchHatsSurveyForProfile(trigger(),
                                     CHECK_DEREF(browser()).profile());
}

IN_PROC_BROWSER_TEST_F(SigninHatsUtilBrowserTest,
                       LaunchHatsSurveyForProfileNoAccount) {
  const std::map<std::string, std::string> survey_data = {
      {"Channel", "unknown"},
      {"Chrome Version", version_info::GetVersion().GetString()},
      {"Number of Chrome Profiles", "1"},
      {"Number of Google Accounts", "0"},
      {"Sign-in Status", "Signed Out"}};

  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(trigger(), _, _, Eq(survey_data)));
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(Not(trigger()), _, _, _))
      .Times(0);

  signin::LaunchHatsSurveyForProfile(trigger(),
                                     CHECK_DEREF(browser()).profile());
}

IN_PROC_BROWSER_TEST_F(SigninHatsUtilBrowserTest,
                       LaunchHatsSurveyForProfileArbitrarySurveyData) {
  const std::map<std::string, std::string> survey_data = {
      {"Lucky number", "997"}};

  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(trigger(), _, _, Eq(survey_data)));
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(Not(trigger()), _, _, _))
      .Times(0);

  signin::LaunchHatsSurveyForProfile(
      trigger(), CHECK_DEREF(browser()).profile(),
      /*defer_if_no_browser=*/false, std::move(survey_data));
}

IN_PROC_BROWSER_TEST_F(SigninHatsUtilBrowserTest,
                       LaunchHatsSurveyDeferIfNoBrowser) {
  Profile* profile = CHECK_DEREF(browser()).profile();
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kProfilePickerView);
  CloseAllBrowsers();

  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(trigger(), _, _, _));
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(Not(trigger()), _, _, _))
      .Times(0);

  signin::LaunchHatsSurveyForProfile(trigger(), profile,
                                     /*defer_if_no_browser=*/true);

  Browser* new_browser = CreateBrowser(profile);
  EXPECT_NE(new_browser, nullptr);
}

IN_PROC_BROWSER_TEST_F(SigninHatsUtilBrowserTest,
                       LaunchHatsSurveyWithUnknownTrigger) {
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey).Times(0);

  signin::LaunchHatsSurveyForProfile(/*trigger=*/"unknown_trigger",
                                     CHECK_DEREF(browser()).profile());
}

struct PromoBubbleTestParams {
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::kStartPage;
  std::string expected_bubble_type;
};

class SigninHatsUtilPromoBubbleDismissedBrowserTest
    : public SigninHatsUtilBaseBrowserTest,
      public testing::WithParamInterface<PromoBubbleTestParams> {
 public:
  SigninHatsUtilPromoBubbleDismissedBrowserTest()
      : SigninHatsUtilBaseBrowserTest(
            /*enabled_features=*/{
                switches::kChromeIdentitySurveySigninPromoBubbleDismissed}) {}

 protected:
  std::string trigger() const {
    return kHatsSurveyTriggerIdentitySigninPromoBubbleDismissed;
  }
};

IN_PROC_BROWSER_TEST_P(SigninHatsUtilPromoBubbleDismissedBrowserTest,
                       LaunchHatsSurveyForProfile) {
  const std::map<std::string, std::string> survey_data = {
      {"Channel", "unknown"},
      {"Chrome Version", version_info::GetVersion().GetString()},
      {"Number of Chrome Profiles", "1"},
      {"Number of Google Accounts", "0"},
      {"Sign-in Status", "Signed Out"},
      {"Data type Sign-in Bubble Dismissed", GetParam().expected_bubble_type}};

  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(trigger(), _, _, Eq(survey_data)));
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey(Not(trigger()), _, _, _))
      .Times(0);

  signin::LaunchHatsSurveyForProfile(
      trigger(), CHECK_DEREF(browser()).profile(),
      /*defer_if_no_browser=*/false, GetParam().access_point);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SigninHatsUtilPromoBubbleDismissedBrowserTest,
    Values(
        PromoBubbleTestParams{
            .access_point = signin_metrics::AccessPoint::kAddressBubble,
            .expected_bubble_type = "Address Bubble"},
        PromoBubbleTestParams{
            .access_point = signin_metrics::AccessPoint::kBookmarkBubble,
            .expected_bubble_type = "Bookmark Bubble"},
        PromoBubbleTestParams{
            .access_point =
                signin_metrics::AccessPoint::kExtensionInstallBubble,
            .expected_bubble_type = "Extension Install Bubble"},
        PromoBubbleTestParams{
            .access_point = signin_metrics::AccessPoint::kPasswordBubble,
            .expected_bubble_type = "Password Bubble"},
        PromoBubbleTestParams{
            .access_point =
                signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo,
            .expected_bubble_type = "Other"}),
    [](const testing::TestParamInfo<PromoBubbleTestParams>& info) {
      std::string test_suffix;
      base::RemoveChars(info.param.expected_bubble_type, " ", &test_suffix);
      return test_suffix;
    });

class SigninHatsUtilConflictingFeaturesBrowserTest
    : public SigninHatsUtilBaseBrowserTest {
 public:
  SigninHatsUtilConflictingFeaturesBrowserTest()
      : SigninHatsUtilBaseBrowserTest(
            /*enabled_features=*/{
                switches::kChromeIdentitySurveyFirstRunSignin,
                switches::kBeforeFirstRunDesktopRefreshSurvey}) {}

 protected:
  std::string trigger_with_no_conflict() const {
    return kHatsSurveyTriggerIdentityFirstRunCompleted;
  }

  std::string trigger_with_conflict() const {
    return kHatsSurveyTriggerIdentityFirstRunSignin;
  }
};

IN_PROC_BROWSER_TEST_F(SigninHatsUtilConflictingFeaturesBrowserTest,
                       LaunchHatsSurveyForProfileIfNoConflict) {
  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(trigger_with_no_conflict(), _, _, _));
  EXPECT_CALL(mock_hats_service(),
              LaunchDelayedSurvey(Not(trigger_with_no_conflict()), _, _, _))
      .Times(0);

  signin::LaunchHatsSurveyForProfile(trigger_with_no_conflict(),
                                     CHECK_DEREF(browser()).profile());
}

IN_PROC_BROWSER_TEST_F(SigninHatsUtilConflictingFeaturesBrowserTest,
                       DoNotLaunchHatsSurveyForProfileIfConflict) {
  EXPECT_CALL(mock_hats_service(), LaunchDelayedSurvey).Times(0);

  signin::LaunchHatsSurveyForProfile(trigger_with_conflict(),
                                     CHECK_DEREF(browser()).profile());
}
