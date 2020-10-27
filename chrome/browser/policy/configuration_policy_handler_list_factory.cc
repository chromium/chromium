// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include <limits.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/login_pref_names.h"
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/browser/net/referrer_policy_policy_handler.h"
#include "chrome/browser/net/secure_dns_policy_handler.h"
#include "chrome/browser/policy/boolean_disabling_policy_handler.h"
#include "chrome/browser/policy/browsing_history_policy_handler.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"
#include "chrome/browser/policy/homepage_location_policy_handler.h"
#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/browser/policy/network_prediction_policy_handler.h"
#include "chrome/browser/policy/printing_restrictions_policy_handler.h"
#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"
#include "chrome/browser/profiles/force_safe_search_policy_handler.h"
#include "chrome/browser/profiles/force_youtube_safety_mode_policy_handler.h"
#include "chrome/browser/profiles/guest_mode_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/browser/search/ntp_custom_background_enabled_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/browser/spellchecker/spellcheck_language_blocklist_policy_handler.h"
#include "chrome/browser/spellchecker/spellcheck_language_policy_handler.h"
#include "chrome/browser/ssl/secure_origin_policy_handler.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/autofill/core/browser/autofill_address_policy_handler.h"
#include "components/autofill/core/browser/autofill_credit_card_policy_handler.h"
#include "components/autofill/core/browser/autofill_policy_handler.h"
#include "components/blocked_content/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmarks_policy_handler.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"
#include "components/content_settings/core/browser/insecure_private_network_policy_handler.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/proxy_policy_handler.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_policy_handler.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "chrome/browser/search/contextual_search_policy_handler_android.h"
#else  // defined(OS_ANDROID)
#include "chrome/browser/download/default_download_dir_policy_handler.h"
#include "chrome/browser/download/download_auto_open_policy_handler.h"
#include "chrome/browser/download/download_dir_policy_handler.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/enterprise_connectors_policy_handler.h"
#include "chrome/browser/enterprise/reporting/extension_request_policy_handler.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/policy/local_sync_policy_handler.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/accessibility/magnifier_type.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_policy_handler.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/secondary_google_account_signin_policy_handler.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/policy/default_geolocation_policy_handler.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/arc/arc_prefs.h"
#include "components/drive/drive_pref_names.h"  // nogncheck crbug.com/1125897
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else  // defined(OS_CHROMEOS)
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/external_protocol/auto_launch_protocols_policy_handler.h"
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_policy_handler.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

