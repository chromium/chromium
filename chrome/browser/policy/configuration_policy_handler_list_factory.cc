// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include <limits.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/browser/policy/browsing_history_policy_handler.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"
#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/browser/policy/managed_bookmarks_policy_handler.h"
#include "chrome/browser/policy/network_prediction_policy_handler.h"
#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"
#include "chrome/browser/profiles/force_safe_search_policy_handler.h"
#include "chrome/browser/profiles/force_youtube_safety_mode_policy_handler.h"
#include "chrome/browser/profiles/guest_mode_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/browser/spellchecker/spellcheck_language_policy_handler.h"
#include "chrome/browser/ssl/secure_origin_policy_handler.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_address_policy_handler.h"
#include "components/autofill/core/browser/autofill_credit_card_policy_handler.h"
#include "components/autofill/core/browser/autofill_policy_handler.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/proxy_policy_handler.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_policy_handler.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/contextual_suggestions/contextual_suggestions_prefs.h"
#include "chrome/browser/search/contextual_search_policy_handler_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/accessibility/magnifier_type.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions_policy_handler.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/secondary_google_account_signin_policy_handler.h"
#include "chrome/browser/policy/default_geolocation_policy_handler.h"
#include "chrome/browser/policy/printing_restrictions_policy_handler.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/arc/arc_prefs.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/download/default_download_dir_policy_handler.h"
#include "chrome/browser/download/download_dir_policy_handler.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/policy/local_sync_policy_handler.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/pref_names.h"
#endif

#if !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/enterprise_reporting_private/prefs.h"
#endif

namespace policy {

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
// clang-format off
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { key::kHomepageLocation,
    prefs::kHomePage,
    base::Value::Type::STRING },
  { key::kHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage,
    base::Value::Type::BOOLEAN },
  { key::kNewTabPageLocation,
    prefs::kNewTabPageLocationOverride,
    base::Value::Type::STRING },
  { key::kRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup,
    base::Value::Type::LIST },
  { key::kAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBuiltInDnsClientEnabled,
    prefs::kBuiltInDnsClientEnabled,
    base::Value::Type::BOOLEAN },
  { key::kWPADQuickCheckEnabled,
    prefs::kQuickCheckEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPacHttpsUrlStrippingEnabled,
    prefs::kPacHttpsUrlStrippingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kQuicAllowed,
    prefs::kQuicAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingForTrustedSourcesEnabled,
    prefs::kSafeBrowsingForTrustedSourcesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUrlKeyedAnonymizedDataCollectionEnabled,
    unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDownloadRestrictions,
    prefs::kDownloadRestrictions,
    base::Value::Type::INTEGER },
  { key::kForceGoogleSafeSearch,
    prefs::kForceGoogleSafeSearch,
    base::Value::Type::BOOLEAN },
  { key::kForceYouTubeRestrict,
    prefs::kForceYouTubeRestrict,
    base::Value::Type::INTEGER},
  { key::kPasswordManagerEnabled,
    password_manager::prefs::kCredentialsEnableService,
    base::Value::Type::BOOLEAN },
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintHeaderFooter,
    prefs::kPrintHeaderFooter,
    base::Value::Type::BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    base::Value::Type::BOOLEAN },
  { key::kApplicationLocaleValue,
    language::prefs::kApplicationLocale,
    base::Value::Type::STRING },
  { key::kAlwaysOpenPdfExternally,
    prefs::kPluginsAlwaysOpenPdfExternally,
    base::Value::Type::BOOLEAN },
  { key::kShowHomeButton,
    prefs::kShowHomeButton,
    base::Value::Type::BOOLEAN },
  { key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowDeletingBrowserHistory,
    prefs::kAllowDeletingBrowserHistory,
    base::Value::Type::BOOLEAN },
  { key::kBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies,
    base::Value::Type::BOOLEAN },
  { key::kAdsSettingForIntrusiveAdsSites,
    prefs::kManagedDefaultAdsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::Type::INTEGER },
  { key::kCookiesAllowedForUrls,
    prefs::kManagedCookiesAllowedForUrls,
    base::Value::Type::LIST },
  { key::kCookiesBlockedForUrls,
    prefs::kManagedCookiesBlockedForUrls,
    base::Value::Type::LIST },
  { key::kCookiesSessionOnlyForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls,
    base::Value::Type::LIST },
  { key::kImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    base::Value::Type::LIST },
  { key::kImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    base::Value::Type::LIST },
  { key::kJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    base::Value::Type::LIST },
  { key::kJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    base::Value::Type::LIST },
  { key::kPluginsAllowedForUrls,
    prefs::kManagedPluginsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kPluginsBlockedForUrls,
    prefs::kManagedPluginsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting,
    base::Value::Type::INTEGER },
  { key::kEnableOnlineRevocationChecks,
    prefs::kCertRevocationCheckingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kMachineLevelUserCloudPolicyEnrollmentToken,
    policy_prefs::kMachineLevelUserCloudPolicyEnrollmentToken,
    base::Value::Type::STRING },
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
    base::Value::Type::BOOLEAN },
  { key::kEnableSha1ForLocalAnchors,
    prefs::kCertEnableSha1LocalAnchors,
    base::Value::Type::BOOLEAN },
  { key::kEnableSymantecLegacyInfrastructure,
    prefs::kCertEnableSymantecLegacyInfrastructure,
    base::Value::Type::BOOLEAN },
  { key::kAuthSchemes,
    prefs::kAuthSchemes,
    base::Value::Type::STRING },
  { key::kDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup,
    base::Value::Type::BOOLEAN },
  { key::kEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort,
    base::Value::Type::BOOLEAN },
  { key::kAuthServerWhitelist,
    prefs::kAuthServerWhitelist,
    base::Value::Type::STRING },
  { key::kAuthNegotiateDelegateWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist,
    base::Value::Type::STRING },
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    base::Value::Type::STRING },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    base::Value::Type::BOOLEAN },
  { key::kPasswordProtectionWarningTrigger,
    prefs::kPasswordProtectionWarningTrigger,
    base::Value::Type::INTEGER},
  { key::kSafeBrowsingWhitelistDomains,
    prefs::kSafeBrowsingWhitelistDomains,
    base::Value::Type::LIST},
  { key::kPasswordProtectionLoginURLs,
    prefs::kPasswordProtectionLoginURLs,
    base::Value::Type::LIST},
  { key::kPasswordProtectionChangePasswordURL,
    prefs::kPasswordProtectionChangePasswordURL,
    base::Value::Type::STRING},
  { key::kSafeSitesFilterBehavior,
    policy_prefs::kSafeSitesFilterBehavior,
    base::Value::Type::INTEGER},
