// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/prefs_util.h"

#include <memory>

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/content_settings/generated_cookie_prefs.h"
#include "chrome/browser/content_settings/generated_notification_pref.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/password_manager/generated_password_leak_detection_pref.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/generated_safe_browsing_pref.h"
#include "chrome/browser/ssl/generated_https_first_mode_pref.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/commerce/core/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/drive/drive_pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/media_router/common/pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_manager.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "components/url_formatter/url_fixer.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_pref_names.h"  // nogncheck
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/supervised_user_cros_settings_provider.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_method_short.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_on_off.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "components/account_manager_core/pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/events/ash/pref_names.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/extensions/controlled_pref_mapping.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsPrivilegedCrosSetting(const std::string& pref_name) {
  if (!ash::CrosSettings::IsCrosSettings(pref_name))
    return false;
  if (!ash::system::PerUserTimezoneEnabled()) {
    // kSystemTimezone should be changeable by all users.
    if (pref_name == ash::kSystemTimezone)
      return false;
  }
  // Cros settings are considered privileged and are either policy
  // controlled or owner controlled.
  return true;
}

bool IsRestrictedCrosSettingForChildUser(Profile* profile,
                                         const std::string& pref_name) {
  if (!profile->IsChild())
    return false;

  return ash::CrosSettings::Get()
      ->supervised_user_cros_settings_provider()
      ->HandlesSetting(pref_name);
}

const base::Value* GetRestrictedCrosSettingValueForChildUser(
    Profile* profile,
    const std::string& pref_name) {
  // Make sure that profile belongs to a child and the preference is
  // pre-set.
  DCHECK(IsRestrictedCrosSettingForChildUser(profile, pref_name));

  return ash::CrosSettings::Get()
      ->supervised_user_cros_settings_provider()
      ->Get(pref_name);
}

#endif

bool IsSettingReadOnly(const std::string& pref_name) {
  // download.default_directory is used to display the directory location and
  // for policy indicators, but should not be changed directly.
  if (pref_name == prefs::kDownloadDefaultDirectory)
    return true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System timezone is never directly changeable by the user.
  if (pref_name == ash::kSystemTimezone)
    return ash::system::PerUserTimezoneEnabled();
  // enable_screen_lock and pin_unlock_autosubmit_enabled
  // must be changed through the quickUnlockPrivate API.
  if (pref_name == ash::prefs::kEnableAutoScreenLock ||
      pref_name == ::prefs::kPinUnlockAutosubmitEnabled) {
    return true;
  }
#endif
#if BUILDFLAG(IS_WIN)
  // Don't allow user to change sw_reporter preferences.
  if (pref_name == prefs::kSwReporterEnabled)
    return true;
#endif
  return false;
}

}  // namespace

namespace extensions {

namespace settings_api = api::settings_private;

PrefsUtil::PrefsUtil(Profile* profile) : profile_(profile) {}

PrefsUtil::~PrefsUtil() {}

const PrefsUtil::TypedPrefMap& PrefsUtil::GetAllowlistedKeys() {
  static PrefsUtil::TypedPrefMap* s_allowlist = nullptr;
  if (s_allowlist)
    return *s_allowlist;
  s_allowlist = new PrefsUtil::TypedPrefMap();

  // Miscellaneous
  (*s_allowlist)[::embedder_support::kAlternateErrorPagesEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[autofill::prefs::kAutofillProfileEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[autofill::prefs::kAutofillCreditCardEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[autofill::prefs::kAutofillCreditCardFidoAuthEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  (*s_allowlist)[autofill::prefs::kAutofillPaymentMethodsMandatoryReauth] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_allowlist)[payments::kCanMakePaymentEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[bookmarks::prefs::kShowBookmarkBar] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kSidePanelHorizontalAlignment] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

#if BUILDFLAG(IS_LINUX)
  (*s_allowlist)[::prefs::kUseCustomChromeFrame] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_allowlist)[::prefs::kShowHomeButton] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Appearance settings.
  (*s_allowlist)[::prefs::kCurrentThemeID] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kPolicyThemeColor] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
#if BUILDFLAG(IS_LINUX)
  (*s_allowlist)[::prefs::kUsesSystemThemeDeprecated] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kSystemTheme] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
#endif
  (*s_allowlist)[::prefs::kHomePage] = settings_api::PrefType::PREF_TYPE_URL;
  (*s_allowlist)[::prefs::kHomePageIsNewTabPage] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kWebKitDefaultFixedFontSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kWebKitDefaultFontSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kWebKitMinimumFontSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kWebKitFixedFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kWebKitSansSerifFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kWebKitMathFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kWebKitSerifFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kWebKitStandardFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kDefaultCharset] =
      settings_api::PrefType::PREF_TYPE_STRING;
#if BUILDFLAG(IS_MAC)
  (*s_allowlist)[::prefs::kWebkitTabsToLinks] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kConfirmToQuitEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_allowlist)[dom_distiller::prefs::kOfferReaderMode] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // On startup.
  (*s_allowlist)[::prefs::kRestoreOnStartup] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kURLsToRestoreOnStartup] =
      settings_api::PrefType::PREF_TYPE_LIST;

  // Downloads settings.
  (*s_allowlist)[::prefs::kDownloadDefaultDirectory] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kPromptForDownload] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[drive::prefs::kDisableDrive] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  (*s_allowlist)[::prefs::kNetworkFileSharesAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kMostRecentlyUsedNetworkFileShareURL] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[drive::prefs::kDriveFsBulkPinningEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_allowlist)[::prefs::kDownloadBubblePartialViewEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Miscellaneous. TODO(stevenjb): categorize.
  (*s_allowlist)[::prefs::kEnableDoNotTrack] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kEnableEncryptedMedia] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::language::prefs::kApplicationLocale] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kNetworkPredictionOptions] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[password_manager::prefs::kCredentialsEnableService] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[password_manager::prefs::kCredentialsEnableAutosignin] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[password_manager::prefs::kPasswordLeakDetectionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [password_manager::prefs::kPasswordDismissCompromisedAlertEnabled] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  (*s_allowlist)
      [password_manager::prefs::kBiometricAuthenticationBeforeFilling] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Privacy page
  (*s_allowlist)[::prefs::kSigninAllowedOnNextStartup] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kDnsOverHttpsMode] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kDnsOverHttpsTemplates] =
      settings_api::PrefType::PREF_TYPE_STRING;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  (*s_allowlist)[::prefs::kDnsOverHttpsSalt] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kDnsOverHttpsTemplatesWithIdentifiers] =
      settings_api::PrefType::PREF_TYPE_STRING;