namespace policy {

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
// clang-format off
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
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
    embedder_support::kAlternateErrorPagesEnabled,
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
  { key::kAutoOpenAllowedForURLs,
    prefs::kDownloadAllowedURLsForOpenByPolicy,
    base::Value::Type::LIST },
  { key::kForceGoogleSafeSearch,
    prefs::kForceGoogleSafeSearch,
    base::Value::Type::BOOLEAN },
  { key::kForceYouTubeRestrict,
    prefs::kForceYouTubeRestrict,
    base::Value::Type::INTEGER },
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
  { key::kAdsSettingForIntrusiveAdsSites,
    prefs::kManagedDefaultAdsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    base::Value::Type::INTEGER },
  { key::kLegacySameSiteCookieBehaviorEnabled,
    prefs::kManagedDefaultLegacyCookieAccessSetting,
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
  { key::kLegacySameSiteCookieBehaviorEnabledForDomainList,
    prefs::kManagedLegacyCookieAccessAllowedForDomains,
    base::Value::Type::LIST },
  { key::kInsecurePrivateNetworkRequestsAllowedForUrls,
    prefs::kManagedInsecurePrivateNetworkAllowedForUrls,
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
  { key::kCloudManagementEnrollmentMandatory,
    policy_prefs::kCloudManagementEnrollmentMandatory,
    base::Value::Type::BOOLEAN },
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
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
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    base::Value::Type::STRING },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    base::Value::Type::BOOLEAN },
  { key::kGloballyScopeHTTPAuthCacheEnabled,
    prefs::kGloballyScopeHTTPAuthCacheEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPasswordProtectionWarningTrigger,
    prefs::kPasswordProtectionWarningTrigger,
    base::Value::Type::INTEGER },
  { key::kPasswordProtectionLoginURLs,
    prefs::kPasswordProtectionLoginURLs,
    base::Value::Type::LIST },
  { key::kPasswordProtectionChangePasswordURL,
    prefs::kPasswordProtectionChangePasswordURL,
    base::Value::Type::STRING },
  { key::kSafeSitesFilterBehavior,
    policy_prefs::kSafeSitesFilterBehavior,
    base::Value::Type::INTEGER },
  { key::kSendFilesForMalwareCheck,
    prefs::kSafeBrowsingSendFilesForMalwareCheck,
    base::Value::Type::INTEGER },
  { key::kAmbientAuthenticationInPrivateModesEnabled,
    prefs::kAmbientAuthenticationInPrivateModesEnabled,
    base::Value::Type::INTEGER },
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    base::Value::Type::BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
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
  { key::kCloudPrintWarningsSuppressed,
    prefs::kCloudPrintDeprecationWarningsSuppressed,
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
  { key::kSharedClipboardEnabled,
    prefs::kSharedClipboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultSensorsSetting,
    prefs::kManagedDefaultSensorsSetting,
    base::Value::Type::INTEGER },
  { key::kSensorsAllowedForUrls,
    prefs::kManagedSensorsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kSensorsBlockedForUrls,
    prefs::kManagedSensorsBlockedForUrls,
    base::Value::Type::LIST },

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
  { key::kScreenCaptureAllowed,
    prefs::kScreenCaptureAllowed,
    base::Value::Type::BOOLEAN },
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
  { key::kNTPContentSuggestionsEnabled,
    feed::prefs::kEnableSnippets,
    base::Value::Type::BOOLEAN },
  { key::kEnableMediaRouter,
    prefs::kEnableMediaRouter,
    base::Value::Type::BOOLEAN },
  { key::kWebRtcUdpPortRange,
    prefs::kWebRTCUDPPortRange,
    base::Value::Type::STRING },
  { key::kTaskManagerEndProcessEnabled,
    prefs::kTaskManagerEndProcessEnabled,
    base::Value::Type::BOOLEAN },
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
  { key::kAbusiveExperienceInterventionEnforce,
    blocked_content::prefs::kAbusiveExperienceInterventionEnforce,
    base::Value::Type::BOOLEAN },
  { key::kDefaultWebUsbGuardSetting,
    prefs::kManagedDefaultWebUsbGuardSetting,
    base::Value::Type::INTEGER },
  { key::kWebUsbAskForUrls,
    prefs::kManagedWebUsbAskForUrls,
    base::Value::Type::LIST },
  { key::kWebUsbBlockedForUrls,
    prefs::kManagedWebUsbBlockedForUrls,
    base::Value::Type::LIST },
  { key::kDefaultSerialGuardSetting,
    prefs::kManagedDefaultSerialGuardSetting,
    base::Value::Type::INTEGER },
  { key::kSerialAskForUrls,
    prefs::kManagedSerialAskForUrls,
    base::Value::Type::LIST },
  { key::kSerialBlockedForUrls,
    prefs::kManagedSerialBlockedForUrls,
    base::Value::Type::LIST },
  { key::kDefaultFileSystemReadGuardSetting,
    prefs::kManagedDefaultFileSystemReadGuardSetting,
    base::Value::Type::INTEGER },
  { key::kFileSystemReadAskForUrls,
    prefs::kManagedFileSystemReadAskForUrls,
    base::Value::Type::LIST },
  { key::kFileSystemReadBlockedForUrls,
    prefs::kManagedFileSystemReadBlockedForUrls,
    base::Value::Type::LIST },
  { key::kDefaultFileSystemWriteGuardSetting,
    prefs::kManagedDefaultFileSystemWriteGuardSetting,
    base::Value::Type::INTEGER },
  { key::kFileSystemWriteAskForUrls,
    prefs::kManagedFileSystemWriteAskForUrls,
    base::Value::Type::LIST },
  { key::kFileSystemWriteBlockedForUrls,
    prefs::kManagedFileSystemWriteBlockedForUrls,
    base::Value::Type::LIST },
  { key::kTabFreezingEnabled,
    prefs::kTabFreezingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCoalesceH2ConnectionsWithClientCertificatesForHosts,
    prefs::kH2ClientCertCoalescingHosts,
    base::Value::Type::LIST },
  { key::kEnterpriseHardwarePlatformAPIEnabled,
    prefs::kEnterpriseHardwarePlatformAPIEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSignedHTTPExchangeEnabled,
    prefs::kSignedHTTPExchangeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowPopupsDuringPageUnload,
    prefs::kAllowPopupsDuringPageUnload,
    base::Value::Type::BOOLEAN },
  { key::kUserFeedbackAllowed,
    prefs::kUserFeedbackAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAllowSyncXHRInPageDismissal,
    prefs::kAllowSyncXHRInPageDismissal,
    base::Value::Type::BOOLEAN },
  { key::kExternalProtocolDialogShowAlwaysOpenCheckbox,
    prefs::kExternalProtocolDialogShowAlwaysOpenCheckbox,
    base::Value::Type::BOOLEAN },
  { key::kPasswordLeakDetectionEnabled,
    password_manager::prefs::kPasswordLeakDetectionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kTotalMemoryLimitMb,
    prefs::kTotalMemoryLimitMb,
    base::Value::Type::INTEGER },
  { key::kPrinterTypeDenyList,
    prefs::kPrinterTypeDenyList,
    base::Value::Type::LIST },
  { key::kPaymentMethodQueryEnabled,
    payments::kCanMakePaymentEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDNSInterceptionChecksEnabled,
    prefs::kDNSInterceptionChecksEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAdvancedProtectionAllowed,
    prefs::kAdvancedProtectionAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAccessibilityImageLabelsEnabled,
    prefs::kAccessibilityImageLabelsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultSearchProviderContextMenuAccessAllowed,
    prefs::kDefaultSearchProviderContextMenuAccessAllowed,
    base::Value::Type::BOOLEAN },

#if defined(OS_ANDROID)
  { key::kDataCompressionProxyEnabled,
    data_reduction_proxy::prefs::kDataSaverEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAuthAndroidNegotiateAccountType,
    prefs::kAuthAndroidNegotiateAccountType,
    base::Value::Type::STRING },
  { key::kBackForwardCacheEnabled,
    prefs::kMixedFormsWarningsEnabled,
    base::Value::Type::BOOLEAN },
#else  // defined(OS_ANDROID)
  { key::kDefaultInsecureContentSetting,
    prefs::kManagedDefaultInsecureContentSetting,
    base::Value::Type::INTEGER },
  { key::kInsecureContentAllowedForUrls,
    prefs::kManagedInsecureContentAllowedForUrls,
    base::Value::Type::LIST },
  { key::kInsecureContentBlockedForUrls,
    prefs::kManagedInsecureContentBlockedForUrls,
    base::Value::Type::LIST },
  { key::kShowCastIconInToolbar,
    prefs::kShowCastIconInToolbar,
    base::Value::Type::BOOLEAN },
  { key::kMediaRouterCastAllowAllIPs,
    media_router::prefs::kMediaRouterCastAllowAllIPs,
    base::Value::Type::BOOLEAN },
  { key::kWebRtcLocalIpsAllowedUrls,
    prefs::kWebRtcLocalIpsAllowedUrls,
    base::Value::Type::LIST },
  { key::kWebRtcEventLogCollectionAllowed,
    prefs::kWebRtcEventLogCollectionAllowed,
    base::Value::Type::BOOLEAN },
  { key::kCloudReportingEnabled,
    enterprise_reporting::kCloudReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSuppressUnsupportedOSWarning,
    prefs::kSuppressUnsupportedOSWarning,
    base::Value::Type::BOOLEAN },
  { key::kRelaunchNotification,
    prefs::kRelaunchNotification,
    base::Value::Type::INTEGER },
  { key::kRelaunchNotificationPeriod,
    prefs::kRelaunchNotificationPeriod,
    base::Value::Type::INTEGER },
  { key::kAutoplayAllowed,
    prefs::kAutoplayAllowed,
    base::Value::Type::BOOLEAN },
  { key::kBrowserGuestModeEnforced,
    prefs::kBrowserGuestModeEnforced,
    base::Value::Type::BOOLEAN },
  { key::kUnsafeEventsReportingEnabled,
    prefs::kUnsafeEventsReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDelayDeliveryUntilVerdict,
    prefs::kDelayDeliveryUntilVerdict,
    base::Value::Type::INTEGER },
  { key::kBlockLargeFileTransfer,
    prefs::kBlockLargeFileTransfer,
    base::Value::Type::INTEGER },
  { key::kAllowPasswordProtectedFiles,
    prefs::kAllowPasswordProtectedFiles,
    base::Value::Type::INTEGER },
  { key::kCheckContentCompliance,
    prefs::kCheckContentCompliance,
    base::Value::Type::INTEGER },
  { key::kBlockUnsupportedFiletypes,
    prefs::kBlockUnsupportedFiletypes,
    base::Value::Type::INTEGER },
  { key::kURLsToCheckComplianceOfDownloadedContent,
    prefs::kURLsToCheckComplianceOfDownloadedContent,
    base::Value::Type::LIST },
  { key::kURLsToCheckForMalwareOfUploadedContent,
    prefs::kURLsToCheckForMalwareOfUploadedContent,
    base::Value::Type::LIST },
  { key::kURLsToNotCheckForMalwareOfDownloadedContent,
    prefs::kURLsToNotCheckForMalwareOfDownloadedContent,
    base::Value::Type::LIST },
  { key::kURLsToNotCheckComplianceOfUploadedContent,
    prefs::kURLsToNotCheckComplianceOfUploadedContent,
    base::Value::Type::LIST },
  { key::kWebRtcAllowLegacyTLSProtocols,
    prefs::kWebRTCAllowLegacyTLSProtocols,
    base::Value::Type::BOOLEAN },
  { key::kMediaRecommendationsEnabled,
    kaleidoscope::prefs::kKaleidoscopePolicyEnabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  { key::kClientCertificateManagementAllowed,
    prefs::kClientCertificateManagementAllowed,
    base::Value::Type::INTEGER },
  { key::kCACertificateManagementAllowed,
    prefs::kCACertificateManagementAllowed,
    base::Value::Type::INTEGER },
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
  { key::kEmojiSuggestionEnabled,
    chromeos::prefs::kEmojiSuggestionEnterpriseAllowed,
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
  { key::kAppRecommendationZeroStateEnabled,
    prefs::kAppReinstallRecommendationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShelfAutoHideBehavior,
    ash::prefs::kShelfAutoHideBehaviorLocal,
    base::Value::Type::STRING },
  { key::kShelfAlignment,
    ash::prefs::kShelfAlignmentLocal,
    base::Value::Type::STRING },
  { key::kManagedGuestSessionAutoLaunchNotificationReduced,
    prefs::kManagedGuestSessionAutoLaunchNotificationReduced,
    base::Value::Type::BOOLEAN },
  { key::kManagedGuestSessionPrivacyWarningsEnabled,
    ash::prefs::kManagedGuestSessionPrivacyWarningsEnabled,
    base::Value::Type::BOOLEAN },
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
  { key::kFloatingAccessibilityMenuEnabled,
    ash::prefs::kAccessibilityFloatingMenuEnabled,
    base::Value::Type::BOOLEAN},
  { key::kLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSelectToSpeakEnabled,
    ash::prefs::kAccessibilitySelectToSpeakEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDictationEnabled,
    ash::prefs::kAccessibilityDictationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrimaryMouseButtonSwitch,
    prefs::kPrimaryMouseButtonRight,
    base::Value::Type::BOOLEAN },
  { key::kKeyboardFocusHighlightEnabled,
    ash::prefs::kAccessibilityFocusHighlightEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCursorHighlightEnabled,
    ash::prefs::kAccessibilityCursorHighlightEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCaretHighlightEnabled,
    ash::prefs::kAccessibilityCaretHighlightEnabled,
    base::Value::Type::BOOLEAN },
  { key::kMonoAudioEnabled,
    ash::prefs::kAccessibilityMonoAudioEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAutoclickEnabled,
    ash::prefs::kAccessibilityAutoclickEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSpokenFeedbackEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    base::Value::Type::BOOLEAN },
  { key::kHighContrastEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAccessibilityShortcutsEnabled,
    ash::prefs::kAccessibilityShortcutsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVirtualKeyboardFeatures,
    ash::prefs::kAccessibilityVirtualKeyboardFeatures,
    base::Value::Type::DICTIONARY },
  { key::kStickyKeysEnabled,
    ash::prefs::kAccessibilityStickyKeysEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultLargeCursorEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenLargeCursorEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenShowOptionsInSystemTrayMenu,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenPrimaryMouseButtonSwitch,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenSpokenFeedbackEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultHighContrastEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenHighContrastEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenAccessibilityShortcutsEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenVirtualKeyboardEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDictationEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenSelectToSpeakEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenCursorHighlightEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenCaretHighlightEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenMonoAudioEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenAutoclickEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenStickyKeysEnabled,
    nullptr,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenKeyboardFocusHighlightEnabled,
    nullptr,
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
  { key::kSchedulerConfiguration,
    prefs::kSchedulerConfiguration,
    base::Value::Type::STRING },
  { key::kDeviceExternalPrintServersAllowlist,
    prefs::kDeviceExternalPrintServersAllowlist,
    base::Value::Type::LIST },
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
  { key::kCrostiniExportImportUIAllowed,
    crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kVmManagementCliAllowed,
    crostini::prefs::kVmManagementCliAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kCrostiniRootAccessAllowed,
    crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kReportCrostiniUsageEnabled,
    crostini::prefs::kReportCrostiniUsageEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCrostiniArcAdbSideloadingAllowed,
    crostini::prefs::kCrostiniArcAdbSideloadingUserPref,
    base::Value::Type::INTEGER },
  { key::kCrostiniPortForwardingAllowed,
    crostini::prefs::kCrostiniPortForwardingAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kNTLMShareAuthenticationEnabled,
    prefs::kNTLMShareAuthenticationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintingSendUsernameAndFilenameEnabled,
    prefs::kPrintingSendUsernameAndFilenameEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUserPluginVmAllowed,
    plugin_vm::prefs::kPluginVmAllowed,
    base::Value::Type::BOOLEAN },
  { key::kPluginVmImage,
    plugin_vm::prefs::kPluginVmImage,
    base::Value::Type::DICTIONARY },
  { key::kPluginVmUserId,
    plugin_vm::prefs::kPluginVmUserId,
    base::Value::Type::STRING },
  { key::kPluginVmDataCollectionAllowed,
    plugin_vm::prefs::kPluginVmDataCollectionAllowed,
    base::Value::Type::BOOLEAN },
  { key::kPluginVmRequiredFreeDiskSpace,
    plugin_vm::prefs::kPluginVmRequiredFreeDiskSpaceGB,
    base::Value::Type::INTEGER },
  { key::kAssistantOnboardingMode,
    chromeos::assistant::prefs::kAssistantOnboardingMode,
    base::Value::Type::STRING },
  { key::kVoiceInteractionContextEnabled,
    chromeos::assistant::prefs::kAssistantContextEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVoiceInteractionHotwordEnabled,
    chromeos::assistant::prefs::kAssistantHotwordEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVoiceInteractionQuickAnswersEnabled,
    chromeos::assistant::prefs::kAssistantQuickAnswersEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDevicePowerPeakShiftEnabled,
    ash::prefs::kPowerPeakShiftEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDevicePowerPeakShiftBatteryThreshold,
    ash::prefs::kPowerPeakShiftBatteryThreshold,
    base::Value::Type::INTEGER },
  { key::kDevicePowerPeakShiftDayConfig,
    ash::prefs::kPowerPeakShiftDayConfig,
    base::Value::Type::DICTIONARY },
  { key::kDeviceBootOnAcEnabled,
    ash::prefs::kBootOnAcEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSamlInSessionPasswordChangeEnabled,
    chromeos::prefs::kSamlInSessionPasswordChangeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSamlPasswordExpirationAdvanceWarningDays,
    chromeos::prefs::kSamlPasswordExpirationAdvanceWarningDays,
    base::Value::Type::INTEGER },
  { key::kSamlLockScreenReauthenticationEnabled,
    chromeos::prefs::kSamlLockScreenReauthenticationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceAdvancedBatteryChargeModeEnabled,
    ash::prefs::kAdvancedBatteryChargeModeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceAdvancedBatteryChargeModeDayConfig,
    ash::prefs::kAdvancedBatteryChargeModeDayConfig,
    base::Value::Type::DICTIONARY },
  { key::kDeviceBatteryChargeMode,
    ash::prefs::kBatteryChargeMode,
    base::Value::Type::INTEGER },
  { key::kDeviceBatteryChargeCustomStartCharging,
    ash::prefs::kBatteryChargeCustomStartCharging,
    base::Value::Type::INTEGER },
  { key::kDeviceBatteryChargeCustomStopCharging,
    ash::prefs::kBatteryChargeCustomStopCharging,
    base::Value::Type::INTEGER },
  { key::kDeviceUsbPowerShareEnabled,
    ash::prefs::kUsbPowerShareEnabled,
    base::Value::Type::BOOLEAN },
  { key::kKerberosEnabled,
    prefs::kKerberosEnabled,
    base::Value::Type::BOOLEAN },
  { key::kKerberosRememberPasswordEnabled,
    prefs::kKerberosRememberPasswordEnabled,
    base::Value::Type::BOOLEAN },
  { key::kKerberosAddAccountsAllowed,
    prefs::kKerberosAddAccountsAllowed,
    base::Value::Type::BOOLEAN },
  { key::kStartupBrowserWindowLaunchSuppressed,
    prefs::kStartupBrowserWindowLaunchSuppressed,
    base::Value::Type::BOOLEAN },
  { key::kLockScreenMediaPlaybackEnabled,
    ash::prefs::kLockScreenMediaControlsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kForceLogoutUnauthenticatedUserEnabled,
    prefs::kForceLogoutUnauthenticatedUserEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSystemTimezoneAutomaticDetection,
    prefs::kSystemTimezoneAutomaticDetectionPolicy,
    base::Value::Type::INTEGER },
  { key::kDeviceWiFiFastTransitionEnabled,
    chromeos::prefs::kDeviceWiFiFastTransitionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNetworkThrottlingEnabled,
    prefs::kNetworkThrottlingEnabled,
    base::Value::Type::DICTIONARY },
  { key::kAllowScreenLock,
    ash::prefs::kAllowScreenLock,
    base::Value::Type::BOOLEAN },
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
  { key::kPinUnlockAutosubmitEnabled,
    prefs::kPinUnlockAutosubmitEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCastReceiverEnabled,
    prefs::kCastReceiverEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVpnConfigAllowed,
    ash::prefs::kVpnConfigAllowed,
    base::Value::Type::BOOLEAN },
  { key::kRelaunchHeadsUpPeriod,
    prefs::kRelaunchHeadsUpPeriod,
    base::Value::Type::INTEGER },
  { key::kPrivacyScreenEnabled,
    ash::prefs::kDisplayPrivacyScreenEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceChromeVariations,
    variations::prefs::kDeviceVariationsRestrictionsByPolicy,
    base::Value::Type::INTEGER },
  { key::kLoginDisplayPasswordButtonEnabled,
    chromeos::prefs::kLoginDisplayPasswordButtonEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeletePrintJobHistoryAllowed,
    prefs::kDeletePrintJobHistoryAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSuggestedContentEnabled,
    chromeos::prefs::kSuggestedContentEnabled,
    base::Value::Type::BOOLEAN },
    { key::kExtensionInstallEventLoggingEnabled,
    prefs::kExtensionInstallEventLoggingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRequiredClientCertificateForUser,
    prefs::kRequiredClientCertificateForUser,
    base::Value::Type::LIST },
  { key::kRequiredClientCertificateForDevice,
    prefs::kRequiredClientCertificateForDevice,
    base::Value::Type::LIST },

#else  // defined(OS_CHROMEOS)
  { key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
  { key::kChromeCleanupEnabled,
    prefs::kSwReporterEnabled,
    base::Value::Type::BOOLEAN },
  { key::kChromeCleanupReportingEnabled,
    prefs::kSwReporterReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRendererCodeIntegrityEnabled,
    prefs::kRendererCodeIntegrityEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBrowserSwitcherUseIeSitelist,
    browser_switcher::prefs::kUseIeSitelist,
    base::Value::Type::BOOLEAN },
  { key::kBrowserSwitcherChromePath,
    browser_switcher::prefs::kChromePath,
    base::Value::Type::STRING },
  { key::kBrowserSwitcherChromeParameters,
    browser_switcher::prefs::kChromeParameters,
    base::Value::Type::LIST },
  { key::kNativeWindowOcclusionEnabled,
    policy::policy_prefs::kNativeWindowOcclusionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintRasterizationMode,
    prefs::kPrintRasterizationMode,
    base::Value::Type::INTEGER },
#else  // defined(OS_WIN)
  { key::kNtlmV2Enabled,
    prefs::kNtlmV2Enabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_WIN)

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  { key::kThirdPartyBlockingEnabled,
    prefs::kThirdPartyBlockingEnabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::Type::BOOLEAN },
  { key::kBrowserAddPersonEnabled,
    prefs::kBrowserAddPersonEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintPreviewUseSystemDefaultPrinter,
    prefs::kPrintPreviewUseSystemDefaultPrinter,
    base::Value::Type::BOOLEAN },
  { key::kCloudPolicyOverridesPlatformPolicy,
    policy_prefs::kCloudPolicyOverridesPlatformPolicy,
    base::Value::Type::BOOLEAN },
  { key::kUserDataSnapshotRetentionLimit,
    prefs::kUserDataSnapshotRetentionLimit,
    base::Value::Type::INTEGER },
  { key::kPromotionalTabsEnabled,
    prefs::kPromotionalTabsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCommandLineFlagSecurityWarningsEnabled,
    prefs::kCommandLineFlagSecurityWarningsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAlternativeBrowserPath,
    browser_switcher::prefs::kAlternativeBrowserPath,
    base::Value::Type::STRING },
  { key::kAlternativeBrowserParameters,
    browser_switcher::prefs::kAlternativeBrowserParameters,
    base::Value::Type::LIST },
  { key::kBrowserSwitcherUrlList,
    browser_switcher::prefs::kUrlList,
    base::Value::Type::LIST },
  { key::kBrowserSwitcherUrlGreylist,
    browser_switcher::prefs::kUrlGreylist,
    base::Value::Type::LIST },
  { key::kBrowserSwitcherExternalSitelistUrl,
    browser_switcher::prefs::kExternalSitelistUrl,
    base::Value::Type::STRING },
  { key::kBrowserSwitcherExternalGreylistUrl,
    browser_switcher::prefs::kExternalGreylistUrl,
    base::Value::Type::STRING },
  { key::kBrowserSwitcherEnabled,
    browser_switcher::prefs::kEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBrowserSwitcherKeepLastChromeTab,
    browser_switcher::prefs::kKeepLastTab,
    base::Value::Type::BOOLEAN },
  { key::kBrowserSwitcherDelay,
    browser_switcher::prefs::kDelay,
    base::Value::Type::INTEGER },
  { key::kChromeVariations,
    variations::prefs::kVariationsRestrictionsByPolicy,
    base::Value::Type::INTEGER },
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if !defined(OS_MAC) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_MAC) && !defined(OS_CHROMEOS)

#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_CHROMEOS)
  { key::kAuthNegotiateDelegateByKdcPolicy,
    prefs::kAuthNegotiateDelegateByKdcPolicy,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_LINUX) || defined(OS_MAC) || defined(OS_CHROMEOS)

#if !defined(OS_MAC)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_MAC)

#if !defined(OS_MAC) && BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kFullscreenAllowed,
    extensions::pref_names::kAppFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_MAC) && BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kSecurityKeyPermitAttestation,
    prefs::kSecurityKeyPermitAttestation,
    base::Value::Type::LIST },
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kBlockExternalExtensions,
    extensions::pref_names::kBlockExternalExtensions,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
  { key::kBuiltinCertificateVerifierEnabled,
    prefs::kBuiltinCertificateVerifierEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
  { key::kClickToCallEnabled,
    prefs::kClickToCallEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  { key::kLocalDiscoveryEnabled,
    prefs::kLocalDiscoveryEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  { key::kSpellCheckServiceEnabled,
    spellcheck::prefs::kSpellCheckUseSpellingService,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  { key::kAllowNativeNotifications,
    prefs::kAllowNativeNotifications,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

  { key::kScrollToTextFragmentEnabled,
    prefs::kScrollToTextFragmentEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAppCacheForceEnabled,
    prefs::kAppCacheForceEnabled,
    base::Value::Type::BOOLEAN },
  { key::kIntensiveWakeUpThrottlingEnabled,
    policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUserAgentClientHintsEnabled,
    policy::policy_prefs::kUserAgentClientHintsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowFullUrlsInAddressBar,
    omnibox::kPreventUrlElisionsInOmnibox,
    base::Value::Type::BOOLEAN },
  { key::kInsecureFormsWarningsEnabled,
    prefs::kMixedFormsWarningsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kLookalikeWarningAllowlistDomains,
    prefs::kLookalikeWarningAllowlistDomains,
    base::Value::Type::LIST },

#if defined(OS_ANDROID)
  { key::kTosDialogBehavior,
    first_run::kTosDialogBehavior,
    base::Value::Type::INTEGER },
#endif  // defined(OS_ANDROID)
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
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Future policies are not supported on Stable and Beta by default.
bool AreFuturePoliciesSupported() {
  // Enable future policies for branded browser tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return true;
  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
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
#endif  // defined(OS_CHROMEOS)
}

std::unique_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  std::unique_ptr<ConfigurationPolicyHandlerList> handlers(
      new ConfigurationPolicyHandlerList(
          base::Bind(&PopulatePolicyHandlerParameters),
          base::Bind(&GetChromePolicyDetails), AreFuturePoliciesSupported()));
  for (size_t i = 0; i < base::size(kSimplePolicyMap); ++i) {
    handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
        kSimplePolicyMap[i].policy_name, kSimplePolicyMap[i].preference_path,
        kSimplePolicyMap[i].value_type));
  }

  handlers->AddHandler(
      std::make_unique<autofill::AutofillAddressPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<autofill::AutofillCreditCardPolicyHandler>());
  handlers->AddHandler(std::make_unique<autofill::AutofillPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<content_settings::CookieSettingsPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          content_settings::InsecurePrivateNetworkPolicyHandler>());
  handlers->AddHandler(std::make_unique<DefaultSearchPolicyHandler>());
  handlers->AddHandler(std::make_unique<ForceSafeSearchPolicyHandler>());
  handlers->AddHandler(std::make_unique<ForceYouTubeSafetyModePolicyHandler>());
  handlers->AddHandler(std::make_unique<IncognitoModePolicyHandler>());
  handlers->AddHandler(std::make_unique<GuestModePolicyHandler>());
  handlers->AddHandler(
      std::make_unique<bookmarks::ManagedBookmarksPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<HomepageLocationPolicyHandler>());
  handlers->AddHandler(std::make_unique<ProxyPolicyHandler>());
  handlers->AddHandler(std::make_unique<SecureDnsPolicyHandler>());
  handlers->AddHandler(std::make_unique<ReferrerPolicyPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForUrls,
      certificate_transparency::prefs::kCTExcludedHosts, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForCas,
      certificate_transparency::prefs::kCTExcludedSPKIs, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForLegacyCas,
      certificate_transparency::prefs::kCTExcludedLegacySPKIs, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kHSTSPolicyBypassList, prefs::kHSTSPolicyBypassList, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      WebUsbAllowDevicesForUrlsPolicyHandler::CreateForUserPolicy(
          chrome_schema));
  handlers->AddHandler(
      std::make_unique<PrintingAllowedBackgroundGraphicsModesPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<PrintingBackgroundGraphicsDefaultPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<PrintingPaperSizeDefaultPolicyHandler>());
  handlers->AddHandler(std::make_unique<DeveloperToolsPolicyHandler>());
  handlers->AddHandler(std::make_unique<FileSelectionDialogsPolicyHandler>());
  handlers->AddHandler(std::make_unique<JavascriptPolicyHandler>());
  handlers->AddHandler(std::make_unique<NetworkPredictionPolicyHandler>());
  handlers->AddHandler(std::make_unique<RestoreOnStartupPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<safe_browsing::SafeBrowsingPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kAuthServerWhitelist,
                                            prefs::kAuthServerAllowlist,
                                            base::Value::Type::STRING),
      std::make_unique<SimplePolicyHandler>(key::kAuthServerAllowlist,
                                            prefs::kAuthServerAllowlist,
                                            base::Value::Type::STRING)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kAuthNegotiateDelegateWhitelist,
          prefs::kAuthNegotiateDelegateAllowlist, base::Value::Type::STRING),
      std::make_unique<SimplePolicyHandler>(
          key::kAuthNegotiateDelegateAllowlist,
          prefs::kAuthNegotiateDelegateAllowlist, base::Value::Type::STRING)));

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kSafeBrowsingWhitelistDomains,
          prefs::kSafeBrowsingWhitelistDomains, base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(
          key::kSafeBrowsingAllowlistDomains,
          prefs::kSafeBrowsingWhitelistDomains, base::Value::Type::LIST)));
  handlers->AddHandler(std::make_unique<syncer::SyncPolicyHandler>());
  handlers->AddHandler(std::make_unique<BrowsingHistoryPolicyHandler>());

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<URLBlocklistPolicyHandler>(key::kURLBlacklist),
      std::make_unique<URLBlocklistPolicyHandler>(key::kURLBlocklist)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kURLWhitelist,
                                            policy_prefs::kUrlWhitelist,
                                            base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(key::kURLAllowlist,
                                            policy_prefs::kUrlWhitelist,
                                            base::Value::Type::LIST)));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kSafeBrowsingExtendedReportingEnabled,
      prefs::kSafeBrowsingScoutReportingEnabled, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

