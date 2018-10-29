// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/prefs_util.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/drive/drive_pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_manager.h"
#include "components/spellcheck/browser/pref_names.h"
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

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"  // nogncheck
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_method_short.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_on_off.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/arc/arc_prefs.h"
#include "ui/chromeos/events/pref_names.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
bool IsPrivilegedCrosSetting(const std::string& pref_name) {
  if (!chromeos::CrosSettings::IsCrosSettings(pref_name))
    return false;
  if (!chromeos::system::PerUserTimezoneEnabled()) {
    // kSystemTimezone should be changeable by all users.
    if (pref_name == chromeos::kSystemTimezone)
      return false;
  }
  // Cros settings are considered privileged and are either policy
  // controlled or owner controlled.
  return true;
}
#endif

bool IsSettingReadOnly(const std::string& pref_name) {
  // download.default_directory is used to display the directory location and
  // for policy indicators, but should not be changed directly.
  if (pref_name == prefs::kDownloadDefaultDirectory)
    return true;
#if defined(OS_CHROMEOS)
  // System timezone is never directly changeable by the user.
  if (pref_name == chromeos::kSystemTimezone)
    return chromeos::system::PerUserTimezoneEnabled();
  // enable_screen_lock must be changed through the quickUnlockPrivate API.
  if (pref_name == ash::prefs::kEnableAutoScreenLock)
    return true;
#endif
#if defined(OS_WIN)
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

#if defined(OS_CHROMEOS)
using CrosSettings = chromeos::CrosSettings;
#endif

const PrefsUtil::TypedPrefMap& PrefsUtil::GetWhitelistedKeys() {
  static PrefsUtil::TypedPrefMap* s_whitelist = nullptr;
  if (s_whitelist)
    return *s_whitelist;
  s_whitelist = new PrefsUtil::TypedPrefMap();

  // Miscellaneous
  (*s_whitelist)[::prefs::kAlternateErrorPagesEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[autofill::prefs::kAutofillProfileEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[autofill::prefs::kAutofillCreditCardEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[payments::kCanMakePaymentEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[bookmarks::prefs::kShowBookmarkBar] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  (*s_whitelist)[::prefs::kUseCustomChromeFrame] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_whitelist)[::prefs::kShowHomeButton] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Appearance settings.
  (*s_whitelist)[::prefs::kCurrentThemeID] =
      settings_api::PrefType::PREF_TYPE_STRING;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  (*s_whitelist)[::prefs::kUsesSystemTheme] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif
  (*s_whitelist)[::prefs::kHomePage] = settings_api::PrefType::PREF_TYPE_URL;
  (*s_whitelist)[::prefs::kHomePageIsNewTabPage] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kWebKitDefaultFixedFontSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kWebKitDefaultFontSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kWebKitMinimumFontSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kWebKitFixedFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kWebKitSansSerifFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kWebKitSerifFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kWebKitStandardFontFamily] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kDefaultCharset] =
      settings_api::PrefType::PREF_TYPE_STRING;
#if defined(OS_MACOSX)
  (*s_whitelist)[::prefs::kWebkitTabsToLinks] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // On startup.
  (*s_whitelist)[::prefs::kRestoreOnStartup] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kURLsToRestoreOnStartup] =
      settings_api::PrefType::PREF_TYPE_LIST;

  // Downloads settings.
  (*s_whitelist)[::prefs::kDownloadDefaultDirectory] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kPromptForDownload] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[drive::prefs::kDisableDrive] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#if defined(OS_CHROMEOS)
  (*s_whitelist)[::prefs::kNetworkFileSharesAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Printing settings.
  (*s_whitelist)[::prefs::kLocalDiscoveryNotificationsEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Miscellaneous. TODO(stevenjb): categorize.
  (*s_whitelist)[::prefs::kEnableDoNotTrack] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kEnableEncryptedMedia] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::language::prefs::kApplicationLocale] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kNetworkPredictionOptions] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[password_manager::prefs::kCredentialsEnableService] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[password_manager::prefs::kCredentialsEnableAutosignin] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Privacy page
  (*s_whitelist)[::prefs::kSigninAllowedOnNextStartup] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Sync and personalization page.
  (*s_whitelist)[::prefs::kSafeBrowsingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kSafeBrowsingScoutReportingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kSearchSuggestEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::unified_consent::prefs::kUnifiedConsentGiven] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)
      [::unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled] =
          settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::omnibox::kDocumentSuggestEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Languages page
  (*s_whitelist)[spellcheck::prefs::kSpellCheckEnable] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[spellcheck::prefs::kSpellCheckDictionaries] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)[spellcheck::prefs::kSpellCheckForcedDictionaries] =
      settings_api::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)[spellcheck::prefs::kSpellCheckUseSpellingService] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kOfferTranslateEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[translate::TranslatePrefs::kPrefTranslateBlockedLanguages] =
      settings_api::PrefType::PREF_TYPE_LIST;