#endif

  // Privacy Guide
  (*s_allowlist)[::prefs::kPrivacyGuideViewed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Privacy Sandbox page
  (*s_allowlist)[::prefs::kPrivacySandboxApisEnabledV2] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kPrivacySandboxManuallyControlledV2] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kPrivacySandboxPageViewed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kPrivacySandboxM1TopicsEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kPrivacySandboxM1FledgeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kPrivacySandboxM1AdMeasurementEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Security page
  (*s_allowlist)[::kGeneratedPasswordLeakDetectionPref] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kSafeBrowsingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kSafeBrowsingEnhanced] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kSafeBrowsingScoutReportingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::safe_browsing::kGeneratedSafeBrowsingPref] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kHttpsOnlyModeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::kGeneratedHttpsFirstModePref] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Cookies page
  (*s_allowlist)[::prefs::kCookieControlsMode] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::content_settings::kCookieDefaultContentSetting] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::content_settings::kCookiePrimarySetting] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::content_settings::kCookieSessionOnly] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kPrivacySandboxFirstPartySetsEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Sync and personalization page.
  (*s_allowlist)[::prefs::kSearchSuggestEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [::unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::omnibox::kDocumentSuggestEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::commerce::kPriceEmailNotificationsEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Languages page
  (*s_allowlist)[spellcheck::prefs::kSpellCheckEnable] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[spellcheck::prefs::kSpellCheckDictionaries] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[spellcheck::prefs::kSpellCheckForcedDictionaries] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[spellcheck::prefs::kSpellCheckBlocklistedDictionaries] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[spellcheck::prefs::kSpellCheckUseSpellingService] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[translate::prefs::kOfferTranslateEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[translate::prefs::kBlockedLanguages] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[translate::prefs::kPrefNeverPromptSitesWithTime] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[language::prefs::kSelectedLanguages] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[language::prefs::kForcedLanguages] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[::language::prefs::kAcceptLanguages] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[translate::prefs::kPrefTranslateRecentTarget] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[translate::prefs::kPrefAlwaysTranslateList] =
      settings_api::PrefType::PREF_TYPE_LIST;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  (*s_allowlist)[::prefs::kLanguageImeMenuActivated] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAssistPersonalInfoEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAssistPredictiveWritingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kEmojiSuggestionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kLacrosProxyControllingExtension] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[::prefs::kLanguageInputMethodSpecificSettings] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[ash::prefs::kLastUsedImeShortcutReminderDismissed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kNextImeShortcutReminderDismissed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Files page.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  (*s_allowlist)[::prefs::kOfficeFilesAlwaysMoveToDrive] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kOfficeFilesAlwaysMoveToOneDrive] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Nearby Share.
  (*s_allowlist)[::prefs::kNearbySharingEnabledPrefName] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [::prefs::kNearbySharingFastInitiationNotificationStatePrefName] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kNearbySharingOnboardingCompletePrefName] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kNearbySharingActiveProfilePrefName] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kNearbySharingDeviceNamePrefName] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kNearbySharingDataUsageName] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