#if defined(OS_POSIX)
  { key::kNtlmV2Enabled,
    prefs::kNtlmV2Enabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_POSIX)
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    base::Value::Type::BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
    base::Value::Type::INTEGER },
  { key::kMediaCacheSize,
    prefs::kMediaCacheSize,
    base::Value::Type::INTEGER },
  { key::kPolicyRefreshRate,
    policy_prefs::kUserPolicyRefreshRate,
    base::Value::Type::INTEGER },
  { key::kDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate,
    base::Value::Type::INTEGER },
  { key::kDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCloudPrintSubmitEnabled,
    prefs::kCloudPrintSubmitEnabled,
    base::Value::Type::BOOLEAN },
  { key::kTranslateEnabled,
    prefs::kOfferTranslateEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowOutdatedPlugins,
    prefs::kPluginsAllowOutdated,
    base::Value::Type::BOOLEAN },
  { key::kRunAllFlashInAllowMode,
    prefs::kRunAllFlashInAllowMode,
    base::Value::Type::BOOLEAN },
  { key::kBookmarkBarEnabled,
    bookmarks::prefs::kShowBookmarkBar,
    base::Value::Type::BOOLEAN },
  { key::kEditBookmarksEnabled,
    bookmarks::prefs::kEditBookmarksEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
    base::Value::Type::BOOLEAN },
  { key::kAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs,
    base::Value::Type::BOOLEAN },
  { key::kPromptForDownloadLocation,
    prefs::kPromptForDownload,
    base::Value::Type::BOOLEAN },
  { key::kSpellcheckEnabled,
    spellcheck::prefs::kSpellCheckEnable,
    base::Value::Type::BOOLEAN },

  // First run import.
  { key::kImportBookmarks,
    prefs::kImportBookmarks,
    base::Value::Type::BOOLEAN },
  { key::kImportHistory,
    prefs::kImportHistory,
    base::Value::Type::BOOLEAN },
  { key::kImportHomepage,
    prefs::kImportHomepage,
    base::Value::Type::BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportSearchEngine,
    base::Value::Type::BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportSavedPasswords,
    base::Value::Type::BOOLEAN },
  { key::kImportAutofillFormData,
    prefs::kImportAutofillFormData,
    base::Value::Type::BOOLEAN },

  // Import data dialog: controlled by same policies as first run import, but
  // uses different prefs.
  { key::kImportBookmarks,
    prefs::kImportDialogBookmarks,
    base::Value::Type::BOOLEAN },
  { key::kImportHistory,
    prefs::kImportDialogHistory,
    base::Value::Type::BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportDialogSearchEngine,
    base::Value::Type::BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportDialogSavedPasswords,
    base::Value::Type::BOOLEAN },
  { key::kImportAutofillFormData,
    prefs::kImportDialogAutofillFormData,
    base::Value::Type::BOOLEAN },

  { key::kMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy,
    base::Value::Type::INTEGER },
  { key::kURLWhitelist,
    policy_prefs::kUrlWhitelist,
    base::Value::Type::LIST },
  { key::kRestrictSigninToPattern,
    prefs::kGoogleServicesUsernamePattern,
    base::Value::Type::STRING },
  { key::kDefaultWebBluetoothGuardSetting,
    prefs::kManagedDefaultWebBluetoothGuardSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    base::Value::Type::INTEGER },
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::Type::BOOLEAN },
  { key::kSSLErrorOverrideAllowed,
    prefs::kSSLErrorOverrideAllowed,
    base::Value::Type::BOOLEAN },
  { key::kHardwareAccelerationModeEnabled,
    prefs::kHardwareAccelerationModeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowDinosaurEasterEgg,
    prefs::kAllowDinosaurEasterEgg,
    base::Value::Type::BOOLEAN },
  { key::kAllowedDomainsForApps,
    prefs::kAllowedDomainsForApps,
    base::Value::Type::STRING },
  { key::kComponentUpdatesEnabled,
    prefs::kComponentUpdatesEnabled,
    base::Value::Type::BOOLEAN },

