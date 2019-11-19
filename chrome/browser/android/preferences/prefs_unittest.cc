// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/prefs.h"

#include "base/stl_util.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PrefsTest : public ::testing::Test {
 protected:
  const char* GetPrefName(Pref pref) {
    pref_count_++;
    return PrefServiceBridge::GetPrefNameExposedToJava(pref);
  }

  int pref_count_;
};

TEST_F(PrefsTest, TestIndex) {
  pref_count_ = 0;

  // If one of these checks fails, most likely the Pref enum and
  // |kPrefExposedToJava| are out of sync.
  EXPECT_EQ(Pref::PREF_NUM_PREFS, base::size(kPrefsExposedToJava));

  EXPECT_EQ(prefs::kAllowDeletingBrowserHistory,
            GetPrefName(ALLOW_DELETING_BROWSER_HISTORY));
  EXPECT_EQ(prefs::kIncognitoModeAvailability,
            GetPrefName(INCOGNITO_MODE_AVAILABILITY));

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
  EXPECT_EQ(feed::prefs::kEnableSnippets,
            GetPrefName(NTP_ARTICLES_SECTION_ENABLED));
  EXPECT_EQ(feed::prefs::kArticlesListVisible,
            GetPrefName(NTP_ARTICLES_LIST_VISIBLE));
#else   // BUILDFLAG(ENABLE_FEED_IN_CHROME)
  EXPECT_EQ(ntp_snippets::prefs::kEnableSnippets,
            GetPrefName(NTP_ARTICLES_SECTION_ENABLED));
  EXPECT_EQ(ntp_snippets::prefs::kArticlesListVisible,
            GetPrefName(NTP_ARTICLES_LIST_VISIBLE));
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)

  EXPECT_EQ(prefs::kPromptForDownloadAndroid,
            GetPrefName(PROMPT_FOR_DOWNLOAD_ANDROID));
  EXPECT_EQ(dom_distiller::prefs::kReaderForAccessibility,
            GetPrefName(READER_FOR_ACCESSIBILITY_ENABLED));
  EXPECT_EQ(prefs::kShowMissingSdCardErrorAndroid,
            GetPrefName(SHOW_MISSING_SD_CARD_ERROR_ANDROID));
  EXPECT_EQ(payments::kCanMakePaymentEnabled,
            GetPrefName(CAN_MAKE_PAYMENT_ENABLED));
  EXPECT_EQ(prefs::kContextualSearchEnabled,
            GetPrefName(CONTEXTUAL_SEARCH_ENABLED));
  EXPECT_EQ(autofill::prefs::kAutofillProfileEnabled,
            GetPrefName(AUTOFILL_PROFILE_ENABLED));
  EXPECT_EQ(autofill::prefs::kAutofillCreditCardEnabled,
            GetPrefName(AUTOFILL_CREDIT_CARD_ENABLED));
  EXPECT_EQ(prefs::kUsageStatsEnabled, GetPrefName(USAGE_STATS_ENABLED));
  EXPECT_EQ(offline_pages::prefetch_prefs::kUserSettingEnabled,
            GetPrefName(OFFLINE_PREFETCH_USER_SETTING_ENABLED));
  EXPECT_EQ(prefs::kSafeBrowsingExtendedReportingOptInAllowed,
            GetPrefName(SAFE_BROWSING_EXTENDED_REPORTING_OPT_IN_ALLOWED));
  EXPECT_EQ(prefs::kSafeBrowsingEnabled, GetPrefName(SAFE_BROWSING_ENABLED));
  EXPECT_EQ(password_manager::prefs::kPasswordManagerOnboardingState,
            GetPrefName(PASSWORD_MANAGER_ONBOARDING_STATE));
  EXPECT_EQ(prefs::kSearchSuggestEnabled, GetPrefName(SEARCH_SUGGEST_ENABLED));
  EXPECT_EQ(password_manager::prefs::kCredentialsEnableService,
            GetPrefName(REMEMBER_PASSWORDS_ENABLED));
  EXPECT_EQ(password_manager::prefs::kCredentialsEnableAutosignin,
            GetPrefName(PASSWORD_MANAGER_AUTO_SIGNIN_ENABLED));
  EXPECT_EQ(password_manager::prefs::kPasswordLeakDetectionEnabled,
            GetPrefName(PASSWORD_MANAGER_LEAK_DETECTION_ENABLED));
  EXPECT_EQ(prefs::kSupervisedUserSafeSites,
            GetPrefName(SUPERVISED_USER_SAFE_SITES));
  EXPECT_EQ(prefs::kDefaultSupervisedUserFilteringBehavior,
            GetPrefName(DEFAULT_SUPERVISED_USER_FILTERING_BEHAVIOR));
  EXPECT_EQ(prefs::kSupervisedUserId, GetPrefName(SUPERVISED_USER_ID));
  EXPECT_EQ(prefs::kSupervisedUserCustodianEmail,
            GetPrefName(SUPERVISED_USER_CUSTODIAN_EMAIL));
  EXPECT_EQ(prefs::kSupervisedUserSecondCustodianName,
            GetPrefName(SUPERVISED_USER_SECOND_CUSTODIAN_NAME));
  EXPECT_EQ(prefs::kSupervisedUserSecondCustodianEmail,
            GetPrefName(SUPERVISED_USER_SECOND_CUSTODIAN_EMAIL));
  EXPECT_EQ(prefs::kClickedUpdateMenuItem,
            GetPrefName(CLICKED_UPDATE_MENU_ITEM));
  EXPECT_EQ(prefs::kLatestVersionWhenClickedUpdateMenuItem,
            GetPrefName(LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM));
  EXPECT_EQ(prefs::kBlockThirdPartyCookies,
            GetPrefName(BLOCK_THIRD_PARTY_COOKIES));
  EXPECT_EQ(prefs::kEnableDoNotTrack, GetPrefName(ENABLE_DO_NOT_TRACK));
  EXPECT_EQ(prefs::kPrintingEnabled, GetPrefName(PRINTING_ENABLED));
  EXPECT_EQ(prefs::kOfferTranslateEnabled,
            GetPrefName(OFFER_TRANSLATE_ENABLED));
  EXPECT_EQ(prefs::kNotificationsVibrateEnabled,
            GetPrefName(NOTIFICATIONS_VIBRATE_ENABLED));
  EXPECT_EQ(prefs::kAlternateErrorPagesEnabled,
            GetPrefName(ALTERNATE_ERROR_PAGES_ENABLED));
  EXPECT_EQ(prefs::kGoogleServicesLastUsername,
            GetPrefName(SYNC_LAST_ACCOUNT_NAME));
  EXPECT_EQ(prefs::kWebKitPasswordEchoEnabled,
            GetPrefName(WEBKIT_PASSWORD_ECHO_ENABLED));
  EXPECT_EQ(prefs::kWebKitForceDarkModeEnabled,
            GetPrefName(WEBKIT_FORCE_DARK_MODE_ENABLED));

  // If this check fails, a pref is missing a test case above.
  EXPECT_EQ(Pref::PREF_NUM_PREFS, pref_count_);
}

}  // namespace