#endif

  // Search page.
  (*s_allowlist)[DefaultSearchManager::kDefaultSearchProviderDataPrefName] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[::omnibox::kKeywordSpaceTriggeringEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Site Settings prefs.
  (*s_allowlist)[::content_settings::kGeneratedNotificationPref] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kPluginsAlwaysOpenPdfExternally] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kProtectedContentDefault] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kEnableQuietNotificationPermissionUi] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Clear browsing data settings.
  (*s_allowlist)[browsing_data::prefs::kDeleteBrowsingHistory] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteBrowsingHistoryBasic] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteDownloadHistory] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteCache] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteCacheBasic] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteCookies] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteCookiesBasic] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeletePasswords] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteFormData] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteSiteSettings] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteHostedAppsData] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[browsing_data::prefs::kDeleteTimePeriod] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[browsing_data::prefs::kDeleteTimePeriodBasic] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[browsing_data::prefs::kLastClearBrowsingDataTab] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Accessibility.
  (*s_allowlist)[::prefs::kAccessibilityImageLabelsEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsTextSize] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsTextFont] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsTextColor] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsTextOpacity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsBackgroundColor] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsTextShadow] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kAccessibilityCaptionsBackgroundOpacity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
#if !BUILDFLAG(IS_ANDROID)
  (*s_allowlist)[::prefs::kLiveCaptionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kLiveCaptionLanguageCode] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kLiveTranslateEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kLiveTranslateTargetLanguageCode] =
      settings_api::PrefType::PREF_TYPE_STRING;