#if BUILDFLAG(ENABLE_SPELLCHECK)
  { key::kSpellCheckServiceEnabled,
    spellcheck::prefs::kSpellCheckUseSpellingService,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

  { key::kDisableScreenshots,
    prefs::kDisableScreenshots,
    base::Value::Type::BOOLEAN },
  { key::kAudioCaptureAllowed,
    prefs::kAudioCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kVideoCaptureAllowed,
    prefs::kVideoCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAudioCaptureAllowedUrls,
    prefs::kAudioCaptureAllowedUrls,
    base::Value::Type::LIST },
  { key::kVideoCaptureAllowedUrls,
    prefs::kVideoCaptureAllowedUrls,
    base::Value::Type::LIST },
  { key::kHideWebStoreIcon,
    prefs::kHideWebStoreIcon,
    base::Value::Type::BOOLEAN },
  { key::kVariationsRestrictParameter,
    variations::prefs::kVariationsRestrictParameter,
    base::Value::Type::STRING },
  { key::kForceEphemeralProfiles,
    prefs::kForceEphemeralProfiles,
    base::Value::Type::BOOLEAN },
  { key::kSSLVersionMin,
    prefs::kSSLVersionMin,
    base::Value::Type::STRING },
  { key::kSSLVersionMax,
    prefs::kSSLVersionMax,
    base::Value::Type::STRING },
  { key::kNTPContentSuggestionsEnabled,
    ntp_snippets::prefs::kEnableSnippets,
    base::Value::Type::BOOLEAN },
  { key::kEnableMediaRouter,
    prefs::kEnableMediaRouter,
    base::Value::Type::BOOLEAN },
#if !defined(OS_ANDROID)
  { key::kShowCastIconInToolbar,
    prefs::kShowCastIconInToolbar,
    base::Value::Type::BOOLEAN },
  { key::kMediaRouterCastAllowAllIPs,
    media_router::prefs::kMediaRouterCastAllowAllIPs,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_ANDROID)
  { key::kWebRtcUdpPortRange,
    prefs::kWebRTCUDPPortRange,
    base::Value::Type::STRING },
#if !defined(OS_ANDROID)
  { key::kWebRtcEventLogCollectionAllowed,
    prefs::kWebRtcEventLogCollectionAllowed,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kSecurityKeyPermitAttestation,
    prefs::kSecurityKeyPermitAttestation,
    base::Value::Type::LIST },
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if !defined(OS_MACOSX)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#if BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kFullscreenAllowed,
    extensions::pref_names::kAppFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // !defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
  { key::kChromeOsLockOnIdleSuspend,
    ash::prefs::kEnableAutoScreenLock,
    base::Value::Type::BOOLEAN },
  { key::kChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel,
    base::Value::Type::STRING },
  { key::kDriveDisabled,
    drive::prefs::kDisableDrive,
    base::Value::Type::BOOLEAN },
  { key::kDriveDisabledOverCellular,
    drive::prefs::kDisableDriveOverCellular,
    base::Value::Type::BOOLEAN },
  { key::kExternalStorageDisabled,
    prefs::kExternalStorageDisabled,
    base::Value::Type::BOOLEAN },
  { key::kExternalStorageReadOnly,
    prefs::kExternalStorageReadOnly,
    base::Value::Type::BOOLEAN },
  { key::kAudioOutputAllowed,
    chromeos::prefs::kAudioOutputAllowed,
    base::Value::Type::BOOLEAN },
  { key::kShowLogoutButtonInTray,
    ash::prefs::kShowLogoutButtonInTray,
    base::Value::Type::BOOLEAN },
  { key::kShelfAutoHideBehavior,
    ash::prefs::kShelfAutoHideBehaviorLocal,
    base::Value::Type::STRING },
  { key::kSessionLengthLimit,
    prefs::kSessionLengthLimit,
    base::Value::Type::INTEGER },
  { key::kWaitForInitialUserActivity,
    prefs::kSessionWaitForInitialUserActivity,
    base::Value::Type::BOOLEAN },
  { key::kPowerManagementUsesAudioActivity,
    ash::prefs::kPowerUseAudioActivity,
    base::Value::Type::BOOLEAN },
  { key::kPowerManagementUsesVideoActivity,
    ash::prefs::kPowerUseVideoActivity,
    base::Value::Type::BOOLEAN },
  { key::kAllowWakeLocks,
    ash::prefs::kPowerAllowWakeLocks,
    base::Value::Type::BOOLEAN },
  { key::kAllowScreenWakeLocks,
    ash::prefs::kPowerAllowScreenWakeLocks,
    base::Value::Type::BOOLEAN },
  { key::kWaitForInitialUserActivity,
    ash::prefs::kPowerWaitForInitialUserActivity,
    base::Value::Type::BOOLEAN },
  { key::kTermsOfServiceURL,
    prefs::kTermsOfServiceURL,
    base::Value::Type::STRING },
  { key::kShowAccessibilityOptionsInSystemTrayMenu,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu,
    base::Value::Type::BOOLEAN },
  { key::kLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSpokenFeedbackEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    base::Value::Type::BOOLEAN },
  { key::kHighContrastEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultLargeCursorEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultHighContrastEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kRebootAfterUpdate,
    prefs::kRebootAfterUpdate,
    base::Value::Type::BOOLEAN },
  { key::kAttestationEnabledForUser,
    prefs::kAttestationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kChromeOsMultiProfileUserBehavior,
    prefs::kMultiProfileUserBehavior,
    base::Value::Type::STRING },
  { key::kKeyboardDefaultToFunctionKeys,
    prefs::kLanguageSendFunctionKeys,
    base::Value::Type::BOOLEAN },
  { key::kTouchVirtualKeyboardEnabled,
    prefs::kTouchVirtualKeyboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kEasyUnlockAllowed,
    chromeos::multidevice_setup::kSmartLockAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kSmartLockSigninAllowed,
    chromeos::multidevice_setup::kSmartLockSigninAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kInstantTetheringAllowed,
    chromeos::multidevice_setup::kInstantTetheringAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kSmsMessagesAllowed,
    chromeos::multidevice_setup::kMessagesAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kCaptivePortalAuthenticationIgnoresProxy,
    prefs::kCaptivePortalAuthenticationIgnoresProxy,
    base::Value::Type::BOOLEAN },
  { key::kForceMaximizeOnFirstRun,
    prefs::kForceMaximizeOnFirstRun,
    base::Value::Type::BOOLEAN },
  { key::kUnifiedDesktopEnabledByDefault,
    prefs::kUnifiedDesktopEnabledByDefault,
    base::Value::Type::BOOLEAN },
  { key::kArcEnabled,
    arc::prefs::kArcEnabled,
    base::Value::Type::BOOLEAN },
  { key::kReportArcStatusEnabled,
    prefs::kReportArcStatusEnabled,
    base::Value::Type::BOOLEAN },
  { key::kEcryptfsMigrationStrategy,
    arc::prefs::kEcryptfsMigrationStrategy,
    base::Value::Type::INTEGER },
  { key::kNativePrintersBulkAccessMode,
    prefs::kRecommendedNativePrintersAccessMode,
    base::Value::Type::INTEGER },
  { key::kNativePrintersBulkBlacklist,
    prefs::kRecommendedNativePrintersBlacklist,
    base::Value::Type::LIST },
  { key::kNativePrintersBulkWhitelist,
    prefs::kRecommendedNativePrintersWhitelist,
    base::Value::Type::LIST },
  { key::kUserNativePrintersAllowed,
    prefs::kUserNativePrintersAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAllowedLanguages,
    prefs::kAllowedLanguages,
    base::Value::Type::LIST },
  { key::kAllowedInputMethods,
    prefs::kLanguageAllowedInputMethods,
    base::Value::Type::LIST },
  { key::kArcAppInstallEventLoggingEnabled,
    prefs::kArcAppInstallEventLoggingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kEnableSyncConsent,
    prefs::kEnableSyncConsent,
    base::Value::Type::BOOLEAN },
  { key::kNetworkFileSharesAllowed,
    prefs::kNetworkFileSharesAllowed,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLocalAccountManagedSessionEnabled,
    prefs::kManagedSessionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPowerSmartDimEnabled,
    ash::prefs::kPowerSmartDimEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNetBiosShareDiscoveryEnabled,
    prefs::kNetBiosShareDiscoveryEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCrostiniAllowed,
    crostini::prefs::kUserCrostiniAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kReportCrostiniUsageEnabled,
    crostini::prefs::kReportCrostiniUsageEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNTLMShareAuthenticationEnabled,
    prefs::kNTLMShareAuthenticationEnabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_CHROMEOS)

// Metrics reporting is controlled by a platform specific policy for ChromeOS
#if defined(OS_CHROMEOS)
  { key::kDeviceMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
#else
  { key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
#endif

#if !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kReportVersionData,
    extensions::enterprise_reporting::kReportVersionData,
    base::Value::Type::BOOLEAN },
  { key::kReportPolicyData,
    extensions::enterprise_reporting::kReportPolicyData,
    base::Value::Type::BOOLEAN },
  { key::kReportMachineIDData,
    extensions::enterprise_reporting::kReportMachineIDData,
    base::Value::Type::BOOLEAN },
  { key::kReportUserIDData,
    extensions::enterprise_reporting::kReportUserIDData,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  { key::kDataCompressionProxyEnabled,
    prefs::kDataSaverEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAuthAndroidNegotiateAccountType,
    prefs::kAuthAndroidNegotiateAccountType,
    base::Value::Type::STRING },
  { key::kContextualSuggestionsEnabled,
    contextual_suggestions::prefs::kContextualSuggestionsEnabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::Type::BOOLEAN },
  { key::kBrowserAddPersonEnabled,
    prefs::kBrowserAddPersonEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintPreviewUseSystemDefaultPrinter,
    prefs::kPrintPreviewUseSystemDefaultPrinter,
    base::Value::Type::BOOLEAN },
  { key::kCloudPolicyOverridesMachinePolicy,
    prefs::kCloudPolicyOverridesMachinePolicy,
    base::Value::Type::BOOLEAN },
  { key::kCloudReportingEnabled,
    prefs::kCloudReportingEnabled,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if defined(OS_WIN)
  { key::kWelcomePageOnOSUpgradeEnabled,
    prefs::kWelcomePageOnOSUpgradeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kChromeCleanupEnabled,
    prefs::kSwReporterEnabled,
    base::Value::Type::BOOLEAN },
  { key::kChromeCleanupReportingEnabled,
    prefs::kSwReporterReportingEnabled,
    base::Value::Type::BOOLEAN },
#endif  // OS_WIN

#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
  { key::kPromotionalTabsEnabled,
    prefs::kPromotionalTabsEnabled,
    base::Value::Type::BOOLEAN },
#endif  // !OS_CHROMEOS
  { key::kSuppressUnsupportedOSWarning,
    prefs::kSuppressUnsupportedOSWarning,
    base::Value::Type::BOOLEAN },
#endif  // !OS_ANDROID

#if defined(OS_CHROMEOS)
  { key::kSystemTimezoneAutomaticDetection,
    prefs::kSystemTimezoneAutomaticDetectionPolicy,
    base::Value::Type::INTEGER },
#endif

  { key::kTaskManagerEndProcessEnabled,
    prefs::kTaskManagerEndProcessEnabled,
    base::Value::Type::BOOLEAN },

#if defined(OS_CHROMEOS)
  { key::kNetworkThrottlingEnabled,
    prefs::kNetworkThrottlingEnabled,
    base::Value::Type::DICTIONARY },

  { key::kAllowScreenLock,
    ash::prefs::kAllowScreenLock,
    base::Value::Type::BOOLEAN },

  { key::kQuickUnlockModeWhitelist,
    prefs::kQuickUnlockModeWhitelist,
    base::Value::Type::LIST },
  { key::kQuickUnlockTimeout,
    prefs::kQuickUnlockTimeout,
    base::Value::Type::INTEGER },
  { key::kPinUnlockMinimumLength,
    prefs::kPinUnlockMinimumLength,
    base::Value::Type::INTEGER },
  { key::kPinUnlockMaximumLength,
    prefs::kPinUnlockMaximumLength,
    base::Value::Type::INTEGER },
  { key::kPinUnlockWeakPinsAllowed,
    prefs::kPinUnlockWeakPinsAllowed,
    base::Value::Type::BOOLEAN },

  { key::kCastReceiverEnabled,
    prefs::kCastReceiverEnabled,
    base::Value::Type::BOOLEAN },

  { key::kVpnConfigAllowed,
    prefs::kVpnConfigAllowed,
    base::Value::Type::BOOLEAN },
#endif

  { key::kRoamingProfileSupportEnabled,
    syncer::prefs::kEnableLocalSyncBackend,
    base::Value::Type::BOOLEAN },

  { key::kBrowserNetworkTimeQueriesEnabled,
    network_time::prefs::kNetworkTimeQueriesEnabled,
    base::Value::Type::BOOLEAN },

  { key::kIsolateOrigins,
    prefs::kIsolateOrigins,
    base::Value::Type::STRING },
  { key::kSitePerProcess,
    prefs::kSitePerProcess,
    base::Value::Type::BOOLEAN },
  { key::kIsolateOriginsAndroid,
    prefs::kIsolateOrigins,
    base::Value::Type::STRING },
  { key::kSitePerProcessAndroid,
    prefs::kSitePerProcess,
    base::Value::Type::BOOLEAN },

  { key::kWebDriverOverridesIncompatiblePolicies,
    prefs::kWebDriverOverridesIncompatiblePolicies,
    base::Value::Type::BOOLEAN },

  { key::kAbusiveExperienceInterventionEnforce,
    prefs::kAbusiveExperienceInterventionEnforce,
    base::Value::Type::BOOLEAN },

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  { key::kThirdPartyBlockingEnabled,
    prefs::kThirdPartyBlockingEnabled,
    base::Value::Type::BOOLEAN },
#endif

#if !defined(OS_ANDROID)
  { key::kRelaunchNotification,
    prefs::kRelaunchNotification,
    base::Value::Type::INTEGER },
  { key::kRelaunchNotificationPeriod,
    prefs::kRelaunchNotificationPeriod,
    base::Value::Type::INTEGER },
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  { key::kAutoplayAllowed,
    prefs::kAutoplayAllowed,
    base::Value::Type::BOOLEAN },

  { key::kAutoplayWhitelist,
    prefs::kAutoplayWhitelist,
    base::Value::Type::LIST },
#endif  // !defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kReportVersionData,
    extensions::enterprise_reporting::kReportVersionData,
    base::Value::Type::BOOLEAN },
  { key::kReportPolicyData,
    extensions::enterprise_reporting::kReportPolicyData,
    base::Value::Type::BOOLEAN },
  { key::kReportMachineIDData,
    extensions::enterprise_reporting::kReportMachineIDData,
    base::Value::Type::BOOLEAN },
  { key::kReportUserIDData,
    extensions::enterprise_reporting::kReportUserIDData,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)

  { key::kDefaultWebUsbGuardSetting,
    prefs::kManagedDefaultWebUsbGuardSetting,
    base::Value::Type::INTEGER },
  { key::kWebUsbAskForUrls,
    prefs::kManagedWebUsbAskForUrls,
    base::Value::Type::LIST },
  { key::kWebUsbBlockedForUrls,
    prefs::kManagedWebUsbBlockedForUrls,
    base::Value::Type::LIST },

  { key::kTabLifecyclesEnabled,
    prefs::kTabLifecyclesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCoalesceH2ConnectionsWithClientCertificatesForHosts,
    prefs::kH2ClientCertCoalescingHosts,
    base::Value::Type::LIST },
  { key::kEnterpriseHardwarePlatformAPIEnabled,
    prefs::kEnterpriseHardwarePlatformAPIEnabled,
    base::Value::Type::BOOLEAN },
};
// clang-format on

#if BUILDFLAG(ENABLE_EXTENSIONS)
void GetExtensionAllowedTypesMap(
    std::vector<std::unique_ptr<StringMappingListPolicyHandler::MappingEntry>>*
        result) {
  // Mapping from extension type names to Manifest::Type.
  for (size_t index = 0;
       index < extensions::schema_constants::kAllowedTypesMapSize; ++index) {
    const extensions::schema_constants::AllowedTypesMapEntry& entry =
        extensions::schema_constants::kAllowedTypesMap[index];
    result->push_back(
        std::make_unique<StringMappingListPolicyHandler::MappingEntry>(
            entry.name, std::unique_ptr<base::Value>(
                            new base::Value(entry.manifest_type))));
  }
}
#endif

void GetDeprecatedFeaturesMap(
    std::vector<std::unique_ptr<StringMappingListPolicyHandler::MappingEntry>>*
        result) {
  // Maps feature tags as specified in policy to the corresponding switch to
  // re-enable them.
}

}  // namespace

void PopulatePolicyHandlerParameters(PolicyHandlerParameters* parameters) {
#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized()) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (user)
      parameters->user_id_hash = user->username_hash();
  }
#endif
}

