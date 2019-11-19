// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_PREFS_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_PREFS_H_

#include <cstddef>

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/feed/buildflags.h"
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
#include "components/feed/core/pref_names.h"
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#include "components/content_settings/core/common/pref_names.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"

// A preference exposed to Java.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.preferences
enum Pref {
  ALLOW_DELETING_BROWSER_HISTORY,
  INCOGNITO_MODE_AVAILABILITY,
  NTP_ARTICLES_SECTION_ENABLED,
  NTP_ARTICLES_LIST_VISIBLE,
  READER_FOR_ACCESSIBILITY_ENABLED,
  PROMPT_FOR_DOWNLOAD_ANDROID,
  SHOW_MISSING_SD_CARD_ERROR_ANDROID,
  CAN_MAKE_PAYMENT_ENABLED,
  CONTEXTUAL_SEARCH_ENABLED,
  AUTOFILL_PROFILE_ENABLED,
  AUTOFILL_CREDIT_CARD_ENABLED,
  USAGE_STATS_ENABLED,
  OFFLINE_PREFETCH_USER_SETTING_ENABLED,
  SAFE_BROWSING_EXTENDED_REPORTING_OPT_IN_ALLOWED,
  SAFE_BROWSING_ENABLED,
  PASSWORD_MANAGER_ONBOARDING_STATE,
  SEARCH_SUGGEST_ENABLED,
  REMEMBER_PASSWORDS_ENABLED,
  PASSWORD_MANAGER_AUTO_SIGNIN_ENABLED,
  PASSWORD_MANAGER_LEAK_DETECTION_ENABLED,
  SUPERVISED_USER_SAFE_SITES,
  DEFAULT_SUPERVISED_USER_FILTERING_BEHAVIOR,
  SUPERVISED_USER_ID,
  SUPERVISED_USER_CUSTODIAN_EMAIL,
  SUPERVISED_USER_SECOND_CUSTODIAN_NAME,
  SUPERVISED_USER_SECOND_CUSTODIAN_EMAIL,
  CLICKED_UPDATE_MENU_ITEM,
  LATEST_VERSION_WHEN_CLICKED_UPDATE_MENU_ITEM,
  BLOCK_THIRD_PARTY_COOKIES,
  ENABLE_DO_NOT_TRACK,
  PRINTING_ENABLED,
  OFFER_TRANSLATE_ENABLED,
  NOTIFICATIONS_VIBRATE_ENABLED,
  ALTERNATE_ERROR_PAGES_ENABLED,
  SYNC_LAST_ACCOUNT_NAME,
  WEBKIT_PASSWORD_ECHO_ENABLED,
  WEBKIT_FORCE_DARK_MODE_ENABLED,
  // PREF_NUM_PREFS must be the last entry.
  PREF_NUM_PREFS
};

// The indices must match value of Pref.
// Remember to update prefs_unittest.cc as well.
const char* const kPrefsExposedToJava[] = {
    prefs::kAllowDeletingBrowserHistory,
    prefs::kIncognitoModeAvailability,

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
    feed::prefs::kEnableSnippets,
    feed::prefs::kArticlesListVisible,
#else   // BUILDFLAG(ENABLE_FEED_IN_CHROME)
    ntp_snippets::prefs::kEnableSnippets,
    ntp_snippets::prefs::kArticlesListVisible,
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)

    dom_distiller::prefs::kReaderForAccessibility,
    prefs::kPromptForDownloadAndroid,
    prefs::kShowMissingSdCardErrorAndroid,
    payments::kCanMakePaymentEnabled,
    prefs::kContextualSearchEnabled,
    autofill::prefs::kAutofillProfileEnabled,
    autofill::prefs::kAutofillCreditCardEnabled,
    prefs::kUsageStatsEnabled,
    offline_pages::prefetch_prefs::kUserSettingEnabled,
    prefs::kSafeBrowsingExtendedReportingOptInAllowed,
    prefs::kSafeBrowsingEnabled,
    password_manager::prefs::kPasswordManagerOnboardingState,
    prefs::kSearchSuggestEnabled,
    password_manager::prefs::kCredentialsEnableService,
    password_manager::prefs::kCredentialsEnableAutosignin,
    password_manager::prefs::kPasswordLeakDetectionEnabled,
    prefs::kSupervisedUserSafeSites,
    prefs::kDefaultSupervisedUserFilteringBehavior,
    prefs::kSupervisedUserId,
    prefs::kSupervisedUserCustodianEmail,
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kClickedUpdateMenuItem,
    prefs::kLatestVersionWhenClickedUpdateMenuItem,
    prefs::kBlockThirdPartyCookies,
    prefs::kEnableDoNotTrack,
    prefs::kPrintingEnabled,
    prefs::kOfferTranslateEnabled,
    prefs::kNotificationsVibrateEnabled,
    prefs::kAlternateErrorPagesEnabled,
    prefs::kGoogleServicesLastUsername,
    prefs::kWebKitPasswordEchoEnabled,
    prefs::kWebKitForceDarkModeEnabled,
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_PREFS_H_