#endif
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  (*s_allowlist)[::prefs::kAccessibilityPdfOcrAlwaysActive] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  (*s_allowlist)[::prefs::kCaretBrowsingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Accounts / Users / People.
  (*s_allowlist)[ash::kAccountsPrefAllowGuest] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kAccountsPrefShowUserNamesOnSignIn] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kAccountsPrefAllowNewUser] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kAccountsPrefUsers] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)
      [::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  // kEnableAutoScreenLock is read-only.
  (*s_allowlist)[ash::prefs::kEnableAutoScreenLock] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  // kPinUnlockAutosubmitEnabled is read-only.
  (*s_allowlist)[::prefs::kPinUnlockAutosubmitEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kMessageCenterLockScreenMode] =
      settings_api::PrefType::PREF_TYPE_STRING;

  // Accessibility.
  (*s_allowlist)[ash::prefs::kAccessibilityAutoclickDelayMs] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityAutoclickEventType] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityAutoclickRevertToLeftClick] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityAutoclickStabilizePosition] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityAutoclickMovementThreshold] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityColorFiltering] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityGreyscaleAmount] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilitySaturationAmount] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilitySepiaAmount] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityHueRotationAmount] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityColorVisionCorrectionAmount] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityColorVisionDeficiencyType] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kShouldAlwaysShowAccessibilityMenu] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityDictationLocale] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityLargeCursorDipSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityCursorColor] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)
      [ash::prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityScreenMagnifierMouseFollowingMode] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityScreenMagnifierScale] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxAutoRead] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [ash::prefs::kAccessibilityChromeVoxAnnounceDownloadNotifications] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [ash::prefs::kAccessibilityChromeVoxAnnounceRichTextAttributes] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxAudioStrategy] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxBrailleSideBySide] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxBrailleTable] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxBrailleTable6] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxBrailleTable8] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxBrailleTableType] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxBrailleWordWrap] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxCapitalStrategy] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxCapitalStrategyBackup] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxEnableBrailleLogging] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxEnableEarconLogging] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxEnableEventStreamLogging] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxEnableSpeechLogging] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxEventStreamFilters] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxLanguageSwitching] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxMenuBrailleCommands] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxNumberReadingStyle] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)
      [ash::prefs::kAccessibilityChromeVoxPreferredBrailleDisplayAddress] =
          settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxPunctuationEcho] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxSmartStickyMode] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxSpeakTextUnderMouse] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxUsePitchChanges] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxUseVerboseMode] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxVirtualBrailleColumns] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxVirtualBrailleRows] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityChromeVoxVoiceName] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[ash::prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[ash::prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySwitchAccessAutoScanSpeedMs] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)
      [ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [ash::prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)
      [ash::prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kAccessibilityMonoAudioEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [ash::prefs::kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakBackgroundShading] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakEnhancedNetworkVoices] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakEnhancedVoiceName] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)
      [ash::prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakHighlightColor] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakNavigationControls] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakVoiceName] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakVoiceSwitching] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kAccessibilitySelectToSpeakWordHighlight] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Text to Speech.
  (*s_allowlist)[::prefs::kTextToSpeechLangToVoiceName] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[::prefs::kTextToSpeechRate] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kTextToSpeechPitch] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kTextToSpeechVolume] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Guest OS
  (*s_allowlist)[bruschetta::prefs::kBruschettaInstalled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[crostini::prefs::kCrostiniEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[crostini::prefs::kCrostiniMicAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[crostini::prefs::kCrostiniSharedUsbDevices] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[guest_os::prefs::kGuestOsContainers] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[crostini::prefs::kCrostiniPortForwarding] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[guest_os::prefs::kGuestOSPathsSharedToVms] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;

  // Plugin Vm
  (*s_allowlist)[plugin_vm::prefs::kPluginVmImageExists] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[plugin_vm::prefs::kPluginVmPrintersAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Android Apps.
  (*s_allowlist)[arc::prefs::kArcEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // App Notifications
  (*s_allowlist)[::ash::prefs::kAppNotificationBadgingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Ambient Mode.
  (*s_allowlist)[ash::ambient::prefs::kAmbientModeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  // The following prefs are not displayed to the user but are configurable to
  // speed up automated testing of Ambient mode.
  (*s_allowlist)
      [ash::ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)
      [ash::ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::ambient::prefs::kAmbientModeAnimationPlaybackSpeed] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::ambient::prefs::kAmbientModeRunningDurationMinutes] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Google Assistant.
  (*s_allowlist)[ash::assistant::prefs::kAssistantConsentStatus] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::assistant::prefs::kAssistantDisabledByPolicy] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantContextEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantHotwordAlwaysOn] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantHotwordEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantVoiceMatchEnabledDuringOobe] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantLaunchWithMicOpen] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::assistant::prefs::kAssistantNotificationEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Quick Answers.
  (*s_allowlist)[quick_answers::prefs::kQuickAnswersEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[quick_answers::prefs::kQuickAnswersDefinitionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[quick_answers::prefs::kQuickAnswersTranslationEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[quick_answers::prefs::kQuickAnswersUnitConversionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Misc.
  (*s_allowlist)[::prefs::kUse24HourClock] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::language::prefs::kPreferredLanguages] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[ash::prefs::kTapDraggingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kStatsReportingPref] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::metrics::prefs::kMetricsUserConsent] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kSuggestedContentEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kAttestationForContentProtectionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[prefs::kRestoreLastLockScreenNote] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kDevicePeripheralDataAccessEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kLocalStateDevicePeripheralDataAccessEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kRevenEnableDeviceHWDataUsage] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Bluetooth & Internet settings.
  (*s_allowlist)[ash::kAllowBluetooth] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[proxy_config::prefs::kUseSharedProxies] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::kSignedDataRoamingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kUserBluetoothAdapterEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kSystemBluetoothAdapterEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kFastPairEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kVpnConfigAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[arc::prefs::kAlwaysOnVpnPackage] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[arc::prefs::kAlwaysOnVpnLockdown] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Restore apps and pages on startup
  (*s_allowlist)[ash::full_restore::kRestoreAppsAndPagesPrefName] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Timezone settings.
  (*s_allowlist)[ash::kSystemTimezone] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[prefs::kUserTimezone] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[settings_private::kResolveTimezoneByGeolocationOnOff] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::kPerUserTimezoneEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[settings_private::kResolveTimezoneByGeolocationMethodShort] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::kFineGrainedTimeZoneResolveEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[prefs::kSystemTimezoneAutomaticDetectionPolicy] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Ash settings.
  (*s_allowlist)[ash::prefs::kAmbientColorEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kEnableStylusTools] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kLaunchPaletteOnEjectEvent] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kNightLightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kNightLightTemperature] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kNightLightScheduleType] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kNightLightCustomStartTime] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kNightLightCustomEndTime] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kDockedMagnifierScale] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kDockedMagnifierScreenHeightDivisor] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Input method settings.
  (*s_allowlist)[::prefs::kLanguagePreloadEngines] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kLanguageEnabledImes] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_allowlist)[::prefs::kLanguageAllowedInputMethods] =
      settings_api::PrefType::PREF_TYPE_LIST;

  // Device settings.
  (*s_allowlist)[ash::prefs::kTapToClickEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kNaturalScroll] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kTouchpadSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kTouchpadScrollSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kTouchpadHapticFeedback] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kTouchpadHapticClickSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kPrimaryMouseButtonRight] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kPrimaryPointingStickButtonRight] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kMouseReverseScroll] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kMouseAcceleration] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kMouseScrollAcceleration] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kPointingStickAcceleration] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kTouchpadAcceleration] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kTouchpadScrollAcceleration] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kMouseSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kMouseScrollSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kPointingStickSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapSearchKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapControlKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapAltKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapCapsLockKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapBackspaceKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapAssistantKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapEscapeKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapExternalCommandKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::prefs::kLanguageRemapExternalMetaKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::ash::prefs::kSendFunctionKeys] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kLongPressDiacriticsEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kXkbAutoRepeatEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kXkbAutoRepeatDelay] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[::ash::prefs::kXkbAutoRepeatInterval] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::kDeviceDisplayResolution] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[ash::kDisplayRotationDefault] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_allowlist)[arc::prefs::kArcVisibleExternalStorages] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[ash::prefs::kPowerAdaptiveChargingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kConsumerAutoUpdateToggle] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Native Printing settings.
  (*s_allowlist)[::prefs::kUserPrintersAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Privacy settings.
  (*s_allowlist)[::ash::prefs::kSnoopingProtectionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)
      [::ash::prefs::kSnoopingProtectionNotificationSuppressionEnabled] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kPowerQuickDimEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::ash::prefs::kPowerQuickLockDelay] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[ash::prefs::kUserCameraAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kUserMicrophoneAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kUserSpeakOnMuteDetectionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[ash::prefs::kUserGeolocationAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#else
  // System settings.
  (*s_allowlist)[::prefs::kBackgroundModeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kHardwareAccelerationModeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Import data
  (*s_allowlist)[::prefs::kImportDialogAutofillFormData] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kImportDialogBookmarks] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kImportDialogHistory] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kImportDialogSavedPasswords] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kImportDialogSearchEngine] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Supervised Users.  This setting is queried in our Tast tests (b/241943380).
  (*s_allowlist)[::prefs::kSupervisedUserExtensionsMayRequestPermissions] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  (*s_allowlist)[::prefs::kUseAshProxy] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  (*s_allowlist)[::prefs::kPrintingAPIExtensionsAllowlist] =
      settings_api::PrefType::PREF_TYPE_LIST;