#if defined(OS_CHROMEOS)
  (*s_whitelist)[::prefs::kLanguageImeMenuActivated] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Search page.
  (*s_whitelist)[DefaultSearchManager::kDefaultSearchProviderDataPrefName] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;

  // Site Settings prefs.
  (*s_whitelist)[::prefs::kBlockThirdPartyCookies] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kPluginsAlwaysOpenPdfExternally] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kEnableDRM] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Clear browsing data settings.
  (*s_whitelist)[browsing_data::prefs::kDeleteBrowsingHistory] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteBrowsingHistoryBasic] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteDownloadHistory] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteCache] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteCacheBasic] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteCookies] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteCookiesBasic] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeletePasswords] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteFormData] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteSiteSettings] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteHostedAppsData] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteMediaLicenses] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[browsing_data::prefs::kDeleteTimePeriod] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[browsing_data::prefs::kDeleteTimePeriodBasic] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[browsing_data::prefs::kLastClearBrowsingDataTab] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

#if defined(OS_CHROMEOS)
  // Accounts / Users / People.
  (*s_whitelist)[chromeos::kAccountsPrefAllowGuest] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefSupervisedUsersEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefShowUserNamesOnSignIn] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefAllowNewUser] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAccountsPrefUsers] =
      settings_api::PrefType::PREF_TYPE_LIST;
  // kEnableAutoScreenLock is read-only.
  (*s_whitelist)[ash::prefs::kEnableAutoScreenLock] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[proximity_auth::prefs::kEasyUnlockProximityThreshold] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kMessageCenterLockScreenMode] =
      settings_api::PrefType::PREF_TYPE_STRING;

  // TODO(crbug.com/894585): After M71, only whitelist the Smart Lock 'sign-in
  // enabled' pref in the pre-Multidevice case, i.e., when
  // kEnableUnifiedMultiDeviceSettings is false. In the Multidevice case, JS
  // access to this pref is restricted.
  (*s_whitelist)[proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Accessibility.
  (*s_whitelist)[ash::prefs::kAccessibilitySpokenFeedbackEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityAutoclickEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityAutoclickDelayMs] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityAutoclickEventType] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kAccessibilityAutoclickRevertToLeftClick] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityCaretHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityCursorHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kShouldAlwaysShowAccessibilityMenu] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityDictationEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityFocusHighlightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityHighContrastEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityLargeCursorEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityLargeCursorDipSize] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kAccessibilityScreenMagnifierEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityScreenMagnifierScale] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kAccessibilitySelectToSpeakEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityStickyKeysEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilitySwitchAccessEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityVirtualKeyboardEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kAccessibilityMonoAudioEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Text to Speech.
  (*s_whitelist)[::prefs::kTextToSpeechLangToVoiceName] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;
  (*s_whitelist)[::prefs::kTextToSpeechRate] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kTextToSpeechPitch] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kTextToSpeechVolume] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Crostini
  (*s_whitelist)[crostini::prefs::kCrostiniEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[crostini::prefs::kCrostiniSharedPaths] =
      settings_api::PrefType::PREF_TYPE_LIST;

  // Android Apps.
  (*s_whitelist)[arc::prefs::kArcEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Google Assistant.
  (*s_whitelist)[arc::prefs::kVoiceInteractionActivityControlAccepted] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[arc::prefs::kArcVoiceInteractionValuePropAccepted] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[arc::prefs::kVoiceInteractionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[arc::prefs::kVoiceInteractionContextEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[arc::prefs::kVoiceInteractionHotwordEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[arc::prefs::kVoiceInteractionNotificationEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[arc::prefs::kVoiceInteractionLaunchWithMicOpen] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Misc.
  (*s_whitelist)[::prefs::kUse24HourClock] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLanguagePreferredLanguages] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[ash::prefs::kTapDraggingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kStatsReportingPref] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kAttestationForContentProtectionEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[prefs::kRestoreLastLockScreenNote] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Bluetooth & Internet settings.
  (*s_whitelist)[chromeos::kAllowBluetooth] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[proxy_config::prefs::kUseSharedProxies] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kWakeOnWifiDarkConnect] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::chromeos::kSignedDataRoamingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::ash::prefs::kUserBluetoothAdapterEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Timezone settings.
  (*s_whitelist)[chromeos::kSystemTimezone] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[prefs::kUserTimezone] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[settings_private::kResolveTimezoneByGeolocationOnOff] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[chromeos::kPerUserTimezoneEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[settings_private::kResolveTimezoneByGeolocationMethodShort] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[chromeos::kFineGrainedTimeZoneResolveEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[prefs::kSystemTimezoneAutomaticDetectionPolicy] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Ash settings.
  (*s_whitelist)[ash::prefs::kEnableStylusTools] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kLaunchPaletteOnEjectEvent] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kNightLightEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kNightLightTemperature] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kNightLightScheduleType] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kNightLightCustomStartTime] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kNightLightCustomEndTime] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[ash::prefs::kDockedMagnifierEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[ash::prefs::kDockedMagnifierScale] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Input method settings.
  (*s_whitelist)[::prefs::kLanguagePreloadEngines] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kLanguageEnabledImes] =
      settings_api::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)[::prefs::kLanguageAllowedInputMethods] =
      settings_api::PrefType::PREF_TYPE_LIST;

  // Device settings.
  (*s_whitelist)[::prefs::kTapToClickEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kNaturalScroll] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kTouchpadSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kPrimaryMouseButtonRight] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kMouseReverseScroll] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kMouseSensitivity] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapSearchKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapControlKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapAltKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapCapsLockKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapBackspaceKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapEscapeKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapDiamondKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapExternalCommandKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageRemapExternalMetaKeyTo] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageSendFunctionKeys] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLanguageXkbAutoRepeatEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kLanguageXkbAutoRepeatDelay] =
      settings_api::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)[::prefs::kLanguageXkbAutoRepeatInterval] =
      settings_api::PrefType::PREF_TYPE_NUMBER;

  // Native Printing settings.
  (*s_whitelist)[::prefs::kUserNativePrintersAllowed] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