#if defined(OS_ANDROID)
  handlers->AddHandler(
      std::make_unique<ContextualSearchPolicyHandlerAndroid>());
#else   // defined(OS_ANDROID)
  handlers->AddHandler(
      std::make_unique<NtpCustomBackgroundEnabledPolicyHandler>());
  handlers->AddHandler(std::make_unique<DefaultDownloadDirPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<DownloadAutoOpenPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<DownloadDirPolicyHandler>());
  handlers->AddHandler(std::make_unique<LocalSyncPolicyHandler>());

  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kRegisteredProtocolHandlers,
      prefs::kPolicyRegisteredProtocolHandlers, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED));

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kAutoplayWhitelist,
                                            prefs::kAutoplayWhitelist,
                                            base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(key::kAutoplayAllowlist,
                                            prefs::kAutoplayWhitelist,
                                            base::Value::Type::LIST)));

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
  handlers->AddHandler(
      std::make_unique<enterprise_reporting::ExtensionRequestPolicyHandler>());

  // Handlers for Chrome Enterprise Connectors policies.
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnFileAttachedEnterpriseConnector,
          enterprise_connectors::kOnFileAttachedPref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnFileDownloadedEnterpriseConnector,
          enterprise_connectors::kOnFileDownloadedPref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnBulkDataEntryEnterpriseConnector,
          enterprise_connectors::kOnBulkDataEntryPref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnSecurityEventEnterpriseConnector,
          enterprise_connectors::kOnSecurityEventPref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kEnterpriseRealTimeUrlCheckMode,
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode, chrome_schema));
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
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
  // TODO(binjin): Remove LegacyPoliciesDeprecatingPolicyHandler for these two
  // policies once deprecation of legacy power management policies is done.
  // http://crbug.com/346229
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(power_management_idle_legacy_policies),
      base::WrapUnique(
          new PowerManagementIdleSettingsPolicyHandler(chrome_schema))));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(screen_lock_legacy_policies),
      std::make_unique<ScreenLockDelayPolicyHandler>(chrome_schema)));

  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kAttestationExtensionAllowlist,
          prefs::kAttestationExtensionAllowlist, false),
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kAttestationExtensionWhitelist,
          prefs::kAttestationExtensionAllowlist, false)));

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kQuickUnlockModeAllowlist,
                                            prefs::kQuickUnlockModeAllowlist,
                                            base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(key::kQuickUnlockModeWhitelist,
                                            prefs::kQuickUnlockModeAllowlist,
                                            base::Value::Type::LIST)));

  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForDevicePolicy()));
  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy()));
  handlers->AddHandler(std::make_unique<PinnedLauncherAppsPolicyHandler>());
  handlers->AddHandler(std::make_unique<ScreenMagnifierPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<LoginScreenPowerManagementPolicyHandler>(chrome_schema));
  // Handler for another policy with JSON strings, lenient but shows warnings.
  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kNativePrinters, prefs::kRecommendedPrinters,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED),
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kPrinters, prefs::kRecommendedPrinters,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kUserNativePrintersAllowed,
                                            prefs::kUserPrintersAllowed,
                                            base::Value::Type::BOOLEAN),
      std::make_unique<SimplePolicyHandler>(key::kUserPrintersAllowed,
                                            prefs::kUserPrintersAllowed,
                                            base::Value::Type::BOOLEAN)));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kSAMLOfflineSigninTimeLimit,
      chromeos::prefs::kSAMLOfflineSigninTimeLimit, -1, INT_MAX, true));
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
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kDeviceLoginScreenDefaultScreenMagnifierType, nullptr,
      chromeos::MAGNIFIER_DISABLED, chromeos::MAGNIFIER_DOCKED, false));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kDeviceLoginScreenScreenMagnifierType, nullptr,
      chromeos::MAGNIFIER_DISABLED, chromeos::MAGNIFIER_DOCKED, false));
  handlers->AddHandler(
      std::make_unique<ScreenBrightnessPercentPolicyHandler>(chrome_schema));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kUserAvatarImage));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kDeviceWallpaperImage));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kWallpaperImage));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kPrintersBulkConfiguration));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kNativePrintersBulkAccessMode,
          prefs::kRecommendedPrintersAccessMode, base::Value::Type::INTEGER),
      std::make_unique<SimplePolicyHandler>(
          key::kPrintersBulkAccessMode, prefs::kRecommendedPrintersAccessMode,
          base::Value::Type::INTEGER)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kNativePrintersBulkBlacklist,
          prefs::kRecommendedPrintersBlocklist, base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(
          key::kPrintersBulkBlocklist, prefs::kRecommendedPrintersBlocklist,
          base::Value::Type::LIST)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kNativePrintersBulkWhitelist,
          prefs::kRecommendedPrintersAllowlist, base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(
          key::kPrintersBulkAllowlist, prefs::kRecommendedPrintersAllowlist,
          base::Value::Type::LIST)));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kExternalPrintServers));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kDeviceExternalPrintServers));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kDeviceWilcoDtcConfiguration));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kCrostiniAnsiblePlaybook));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kSessionLocales, nullptr, chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED));
  handlers->AddHandler(
      std::make_unique<chromeos::platform_keys::KeyPermissionsPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<DefaultGeolocationPolicyHandler>());
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kNoteTakingAppsLockScreenAllowlist,
      prefs::kNoteTakingAppsLockScreenAllowlist, false /*allow_wildcards*/));
  handlers->AddHandler(
      std::make_unique<SecondaryGoogleAccountSigninPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kUsageTimeLimit, prefs::kUsageTimeLimit, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
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
      std::make_unique<PrintingAllowedPinModesPolicyHandler>());
  handlers->AddHandler(std::make_unique<PrintingColorDefaultPolicyHandler>());
  handlers->AddHandler(std::make_unique<PrintingDuplexDefaultPolicyHandler>());
  handlers->AddHandler(std::make_unique<PrintingPinDefaultPolicyHandler>());
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kPrintingMaxSheetsAllowed, prefs::kPrintingMaxSheetsAllowed, 1,
      INT_MAX, true));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kPrintJobHistoryExpirationPeriod,
      prefs::kPrintJobHistoryExpirationPeriod, -1, INT_MAX, true));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kNetworkFileSharesPreconfiguredShares,
      prefs::kNetworkFileSharesPreconfiguredShares, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kParentAccessCodeConfig, prefs::kParentAccessCodeConfig,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kPerAppTimeLimits, prefs::kPerAppTimeLimitsPolicy, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimpleSchemaValidatingPolicyHandler>(
          key::kPerAppTimeLimitsWhitelist,
          prefs::kPerAppTimeLimitsAllowlistPolicy, chrome_schema,
          SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED),
      std::make_unique<SimpleSchemaValidatingPolicyHandler>(
          key::kPerAppTimeLimitsAllowlist,
          prefs::kPerAppTimeLimitsAllowlistPolicy, chrome_schema,
          SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED)));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kEduCoexistenceToSVersion,
      chromeos::prefs::kEduCoexistenceToSVersion, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      std::make_unique<EcryptfsMigrationStrategyPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kKerberosAccounts, prefs::kKerberosAccounts, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      WebUsbAllowDevicesForUrlsPolicyHandler::CreateForDevicePolicy(
          chrome_schema));
  handlers->AddHandler(
      std::make_unique<SystemFeaturesDisableListPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kExternalPrintServersWhitelist,
          prefs::kExternalPrintServersAllowlist, base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(
          key::kExternalPrintServersAllowlist,
          prefs::kExternalPrintServersAllowlist, base::Value::Type::LIST)));
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kNearbyShareAllowed, prefs::kNearbySharingEnabledPrefName));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kDataLeakPreventionRulesList, policy_prefs::kDlpRulesList,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
#if defined(USE_CUPS)
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kPrintingAPIExtensionsWhitelist,
          prefs::kPrintingAPIExtensionsAllowlist, /*allow_wildcards=*/false),
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kPrintingAPIExtensionsAllowlist,
          prefs::kPrintingAPIExtensionsAllowlist, /*allow_wildcards=*/false)));