std::unique_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  std::unique_ptr<ConfigurationPolicyHandlerList> handlers(
      new ConfigurationPolicyHandlerList(
          base::Bind(&PopulatePolicyHandlerParameters),
          base::Bind(&GetChromePolicyDetails)));
  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
        kSimplePolicyMap[i].policy_name, kSimplePolicyMap[i].preference_path,
        kSimplePolicyMap[i].value_type));
  }

  handlers->AddHandler(
      std::make_unique<autofill::AutofillAddressPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<autofill::AutofillCreditCardPolicyHandler>());
  handlers->AddHandler(std::make_unique<autofill::AutofillPolicyHandler>());
  handlers->AddHandler(std::make_unique<DefaultSearchPolicyHandler>());
  handlers->AddHandler(std::make_unique<ForceSafeSearchPolicyHandler>());
  handlers->AddHandler(std::make_unique<ForceYouTubeSafetyModePolicyHandler>());
  handlers->AddHandler(std::make_unique<IncognitoModePolicyHandler>());
  handlers->AddHandler(std::make_unique<GuestModePolicyHandler>());
  handlers->AddHandler(
      std::make_unique<ManagedBookmarksPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<ProxyPolicyHandler>());
  handlers->AddHandler(std::make_unique<URLBlacklistPolicyHandler>());

  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForUrls,
      certificate_transparency::prefs::kCTExcludedHosts, chrome_schema,
      SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForCas,
      certificate_transparency::prefs::kCTExcludedSPKIs, chrome_schema,
      SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForLegacyCas,
      certificate_transparency::prefs::kCTExcludedLegacySPKIs, chrome_schema,
      SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      std::make_unique<WebUsbAllowDevicesForUrlsPolicyHandler>(chrome_schema));

