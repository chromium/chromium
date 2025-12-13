// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
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
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

using testing::_;
using testing::Eq;
using testing::Not;
using testing::Pair;
using testing::UnorderedElementsAre;

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
  // TODO(crbug.com/430925046): Investigate the number of Google Accounts.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchDelayedSurvey(
          kHatsSurveyTriggerIdentityPasswordBubbleSignin, _, _,
          UnorderedElementsAre(
              Pair("Channel", _),
              Pair("Chrome Version", version_info::GetVersion().GetString()),
              Pair("Number of Chrome Profiles", "1"),
              Pair("Number of Google Accounts", "0"),
              Pair("Sign-in Status", "Signed In"))));
  // Expect that the surveys for other access point will NOT be launched.
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(
                  Not(kHatsSurveyTriggerIdentityPasswordBubbleSignin), _, _, _))
      .Times(0);

  // Simulate a user signing in via the password bubble, which should trigger
  // the survey.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kPasswordBubble)
          .Build("alice@example.com"));
  signin::WaitForRefreshTokensLoaded(identity_manager);
}

// Tests that if a user signs in when no browser is open, the HaTS survey is
// launched immediately when a browser is subsequently created for that profile.
IN_PROC_BROWSER_TEST_F(ChromeSigninClientHatsSurveyBrowserTest,
                       HatsSurveyLaunchedOnBrowserCreationAfterSignin) {
  Profile* profile = browser()->profile();
  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  // Keep the profile alive and close all existing browsers.
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kProfilePickerView);
  CloseAllBrowsers();

  // Sign in to Chrome. The survey won't launch yet, as it requires an active
  // browser.
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kForYouFre)
          .Build("alice@example.com"));

  // Expect the HaTS service to launch the first run sign-in survey.
  std::map<std::string, std::string> expected_string_psd = {
      {"Channel", "unknown"},
      {"Chrome Version", version_info::GetVersion().GetString()},
      {"Number of Chrome Profiles", "1"},
      {"Number of Google Accounts", "1"},
      {"Sign-in Status", "Signed In"}};
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(kHatsSurveyTriggerIdentityFirstRunSignin, _,
                                  _, Eq(expected_string_psd)));
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(Not(kHatsSurveyTriggerIdentityFirstRunSignin),
                                  _, _, _))
      .Times(0);

  // Create a new browser for the signed-in profile, which should now trigger
  // the survey.
  Browser* new_browser = CreateBrowser(profile);
  ASSERT_TRUE(new_browser);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