#endif

// Accessibility features in ash that can be set in lacros.
// The Lacros version of these have their own name.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)
  (*s_allowlist)[chromeos::prefs::kAccessibilityFocusHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kDockedMagnifierEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityAutoclickEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityCaretHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityCursorColorEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityCursorHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityDictationEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityHighContrastEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityLargeCursorEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityScreenMagnifierEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilitySelectToSpeakEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilitySpokenFeedbackEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityStickyKeysEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilitySwitchAccessEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[chromeos::prefs::kAccessibilityVirtualKeyboardEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // This feature exists in all platforms but is enabled in ash above. In lacros
  // the value in ash can be controlled by extensions.
  (*s_allowlist)[prefs::kAccessibilityFocusHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Proxy settings.
  (*s_allowlist)[proxy_config::prefs::kProxy] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  (*s_allowlist)[::prefs::kUserFeedbackAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Media Remoting settings.
  (*s_allowlist)[media_router::prefs::kMediaRouterMediaRemotingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

#if BUILDFLAG(IS_WIN)
  // SwReporter settings.
  (*s_allowlist)[::prefs::kSwReporterEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_allowlist)[::prefs::kSwReporterReportingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Performance settings.
  (*s_allowlist)
      [performance_manager::user_tuning::prefs::kHighEfficiencyModeState] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)[performance_manager::user_tuning::prefs::
                     kHighEfficiencyModeTimeBeforeDiscardInMinutes] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)
      [performance_manager::user_tuning::prefs::kBatterySaverModeState] =
          settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_allowlist)
      [performance_manager::user_tuning::prefs::kTabDiscardingExceptions] =
          settings_api::PrefType::PREF_TYPE_LIST;
  (*s_allowlist)[performance_manager::user_tuning::prefs::
                     kManagedTabDiscardingExceptions] =
      settings_api::PrefType::PREF_TYPE_LIST;

  return *s_allowlist;
}

settings_api::PrefType PrefsUtil::GetAllowlistedPrefType(
    const std::string& pref_name) {
  const TypedPrefMap& keys = GetAllowlistedKeys();
  const auto& iter = keys.find(pref_name);
  return iter != keys.end() ? iter->second
                            : settings_api::PrefType::PREF_TYPE_NONE;
}

settings_api::PrefType PrefsUtil::GetType(const std::string& name,
                                          base::Value::Type type) {
  switch (type) {
    case base::Value::Type::BOOLEAN:
      return settings_api::PrefType::PREF_TYPE_BOOLEAN;
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
      return settings_api::PrefType::PREF_TYPE_NUMBER;
    case base::Value::Type::STRING:
      return IsPrefTypeURL(name) ? settings_api::PrefType::PREF_TYPE_URL
                                 : settings_api::PrefType::PREF_TYPE_STRING;
    case base::Value::Type::LIST:
      return settings_api::PrefType::PREF_TYPE_LIST;
    case base::Value::Type::DICT:
      return settings_api::PrefType::PREF_TYPE_DICTIONARY;
    default:
      return settings_api::PrefType::PREF_TYPE_NONE;
  }
}

absl::optional<settings_api::PrefObject> PrefsUtil::GetCrosSettingsPref(
    const std::string& name) {
  absl::optional<settings_api::PrefObject> pref_object(absl::in_place);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::Value* value = ash::CrosSettings::Get()->GetPref(name);
  if (!value) {
    LOG(WARNING) << "Cros settings pref not found: " << name;
    return absl::nullopt;
  }
  pref_object->key = name;
  pref_object->type = GetType(name, value->type());
  pref_object->value = value->Clone();
#endif

  return pref_object;
}

absl::optional<settings_api::PrefObject> PrefsUtil::GetPref(
    const std::string& name) {
  if (GetAllowlistedPrefType(name) == settings_api::PrefType::PREF_TYPE_NONE) {
    return absl::nullopt;
  }

  settings_private::GeneratedPrefs* generated_prefs =
      settings_private::GeneratedPrefsFactory::GetForBrowserContext(profile_);

  const PrefService::Preference* pref = nullptr;
  absl::optional<settings_api::PrefObject> pref_object;
  if (IsCrosSetting(name)) {
    pref_object = GetCrosSettingsPref(name);
    if (!pref_object)
      return absl::nullopt;
  } else if (generated_prefs && generated_prefs->HasPref(name)) {
    return generated_prefs->GetPref(name);
  } else {
    PrefService* pref_service = FindServiceForPref(name);
    pref = pref_service->FindPreference(name);
    if (!pref)
      return absl::nullopt;
    pref_object.emplace();
    pref_object->key = pref->name();
    pref_object->type = GetType(name, pref->GetType());
    pref_object->value = pref->GetValue()->Clone();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We first check for enterprise-managed, then for primary-user managed.
  // Otherwise in multiprofile mode enterprise preference for the secondary
  // user will appear primary-user-controlled, which looks strange, because
  // primary user preference will be disabled with "enterprise controlled"
  // status.
  if (IsPrefEnterpriseManaged(name)) {
    // Enterprise managed prefs are treated the same as device policy restricted
    // prefs in the UI.
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    return pref_object;
  }

  if (IsPrefPrimaryUserControlled(name)) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_PRIMARY_USER;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->controlled_by_name = user_manager::UserManager::Get()
                                          ->GetPrimaryUser()
                                          ->GetAccountId()
                                          .GetUserEmail();
    return pref_object;
  }
#endif

  if (pref && pref->IsManaged()) {
    if (profile_->IsChild()) {
      pref_object->controlled_by =
          settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    } else {
      pref_object->controlled_by =
          settings_api::ControlledBy::CONTROLLED_BY_USER_POLICY;
    }
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    return pref_object;
  }

  // A pref is recommended if it has a recommended value, regardless of whether
  // the current value is set by policy. The UI will test to see whether the
  // current value matches the recommended value and inform the user.
  const base::Value* recommended = pref ? pref->GetRecommendedValue() : nullptr;
  if (recommended) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_USER_POLICY;
    pref_object->enforcement =
        settings_api::Enforcement::ENFORCEMENT_RECOMMENDED;
    pref_object->recommended_value = recommended->Clone();
    return pref_object;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsPrefOwnerControlled(name)) {
    // Check for owner controlled after managed checks because if there is a
    // device policy there is no "owner". (In the unlikely case that both
    // situations apply, either badge is potentially relevant, so the order
    // is somewhat arbitrary).
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_OWNER;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->controlled_by_name =
        user_manager::UserManager::Get()->GetOwnerAccountId().GetUserEmail();
    return pref_object;
  }

  if (IsRestrictedCrosSettingForChildUser(profile_, name)) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->value = base::Value(
        GetRestrictedCrosSettingValueForChildUser(profile_, name)->Clone());
    return pref_object;
  }

  if (IsHotwordDisabledForChildUser(name)) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    return pref_object;
  }