// On most platforms, there is a legacy policy
// kUnsafelyTreatInsecureOriginAsSecure which has been replaced by
// kOverrideSecurityRestrictionsOnInsecureOrigins. The legacy policy was never
// supported on ChromeOS or Android, so on those platforms, simply use the new
// one.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  handlers->AddHandler(std::make_unique<SecureOriginPolicyHandler>(
      key::kOverrideSecurityRestrictionsOnInsecureOrigin, chrome_schema));
#else
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      secure_origin_legacy_policy;
  secure_origin_legacy_policy.push_back(
      std::make_unique<SecureOriginPolicyHandler>(
          key::kUnsafelyTreatInsecureOriginAsSecure, chrome_schema));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(secure_origin_legacy_policy),
      base::WrapUnique(new SecureOriginPolicyHandler(
          key::kOverrideSecurityRestrictionsOnInsecureOrigin, chrome_schema))));
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID)

  handlers->AddHandler(std::make_unique<DeveloperToolsPolicyHandler>());

#if defined(OS_ANDROID)
  handlers->AddHandler(
      std::make_unique<ContextualSearchPolicyHandlerAndroid>());
#endif

  handlers->AddHandler(std::make_unique<FileSelectionDialogsPolicyHandler>());
  handlers->AddHandler(std::make_unique<JavascriptPolicyHandler>());
  handlers->AddHandler(std::make_unique<NetworkPredictionPolicyHandler>());
  handlers->AddHandler(std::make_unique<RestoreOnStartupPolicyHandler>());
  handlers->AddHandler(std::make_unique<syncer::SyncPolicyHandler>());

  handlers->AddHandler(std::make_unique<StringMappingListPolicyHandler>(
      key::kEnableDeprecatedWebPlatformFeatures,
      prefs::kEnableDeprecatedWebPlatformFeatures,
      base::Bind(GetDeprecatedFeaturesMap)));

  handlers->AddHandler(std::make_unique<BrowsingHistoryPolicyHandler>());