#else
  (*s_whitelist)[::prefs::kAcceptLanguages] =
      settings_api::PrefType::PREF_TYPE_STRING;

  // System settings.
  (*s_whitelist)[::prefs::kBackgroundModeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kHardwareAccelerationModeEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

  // Import data
  (*s_whitelist)[::prefs::kImportDialogAutofillFormData] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportDialogBookmarks] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportDialogHistory] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportDialogSavedPasswords] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)[::prefs::kImportDialogSearchEngine] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  // Proxy settings.
  (*s_whitelist)[proxy_config::prefs::kProxy] =
      settings_api::PrefType::PREF_TYPE_DICTIONARY;

#if defined(GOOGLE_CHROME_BUILD)
  (*s_whitelist)[::prefs::kMediaRouterEnableCloudServices] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif  // defined(GOOGLE_CHROME_BUILD)

  // Media Remoting settings.
  (*s_whitelist)[::prefs::kMediaRouterMediaRemotingEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;

#if defined(OS_WIN)
  // SwReporter settings.
  (*s_whitelist)[::prefs::kSwReporterEnabled] =
      settings_api::PrefType::PREF_TYPE_BOOLEAN;
#endif

  return *s_whitelist;
}

settings_api::PrefType PrefsUtil::GetWhitelistedPrefType(
    const std::string& pref_name) {
  const TypedPrefMap& keys = GetWhitelistedKeys();
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
    case base::Value::Type::DICTIONARY:
      return settings_api::PrefType::PREF_TYPE_DICTIONARY;
    default:
      return settings_api::PrefType::PREF_TYPE_NONE;
  }
}