#endif

  const Extension* extension = GetExtensionControllingPref(*pref_object);

  if (extension) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->extension_id = extension->id();
    pref_object->controlled_by_name = extension->name();
    bool can_be_disabled =
        !ExtensionSystem::Get(profile_)->management_policy()->MustRemainEnabled(
            extension, nullptr);
    pref_object->extension_can_be_disabled = can_be_disabled;
    return pref_object;
  }

  // TODO(dbeam): surface !IsUserModifiable or IsPrefSupervisorControlled?

  return pref_object;
}

settings_private::SetPrefResult PrefsUtil::SetPref(const std::string& pref_name,
                                                   const base::Value* value) {
  if (GetAllowlistedPrefType(pref_name) ==
      settings_api::PrefType::PREF_TYPE_NONE) {
    return settings_private::SetPrefResult::PREF_NOT_FOUND;
  }

  if (IsCrosSetting(pref_name))
    return SetCrosSettingsPref(pref_name, value);

  settings_private::GeneratedPrefs* generated_prefs =
      settings_private::GeneratedPrefsFactory::GetForBrowserContext(profile_);
  if (generated_prefs && generated_prefs->HasPref(pref_name))
    return generated_prefs->SetPref(pref_name, value);

  PrefService* pref_service = FindServiceForPref(pref_name);

  if (!IsPrefUserModifiable(pref_name))
    return settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  const PrefService::Preference* pref = pref_service->FindPreference(pref_name);
  if (!pref)
    return settings_private::SetPrefResult::PREF_NOT_FOUND;

  switch (pref->GetType()) {
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::LIST:
    case base::Value::Type::DICT:
      pref_service->Set(pref_name, *value);
      break;
    case base::Value::Type::DOUBLE:
    case base::Value::Type::INTEGER:
      // Explicitly set the double value or the integer value.
      // Otherwise if the number is a whole number like 2.0, it will
      // automatically be of type INTEGER causing type mismatches in
      // PrefService::SetUserPrefValue for doubles, and vice versa.
      if (!value->is_double() && !value->is_int())
        return settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
      double double_value;
      double_value = value->GetDouble();

      if (pref->GetType() == base::Value::Type::DOUBLE)
        pref_service->SetDouble(pref_name, double_value);
      else
        pref_service->SetInteger(pref_name, static_cast<int>(double_value));
      break;
    case base::Value::Type::STRING: {
      if (!value->is_string())
        return settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

      std::string string_value = value->GetString();
      if (IsPrefTypeURL(pref_name)) {
        GURL fixed = url_formatter::FixupURL(string_value, std::string());
        if (fixed.is_valid())
          string_value = fixed.spec();
        else
          string_value = std::string();
      }

      pref_service->SetString(pref_name, string_value);
      break;
    }
    default:
      return settings_private::SetPrefResult::PREF_TYPE_UNSUPPORTED;
  }

  // TODO(orenb): Process setting metrics here and in the CrOS setting method
  // too (like "ProcessUserMetric" in CoreOptionsHandler).
  return settings_private::SetPrefResult::SUCCESS;
}