#endif  // defined(USE_CUPS)
#else   // defined(OS_CHROMEOS)
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
#else   // defined(OS_ANDROID)
      prefs::kSigninAllowedOnNextStartup,
#endif  // defined(OS_ANDROID)
      base::Value::Type::BOOLEAN));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(signin_legacy_policies),
      std::make_unique<BrowserSigninPolicyHandler>(chrome_schema)));
#endif  // defined(OS_CHROMEOS)

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
      std::make_unique<SecureOriginPolicyHandler>(
          key::kOverrideSecurityRestrictionsOnInsecureOrigin, chrome_schema)));
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  handlers->AddHandler(std::make_unique<DiskCacheDirPolicyHandler>());

  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingWhitelist,
          extensions::pref_names::kNativeMessagingAllowlist, false),
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingAllowlist,
          extensions::pref_names::kNativeMessagingAllowlist, false)));
  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingBlacklist,
          extensions::pref_names::kNativeMessagingBlocklist, true),
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingBlocklist,
          extensions::pref_names::kNativeMessagingBlocklist, true)));
  handlers->AddHandler(
      std::make_unique<AutoLaunchProtocolsPolicyHandler>(chrome_schema));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kExtensionInstallWhitelist,
          extensions::pref_names::kInstallAllowList, false),
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kExtensionInstallAllowlist,
          extensions::pref_names::kInstallAllowList, false)));
  handlers->AddHandler(std::make_unique<policy::SimpleDeprecatingPolicyHandler>(
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kExtensionInstallBlacklist,
          extensions::pref_names::kInstallDenyList, true),
      std::make_unique<extensions::ExtensionListPolicyHandler>(
          key::kExtensionInstallBlocklist,
          extensions::pref_names::kInstallDenyList, true)));
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionInstallForcelistPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          extensions::ExtensionInstallLoginScreenExtensionsPolicyHandler>());
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
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kDeviceChromeVariations, nullptr,
      static_cast<int>(variations::RestrictionPolicy::NO_RESTRICTIONS),
      static_cast<int>(variations::RestrictionPolicy::ALL), false));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kWebAppInstallForceList, prefs::kWebAppInstallForceList,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  handlers->AddHandler(std::make_unique<SpellcheckLanguagePolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SpellcheckLanguageBlocklistPolicyHandler>(
          policy::key::kSpellcheckLanguageBlacklist),
      std::make_unique<SpellcheckLanguageBlocklistPolicyHandler>(
          policy::key::kSpellcheckLanguageBlocklist)));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(ENABLE_PLUGINS)
  handlers->AddHandler(std::make_unique<PluginPolicyHandler>());
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  return handlers;
}

}  // namespace policy