std::unique_ptr<settings_api::PrefObject> PrefsUtil::GetCrosSettingsPref(
    const std::string& name) {
  std::unique_ptr<settings_api::PrefObject> pref_object(
      new settings_api::PrefObject());

#if defined(OS_CHROMEOS)
  const base::Value* value = CrosSettings::Get()->GetPref(name);
  if (!value) {
    LOG(ERROR) << "Cros settings pref not found: " << name;
    return nullptr;
  }
  pref_object->key = name;
  pref_object->type = GetType(name, value->type());
  pref_object->value.reset(value->DeepCopy());
#endif

  return pref_object;
}

std::unique_ptr<settings_api::PrefObject> PrefsUtil::GetPref(
    const std::string& name) {
  if (GetWhitelistedPrefType(name) == settings_api::PrefType::PREF_TYPE_NONE) {
    return nullptr;
  }

  settings_private::GeneratedPrefs* generated_prefs =
      settings_private::GeneratedPrefsFactory::GetForBrowserContext(profile_);

  const PrefService::Preference* pref = nullptr;
  std::unique_ptr<settings_api::PrefObject> pref_object;
  if (IsCrosSetting(name)) {
    pref_object = GetCrosSettingsPref(name);
    if (!pref_object)
      return nullptr;
  } else if (generated_prefs && generated_prefs->HasPref(name)) {
    return generated_prefs->GetPref(name);
  } else {
    PrefService* pref_service = FindServiceForPref(name);
    pref = pref_service->FindPreference(name);
    if (!pref)
      return nullptr;
    pref_object.reset(new settings_api::PrefObject());
    pref_object->key = pref->name();
    pref_object->type = GetType(name, pref->GetType());
    pref_object->value.reset(pref->GetValue()->DeepCopy());
  }

#if defined(OS_CHROMEOS)
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
    pref_object->controlled_by_name.reset(
        new std::string(user_manager::UserManager::Get()
                            ->GetPrimaryUser()
                            ->GetAccountId()
                            .GetUserEmail()));
    return pref_object;
  }
#endif

  if (pref && pref->IsManaged()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_USER_POLICY;
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
    pref_object->recommended_value.reset(recommended->DeepCopy());
    return pref_object;
  }

#if defined(OS_CHROMEOS)
  if (IsPrefOwnerControlled(name)) {
    // Check for owner controlled after managed checks because if there is a
    // device policy there is no "owner". (In the unlikely case that both
    // situations apply, either badge is potentially relevant, so the order
    // is somewhat arbitrary).
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_OWNER;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->controlled_by_name.reset(new std::string(
        user_manager::UserManager::Get()->GetOwnerAccountId().GetUserEmail()));
    return pref_object;
  }
#endif

  const Extension* extension = GetExtensionControllingPref(*pref_object);

  if (extension) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    pref_object->extension_id.reset(new std::string(extension->id()));
    pref_object->controlled_by_name.reset(new std::string(extension->name()));
    bool can_be_disabled =
        !ExtensionSystem::Get(profile_)->management_policy()->MustRemainEnabled(
            extension, nullptr);
    pref_object->extension_can_be_disabled.reset(new bool(can_be_disabled));
    return pref_object;
  }

  // TODO(dbeam): surface !IsUserModifiable or IsPrefSupervisorControlled?

  return pref_object;
}