settings_private::SetPrefResult PrefsUtil::SetCrosSettingsPref(
    const std::string& pref_name,
    const base::Value* value) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (pref_name == ash::kSystemTimezone) {
    const std::string* string_value = value->GetIfString();
    if (!string_value)
      return settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user && ash::system::SetSystemTimezone(user, *string_value))
      return settings_private::SetPrefResult::SUCCESS;
    return settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  ash::OwnerSettingsServiceAsh* service =
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_);

  if (service && service->HandlesSetting(pref_name) &&
      service->Set(pref_name, *value)) {
    return settings_private::SetPrefResult::SUCCESS;
  }
  return settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

#else
  return settings_private::SetPrefResult::PREF_NOT_FOUND;
#endif
}

bool PrefsUtil::AppendToListCrosSetting(const std::string& pref_name,
                                        const base::Value& value) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::OwnerSettingsServiceAsh* service =
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_);

  return service && service->HandlesSetting(pref_name) &&
         service->AppendToList(pref_name, value);

#else
  return false;
#endif
}

bool PrefsUtil::RemoveFromListCrosSetting(const std::string& pref_name,
                                          const base::Value& value) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::OwnerSettingsServiceAsh* service =
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_);

  return service && service->HandlesSetting(pref_name) &&
         service->RemoveFromList(pref_name, value);

#else
  return false;
#endif
}

bool PrefsUtil::IsPrefTypeURL(const std::string& pref_name) {
  return GetAllowlistedPrefType(pref_name) ==
         settings_api::PrefType::PREF_TYPE_URL;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool PrefsUtil::IsPrefEnterpriseManaged(const std::string& pref_name) {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged())
    return false;
  if (IsPrivilegedCrosSetting(pref_name))
    return true;
  if (pref_name == ash::kSystemTimezone || pref_name == prefs::kUserTimezone) {
    return ash::system::IsTimezonePrefsManaged(pref_name);
  }
  return false;
}