#if BUILDFLAG(ENABLE_SPELLCHECK)
  handlers->AddHandler(std::make_unique<SpellcheckLanguagePolicyHandler>());
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kExtensionInstallWhitelist,
      extensions::pref_names::kInstallAllowList, false));
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kExtensionInstallBlacklist, extensions::pref_names::kInstallDenyList,
      true));
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionInstallForcelistPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          extensions::ExtensionInstallLoginScreenAppListPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionURLPatternListPolicyHandler>(
          key::kExtensionInstallSources,
          extensions::pref_names::kAllowedInstallSites));
  handlers->AddHandler(std::make_unique<StringMappingListPolicyHandler>(
      key::kExtensionAllowedTypes, extensions::pref_names::kAllowedTypes,
      base::Bind(GetExtensionAllowedTypesMap)));
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionSettingsPolicyHandler>(
          chrome_schema));
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  handlers->AddHandler(std::make_unique<DiskCacheDirPolicyHandler>());

  handlers->AddHandler(
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingWhitelist,
          extensions::pref_names::kNativeMessagingWhitelist, false));
  handlers->AddHandler(
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingBlacklist,
          extensions::pref_names::kNativeMessagingBlacklist, true));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  handlers->AddHandler(std::make_unique<DefaultDownloadDirPolicyHandler>());
  handlers->AddHandler(std::make_unique<DownloadDirPolicyHandler>());
  handlers->AddHandler(std::make_unique<LocalSyncPolicyHandler>());

  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kRegisteredProtocolHandlers,
      prefs::kPolicyRegisteredProtocolHandlers, chrome_schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED));

  // Here we are deprecating policy SafeBrowsingExtendedReportingOptInAllowed
  // in favour of new policy for SafeBrowsingExtendedReportingEnabled.
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>> sber_legacy_policy;
  sber_legacy_policy.push_back(std::make_unique<SimplePolicyHandler>(
      key::kSafeBrowsingExtendedReportingOptInAllowed,
      prefs::kSafeBrowsingExtendedReportingOptInAllowed,
      base::Value::Type::BOOLEAN));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(sber_legacy_policy),
      base::WrapUnique(new SimpleSchemaValidatingPolicyHandler(
          key::kSafeBrowsingExtendedReportingEnabled,
          prefs::kSafeBrowsingScoutReportingEnabled, chrome_schema,
          SCHEMA_STRICT,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED))));

  // Handlers for policies with embedded JSON strings. These handlers are very
  // lenient - as long as the root value is of the right type, they only display
  // warnings and never reject the policy value.
  handlers->AddHandler(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kDefaultPrinterSelection,
          prefs::kPrintPreviewDefaultDestinationSelectionRules,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kAutoSelectCertificateForUrls,
          prefs::kManagedAutoSelectCertificateForUrls,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
#endif

#if !defined(OS_CHROMEOS)
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      signin_legacy_policies;
  signin_legacy_policies.push_back(std::make_unique<SimplePolicyHandler>(
      key::kForceBrowserSignin, prefs::kForceBrowserSignin,
      base::Value::Type::BOOLEAN));
  signin_legacy_policies.push_back(std::make_unique<SimplePolicyHandler>(
      key::kSigninAllowed,
#if defined(OS_ANDROID)
      // The new kSigninAllowedOnNextStartup pref is only used on Desktop.
      // Keep the old kSigninAllowed pref for Android until the policy is
      // fully deprecated in M71 and can be removed.
      prefs::kSigninAllowed,
#else
      prefs::kSigninAllowedOnNextStartup,
#endif
      base::Value::Type::BOOLEAN));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(signin_legacy_policies),
      std::make_unique<BrowserSigninPolicyHandler>(chrome_schema)));