settings_private::SetPrefResult PrefsUtil::SetPref(const std::string& pref_name,
                                                   const base::Value* value) {
  if (GetWhitelistedPrefType(pref_name) ==
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
    case base::Value::Type::DICTIONARY:
      pref_service->Set(pref_name, *value);
      break;
    case base::Value::Type::DOUBLE:
    case base::Value::Type::INTEGER:
      // Explicitly set the double value or the integer value.
      // Otherwise if the number is a whole number like 2.0, it will
      // automatically be of type INTEGER causing type mismatches in
      // PrefService::SetUserPrefValue for doubles, and vice versa.
      double double_value;
      if (!value->GetAsDouble(&double_value))
        return settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
      if (pref->GetType() == base::Value::Type::DOUBLE)
        pref_service->SetDouble(pref_name, double_value);
      else
        pref_service->SetInteger(pref_name, static_cast<int>(double_value));
      break;
    case base::Value::Type::STRING: {
      std::string string_value;
      if (!value->GetAsString(&string_value))
        return settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

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
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Check if setting requires owner.
  if (service && service->HandlesSetting(pref_name)) {
    if (service->Set(pref_name, *value))
      return settings_private::SetPrefResult::SUCCESS;
    return settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  CrosSettings::Get()->Set(pref_name, *value);
  return settings_private::SetPrefResult::SUCCESS;
#else
  return settings_private::SetPrefResult::PREF_NOT_FOUND;
#endif
}

bool PrefsUtil::AppendToListCrosSetting(const std::string& pref_name,
                                        const base::Value& value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name)) {
    return service->AppendToList(pref_name, value);
  }

  CrosSettings::Get()->AppendToList(pref_name, &value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::RemoveFromListCrosSetting(const std::string& pref_name,
                                          const base::Value& value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name)) {
    return service->RemoveFromList(pref_name, value);
  }

  CrosSettings::Get()->RemoveFromList(pref_name, &value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::IsPrefTypeURL(const std::string& pref_name) {
  return GetWhitelistedPrefType(pref_name) ==
         settings_api::PrefType::PREF_TYPE_URL;
}

#if defined(OS_CHROMEOS)
bool PrefsUtil::IsPrefEnterpriseManaged(const std::string& pref_name) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged())
    return false;
  if (IsPrivilegedCrosSetting(pref_name))
    return true;
  if (pref_name == chromeos::kSystemTimezone ||
      pref_name == prefs::kUserTimezone) {
    return chromeos::system::IsTimezonePrefsManaged(pref_name);
  }
  return false;
}

bool PrefsUtil::IsPrefOwnerControlled(const std::string& pref_name) {
  // chromeos::kSystemTimezone is global display-only preference and
  // it should appear as disabled, but not owned.
  if (pref_name == chromeos::kSystemTimezone)
    return false;

  if (IsPrivilegedCrosSetting(pref_name)) {
    if (!chromeos::ProfileHelper::IsOwnerProfile(profile_))
      return true;
  }
  return false;
}

bool PrefsUtil::IsPrefPrimaryUserControlled(const std::string& pref_name) {
  // chromeos::kSystemTimezone is read-only, but for the non-primary users
  // it should have "primary user controlled" attribute.
  if (pref_name == prefs::kWakeOnWifiDarkConnect ||
      pref_name == prefs::kUserTimezone ||
      pref_name == chromeos::kSystemTimezone) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (user && user->GetAccountId() !=
                    user_manager->GetPrimaryUser()->GetAccountId()) {
      return true;
    }
  }
  return false;
}
#endif

bool PrefsUtil::IsPrefSupervisorControlled(const std::string& pref_name) {
  if (pref_name != prefs::kBrowserGuestModeEnabled &&
      pref_name != prefs::kBrowserAddPersonEnabled) {
    return false;
  }
  return profile_->IsSupervised();
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
#if defined(OS_CHROMEOS)
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
#if defined(OS_CHROMEOS)
  return CrosSettings::Get()->IsCrosSettings(pref_name);
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
    int restore_on_startup;
    CHECK(pref_object.value->GetAsInteger(&restore_on_startup));

    if (restore_on_startup == SessionStartupPref::kPrefValueURLs)
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