bool PrefsUtil::IsPrefOwnerControlled(const std::string& pref_name) {
  // ash::kSystemTimezone is global display-only preference and
  // it should appear as disabled, but not owned.
  if (pref_name == ash::kSystemTimezone)
    return false;

  if (IsPrivilegedCrosSetting(pref_name)) {
    if (!ash::ProfileHelper::IsOwnerProfile(profile_))
      return true;
  }
  return false;
}

bool PrefsUtil::IsPrefPrimaryUserControlled(const std::string& pref_name) {
  // ash::kSystemTimezone is read-only, but for the non-primary users
  // it should have "primary user controlled" attribute.
  if (pref_name == prefs::kUserTimezone || pref_name == ash::kSystemTimezone) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user && user->GetAccountId() !=
                    user_manager->GetPrimaryUser()->GetAccountId()) {
      return true;
    }
  }
  return false;
}

bool PrefsUtil::IsHotwordDisabledForChildUser(const std::string& pref_name) {
  const std::string& hotwordEnabledPref =
      ash::assistant::prefs::kAssistantHotwordEnabled;
  if (!profile_->IsChild() || pref_name != hotwordEnabledPref)
    return false;

  PrefService* pref_service = FindServiceForPref(hotwordEnabledPref);
  const PrefService::Preference* pref =
      pref_service->FindPreference(hotwordEnabledPref);
  DCHECK(pref);
  const bool isHotwordEnabled = pref->GetValue()->GetIfBool().value_or(false);
  return !isHotwordEnabled;
}
#endif

bool PrefsUtil::IsPrefSupervisorControlled(const std::string& pref_name) {
  if (pref_name != prefs::kBrowserGuestModeEnabled &&
      pref_name != prefs::kBrowserAddPersonEnabled) {
    return false;
  }
  return profile_->IsChild();
}

bool PrefsUtil::IsPrefUserModifiable(const std::string& pref_name) {
  if (IsSettingReadOnly(pref_name))
    return false;

  const PrefService::Preference* profile_pref =
      profile_->GetPrefs()->FindPreference(pref_name);
  if (profile_pref)
    return profile_pref->IsUserModifiable();

  const PrefService::Preference* local_state_pref =
      g_browser_process->local_state()->FindPreference(pref_name);
  if (local_state_pref)
    return local_state_pref->IsUserModifiable();

  return false;
}

PrefService* PrefsUtil::FindServiceForPref(const std::string& pref_name) {
  PrefService* user_prefs = profile_->GetPrefs();

  // Proxy is a peculiar case: on ChromeOS, settings exist in both user
  // prefs and local state, but chrome://settings should affect only user prefs.
  // Elsewhere the proxy settings are stored in local state.
  // See http://crbug.com/157147

  if (pref_name == proxy_config::prefs::kProxy) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return user_prefs;
#else
    return g_browser_process->local_state();
#endif
  }

  // Find which PrefService contains the given pref. Pref names should not
  // be duplicated across services, however if they are, prefer the user's
  // prefs.
  if (user_prefs->FindPreference(pref_name))
    return user_prefs;

  if (g_browser_process->local_state()->FindPreference(pref_name))
    return g_browser_process->local_state();

  return user_prefs;
}

bool PrefsUtil::IsCrosSetting(const std::string& pref_name) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::CrosSettings::Get()->IsCrosSettings(pref_name);
#else
  return false;
#endif
}

const Extension* PrefsUtil::GetExtensionControllingPref(
    const settings_api::PrefObject& pref_object) {
  // Look for specific prefs that might be extension controlled. This generally
  // corresponds with some indiciator that should be shown in the settings UI.
  if (pref_object.key == ::prefs::kHomePage)
    return GetExtensionOverridingHomepage(profile_);

  if (pref_object.key == ::prefs::kRestoreOnStartup) {
    if (pref_object.value->GetInt() == SessionStartupPref::kPrefValueURLs)
      return GetExtensionOverridingStartupPages(profile_);
  }

  if (pref_object.key == ::prefs::kURLsToRestoreOnStartup)
    return GetExtensionOverridingStartupPages(profile_);

  if (pref_object.key ==
      DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
    return GetExtensionOverridingSearchEngine(profile_);
  }

  if (pref_object.key == proxy_config::prefs::kProxy)
    return GetExtensionOverridingProxy(profile_);

  // If it's none of the above, attempt a more general strategy.
  std::string extension_id =
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile_)
          ->GetExtensionControllingPref(pref_object.key);
  if (extension_id.empty())
    return nullptr;

  return ExtensionRegistry::Get(profile_)->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED);
}

}  // namespace extensions
