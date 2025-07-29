// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

using testing::_;
using testing::Not;

class ChromeSigninClientWithBookmarksInTransportModeBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  // Equivalent to `kSigninFromBookmarksBubbleSyntheticTrialGroupNamePref` that
  // is defined in `chrome_signin_client.cc`.
  static constexpr char
      kSigninFromBookmarksBubbleSyntheticTrialGroupNamePrefForTesting[] =
          "UnoDesktopBookmarksEnabledInAccountFromBubbleGroup";
  // Equivalent to `kBookmarksBubblePromoShownSyntheticTrialGroupNamePref` that
  // is defined in `chrome_signin_client.cc`.
  static constexpr char
      kBookmarksBubblePromoShownSyntheticTrialGroupNamePrefForTesting[] =
          "UnoDesktopBookmarksBubblePromoShownGroup";

  ChromeSigninClientWithBookmarksInTransportModeBrowserTest() {
    // Enables feature and register field trial. Note: disabling a feature will
    // not register the field trial for the equivalent control group in tests -
    // so we cannot test the Synthetic field trial tags for disabled features.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{switches::kSyncEnableBookmarksInTransportMode, {}}}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeSigninClientWithBookmarksInTransportModeBrowserTest,
    UnoDesktopSyntheticFieldTrialTags) {
  PrefService* local_prefs = g_browser_process->local_state();
  ASSERT_TRUE(
      local_prefs
          ->GetString(
              kBookmarksBubblePromoShownSyntheticTrialGroupNamePrefForTesting)
          .empty());
  ASSERT_TRUE(
      local_prefs
          ->GetString(
              kSigninFromBookmarksBubbleSyntheticTrialGroupNamePrefForTesting)
          .empty());

  // Simulates seeing the Signin Promo in the Bookmarks Saving bubble.
  ChromeSigninClient::
      MaybeAddUserToBookmarksBubblePromoShownSyntheticFieldTrial();

  EXPECT_EQ(
      local_prefs->GetString(
          kBookmarksBubblePromoShownSyntheticTrialGroupNamePrefForTesting),
      "scoped_feature_list_trial_group");
  EXPECT_TRUE(
      local_prefs
          ->GetString(
              kSigninFromBookmarksBubbleSyntheticTrialGroupNamePrefForTesting)
          .empty());

  // Simulates Signing in through the bookmarks bubble.
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kBookmarkBubble)
          .Build("test@gmail.com"));

  EXPECT_EQ(
      local_prefs->GetString(
          kBookmarksBubblePromoShownSyntheticTrialGroupNamePrefForTesting),
      "scoped_feature_list_trial_group");
  EXPECT_EQ(
      local_prefs->GetString(
          kSigninFromBookmarksBubbleSyntheticTrialGroupNamePrefForTesting),
      "scoped_feature_list_trial_group");
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
class ChromeSigninClientHatsSurveyBrowserTest : public InProcessBrowserTest {
 public:
  ChromeSigninClientHatsSurveyBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {{switches::kChromeIdentitySurveyPasswordBubbleSignin,
          switches::kChromeIdentitySurveyFirstRunSignin}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  }

  void TearDownOnMainThread() override { mock_hats_service_ = nullptr; }

  MockHatsService* mock_hats_service() { return mock_hats_service_; }

 private:
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a HaTS survey is launched when a user signs in through an eligible
// access point.
IN_PROC_BROWSER_TEST_F(ChromeSigninClientHatsSurveyBrowserTest,
                       HatsSurveyLaunchedOnSignin) {
  // Expect the HaTS service to launch the password bubble sign-in survey.
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerIdentityPasswordBubbleSignin, _, _, _));
  // Expect that the surveys for other access point will NOT be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(
                  Not(kHatsSurveyTriggerIdentityPasswordBubbleSignin), _, _, _))
      .Times(0);

  // Simulate a user signing in via the password bubble, which should trigger
  // the survey.
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kPasswordBubble)
          .Build("alice@example.com"));
}

// TODO(crbug.com/433498793): Re-enable this flaky test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HatsSurveyLaunchedOnBrowserCreationAfterSignin DISABLED_HatsSurveyLaunchedOnBrowserCreationAfterSignin
#else
#define MAYBE_HatsSurveyLaunchedOnBrowserCreationAfterSignin HatsSurveyLaunchedOnBrowserCreationAfterSignin
#endif

// Tests that if a user signs in when no browser is open, the HaTS survey is
// launched immediately when a browser is subsequently created for that profile.
IN_PROC_BROWSER_TEST_F(ChromeSigninClientHatsSurveyBrowserTest,
  MAYBE_HatsSurveyLaunchedOnBrowserCreationAfterSignin) {
  // Keep the profile alive and close all existing browsers.
  Profile* profile = browser()->profile();
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kProfilePickerView);
  CloseAllBrowsers();

  // Sign in to Chrome. The survey won't launch yet, as it requires an active
  // browser.
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kForYouFre)
          .Build("alice@example.com"));

  // Expect the HaTS service to launch the first run sign-in survey.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurvey(kHatsSurveyTriggerIdentityFirstRunSignin, _, _, _));
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(Not(kHatsSurveyTriggerIdentityFirstRunSignin),
                                  _, _, _))
      .Times(0);

  // Create a new browser for the signed-in profile, which should now trigger
  // the survey.
  CreateBrowser(profile);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