#endif

#if defined(OS_CHROMEOS)
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kAttestationExtensionWhitelist,
      prefs::kAttestationExtensionWhitelist, false));
  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForDevicePolicy()));
  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy()));
  handlers->AddHandler(std::make_unique<PinnedLauncherAppsPolicyHandler>());
  handlers->AddHandler(std::make_unique<ScreenMagnifierPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<LoginScreenPowerManagementPolicyHandler>(chrome_schema));

  // Handler for another policy with JSON strings, lenient but shows warnings.
  handlers->AddHandler(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kNativePrinters, prefs::kRecommendedNativePrinters,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      power_management_idle_legacy_policies;
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kScreenDimDelayAC, ash::prefs::kPowerAcScreenDimDelayMs, 0,
          INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kScreenOffDelayAC, ash::prefs::kPowerAcScreenOffDelayMs, 0,
          INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kIdleWarningDelayAC, ash::prefs::kPowerAcIdleWarningDelayMs, 0,
          INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(key::kIdleDelayAC,
                                              ash::prefs::kPowerAcIdleDelayMs,
                                              0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kScreenDimDelayBattery,
          ash::prefs::kPowerBatteryScreenDimDelayMs, 0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kScreenOffDelayBattery,
          ash::prefs::kPowerBatteryScreenOffDelayMs, 0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kIdleWarningDelayBattery,
          ash::prefs::kPowerBatteryIdleWarningDelayMs, 0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kIdleDelayBattery, ash::prefs::kPowerBatteryIdleDelayMs, 0,
          INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kIdleActionAC, ash::prefs::kPowerAcIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING, false));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<IntRangePolicyHandler>(
          key::kIdleActionBattery, ash::prefs::kPowerBatteryIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING, false));
  power_management_idle_legacy_policies.push_back(
      std::make_unique<DeprecatedIdleActionHandler>());

  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      screen_lock_legacy_policies;
  screen_lock_legacy_policies.push_back(std::make_unique<IntRangePolicyHandler>(
      key::kScreenLockDelayAC, ash::prefs::kPowerAcScreenLockDelayMs, 0,
      INT_MAX, true));
  screen_lock_legacy_policies.push_back(std::make_unique<IntRangePolicyHandler>(
      key::kScreenLockDelayBattery, ash::prefs::kPowerBatteryScreenLockDelayMs,
      0, INT_MAX, true));

  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kSAMLOfflineSigninTimeLimit, prefs::kSAMLOfflineSigninTimeLimit, -1,
      INT_MAX, true));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kLidCloseAction, ash::prefs::kPowerLidClosedAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND,
      chromeos::PowerPolicyController::ACTION_DO_NOTHING, false));
  handlers->AddHandler(std::make_unique<IntPercentageToDoublePolicyHandler>(
      key::kPresentationScreenDimDelayScale,
      ash::prefs::kPowerPresentationScreenDimDelayFactor, 100, INT_MAX, true));
  handlers->AddHandler(std::make_unique<IntPercentageToDoublePolicyHandler>(
      key::kUserActivityScreenDimDelayScale,
      ash::prefs::kPowerUserActivityScreenDimDelayFactor, 100, INT_MAX, true));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kUptimeLimit, prefs::kUptimeLimit, 3600, INT_MAX, true));
  handlers->AddHandler(base::WrapUnique(new IntRangePolicyHandler(
      key::kDeviceLoginScreenDefaultScreenMagnifierType, nullptr,
      chromeos::MAGNIFIER_DISABLED, chromeos::MAGNIFIER_FULL, false)));
  // TODO(binjin): Remove LegacyPoliciesDeprecatingPolicyHandler for these two
  // policies once deprecation of legacy power management policies is done.
  // http://crbug.com/346229
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(power_management_idle_legacy_policies),
      base::WrapUnique(
          new PowerManagementIdleSettingsPolicyHandler(chrome_schema))));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(screen_lock_legacy_policies),
      base::WrapUnique(new ScreenLockDelayPolicyHandler(chrome_schema))));
  handlers->AddHandler(
      std::make_unique<ScreenBrightnessPercentPolicyHandler>(chrome_schema));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kUserAvatarImage));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kDeviceWallpaperImage));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kWallpaperImage));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kNativePrintersBulkConfiguration));
  handlers->AddHandler(base::WrapUnique(new SimpleSchemaValidatingPolicyHandler(
      key::kSessionLocales, NULL, chrome_schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED)));
  handlers->AddHandler(
      std::make_unique<chromeos::KeyPermissionsPolicyHandler>(chrome_schema));
  handlers->AddHandler(base::WrapUnique(new DefaultGeolocationPolicyHandler()));
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kNoteTakingAppsLockScreenWhitelist,
      prefs::kNoteTakingAppsLockScreenWhitelist, false /*allow_wildcards*/));
  handlers->AddHandler(
      std::make_unique<SecondaryGoogleAccountSigninPolicyHandler>());
  if (base::FeatureList::IsEnabled(features::kUsageTimeLimitPolicy)) {
    handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
        key::kUsageTimeLimit, prefs::kUsageTimeLimit, chrome_schema,
        SCHEMA_STRICT,
        SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
        SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  }
  handlers->AddHandler(std::make_unique<ArcServicePolicyHandler>(
      key::kArcBackupRestoreServiceEnabled,
      arc::prefs::kArcBackupRestoreEnabled));
  handlers->AddHandler(std::make_unique<ArcServicePolicyHandler>(
      key::kArcGoogleLocationServicesEnabled,
      arc::prefs::kArcLocationServiceEnabled));
  handlers->AddHandler(
      std::make_unique<PrintingAllowedColorModesPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<PrintingAllowedDuplexModesPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<PrintingAllowedPageSizesPolicyHandler>());
  handlers->AddHandler(std::make_unique<PrintingColorDefaultPolicyHandler>());
  handlers->AddHandler(std::make_unique<PrintingDuplexDefaultPolicyHandler>());
  handlers->AddHandler(std::make_unique<PrintingSizeDefaultPolicyHandler>());

  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kNetworkFileSharesPreconfiguredShares,
      prefs::kNetworkFileSharesPreconfiguredShares, chrome_schema,
      SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
  handlers->AddHandler(std::make_unique<PluginPolicyHandler>());
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  return handlers;
}

}  // namespace policy
