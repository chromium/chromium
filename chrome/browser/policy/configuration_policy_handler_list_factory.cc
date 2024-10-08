// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include <limits.h>
#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/browsing_data_lifetime_policy_handler.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_policy_handler.h"
#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/browser/net/explicitly_allowed_network_ports_policy_handler.h"
#include "chrome/browser/net/secure_dns_policy_handler.h"
#include "chrome/browser/performance_manager/public/user_tuning/memory_saver_policy_handler.h"
#include "chrome/browser/policy/annotations/blocklist_handler.h"
#include "chrome/browser/policy/browsing_history_policy_handler.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/drive_file_sync_available_policy_handler.h"
#include "chrome/browser/policy/extension_developer_mode_policy_handler.h"
#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"
#include "chrome/browser/policy/homepage_location_policy_handler.h"
#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/browser/policy/webhid_device_policy_handler.h"
#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/profiles/force_safe_search_policy_handler.h"
#include "chrome/browser/profiles/force_youtube_safety_mode_policy_handler.h"
#include "chrome/browser/profiles/guest_mode_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/browser/search/ntp_custom_background_enabled_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/browser/spellchecker/spellcheck_language_blocklist_policy_handler.h"
#include "chrome/browser/spellchecker/spellcheck_language_policy_handler.h"
#include "chrome/browser/ssl/secure_origin_policy_handler.h"
#include "chrome/browser/themes/theme_color_policy_handler.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_policy_handler.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/blocked_content/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmarks_policy_handler.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/commerce/core/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"
#include "components/content_settings/core/browser/insecure_private_network_policy_handler.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/pref_names.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/embedder_support/pref_names.h"
#include "components/enterprise/browser/reporting/cloud_profile_reporting_policy_handler.h"
#include "components/enterprise/browser/reporting/cloud_reporting_frequency_policy_handler.h"
#include "components/enterprise/browser/reporting/cloud_reporting_policy_handler.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/enterprise_connectors_policy_handler.h"
#include "components/enterprise/content/copy_prevention_settings_policy_handler.h"
#include "components/enterprise/content/pref_names.h"
#include "components/enterprise/data_controls/core/browser/data_controls_policy_handler.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/policy/core/browser/boolean_disabling_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/browser/url_scheme_list_policy_handler.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/proxy_config/proxy_policy_handler.h"
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/enterprise/default_search_policy_handler.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/security_interstitials/core/https_only_mode_policy_handler.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/sharing_message/buildflags.h"
#include "components/sharing_message/pref_names.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_policy_handler.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "chrome/browser/lens/android/lens_prefs.h"
#include "chrome/browser/search/contextual_search_policy_handler_android.h"
#include "ui/accessibility/accessibility_prefs.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/default_download_dir_policy_handler.h"
#include "chrome/browser/download/download_auto_open_policy_handler.h"
#include "chrome/browser/download/download_dir_policy_handler.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_policy_handler.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/policy/local_sync_policy_handler.h"
#include "chrome/browser/policy/managed_account_policy_handler.h"
#include "chrome/browser/web_applications/policy/web_app_settings_policy_handler.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/headless/policy/headless_mode_policy_handler.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/media_router/common/pref_names.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#endif  // BUILDFLAG(TOOLKIT_VIEWS)

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#else
#include "chrome/browser/chromeos/quickoffice/quickoffice_prefs.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/accessibility/magnifier_type.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_policy_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/arc/policy/arc_policy_handler.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer_policy_handler.h"
#include "chrome/browser/ash/bruschetta/bruschetta_policy_handler.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_policy_handler.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/handlers/app_launch_automation_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/ash/policy/handlers/contextual_google_integrations_policies_handler.h"
#include "chrome/browser/ash/policy/handlers/help_me_read_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/lacros_availability_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/lacros_selection_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/multi_screen_capture_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/screen_capture_location_policy_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/policy/default_geolocation_policy_handler.h"
#include "chrome/browser/policy/device_login_screen_geolocation_access_level_policy_handler.h"
#include "chrome/browser/policy/os_color_mode_policy_handler.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/pref_names.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/account_manager_core/pref_names.h"
#include "components/drive/drive_pref_names.h"  // nogncheck crbug.com/1125897
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/external_protocol/auto_launch_protocols_policy_handler.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_pref_names.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/policy/printing_restrictions_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/device_signals/core/browser/pref_names.h"  // nogncheck due to crbug.com/1125897
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
#include "components/enterprise/idle/idle_timeout_policy_handler.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "chrome/browser/privacy_sandbox/privacy_sandbox_policy_handler.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_prefs.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/prefs.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/policy/battery_saver_policy_handler.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/search_engines/enterprise/site_search_policy_handler.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace policy {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ::ash::MagnifierType;
#endif

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
// clang-format off
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
// Policies for all platforms - Start
  { key::kComponentUpdatesEnabled,
    prefs::kComponentUpdatesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDataURLWhitespacePreservationEnabled,
    prefs::kDataURLWhitespacePreservationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::Type::INTEGER },
  { key::kForcePermissionPolicyUnloadDefaultEnabled,
    policy_prefs::kForcePermissionPolicyUnloadDefaultEnabled,
    base::Value::Type::BOOLEAN},
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::Type::BOOLEAN },
  { key::kDomainReliabilityAllowed,
    domain_reliability::prefs::kDomainReliabilityAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kEditBookmarksEnabled,
    bookmarks::prefs::kEditBookmarksEnabled,
    base::Value::Type::BOOLEAN },
  { key::kFeedbackSurveysEnabled,
    policy::policy_prefs::kFeedbackSurveysEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPasswordManagerEnabled,
    password_manager::prefs::kCredentialsEnableService,
    base::Value::Type::BOOLEAN },
  { key::kPasswordSharingEnabled,
    password_manager::prefs::kPasswordSharingEnabled,
    base::Value::Type::BOOLEAN},
  { key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kNetworkPredictionOptions, prefetch::prefs::kNetworkPredictionOptions,
    base::Value::Type::INTEGER },
#if BUILDFLAG(ENABLE_PRINTING)
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::Type::BOOLEAN },
#endif // BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  { key::kOopPrintDriversAllowed,
    prefs::kOopPrintDriversAllowedByPolicy,
    base::Value::Type::BOOLEAN },
#endif
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingProxiedRealTimeChecksAllowed,
    prefs::kHashPrefixRealTimeChecksAllowedByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::Type::BOOLEAN },
  { key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::Type::BOOLEAN },
  { key::kTranslateEnabled,
    translate::prefs::kOfferTranslateEnabled,
    base::Value::Type::BOOLEAN },
  { key::kURLAllowlist,
    policy_prefs::kUrlAllowlist,
    base::Value::Type::LIST
  },
  { key::kHistoryClustersVisible,
    history_clusters::prefs::kVisible,
    base::Value::Type::BOOLEAN },
  { key::kAllowWebAuthnWithBrokenTlsCerts,
    webauthn::pref_names::kAllowWithBrokenCerts,
    base::Value::Type::BOOLEAN },
  { key::kHttpAllowlist,
    prefs::kHttpAllowlist,
    base::Value::Type::LIST },
  { key::kPrivacySandboxFingerprintingProtectionEnabled,
    prefs::kFingerprintingProtectionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrivacySandboxIpProtectionEnabled,
    prefs::kIpProtectionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kHttpsUpgradesEnabled,
    prefs::kHttpsUpgradesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultThirdPartyStoragePartitioningSetting,
    prefs::kManagedDefaultThirdPartyStoragePartitioningSetting,
    base::Value::Type::INTEGER },
  { key::kThirdPartyStoragePartitioningBlockedForOrigins,
    prefs::kManagedThirdPartyStoragePartitioningBlockedForOrigins,
    base::Value::Type::LIST },
  { key::kDataUrlInSvgUseEnabled,
    prefs::kDataUrlInSvgUseEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPolicyTestPageEnabled,
    policy_prefs::kPolicyTestPageEnabled,
    base::Value::Type::BOOLEAN},
  { key::kZstdContentEncodingEnabled,
    prefs::kZstdContentEncodingEnabled,
    base::Value::Type::BOOLEAN},
  { key::kPrefixedVideoFullscreenApiAvailability,
    prefs::kPrefixedVideoFullscreenApiAvailability,
    base::Value::Type::STRING },
  { key::kQRCodeGeneratorEnabled,
    prefs::kQRCodeGeneratorEnabled,
    base::Value::Type::BOOLEAN },
  { key::kReportingEndpoints,
    prefs::kReportingEndpoints,
    base::Value::Type::DICT },
  { key::kDownloadRestrictions,
    prefs::kDownloadRestrictions,
    base::Value::Type::INTEGER },
// Policies for all platforms - End
#if BUILDFLAG(IS_ANDROID)
  { key::kAccessibilityPerformanceFilteringAllowed,
    prefs::kAccessibilityPerformanceFilteringAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAuthAndroidNegotiateAccountType,
    prefs::kAuthAndroidNegotiateAccountType,
    base::Value::Type::STRING },
  { key::kBackForwardCacheEnabled,
    policy_prefs::kBackForwardCacheEnabled,
    base::Value::Type::BOOLEAN },
  { key::kIsolateOriginsAndroid,
    prefs::kIsolateOrigins,
    base::Value::Type::STRING },
  { key::kLensCameraAssistedSearchEnabled,
    lens::kLensCameraAssistedSearchEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNTPContentSuggestionsEnabled,
    feed::prefs::kEnableSnippets,
    base::Value::Type::BOOLEAN },
  { key::kSitePerProcessAndroid,
    prefs::kSitePerProcess,
    base::Value::Type::BOOLEAN },
  { key::kTosDialogBehavior,
    first_run::kTosDialogBehavior,
    base::Value::Type::INTEGER },
  { key::kThirdPartyPasswordManagersAllowed,
    autofill::prefs::kAutofillThirdPartyPasswordManagersAllowed,
    base::Value::Type::BOOLEAN },
  { key::kWebXRImmersiveArEnabled,
    prefs::kWebXRImmersiveArEnabled,
    base::Value::Type::BOOLEAN },
  { key::kListenToThisPageEnabled,
    prefs::kListenToThisPageEnabled,
    base::Value::Type::BOOLEAN },
#else // !BUILDFLAG(IS_ANDROID)
  { key::kAbusiveExperienceInterventionEnforce,
    blocked_content::prefs::kAbusiveExperienceInterventionEnforce,
    base::Value::Type::BOOLEAN },
  { key::kAccessCodeCastDeviceDuration,
    media_router::prefs::kAccessCodeCastDeviceDuration,
    base::Value::Type::INTEGER },
  { key::kAccessCodeCastEnabled,
    media_router::prefs::kAccessCodeCastEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAccessibilityImageLabelsEnabled,
    prefs::kAccessibilityImageLabelsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAdsSettingForIntrusiveAdsSites,
    prefs::kManagedDefaultAdsSetting,
    base::Value::Type::INTEGER },
  { key::kAdvancedProtectionAllowed,
    prefs::kAdvancedProtectionAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    base::Value::Type::BOOLEAN },
  { key::kAllowDeletingBrowserHistory,
    prefs::kAllowDeletingBrowserHistory,
    base::Value::Type::BOOLEAN },
  { key::kAllowDinosaurEasterEgg,
    prefs::kAllowDinosaurEasterEgg,
    base::Value::Type::BOOLEAN },
  { key::kAmbientAuthenticationInPrivateModesEnabled,
    prefs::kAmbientAuthenticationInPrivateModesEnabled,
    base::Value::Type::INTEGER },
  { key::kAudioCaptureAllowed,
    prefs::kAudioCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAudioCaptureAllowedUrls,
    prefs::kAudioCaptureAllowedUrls,
    base::Value::Type::LIST },
  { key::kAutoOpenAllowedForURLs,
    prefs::kDownloadAllowedURLsForOpenByPolicy,
    base::Value::Type::LIST },
  { key::kAutoplayAllowlist,
    prefs::kAutoplayAllowlist,
    base::Value::Type::LIST },
  { key::kScreenCaptureWithoutGestureAllowedForOrigins,
    prefs::kScreenCaptureWithoutGestureAllowedForOrigins,
    base::Value::Type::LIST },
  { key::kFileOrDirectoryPickerWithoutGestureAllowedForOrigins,
    prefs::kFileOrDirectoryPickerWithoutGestureAllowedForOrigins,
    base::Value::Type::LIST },
  { key::kBasicAuthOverHttpEnabled,
    prefs::kBasicAuthOverHttpEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBookmarkBarEnabled,
    bookmarks::prefs::kShowBookmarkBar,
    base::Value::Type::BOOLEAN },
  { key::kBrowserAddPersonEnabled,
    prefs::kBrowserAddPersonEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBrowserLabsEnabled,
    chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy,
    base::Value::Type::BOOLEAN },
#if defined(TOOLKIT_VIEWS)
  { key::kSideSearchEnabled,
    side_search_prefs::kSideSearchEnabled,
    base::Value::Type::BOOLEAN },
#endif // defined(TOOLKIT_VIEWS)
#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
  { key::kClickToCallEnabled,
    prefs::kClickToCallEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)
  { key::kClipboardAllowedForUrls,
    prefs::kManagedClipboardAllowedForUrls,
    base::Value::Type::LIST },
  { key::kClipboardBlockedForUrls,
    prefs::kManagedClipboardBlockedForUrls,
    base::Value::Type::LIST },
  { key::kCompressionDictionaryTransportEnabled,
    prefs::kCompressionDictionaryTransportEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultClipboardSetting,
    prefs::kManagedDefaultClipboardSetting,
    base::Value::Type::INTEGER },
  { key::kDNSInterceptionChecksEnabled,
    prefs::kDNSInterceptionChecksEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultFileSystemReadGuardSetting,
    prefs::kManagedDefaultFileSystemReadGuardSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultFileSystemWriteGuardSetting,
    prefs::kManagedDefaultFileSystemWriteGuardSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultInsecureContentSetting,
    prefs::kManagedDefaultInsecureContentSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultLocalFontsSetting,
    prefs::kManagedDefaultLocalFontsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultSearchProviderContextMenuAccessAllowed,
    prefs::kDefaultSearchProviderContextMenuAccessAllowed,
    base::Value::Type::BOOLEAN },
  { key::kDefaultSerialGuardSetting,
    prefs::kManagedDefaultSerialGuardSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultWebHidGuardSetting,
    prefs::kManagedDefaultWebHidGuardSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultDirectSocketsSetting,
    prefs::kManagedDefaultDirectSocketsSetting,
    base::Value::Type::INTEGER },
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  { key::kDeletingUndecryptablePasswordsEnabled,
    password_manager::prefs::kDeletingUndecryptablePasswordsEnabled,
    base::Value::Type::BOOLEAN },
#endif
  { key::kDirectSocketsAllowedForUrls,
    prefs::kManagedDirectSocketsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kDirectSocketsBlockedForUrls,
    prefs::kManagedDirectSocketsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    base::Value::Type::BOOLEAN },
  { key::kDisableScreenshots,
    prefs::kDisableScreenshots,
    base::Value::Type::BOOLEAN },
  { key::kEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort,
    base::Value::Type::BOOLEAN },
  { key::kEnableOnlineRevocationChecks,
    prefs::kCertRevocationCheckingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kFetchKeepaliveDurationSecondsOnShutdown,
    prefs::kFetchKeepaliveDurationOnShutdown,
    base::Value::Type::INTEGER },
  { key::kFileSystemReadAskForUrls,
    prefs::kManagedFileSystemReadAskForUrls,
    base::Value::Type::LIST },
  { key::kFileSystemReadBlockedForUrls,
    prefs::kManagedFileSystemReadBlockedForUrls,
    base::Value::Type::LIST },
  { key::kFileSystemWriteAskForUrls,
    prefs::kManagedFileSystemWriteAskForUrls,
    base::Value::Type::LIST },
  { key::kFileSystemWriteBlockedForUrls,
    prefs::kManagedFileSystemWriteBlockedForUrls,
    base::Value::Type::LIST },
  { key::kForcedLanguages,
    language::prefs::kForcedLanguages,
    base::Value::Type::LIST },
  { key::kGloballyScopeHTTPAuthCacheEnabled,
    prefs::kGloballyScopeHTTPAuthCacheEnabled,
    base::Value::Type::BOOLEAN },
  { key::kHideWebStoreIcon,
    policy::policy_prefs::kHideWebStoreIcon,
    base::Value::Type::BOOLEAN },
  { key::kHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage,
    base::Value::Type::BOOLEAN },
  { key::kImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    base::Value::Type::LIST },
  { key::kImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    base::Value::Type::LIST },
  { key::kInsecureContentAllowedForUrls,
    prefs::kManagedInsecureContentAllowedForUrls,
    base::Value::Type::LIST },
  { key::kInsecureContentBlockedForUrls,
    prefs::kManagedInsecureContentBlockedForUrls,
    base::Value::Type::LIST },
  { key::kIntranetRedirectBehavior,
    omnibox::kIntranetRedirectBehavior,
    base::Value::Type::INTEGER },
  { key::kIsolateOrigins,
    prefs::kIsolateOrigins,
    base::Value::Type::STRING },
  { key::kLensDesktopNTPSearchEnabled,
    prefs::kLensDesktopNTPSearchEnabled,
    base::Value::Type::BOOLEAN },
  { key::kLensRegionSearchEnabled,
    prefs::kLensRegionSearchEnabled,
    base::Value::Type::BOOLEAN },
  { key::kLocalFontsAllowedForUrls,
    prefs::kManagedLocalFontsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kLocalFontsBlockedForUrls,
    prefs::kManagedLocalFontsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy,
    base::Value::Type::INTEGER },
  { key::kMediaRouterCastAllowAllIPs,
    media_router::prefs::kMediaRouterCastAllowAllIPs,
    base::Value::Type::BOOLEAN },
  { key::kNewTabPageLocation,
    prefs::kNewTabPageLocationOverride,
    base::Value::Type::STRING },
  { key::kNTPCardsVisible,
    prefs::kNtpModulesVisible,
    base::Value::Type::BOOLEAN },
  { key::kNTPMiddleSlotAnnouncementVisible,
    prefs::kNtpPromoVisible,
    base::Value::Type::BOOLEAN },
  { key::kNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kOriginAgentClusterDefaultEnabled,
    prefs::kOriginAgentClusterDefaultEnabled,
    base::Value::Type::BOOLEAN},
  { key::kPasswordDismissCompromisedAlertEnabled,
    password_manager::prefs::kPasswordDismissCompromisedAlertEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPasswordProtectionChangePasswordURL,
    prefs::kPasswordProtectionChangePasswordURL,
    base::Value::Type::STRING },
  { key::kPasswordProtectionLoginURLs,
    prefs::kPasswordProtectionLoginURLs,
    base::Value::Type::LIST },
  { key::kPasswordProtectionWarningTrigger,
    prefs::kPasswordProtectionWarningTrigger,
    base::Value::Type::INTEGER },
#if BUILDFLAG(ENABLE_PDF)
  { key::kPdfLocalFileAccessAllowedForDomains,
    prefs::kPdfLocalFileAccessAllowedForDomains,
    base::Value::Type::LIST},
  { key::kPdfUseSkiaRendererEnabled,
    prefs::kPdfUseSkiaRendererEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPdfViewerOutOfProcessIframeEnabled,
    prefs::kPdfViewerOutOfProcessIframeEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_PDF)
  { key::kPolicyRefreshRate,
    policy_prefs::kUserPolicyRefreshRate,
    base::Value::Type::INTEGER },
  { key::kPrintHeaderFooter,
    prefs::kPrintHeaderFooter,
    base::Value::Type::BOOLEAN },
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  { key::kPrintPdfAsImageDefault,
    prefs::kPrintPdfAsImageDefault,
    base::Value::Type::BOOLEAN },
    { key::kPrintRasterizePdfDpi,
      prefs::kPrintRasterizePdfDpi,
      base::Value::Type::INTEGER },
#endif  // BUILDFLAGS(ENABLE_PRINT_PREVIEW)
  { key::kPrinterTypeDenyList,
    prefs::kPrinterTypeDenyList,
    base::Value::Type::LIST },
  { key::kPromptOnMultipleMatchingCertificates,
    prefs::kPromptOnMultipleMatchingCertificates,
    base::Value::Type::BOOLEAN },
  { key::kQuicAllowed,
    prefs::kQuicAllowed,
    base::Value::Type::BOOLEAN },
  { key::kRelaunchNotification,
    prefs::kRelaunchNotification,
    base::Value::Type::INTEGER },
  { key::kRelaunchNotificationPeriod,
    prefs::kRelaunchNotificationPeriod,
    base::Value::Type::INTEGER },
  { key::kRemoteDebuggingAllowed,
    prefs::kDevToolsRemoteDebuggingAllowed,
    base::Value::Type::BOOLEAN },
  { key::kRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup,
    base::Value::Type::LIST },
  { key::kSafeBrowsingAllowlistDomains,
    prefs::kSafeBrowsingAllowlistDomains,
    base::Value::Type::LIST },
  { key::kSameOriginTabCaptureAllowedByOrigins,
    prefs::kSameOriginTabCaptureAllowedByOrigins,
    base::Value::Type::LIST },
  { key::kSandboxExternalProtocolBlocked,
    prefs::kSandboxExternalProtocolBlocked,
    base::Value::Type::BOOLEAN },
  { key::kScreenCaptureAllowed,
    prefs::kScreenCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kScreenCaptureAllowedByOrigins,
    prefs::kScreenCaptureAllowedByOrigins,
    base::Value::Type::LIST },
  { key::kSecurityKeyPermitAttestation,
    prefs::kSecurityKeyPermitAttestation,
    base::Value::Type::LIST },
  { key::kSerialAllowAllPortsForUrls,
    prefs::kManagedSerialAllowAllPortsForUrls,
    base::Value::Type::LIST },
  { key::kSerialAskForUrls,
    prefs::kManagedSerialAskForUrls,
    base::Value::Type::LIST },
  { key::kSerialBlockedForUrls,
    prefs::kManagedSerialBlockedForUrls,
    base::Value::Type::LIST },
  { key::kSharedArrayBufferUnrestrictedAccessAllowed,
    prefs::kSharedArrayBufferUnrestrictedAccessAllowed,
    base::Value::Type::BOOLEAN },
  { key::kShowCastIconInToolbar,
    prefs::kShowCastIconInToolbar,
    base::Value::Type::BOOLEAN },
  { key::kShowCastSessionsStartedByOtherDevices,
    media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
    base::Value::Type::BOOLEAN },
  { key::kShowFullUrlsInAddressBar,
    omnibox::kPreventUrlElisionsInOmnibox,
    base::Value::Type::BOOLEAN },
  { key::kShowHomeButton,
    prefs::kShowHomeButton,
    base::Value::Type::BOOLEAN },
  { key::kSignedHTTPExchangeEnabled,
    prefs::kSignedHTTPExchangeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSitePerProcess,
    prefs::kSitePerProcess,
    base::Value::Type::BOOLEAN },
#if BUILDFLAG(ENABLE_SPELLCHECK)
  { key::kSpellCheckServiceEnabled,
    spellcheck::prefs::kSpellCheckUseSpellingService,
    base::Value::Type::BOOLEAN },
  { key::kSpellcheckEnabled,
    spellcheck::prefs::kSpellCheckEnable,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
  { key::kSuppressUnsupportedOSWarning,
    prefs::kSuppressUnsupportedOSWarning,
    base::Value::Type::BOOLEAN },
  { key::kTabCaptureAllowedByOrigins,
    prefs::kTabCaptureAllowedByOrigins,
    base::Value::Type::LIST },
  { key::kTaskManagerEndProcessEnabled,
    prefs::kTaskManagerEndProcessEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUserFeedbackAllowed,
    prefs::kUserFeedbackAllowed,
    base::Value::Type::BOOLEAN },
  { key::kVideoCaptureAllowed,
    prefs::kVideoCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kVideoCaptureAllowedUrls,
    prefs::kVideoCaptureAllowedUrls,
    base::Value::Type::LIST },
  { key::kWPADQuickCheckEnabled,
    prefs::kQuickCheckEnabled,
    base::Value::Type::BOOLEAN },
  { key::kWebAuthenticationRemoteProxiedRequestsAllowed,
    webauthn::pref_names::kRemoteProxiedRequestsAllowed,
    base::Value::Type::BOOLEAN },
  { key::kWebHidAllowAllDevicesForUrls,
    prefs::kManagedWebHidAllowAllDevicesForUrls,
    base::Value::Type::LIST },
  { key::kWebHidAskForUrls,
    prefs::kManagedWebHidAskForUrls,
    base::Value::Type::LIST },
  { key::kWebHidBlockedForUrls,
    prefs::kManagedWebHidBlockedForUrls,
    base::Value::Type::LIST },
  { key::kWebRtcEventLogCollectionAllowed,
    prefs::kWebRtcEventLogCollectionAllowed,
    base::Value::Type::BOOLEAN },
  { key::kWebRtcIPHandling,
    prefs::kWebRTCIPHandlingPolicy,
    base::Value::Type::STRING },
  { key::kWebRtcLocalIpsAllowedUrls,
    prefs::kWebRtcLocalIpsAllowedUrls,
    base::Value::Type::LIST },
  { key::kWebRtcTextLogCollectionAllowed,
    prefs::kWebRtcTextLogCollectionAllowed,
    base::Value::Type::BOOLEAN },
  { key::kWindowCaptureAllowedByOrigins,
    prefs::kWindowCaptureAllowedByOrigins,
    base::Value::Type::LIST },
#endif // BUILDFLAG(IS_ANDROID)
  { key::kAlternateErrorPagesEnabled,
    embedder_support::kAlternateErrorPagesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBuiltInDnsClientEnabled,
    prefs::kBuiltInDnsClientEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAdditionalDnsQueryTypesEnabled,
    prefs::kAdditionalDnsQueryTypesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingExtendedReportingEnabled,
    prefs::kSafeBrowsingScoutReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kForceGoogleSafeSearch,
    policy_prefs::kForceGoogleSafeSearch,
    base::Value::Type::BOOLEAN },
#if BUILDFLAG(IS_CHROMEOS)
  { key::kEssentialSearchEnabled,
    prefs::kEssentialSearchEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins,
    prefs::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins,
    base::Value::Type::LIST },
#endif  // BUILDFLAG(IS_CHROMEOS)
  { key::kForceYouTubeRestrict,
    policy::policy_prefs::kForceYouTubeRestrict,
    base::Value::Type::INTEGER },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultJavaScriptJitSetting,
    prefs::kManagedDefaultJavaScriptJitSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultJavaScriptOptimizerSetting,
    prefs::kManagedDefaultJavaScriptOptimizerSetting,
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
  { key::kJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    base::Value::Type::LIST },
  { key::kJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    base::Value::Type::LIST },
  { key::kJavaScriptJitAllowedForSites,
    prefs::kManagedJavaScriptJitAllowedForSites,
    base::Value::Type::LIST },
  { key::kJavaScriptJitBlockedForSites,
    prefs::kManagedJavaScriptJitBlockedForSites,
    base::Value::Type::LIST },
  { key::kJavaScriptOptimizerAllowedForSites,
    prefs::kManagedJavaScriptOptimizerAllowedForSites,
    base::Value::Type::LIST },
  { key::kJavaScriptOptimizerBlockedForSites,
    prefs::kManagedJavaScriptOptimizerBlockedForSites,
    base::Value::Type::LIST },
  { key::kLegacySameSiteCookieBehaviorEnabledForDomainList,
    prefs::kManagedLegacyCookieAccessAllowedForDomains,
    base::Value::Type::LIST },
  { key::kInsecurePrivateNetworkRequestsAllowedForUrls,
    prefs::kManagedInsecurePrivateNetworkAllowedForUrls,
    base::Value::Type::LIST },
  { key::kDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting,
    base::Value::Type::INTEGER },
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) \
     || BUILDFLAG(IS_MAC)
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingSurveysEnabled,
    prefs::kSafeBrowsingSurveysEnabled,
    base::Value::Type::BOOLEAN },
#endif  // #if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
        // || BUILDFLAG(IS_WIN)  || BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#endif  // #if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
        // || BUILDFLAG(IS_WIN)
#if !BUILDFLAG(IS_ANDROID)
  { key::kAutomaticFullscreenAllowedForUrls,
    prefs::kManagedAutomaticFullscreenAllowedForUrls,
    base::Value::Type::LIST },
  { key::kAutomaticFullscreenBlockedForUrls,
    prefs::kManagedAutomaticFullscreenBlockedForUrls,
    base::Value::Type::LIST },
#endif  // !BUILDFLAG(IS_ANDROID)
  { key::kAuthSchemes,
    prefs::kAuthSchemes,
    base::Value::Type::STRING },
  { key::kDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup,
    base::Value::Type::BOOLEAN },
  { key::kAuthNegotiateDelegateAllowlist,
    prefs::kAuthNegotiateDelegateAllowlist,
    base::Value::Type::STRING },
  { key::kAuthServerAllowlist,
    prefs::kAuthServerAllowlist,
    base::Value::Type::STRING },
  { key::kPromptForDownloadLocation,
    prefs::kPromptForDownload,
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

  { key::kDefaultWebBluetoothGuardSetting,
    prefs::kManagedDefaultWebBluetoothGuardSetting,
    base::Value::Type::INTEGER },
  { key::kEncryptedClientHelloEnabled,
    prefs::kEncryptedClientHelloEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPostQuantumKeyAgreementEnabled,
    prefs::kPostQuantumKeyAgreementEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSSLErrorOverrideAllowed,
    prefs::kSSLErrorOverrideAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSSLErrorOverrideAllowedForOrigins,
    prefs::kSSLErrorOverrideAllowedForOrigins,
    base::Value::Type::LIST },
  { key::kAllowedDomainsForApps,
    prefs::kAllowedDomainsForApps,
    base::Value::Type::STRING },
  { key::kEnableMediaRouter,
    prefs::kEnableMediaRouter,
    base::Value::Type::BOOLEAN },
  { key::kWebRtcUdpPortRange,
    prefs::kWebRTCUDPPortRange,
    base::Value::Type::STRING },
  { key::kDefaultWebUsbGuardSetting,
    prefs::kManagedDefaultWebUsbGuardSetting,
    base::Value::Type::INTEGER },
  { key::kWebUsbAskForUrls,
    prefs::kManagedWebUsbAskForUrls,
    base::Value::Type::LIST },
  { key::kWebUsbBlockedForUrls,
    prefs::kManagedWebUsbBlockedForUrls,
    base::Value::Type::LIST },
  { key::kCoalesceH2ConnectionsWithClientCertificatesForHosts,
    prefs::kH2ClientCertCoalescingHosts,
    base::Value::Type::LIST },
  { key::kEnterpriseHardwarePlatformAPIEnabled,
    prefs::kEnterpriseHardwarePlatformAPIEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPasswordLeakDetectionEnabled,
    password_manager::prefs::kPasswordLeakDetectionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPaymentMethodQueryEnabled,
    payments::kCanMakePaymentEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeSitesFilterBehavior,
    policy_prefs::kSafeSitesFilterBehavior,
    base::Value::Type::INTEGER },

#if !BUILDFLAG(IS_CHROMEOS)
  { key::kChromeVariations,
    variations::prefs::kVariationsRestrictionsByPolicy,
    base::Value::Type::INTEGER },
  { key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVariationsRestrictParameter,
    variations::prefs::kVariationsRestrictParameter,
    base::Value::Type::STRING },
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  { key::kKerberosEnabled,
    prefs::kKerberosEnabled,
    base::Value::Type::BOOLEAN },
  { key::kMandatoryExtensionsForIncognitoNavigation,
    prefs::kMandatoryExtensionsForIncognitoNavigation,
    base::Value::Type::LIST },
  { key::kReportWebsiteActivityAllowlist,
    ::reporting::kReportWebsiteActivityAllowlist,
    base::Value::Type::LIST },
  { key::kReportWebsiteTelemetryAllowlist,
    ::reporting::kReportWebsiteTelemetryAllowlist,
    base::Value::Type::LIST },
  { key::kReportWebsiteTelemetry,
    ::reporting::kReportWebsiteTelemetry,
    base::Value::Type::LIST },
  { key::kReportWebsiteTelemetryCollectionRateMs,
    ::reporting::kReportWebsiteTelemetryCollectionRateMs,
    base::Value::Type::INTEGER },
  { key::kMicrosoftOfficeCloudUpload,
    prefs::kMicrosoftOfficeCloudUpload,
    base::Value::Type::STRING },
  { key::kGoogleWorkspaceCloudUpload,
    prefs::kGoogleWorkspaceCloudUpload,
    base::Value::Type::STRING},
    { key::kMicrosoftOneDriveMount,
    prefs::kMicrosoftOneDriveMount,
    base::Value::Type::STRING},
  { key::kMicrosoftOneDriveAccountRestrictions,
    prefs::kMicrosoftOneDriveAccountRestrictions,
    base::Value::Type::LIST},
  { key::kExtensionOAuthRedirectUrls,
    extensions::pref_names::kOAuthRedirectUrls,
    base::Value::Type::DICT },
  { key::kQuickOfficeForceFileDownloadEnabled,
    quickoffice::kQuickOfficeForceFileDownloadEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAlwaysOnVpnPreConnectUrlAllowlist,
    policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist,
    base::Value::Type::LIST },
  { key::kDefaultWebPrintingSetting,
    prefs::kManagedDefaultWebPrintingSetting,
    base::Value::Type::INTEGER },
  { key::kWebPrintingAllowedForUrls,
    prefs::kManagedWebPrintingAllowedForUrls,
    base::Value::Type::LIST },
  { key::kWebPrintingBlockedForUrls,
    prefs::kManagedWebPrintingBlockedForUrls,
    base::Value::Type::LIST },
  { key::kFloatingWorkspaceEnabled,
    policy_prefs::kFloatingWorkspaceEnabled,
    base::Value::Type::BOOLEAN },
  { key::kFloatingSsoEnabled,
    prefs::kFloatingSsoEnabled,
    base::Value::Type::BOOLEAN },
  { key::kFloatingSsoDomainBlocklist,
    prefs::kFloatingSsoDomainBlocklist,
    base::Value::Type::LIST },
  { key::kFloatingSsoDomainBlocklistExceptions,
    prefs::kFloatingSsoDomainBlocklistExceptions,
    base::Value::Type::LIST },
  { key::kDeviceAllowEnterpriseRemoteAccessConnections,
    prefs::kDeviceAllowEnterpriseRemoteAccessConnections,
    base::Value::Type::BOOLEAN
  },
  { key::kDevicePostQuantumKeyAgreementEnabled,
    prefs::kDevicePostQuantumKeyAgreementEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kChromeOsLockOnIdleSuspend,
    ash::prefs::kEnableAutoScreenLock,
    base::Value::Type::BOOLEAN },
  { key::kDriveDisabled,
    drive::prefs::kDisableDrive,
    base::Value::Type::BOOLEAN },
  { key::kDriveDisabledOverCellular,
    drive::prefs::kDisableDriveOverCellular,
    base::Value::Type::BOOLEAN },
  { key::kEmojiSuggestionEnabled,
    ash::prefs::kEmojiSuggestionEnterpriseAllowed,
    base::Value::Type::BOOLEAN },
  { key::kEmojiPickerGifSupportEnabled,
    ash::prefs::kEmojiPickerGifSupportEnabled,
    base::Value::Type::BOOLEAN },
  { key::kExternalStorageDisabled,
    disks::prefs::kExternalStorageDisabled,
    base::Value::Type::BOOLEAN },
  { key::kExternalStorageReadOnly,
    disks::prefs::kExternalStorageReadOnly,
    base::Value::Type::BOOLEAN },
  { key::kAudioOutputAllowed,
    ash::prefs::kAudioOutputAllowed,
    base::Value::Type::BOOLEAN },
  { key::kShowLogoutButtonInTray,
    ash::prefs::kShowLogoutButtonInTray,
    base::Value::Type::BOOLEAN },
  { key::kSuggestLogoutAfterClosingLastWindow,
    ash::prefs::kSuggestLogoutAfterClosingLastWindow,
    base::Value::Type::BOOLEAN },
  { key::kShelfAutoHideBehavior,
    ash::prefs::kShelfAutoHideBehaviorLocal,
    base::Value::Type::STRING },
  { key::kShelfAlignment,
    ash::prefs::kShelfAlignmentLocal,
    base::Value::Type::STRING },
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
    ash::prefs::kPrimaryMouseButtonRight,
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
    base::Value::Type::DICT },
  { key::kOrcaEnabled,
    ash::prefs::kManagedOrcaEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPhysicalKeyboardAutocorrect,
    ash::prefs::kManagedPhysicalKeyboardAutocorrectAllowed,
    base::Value::Type::BOOLEAN },
  { key::kPhysicalKeyboardPredictiveWriting,
    ash::prefs::kManagedPhysicalKeyboardPredictiveWritingAllowed,
    base::Value::Type::BOOLEAN },
  { key::kShortcutCustomizationAllowed,
    ash::prefs::kShortcutCustomizationAllowed,
    base::Value::Type::BOOLEAN },
  { key::kStickyKeysEnabled,
    ash::prefs::kAccessibilityStickyKeysEnabled,
    base::Value::Type::BOOLEAN },
  { key::kColorCorrectionEnabled,
    ash::prefs::kAccessibilityColorCorrectionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kFullscreenAlertEnabled,
    ash::prefs::kFullscreenAlertEnabled,
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
    ash::prefs::kOwnerPrimaryMouseButtonRight,
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
  { key::kDeviceLoginScreenTouchVirtualKeyboardEnabled,
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
  // Note that this pref exists in both user PrefStore and local_state
  // PrefStore, and it is intended that the device policy is mapped to
  // both. See the comment at the definition of
  // ash::prefs::kPersonalizationKeyboardBacklightColor for details.
  { key::kDeviceKeyboardBacklightColor,
    ash::prefs::kPersonalizationKeyboardBacklightColor,
    base::Value::Type::INTEGER },
  { key::kRebootAfterUpdate,
    prefs::kRebootAfterUpdate,
    base::Value::Type::BOOLEAN },
  { key::kChromeOsMultiProfileUserBehavior,
    user_manager::prefs::kMultiProfileUserBehaviorPref,
    base::Value::Type::STRING },
  { key::kKeyboardDefaultToFunctionKeys,
    ash::prefs::kSendFunctionKeys,
    base::Value::Type::BOOLEAN },
  { key::kTouchVirtualKeyboardEnabled,
    prefs::kTouchVirtualKeyboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVirtualKeyboardSmartVisibilityEnabled,
    prefs::kVirtualKeyboardSmartVisibilityEnabled,
    base::Value::Type::BOOLEAN },
  { key::kEasyUnlockAllowed,
    ash::multidevice_setup::kSmartLockAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kInstantTetheringAllowed,
    ash::multidevice_setup::kInstantTetheringAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kSmsMessagesAllowed,
    ash::multidevice_setup::kMessagesAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kPhoneHubAllowed,
    ash::multidevice_setup::kPhoneHubAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kPhoneHubCameraRollAllowed,
    ash::multidevice_setup::kPhoneHubCameraRollAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kPhoneHubNotificationsAllowed,
    ash::multidevice_setup::kPhoneHubNotificationsAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kPhoneHubTaskContinuationAllowed,
    ash::multidevice_setup::kPhoneHubTaskContinuationAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kWifiSyncAndroidAllowed,
    ash::multidevice_setup::kWifiSyncAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kEcheAllowed,
    ash::multidevice_setup::kEcheAllowedPrefName,
    base::Value::Type::BOOLEAN },
  { key::kCaptivePortalAuthenticationIgnoresProxy,
    chromeos::prefs::kCaptivePortalAuthenticationIgnoresProxy,
    base::Value::Type::BOOLEAN },
  { key::kUnifiedDesktopEnabledByDefault,
    prefs::kUnifiedDesktopEnabledByDefault,
    base::Value::Type::BOOLEAN },
  { key::kArcEnabled,
    arc::prefs::kArcEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUnaffiliatedDeviceArcAllowed,
    arc::prefs::kUnaffiliatedDeviceArcAllowed,
    base::Value::Type::BOOLEAN },
  { key::kReportArcStatusEnabled,
    prefs::kReportArcStatusEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSchedulerConfiguration,
    prefs::kSchedulerConfiguration,
    base::Value::Type::STRING },
  { key::kExternalPrintServersAllowlist,
    prefs::kExternalPrintServersAllowlist,
    base::Value::Type::LIST },
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
  { key::kNetworkFileSharesAllowed,
    prefs::kNetworkFileSharesAllowed,
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
    base::Value::Type::DICT },
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
    ash::assistant::prefs::kAssistantOnboardingMode,
    base::Value::Type::STRING },
  { key::kAssistantVoiceMatchEnabledDuringOobe,
    ash::assistant::prefs::kAssistantVoiceMatchEnabledDuringOobe,
    base::Value::Type::BOOLEAN },
  { key::kVoiceInteractionContextEnabled,
    ash::assistant::prefs::kAssistantContextEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVoiceInteractionHotwordEnabled,
    ash::assistant::prefs::kAssistantHotwordEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDevicePowerPeakShiftEnabled,
    ash::prefs::kPowerPeakShiftEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDevicePowerPeakShiftBatteryThreshold,
    ash::prefs::kPowerPeakShiftBatteryThreshold,
    base::Value::Type::INTEGER },
  { key::kDevicePowerPeakShiftDayConfig,
    ash::prefs::kPowerPeakShiftDayConfig,
    base::Value::Type::DICT },
  { key::kDeviceBootOnAcEnabled,
    ash::prefs::kBootOnAcEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSamlInSessionPasswordChangeEnabled,
    ash::prefs::kSamlInSessionPasswordChangeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSamlPasswordExpirationAdvanceWarningDays,
    ash::prefs::kSamlPasswordExpirationAdvanceWarningDays,
    base::Value::Type::INTEGER },
  { key::kLockScreenReauthenticationEnabled,
    ash::prefs::kLockScreenReauthenticationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kLockScreenAutoStartOnlineReauth,
    prefs::kLockScreenAutoStartOnlineReauth,
    base::Value::Type::BOOLEAN },
  { key::kDeviceAdvancedBatteryChargeModeEnabled,
    ash::prefs::kAdvancedBatteryChargeModeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceAdvancedBatteryChargeModeDayConfig,
    ash::prefs::kAdvancedBatteryChargeModeDayConfig,
    base::Value::Type::DICT },
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
  { key::kKerberosRememberPasswordEnabled,
    prefs::kKerberosRememberPasswordEnabled,
    base::Value::Type::BOOLEAN },
  { key::kKerberosAddAccountsAllowed,
    prefs::kKerberosAddAccountsAllowed,
    base::Value::Type::BOOLEAN },
  { key::kKerberosDomainAutocomplete,
    prefs::kKerberosDomainAutocomplete,
    base::Value::Type::STRING },
  { key::kKerberosUseCustomPrefilledConfig,
    prefs::kKerberosUseCustomPrefilledConfig,
    base::Value::Type::BOOLEAN },
  { key::kKerberosCustomPrefilledConfig,
    prefs::kKerberosCustomPrefilledConfig,
    base::Value::Type::STRING },
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
    ash::prefs::kDeviceWiFiFastTransitionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNetworkThrottlingEnabled,
    prefs::kNetworkThrottlingEnabled,
    base::Value::Type::DICT },
  { key::kAllowScreenLock,
    ash::prefs::kAllowScreenLock,
    base::Value::Type::BOOLEAN },
  { key::kQuickUnlockTimeout,
    ash::prefs::kQuickUnlockTimeout,
    base::Value::Type::INTEGER },
  { key::kPinUnlockMinimumLength,
    ash::prefs::kPinUnlockMinimumLength,
    base::Value::Type::INTEGER },
  { key::kPinUnlockMaximumLength,
    ash::prefs::kPinUnlockMaximumLength,
    base::Value::Type::INTEGER },
  { key::kPinUnlockWeakPinsAllowed,
    ash::prefs::kPinUnlockWeakPinsAllowed,
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
    ash::prefs::kLoginDisplayPasswordButtonEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeletePrintJobHistoryAllowed,
    prefs::kDeletePrintJobHistoryAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSuggestedContentEnabled,
    ash::prefs::kSuggestedContentEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRequiredClientCertificateForUser,
    prefs::kRequiredClientCertificateForUser,
    base::Value::Type::LIST },
  { key::kRequiredClientCertificateForDevice,
    prefs::kRequiredClientCertificateForDevice,
    base::Value::Type::LIST },
  { key::kSecurityTokenSessionBehavior,
    prefs::kSecurityTokenSessionBehavior,
    base::Value::Type::STRING },
  { key::kSecurityTokenSessionNotificationSeconds,
    prefs::kSecurityTokenSessionNotificationSeconds,
    base::Value::Type::INTEGER },
  { key::kDeviceAllowMGSToStoreDisplayProperties,
    ash::prefs::kAllowMGSToStoreDisplayProperties,
    base::Value::Type::BOOLEAN },
  { key::kDeviceEphemeralNetworkPoliciesEnabled,
    ash::prefs::kDeviceEphemeralNetworkPoliciesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSystemFeaturesDisableMode,
    policy::policy_prefs::kSystemFeaturesDisableMode,
    base::Value::Type::STRING },
  { key::kDeviceSystemWideTracingEnabled,
    ash::prefs::kDeviceSystemWideTracingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUserBorealisAllowed,
    borealis::prefs::kBorealisAllowedForUser,
    base::Value::Type::BOOLEAN },
  { key::kDevicePciPeripheralDataAccessEnabled,
    ash::prefs::kLocalStateDevicePeripheralDataAccessEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceI18nShortcutsEnabled,
    ash::prefs::kDeviceI18nShortcutsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kArcAppToWebAppSharingEnabled,
    apps::webapk_prefs::kGeneratedWebApksEnabled,
    base::Value::Type::BOOLEAN},
  { key::kEnhancedNetworkVoicesInSelectToSpeakAllowed,
    ash::prefs::kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed,
    base::Value::Type::BOOLEAN },
  { key::kFullRestoreEnabled,
    ash::full_restore::kRestoreAppsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kGhostWindowEnabled,
    ash::full_restore::kGhostWindowEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeskTemplatesEnabled,
    ash::prefs::kDeskTemplatesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kQuickAnswersEnabled,
    quick_answers::prefs::kQuickAnswersEnabled,
    base::Value::Type::BOOLEAN },
  { key::kQuickAnswersDefinitionEnabled,
    quick_answers::prefs::kQuickAnswersDefinitionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kQuickAnswersTranslationEnabled,
    quick_answers::prefs::kQuickAnswersTranslationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kQuickAnswersUnitConversionEnabled,
    quick_answers::prefs::kQuickAnswersUnitConversionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kProjectorEnabled,
    ash::prefs::kProjectorAllowByPolicy,
    base::Value::Type::BOOLEAN },
  { key::kProjectorDogfoodForFamilyLinkEnabled,
    ash::prefs::kProjectorDogfoodForFamilyLinkEnabled,
    base::Value::Type::BOOLEAN },
  { key::kFloatingWorkspaceV2Enabled,
    ash::prefs::kFloatingWorkspaceV2Enabled,
    base::Value::Type::BOOLEAN },
  { key::kDevicePowerAdaptiveChargingEnabled,
    ash::prefs::kPowerAdaptiveChargingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCalendarIntegrationEnabled,
    ash::prefs::kCalendarIntegrationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kTrashEnabled,
    ash::prefs::kFilesAppTrashEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUsbDetectorNotificationEnabled,
    ash::prefs::kUsbDetectorNotificationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowAiIntroScreenEnabled,
    ash::prefs::kShowAiIntroScreenEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowTouchpadScrollScreenEnabled,
    ash::prefs::kShowTouchpadScrollScreenEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowGeminiIntroScreenEnabled,
    ash::prefs::kShowGeminiIntroScreenEnabled,
    base::Value::Type::BOOLEAN },
  { key::kWallpaperGooglePhotosIntegrationEnabled,
    wallpaper_handlers::prefs::kWallpaperGooglePhotosIntegrationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kScreensaverLockScreenEnabled,
    ash::ambient::prefs::kAmbientModeManagedScreensaverEnabled,
    base::Value::Type::BOOLEAN },
    { key::kScreensaverLockScreenIdleTimeoutSeconds,
    ash::ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
    base::Value::Type::INTEGER },
    { key::kScreensaverLockScreenImageDisplayIntervalSeconds,
    ash::ambient::prefs::kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
    base::Value::Type::INTEGER },
  { key::kScreensaverLockScreenImages,
    ash::ambient::prefs::kAmbientModeManagedScreensaverImages,
    base::Value::Type::LIST },
  { key::kUserAvatarCustomizationSelectorsEnabled,
    ash::user_image::prefs::kUserAvatarCustomizationSelectorsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowDisplaySizeScreenEnabled,
    ash::prefs::kShowDisplaySizeScreenEnabled,
    base::Value::Type::BOOLEAN },
  { key::kReportAppInventory,
    ash::reporting::kReportAppInventory,
    base::Value::Type::LIST },
  { key::kReportAppUsage,
    ash::reporting::kReportAppUsage,
    base::Value::Type::LIST },
  { key::kReportAppUsageCollectionRateMs,
    ash::reporting::kReportAppUsageCollectionRateMs,
    base::Value::Type::INTEGER },
  { key::kDeviceChargingSoundsEnabled,
    ash::prefs::kChargingSoundsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLowBatterySoundEnabled,
    ash::prefs::kLowBatterySoundEnabled,
    base::Value::Type::BOOLEAN },
  { key::kArcVmDataMigrationStrategy,
    arc::prefs::kArcVmDataMigrationStrategy,
    base::Value::Type::INTEGER },
  { key::kFullRestoreMode,
    ash::prefs::kRestoreAppsAndPagesPrefName,
    base::Value::Type::INTEGER },
  { key::kDeviceSwitchFunctionKeysBehaviorEnabled,
    ash::prefs::kDeviceSwitchFunctionKeysBehaviorEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSystemShortcutBehavior,
    ash::prefs::kSystemShortcutBehavior,
    base::Value::Type::INTEGER },
  { key::kShowHumanPresenceSensorScreenEnabled,
    ash::prefs::kShowHumanPresenceSensorScreenEnabled,
    base::Value::Type::BOOLEAN },
  { key::kUserFeedbackWithLowLevelDebugDataAllowed,
    ash::prefs::kUserFeedbackWithLowLevelDebugDataAllowed,
    base::Value::Type::LIST },
  { key::kHeartbeatEnabled,
    ash::kHeartbeatEnabled,
    base::Value::Type::BOOLEAN },
  { key::kF11KeyModifier,
    ash::prefs::kF11KeyModifier,
    base::Value::Type::INTEGER },
  { key::kF12KeyModifier,
    ash::prefs::kF12KeyModifier,
    base::Value::Type::INTEGER },
  { key::kHomeAndEndKeysModifier,
    ash::prefs::kHomeAndEndKeysModifier,
    base::Value::Type::INTEGER },
  { key::kDeleteKeyModifier,
    ash::prefs::kDeleteKeyModifier,
    base::Value::Type::INTEGER },
  { key::kPageUpAndPageDownKeysModifier,
    ash::prefs::kPageUpAndPageDownKeysModifier,
    base::Value::Type::INTEGER },
  { key::kInsertKeyModifier,
    ash::prefs::kInsertKeyModifier,
    base::Value::Type::INTEGER },
  { key::kGoogleLocationServicesEnabled,
    ash::prefs::kUserGeolocationAccessLevel,
    base::Value::Type::INTEGER },
  { key::kLocalUserFilesAllowed,
    prefs::kLocalUserFilesAllowed,
    base::Value::Type::BOOLEAN },
  { key::kKioskVisionTelemetryEnabled,
    ash::prefs::kKioskVisionTelemetryEnabled,
    base::Value::Type::BOOLEAN },
  { key::kGenAIWallpaperSettings,
    ash::prefs::kGenAIWallpaperSettings,
    base::Value::Type::INTEGER },
  { key::kGenAIVcBackgroundSettings,
    ash::prefs::kGenAIVcBackgroundSettings,
    base::Value::Type::INTEGER },
  { key::kLocalUserFilesMigrationDestination,
    prefs::kLocalUserFilesMigrationDestination,
    base::Value::Type::STRING },
  { key::kFocusModeSoundsEnabled,
    ash::prefs::kFocusModeSoundsEnabled,
    base::Value::Type::STRING },
  { key::kDnsOverHttpsExcludedDomains,
    ash::prefs::kDnsOverHttpsExcludedDomains,
    base::Value::Type::LIST },
  { key::kDnsOverHttpsIncludedDomains,
    ash::prefs::kDnsOverHttpsIncludedDomains,
    base::Value::Type::LIST },
  { key::kHelpMeReadSettings,
    ash::prefs::kHmrManagedSettings,
    base::Value::Type::INTEGER},
  { key::kAllowExcludeDisplayInMirrorMode,
    prefs::kAllowExcludeDisplayInMirrorMode,
    base::Value::Type::BOOLEAN},
  { key::kArcOpenLinksInBrowserByDefault,
    arc::prefs::kArcOpenLinksInBrowserByDefault,
    base::Value::Type::BOOLEAN },
#endif // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX)
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    base::Value::Type::STRING },
#endif // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
  { key::kApplicationLocaleValue,
    language::prefs::kApplicationLocale,
    base::Value::Type::STRING },
  { key::kRendererCodeIntegrityEnabled,
    prefs::kRendererCodeIntegrityEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRendererAppContainerEnabled,
    prefs::kRendererAppContainerEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBoundSessionCredentialsEnabled,
    prefs::kBoundSessionCredentialsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBrowserLegacyExtensionPointsBlocked,
    prefs::kBlockBrowserLegacyExtensionPoints,
    base::Value::Type::BOOLEAN },
  { key::kDynamicCodeSettings,
    prefs::kDynamicCodeSettings,
    base::Value::Type::INTEGER },
  { key::kBrowserSwitcherUseIeSitelist,
    browser_switcher::prefs::kUseIeSitelist,
    base::Value::Type::BOOLEAN },
  { key::kBrowserSwitcherChromePath,
    browser_switcher::prefs::kChromePath,
    base::Value::Type::STRING },
  { key::kBrowserSwitcherChromeParameters,
    browser_switcher::prefs::kChromeParameters,
    base::Value::Type::LIST },
#if BUILDFLAG(ENABLE_PRINTING)
  { key::kPrintPostScriptMode,
    prefs::kPrintPostScriptMode,
    base::Value::Type::INTEGER },
  { key::kPrintRasterizationMode,
    prefs::kPrintRasterizationMode,
    base::Value::Type::INTEGER },
#endif
  { key::kSafeBrowsingForTrustedSourcesEnabled,
    prefs::kSafeBrowsingForTrustedSourcesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kWindowOcclusionEnabled,
    policy::policy_prefs::kNativeWindowOcclusionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNativeHostsExecutablesLaunchDirectly,
    prefs::kNativeHostsExecutablesLaunchDirectly,
    base::Value::Type::BOOLEAN },
  { key::kApplicationBoundEncryptionEnabled,
    prefs::kApplicationBoundEncryptionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintingLPACSandboxEnabled,
    prefs::kPrintingLPACSandboxEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  { key::kNetworkServiceSandboxEnabled,
    prefs::kNetworkServiceSandboxEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if  !BUILDFLAG(IS_WIN)
  { key::kNtlmV2Enabled,
    prefs::kNtlmV2Enabled,
    base::Value::Type::BOOLEAN },
#endif  // !BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  { key::kThirdPartyBlockingEnabled,
    prefs::kThirdPartyBlockingEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  { key::kPrintPdfAsImageAvailability,
    prefs::kPrintPdfAsImageAvailability,
    base::Value::Type::BOOLEAN },
  { key::kTotalMemoryLimitMb,
    prefs::kTotalMemoryLimitMb,
    base::Value::Type::INTEGER },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  { key::kUnmanagedDeviceSignalsConsentFlowEnabled,
    device_signals::prefs::kUnmanagedDeviceSignalsConsentFlowEnabled,
    base::Value::Type::BOOLEAN },
  { key::kProfileSeparationDomainExceptionList,
    prefs::kProfileSeparationDomainExceptionList,
    base::Value::Type::LIST },
#endif // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  { key::kDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRoamingProfileSupportEnabled,
    syncer::prefs::kEnableLocalSyncBackend,
    base::Value::Type::BOOLEAN },
  { key::kDesktopSharingHubEnabled,
    prefs::kDesktopSharingHubEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) \
    || BUILDFLAG(IS_CHROMEOS)
  { key::kAutoplayAllowed,
    prefs::kAutoplayAllowed,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        // || BUILDFLAG(IS_CHROMEOS)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
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

  { key::kRestrictSigninToPattern,
    prefs::kGoogleServicesUsernamePattern,
    base::Value::Type::STRING },
  { key::kHardwareAccelerationModeEnabled,
    prefs::kHardwareAccelerationModeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kForceEphemeralProfiles,
    prefs::kForceEphemeralProfiles,
    base::Value::Type::BOOLEAN },
  { key::kBrowserNetworkTimeQueriesEnabled,
    network_time::prefs::kNetworkTimeQueriesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kExternalProtocolDialogShowAlwaysOpenCheckbox,
    prefs::kExternalProtocolDialogShowAlwaysOpenCheckbox,
    base::Value::Type::BOOLEAN },
  { key::kAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs,
    base::Value::Type::BOOLEAN },
  { key::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
    base::Value::Type::BOOLEAN },
  { key::kCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
    base::Value::Type::INTEGER },
  { key::kCloudManagementEnrollmentMandatory,
    policy_prefs::kCloudManagementEnrollmentMandatory,
    base::Value::Type::BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    base::Value::Type::BOOLEAN },
  { key::kAlwaysOpenPdfExternally,
    prefs::kPluginsAlwaysOpenPdfExternally,
    base::Value::Type::BOOLEAN },
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::Type::BOOLEAN },
  { key::kPrintPreviewUseSystemDefaultPrinter,
    prefs::kPrintPreviewUseSystemDefaultPrinter,
    base::Value::Type::BOOLEAN },
  { key::kUserDataSnapshotRetentionLimit,
    prefs::kUserDataSnapshotRetentionLimit,
    base::Value::Type::INTEGER },
  { key::kCommandLineFlagSecurityWarningsEnabled,
    prefs::kCommandLineFlagSecurityWarningsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBrowserGuestModeEnforced,
    prefs::kBrowserGuestModeEnforced,
    base::Value::Type::BOOLEAN },
  { key::kSigninInterceptionEnabled,
    prefs::kSigninInterceptionEnabled,
    base::Value::Type::BOOLEAN },
#endif // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  { key::kAlternativeBrowserPath,
    browser_switcher::prefs::kAlternativeBrowserPath,
    base::Value::Type::STRING },
  { key::kAlternativeBrowserParameters,
    browser_switcher::prefs::kAlternativeBrowserParameters,
    base::Value::Type::LIST },
  { key::kBrowserSwitcherParsingMode,
    browser_switcher::prefs::kParsingMode,
    base::Value::Type::INTEGER },
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
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  { key::kDeskAPIThirdPartyAccessEnabled,
    prefs::kDeskAPIThirdPartyAccessEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeskAPIThirdPartyAllowlist,
    prefs::kDeskAPIThirdPartyAllowlist,
    base::Value::Type::LIST },
  { key::kDeskAPIDeskSaveAndShareEnabled,
    prefs::kDeskAPIDeskSaveAndShareEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceAttributesAllowedForOrigins,
    prefs::kDeviceAttributesAllowedForOrigins,
    base::Value::Type::LIST },
  { key::kKioskBrowserPermissionsAllowedForOrigins,
    prefs::kKioskBrowserPermissionsAllowedForOrigins,
    base::Value::Type::LIST },
  { key::kKioskWebAppOfflineEnabled,
    prefs::kKioskWebAppOfflineEnabled,
    base::Value::Type::BOOLEAN},
  { key::kDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate,
    base::Value::Type::INTEGER },
  { key::kLacrosSecondaryProfilesAllowed,
    prefs::kLacrosSecondaryProfilesAllowed,
    base::Value::Type::BOOLEAN },
  { key::kLacrosDataBackwardMigrationMode,
    prefs::kLacrosDataBackwardMigrationMode,
    base::Value::Type::STRING },
  { key::kClientCertificateManagementAllowed,
    prefs::kClientCertificateManagementAllowed,
    base::Value::Type::INTEGER },
  { key::kCACertificateManagementAllowed,
    prefs::kCACertificateManagementAllowed,
    base::Value::Type::INTEGER },
  { key::kDataLeakPreventionReportingEnabled,
    policy_prefs::kDlpReportingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDataLeakPreventionClipboardCheckSizeLimit,
    policy_prefs::kDlpClipboardCheckSizeLimit,
    base::Value::Type::INTEGER },
  { key::kForceMaximizeOnFirstRun,
    prefs::kForceMaximizeOnFirstRun,
    base::Value::Type::BOOLEAN },
  { key::kInsightsExtensionEnabled,
    prefs::kInsightsExtensionEnabled,
    base::Value::Type::BOOLEAN },
  { key::kEnableSyncConsent,
    prefs::kEnableSyncConsent,
    base::Value::Type::BOOLEAN },
  { key::kKeepFullscreenWithoutNotificationUrlAllowList,
    chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
    base::Value::Type::LIST },
  { key::kRestrictedManagedGuestSessionExtensionCleanupExemptList,
    prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList,
    base::Value::Type::LIST },
  { key::kNewWindowsInKioskAllowed,
    prefs::kNewWindowsInKioskAllowed,
    base::Value::Type::BOOLEAN },
  { key::kKioskTroubleshootingToolsEnabled,
    prefs::kKioskTroubleshootingToolsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
    prefs::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
    base::Value::Type::BOOLEAN },
  // We avoid checking for BUILDFLAG(ENABLE_NACL) since we may want the policy
  // to exist (deprecated) even if NACL is no longer being built.
  { key::kNativeClientForceAllowed,
    prefs::kNativeClientForceAllowed,
    base::Value::Type::BOOLEAN },
  { key::kKioskActiveWiFiCredentialsScopeChangeEnabled,
    prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled,
    base::Value::Type::BOOLEAN },
#endif // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/329064666): Remove this pref once the pivot to IWAs is complete.
  { key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
    capture_policy::kManagedAccessToGetAllScreensMediaAllowedForUrls,
    base::Value::Type::LIST },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  { key::kAuthNegotiateDelegateByKdcPolicy,
    prefs::kAuthNegotiateDelegateByKdcPolicy,
    base::Value::Type::BOOLEAN },
#endif // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kFullscreenAllowed,
    extensions::pref_names::kAppFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#endif  // !BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kBlockExternalExtensions,
    extensions::pref_names::kBlockExternalExtensions,
    base::Value::Type::BOOLEAN },
#endif // !BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kExtensionExtendedBackgroundLifetimeForPortConnectionsToUrls,
    extensions::pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls,
    base::Value::Type::LIST },
#endif // BUILDFLAG(ENABLE_EXTENSIONS)

  { key::kScrollToTextFragmentEnabled,
    prefs::kScrollToTextFragmentEnabled,
    base::Value::Type::BOOLEAN },
  { key::kIntensiveWakeUpThrottlingEnabled,
    policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kInsecureFormsWarningsEnabled,
    prefs::kMixedFormsWarningsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kLookalikeWarningAllowlistDomains,
    prefs::kLookalikeWarningAllowlistDomains,
    base::Value::Type::LIST },
  { key::kSuppressDifferentOriginSubframeDialogs,
    prefs::kSuppressDifferentOriginSubframeJSDialogs,
    base::Value::Type::BOOLEAN },

#if BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kPdfAnnotationsEnabled,
    prefs::kPdfAnnotationsEnabled,
    base::Value::Type::BOOLEAN },
  { key::kLensOnGalleryEnabled,
    prefs::kMediaAppLensEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  { key::kCORSNonWildcardRequestHeadersSupport,
    prefs::kCorsNonWildcardRequestHeadersSupport,
    base::Value::Type::BOOLEAN },
  { key::kUserAgentReduction,
    prefs::kUserAgentReduction,
    base::Value::Type::INTEGER},
#if BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kDeviceLoginScreenWebUILazyLoading,
    ash::prefs::kLoginScreenWebUILazyLoading,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_MAC)
  { key::kWarnBeforeQuittingEnabled,
    prefs::kConfirmToQuitEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCreatePasskeysInICloudKeychain,
    prefs::kCreatePasskeysInICloudKeychain,
    base::Value::Type::BOOLEAN },
  { key::kExtensibleEnterpriseSSOEnabled,
    prefs::kExtensibleEnterpriseSSOEnabled,
    base::Value::Type::INTEGER},
#endif
  { key::kAccessControlAllowMethodsInCORSPreflightSpecConformant,
    prefs::kAccessControlAllowMethodsInCORSPreflightSpecConformant,
    base::Value::Type::BOOLEAN},
#if BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kDeviceAutofillSAMLUsername,
    ash::prefs::kUrlParameterToAutofillSAMLUsername,
    base::Value::Type::STRING },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kTabDiscardingExceptions,
    performance_manager::user_tuning::prefs::kManagedTabDiscardingExceptions,
    base::Value::Type::LIST },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kStrictMimetypeCheckForWorkerScriptsEnabled,
    prefs::kStrictMimetypeCheckForWorkerScriptsEnabled,
    base::Value::Type::BOOLEAN},
#if BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kRecoveryFactorBehavior,
    ash::prefs::kRecoveryFactorBehavior,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kShoppingListEnabled,
    commerce::kShoppingListEnabledPrefName,
    base::Value::Type::BOOLEAN},
#if BUILDFLAG(IS_ANDROID)
  { key::kVirtualKeyboardResizesLayoutByDefault,
    prefs::kVirtualKeyboardResizesLayoutByDefault,
    base::Value::Type::BOOLEAN},
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  { key::kCloudAPAuthEnabled,
    prefs::kCloudApAuthEnabled,
    base::Value::Type::INTEGER },
  { key::kUiAutomationProviderEnabled,
    prefs::kUiAutomationProviderEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  { key::kOutOfProcessSystemDnsResolutionEnabled,
    prefs::kOutOfProcessSystemDnsResolutionEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#if !BUILDFLAG(IS_ANDROID)
  { key::kGoogleSearchSidePanelEnabled,
    prefs::kGoogleSearchSidePanelEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingDeepScanningEnabled,
    prefs::kSafeBrowsingDeepScanningEnabled,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(IS_ANDROID)
  { key::kAllowBackForwardCacheForCacheControlNoStorePageEnabled,
    policy_prefs::kAllowBackForwardCacheForCacheControlNoStorePageEnabled,
    base::Value::Type::BOOLEAN},
  { key::kMutationEventsEnabled,
    policy_prefs::kMutationEventsEnabled,
    base::Value::Type::BOOLEAN},
  { key::kCSSCustomStateDeprecatedSyntaxEnabled,
    policy_prefs::kCSSCustomStateDeprecatedSyntaxEnabled,
    base::Value::Type::BOOLEAN},
  { key::kBeforeunloadEventCancelByPreventDefaultEnabled,
    policy_prefs::kBeforeunloadEventCancelByPreventDefaultEnabled,
    base::Value::Type::BOOLEAN},
  { key::kKeyboardFocusableScrollersEnabled,
    policy_prefs::kKeyboardFocusableScrollersEnabled,
    base::Value::Type::BOOLEAN},
  { key::kStandardizedBrowserZoomEnabled,
    policy_prefs::kStandardizedBrowserZoomEnabled,
    base::Value::Type::BOOLEAN},
  { key::kIPv6ReachabilityOverrideEnabled,
    prefs::kIPv6ReachabilityOverrideEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrivateNetworkAccessRestrictionsEnabled,
    prefs::kManagedPrivateNetworkAccessRestrictionsEnabled,
    base::Value::Type::BOOLEAN },
#if !BUILDFLAG(IS_ANDROID)
  { key::kDevToolsGenAiSettings,
    prefs::kDevToolsGenAiSettings,
    base::Value::Type::INTEGER },
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  { key::kExtensionInstallTypeBlocklist,
    extensions::pref_names::kExtensionInstallTypeBlocklist,
    base::Value::Type::LIST},
#endif // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif // BUILDFLAG(ENABLE_EXTENSIONS)
#if !BUILDFLAG(IS_ANDROID)
  { key::kTabOrganizerSettings,
    optimization_guide::prefs::kTabOrganizationEnterprisePolicyAllowed,
    base::Value::Type::INTEGER},
  { key::kHelpMeWriteSettings,
    optimization_guide::prefs::kComposeEnterprisePolicyAllowed,
    base::Value::Type::INTEGER},
  { key::kCreateThemesSettings,
    optimization_guide::prefs::kWallpaperSearchEnterprisePolicyAllowed,
    base::Value::Type::INTEGER},
  { key::kHistorySearchSettings,
    optimization_guide::prefs::kHistorySearchEnterprisePolicyAllowed,
    base::Value::Type::INTEGER},
#endif
  { key::kTabCompareSettings,
    optimization_guide::prefs::kProductSpecificationsEnterprisePolicyAllowed,
    base::Value::Type::INTEGER},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  { key::kChromeForTestingAllowed,
    prefs::kChromeForTestingAllowed,
    base::Value::Type::BOOLEAN },
#endif

  { key::kCACertificates,
    prefs::kCACertificates,
    base::Value::Type::LIST },
  { key::kCACertificatesWithConstraints,
    prefs::kCACertificatesWithConstraints,
    base::Value::Type::LIST },
  { key::kCADistrustedCertificates,
    prefs::kCADistrustedCertificates,
    base::Value::Type::LIST },
  { key::kCAHintCertificates,
    prefs::kCAHintCertificates,
    base::Value::Type::LIST },
#if !BUILDFLAG(IS_CHROMEOS)
  { key::kCAPlatformIntegrationEnabled,
    prefs::kCAPlatformIntegrationEnabled,
    base::Value::Type::BOOLEAN },
#endif

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
  { key::kProvisionManagedClientCertificateForUser,
    client_certificates::prefs::kProvisionManagedClientCertificateForUserPrefs,
    base::Value::Type::INTEGER },
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#if !BUILDFLAG(IS_ANDROID)
  { key::kLensOverlaySettings,
    lens::prefs::kLensOverlaySettings,
    base::Value::Type::INTEGER},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  { key::kDeviceAuthenticationFlowAutoReloadInterval,
    ash::prefs::kAuthenticationFlowAutoReloadInterval,
    base::Value::Type::INTEGER },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
  { key::kAdHocCodeSigningForPWAsEnabled,
    prefs::kWebAppsUseAdHocCodeSigningForAppShims,
    base::Value::Type::BOOLEAN },
#endif // BUILDFLAG(IS_MAC)
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
            entry.name, std::make_unique<base::Value>(entry.manifest_type)));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Future policies are not supported on Stable and Beta by default.
bool AreFuturePoliciesEnabledByDefault() {
  // Enable future policies for branded browser tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    return true;
  }
  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
}

}  // namespace

void PopulatePolicyHandlerParameters(PolicyHandlerParameters* parameters) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_manager::UserManager::IsInitialized()) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (user) {
      parameters->user_id_hash = user->username_hash();
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

std::unique_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  std::unique_ptr<ConfigurationPolicyHandlerList> handlers(
      new ConfigurationPolicyHandlerList(
          base::BindRepeating(&PopulatePolicyHandlerParameters),
          base::BindRepeating(&GetChromePolicyDetails),
          AreFuturePoliciesEnabledByDefault()));
  for (PolicyToPreferenceMapEntry entry : kSimplePolicyMap) {
    handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
        entry.policy_name, entry.preference_path, entry.value_type));
  }

  // Policies for all platforms - Start
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kAutofillAddressEnabled, autofill::prefs::kAutofillProfileEnabled));
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kAutofillCreditCardEnabled,
      autofill::prefs::kAutofillCreditCardEnabled));
  handlers->AddHandler(std::make_unique<autofill::AutofillPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<enterprise_reporting::CloudReportingPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          enterprise_reporting::CloudReportingFrequencyPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<enterprise_reporting::LegacyTechReportPolicyHandler>());
  handlers->AddHandler(std::make_unique<DefaultSearchPolicyHandler>());
  handlers->AddHandler(std::make_unique<IncognitoModePolicyHandler>());
  handlers->AddHandler(
      std::make_unique<bookmarks::ManagedBookmarksPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(
      std::make_unique<safe_browsing::SafeBrowsingPolicyHandler>());
  handlers->AddHandler(std::make_unique<syncer::SyncPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<URLBlocklistPolicyHandler>(key::kURLBlocklist));

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kUrlKeyedAnonymizedDataCollectionEnabled,
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
          base::Value::Type::BOOLEAN),
      std::make_unique<BooleanDisablingPolicyHandler>(
          key::kUrlKeyedMetricsAllowed,
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)));
  // Policies for all platforms - End

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kMemorySaverModeSavings,
      performance_manager::user_tuning::prefs::kMemorySaverModeAggressiveness,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           MemorySaverModeAggressiveness::kConservative),
      static_cast<int>(performance_manager::user_tuning::prefs::
                           MemorySaverModeAggressiveness::kAggressive),
      false));
  handlers->AddHandler(
      std::make_unique<performance_manager::MemorySaverPolicyHandler>());
  // Note: This needs to be created after `DefaultSearchPolicyHandler`.
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(
      std::make_unique<SiteSearchPolicyHandler>(chrome_schema));

  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kGenAILocalFoundationalModelSettings,
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          optimization_guide::model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed),
      static_cast<int>(
          optimization_guide::model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::kMaxValue),
      false));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kEnterpriseRealTimeUrlCheckMode,
          enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
          enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
          chrome_schema));
#if BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(
      std::make_unique<ContextualSearchPolicyHandlerAndroid>());
#else  // !BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(std::make_unique<BrowsingHistoryPolicyHandler>());
  handlers->AddHandler(std::make_unique<BrowsingDataLifetimePolicyHandler>(
      key::kClearBrowsingDataOnExitList,
      browsing_data::prefs::kClearBrowsingDataOnExitList, chrome_schema));
  handlers->AddHandler(
      std::make_unique<enterprise_reporting::ExtensionRequestPolicyHandler>());
  handlers->AddHandler(std::make_unique<CopyPreventionSettingsPolicyHandler>(
      key::kCopyPreventionSettings,
      enterprise::content::kCopyPreventionSettings, chrome_schema));
  handlers->AddHandler(std::make_unique<DefaultDownloadDirPolicyHandler>());
  handlers->AddHandler(std::make_unique<DownloadDirPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kUserContextAwareAccessSignalsAllowlist,
          enterprise_connectors::kUserContextAwareAccessSignalsAllowlistPref,
          chrome_schema));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kPromotionalTabsEnabled,
                                            prefs::kPromotionsEnabled,
                                            base::Value::Type::BOOLEAN),
      std::make_unique<SimplePolicyHandler>(key::kPromotionsEnabled,
                                            prefs::kPromotionsEnabled,
                                            base::Value::Type::BOOLEAN)));

  handlers->AddHandler(std::make_unique<GuestModePolicyHandler>());
  handlers->AddHandler(std::make_unique<headless::HeadlessModePolicyHandler>());
  handlers->AddHandler(std::make_unique<DeveloperToolsPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<DownloadAutoOpenPolicyHandler>(chrome_schema));

  // Handlers for policies with embedded JSON strings. These handlers are very
  // lenient - as long as the root value is of the right type, they only display
  // warnings and never reject the policy value.
  handlers->AddHandler(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kAutoSelectCertificateForUrls,
          prefs::kManagedAutoSelectCertificateForUrls,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kDefaultPrinterSelection,
          prefs::kPrintPreviewDefaultDestinationSelectionRules,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      safe_browsing::file_type::prefs::
          kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kManagedConfigurationPerOrigin,
      prefs::kManagedConfigurationPerOrigin, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      std::make_unique<NtpCustomBackgroundEnabledPolicyHandler>());

  // Handlers for Chrome Enterprise Connectors policies.
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnBulkDataEntryEnterpriseConnector,
          enterprise_connectors::kOnBulkDataEntryPref,
          enterprise_connectors::kOnBulkDataEntryScopePref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnFileAttachedEnterpriseConnector,
          enterprise_connectors::kOnFileAttachedPref,
          enterprise_connectors::kOnFileAttachedScopePref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnFileDownloadedEnterpriseConnector,
          enterprise_connectors::kOnFileDownloadedPref,
          enterprise_connectors::kOnFileDownloadedScopePref, chrome_schema));
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnPrintEnterpriseConnector, enterprise_connectors::kOnPrintPref,
          enterprise_connectors::kOnPrintScopePref, chrome_schema));
#if BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnFileTransferEnterpriseConnector,
          enterprise_connectors::kOnFileTransferPref,
          enterprise_connectors::kOnFileTransferScopePref, chrome_schema));
#endif  // BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kOnSecurityEventEnterpriseConnector,
          enterprise_connectors::kOnSecurityEventPref,
          enterprise_connectors::kOnSecurityEventScopePref, chrome_schema));

  handlers->AddHandler(
      std::make_unique<web_app::WebAppSettingsPolicyHandler>(chrome_schema));

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  handlers->AddHandler(
      std::make_unique<PrintingAllowedBackgroundGraphicsModesPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<PrintingBackgroundGraphicsDefaultPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<PrintingPaperSizeDefaultPolicyHandler>());
#endif
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kRegisteredProtocolHandlers,
      custom_handlers::prefs::kPolicyRegisteredProtocolHandlers, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kRelaunchWindow, prefs::kRelaunchWindow, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

  handlers->AddHandler(std::make_unique<RestoreOnStartupPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kSerialAllowUsbDevicesForUrls,
      prefs::kManagedSerialAllowUsbDevicesForUrls, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kWebAppInstallForceList, prefs::kWebAppInstallForceList,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN_WITHOUT_WARNING,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<WebHidDevicePolicyHandler>(
      key::kWebHidAllowDevicesForUrls, prefs::kManagedWebHidAllowDevicesForUrls,
      chrome_schema));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  handlers->AddHandler(std::make_unique<WebHidDevicePolicyHandler>(
      key::kDeviceLoginScreenWebHidAllowDevicesForUrls,
      prefs::kManagedWebHidAllowDevicesForUrlsOnLoginScreen, chrome_schema));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  handlers->AddHandler(std::make_unique<WebHidDevicePolicyHandler>(
      key::kWebHidAllowDevicesWithHidUsagesForUrls,
      prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls, chrome_schema));

  // WindowPlacement policies to be deprecated and replaced by WindowManagement.
  // crbug.com/1328581
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kDefaultWindowPlacementSetting,
          prefs::kManagedDefaultWindowManagementSetting,
          base::Value::Type::INTEGER),
      std::make_unique<SimplePolicyHandler>(
          key::kDefaultWindowManagementSetting,
          prefs::kManagedDefaultWindowManagementSetting,
          base::Value::Type::INTEGER)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kWindowPlacementAllowedForUrls,
          prefs::kManagedWindowManagementAllowedForUrls,
          base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(
          key::kWindowManagementAllowedForUrls,
          prefs::kManagedWindowManagementAllowedForUrls,
          base::Value::Type::LIST)));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kWindowPlacementBlockedForUrls,
          prefs::kManagedWindowManagementBlockedForUrls,
          base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(
          key::kWindowManagementBlockedForUrls,
          prefs::kManagedWindowManagementBlockedForUrls,
          base::Value::Type::LIST)));
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(
      std::make_unique<enterprise_idle::IdleTimeoutPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<enterprise_idle::IdleTimeoutActionsPolicyHandler>(
          chrome_schema));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_ANDROID)

  handlers->AddHandler(
      std::make_unique<content_settings::CookieSettingsPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          content_settings::InsecurePrivateNetworkPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          enterprise_reporting::CloudProfileReportingPolicyHandler>());
  handlers->AddHandler(std::make_unique<ForceSafeSearchPolicyHandler>());
  handlers->AddHandler(std::make_unique<ForceYouTubeSafetyModePolicyHandler>());
  handlers->AddHandler(std::make_unique<HomepageLocationPolicyHandler>());
  handlers->AddHandler(std::make_unique<proxy_config::ProxyPolicyHandler>());
  handlers->AddHandler(std::make_unique<SecureDnsPolicyHandler>());
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
      key::kHSTSPolicyBypassList, prefs::kHSTSPolicyBypassList, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(
      std::make_unique<WebUsbAllowDevicesForUrlsPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<JavascriptPolicyHandler>());

  handlers->AddHandler(
      std::make_unique<ExplicitlyAllowedNetworkPortsPolicyHandler>());
  handlers->AddHandler(std::make_unique<HttpsOnlyModePolicyHandler>(
      prefs::kHttpsOnlyModeEnabled, prefs::kHttpsFirstModeIncognito,
      prefs::kHttpsFirstBalancedMode));

  handlers->AddHandler(std::make_unique<BrowsingDataLifetimePolicyHandler>(
      key::kBrowsingDataLifetime, browsing_data::prefs::kBrowsingDataLifetime,
      chrome_schema));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  handlers->AddHandler(std::make_unique<LocalSyncPolicyHandler>());
  handlers->AddHandler(std::make_unique<ThemeColorPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<
          enterprise_connectors::EnterpriseConnectorsPolicyHandler>(
          key::kBrowserContextAwareAccessSignalsAllowlist,
          enterprise_connectors::kBrowserContextAwareAccessSignalsAllowlistPref,
          chrome_schema));
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<ManagedAccountRestrictionsPolicyHandler>(chrome_schema),
      std::make_unique<SimplePolicyHandler>(key::kProfileSeparationSettings,
                                            prefs::kProfileSeparationSettings,
                                            base::Value::Type::INTEGER)));

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kEnterpriseProfileCreationKeepBrowsingData,
          prefs::kEnterpriseProfileCreationKeepBrowsingData,
          base::Value::Type::BOOLEAN),
      std::make_unique<SimplePolicyHandler>(
          key::kProfileSeparationDataMigrationSettings,
          prefs::kProfileSeparationDataMigrationSettings,
          base::Value::Type::INTEGER)));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kProfileReauthPrompt, enterprise_signin::prefs::kProfileReauthPrompt,
      static_cast<int>(enterprise_signin::ProfileReauthPrompt::kDoNotPrompt),
      static_cast<int>(enterprise_signin::ProfileReauthPrompt::kPromptInTab),
      false));

  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kEnterpriseCustomLabel, prefs::kEnterpriseCustomLabel,
      base::Value::Type::STRING));
  handlers->AddHandler(std::make_unique<CloudUserOnlyPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(
          key::kEnterpriseCustomLabel, prefs::kEnterpriseCustomLabelForProfile,
          base::Value::Type::STRING)));

  handlers->AddHandler(std::make_unique<URLPolicyHandler>(
      key::kEnterpriseLogoUrl, prefs::kEnterpriseLogoUrl));
  handlers->AddHandler(std::make_unique<CloudUserOnlyPolicyHandler>(
      std::make_unique<URLPolicyHandler>(key::kEnterpriseLogoUrl,
                                         prefs::kEnterpriseLogoUrlForProfile)));

  handlers->AddHandler(
      std::make_unique<policy::SimpleSchemaValidatingPolicyHandler>(
          policy::key::kEnterpriseProfileBadgeToolbarSettings,
          prefs::kEnterpriseProfileBadgeToolbarSettings, chrome_schema,
          policy::SchemaOnErrorStrategy::SCHEMA_STRICT,
          policy::SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          policy::SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

#elif BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(
      std::make_unique<ManagedAccountRestrictionsPolicyHandler>(chrome_schema));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kDeviceChromeVariations, nullptr,
      static_cast<int>(variations::RestrictionPolicy::NO_RESTRICTIONS),
      static_cast<int>(variations::RestrictionPolicy::ALL), false));
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kAttestationExtensionAllowlist,
      prefs::kAttestationExtensionAllowlist, false));
  handlers->AddHandler(
      std::make_unique<SystemFeaturesDisableListPolicyHandler>());
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kDataLeakPreventionRulesList, policy_prefs::kDlpRulesList,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kDocumentScanAPITrustedExtensions,
      prefs::kDocumentScanAPITrustedExtensions, /*allow_wildcards=*/false));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kIsolatedWebAppInstallForceList,
      prefs::kIsolatedWebAppInstallForceList, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<NetworkAnnotationBlocklistHandler>());
#if BUILDFLAG(USE_CUPS)
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kPrintingAPIExtensionsAllowlist,
      prefs::kPrintingAPIExtensionsAllowlist, /*allow_wildcards=*/false));
#endif  // BUILDFLAG(USE_CUPS)
#else   // BUILDFLAG(IS_CHROMEOS)
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      signin_legacy_policies;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_LINUX)
  signin_legacy_policies.push_back(std::make_unique<SimplePolicyHandler>(
      key::kForceBrowserSignin, prefs::kForceBrowserSignin,
      base::Value::Type::BOOLEAN));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_LINUX)
  signin_legacy_policies.push_back(std::make_unique<SimplePolicyHandler>(
      key::kSigninAllowed,
#if BUILDFLAG(IS_ANDROID)
      // The new kSigninAllowedOnNextStartup pref is only used on Desktop.
      // Keep the old kSigninAllowed pref for Android until the policy is
      // fully deprecated in M71 and can be removed.
      prefs::kSigninAllowed,
#else   // BUILDFLAG(IS_ANDROID)
      prefs::kSigninAllowedOnNextStartup,
#endif  // BUILDFLAG(IS_ANDROID)
      base::Value::Type::BOOLEAN));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(signin_legacy_policies),
      std::make_unique<BrowserSigninPolicyHandler>(chrome_schema)));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kQuickUnlockModeAllowlist, ash::prefs::kQuickUnlockModeAllowlist,
      base::Value::Type::LIST));

  // TODO(b/214871750): Here we're not deprecating QuickUnlockModeAllowlist, but
  // just want to set WebAuthnFactors policy value as QuickUnlockModeAllowlist
  // if it's not set. Move this logic to server side afterwards.
  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      std::make_unique<SimplePolicyHandler>(key::kQuickUnlockModeAllowlist,
                                            ash::prefs::kWebAuthnFactors,
                                            base::Value::Type::LIST),
      std::make_unique<SimplePolicyHandler>(key::kWebAuthnFactors,
                                            ash::prefs::kWebAuthnFactors,
                                            base::Value::Type::LIST)));

  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForDevicePolicy()));
  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy()));
  handlers->AddHandler(std::make_unique<PinnedLauncherAppsPolicyHandler>());

  handlers->AddHandler(
      std::make_unique<DefaultHandlersForFileExtensionsPolicyHandler>(
          chrome_schema));

  handlers->AddHandler(std::make_unique<ScreenMagnifierPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<LoginScreenPowerManagementPolicyHandler>(chrome_schema));
  // Handler for another policy with JSON strings, lenient but shows warnings.
  handlers->AddHandler(
      std::make_unique<SimpleJsonStringSchemaValidatingPolicyHandler>(
          key::kPrinters, prefs::kRecommendedPrinters,
          chrome_schema.GetValidationSchema(),
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kUserPrintersAllowed, prefs::kUserPrintersAllowed,
      base::Value::Type::BOOLEAN));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kGaiaOfflineSigninTimeLimitDays,
      ash::prefs::kGaiaOfflineSigninTimeLimitDays, -1, 365, true));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kSAMLOfflineSigninTimeLimit, ash::prefs::kSAMLOfflineSigninTimeLimit,
      -1, INT_MAX, true));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kGaiaLockScreenOfflineSigninTimeLimitDays,
      ash::prefs::kGaiaLockScreenOfflineSigninTimeLimitDays, -2, 365, true));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kSamlLockScreenOfflineSigninTimeLimitDays,
      ash::prefs::kSamlLockScreenOfflineSigninTimeLimitDays, -2, 365, true));
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
      static_cast<int>(MagnifierType::kDisabled),
      static_cast<int>(MagnifierType::kDocked), false));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kDeviceLoginScreenScreenMagnifierType, nullptr,
      static_cast<int>(MagnifierType::kDisabled),
      static_cast<int>(MagnifierType::kDocked), false));
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
  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kPrintersBulkAccessMode, prefs::kRecommendedPrintersAccessMode,
      base::Value::Type::INTEGER));
  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kPrintersBulkBlocklist, prefs::kRecommendedPrintersBlocklist,
      base::Value::Type::LIST));
  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kPrintersBulkAllowlist, prefs::kRecommendedPrintersAllowlist,
      base::Value::Type::LIST));
  handlers->AddHandler(
      std::make_unique<ExternalDataPolicyHandler>(key::kExternalPrintServers));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kDeviceExternalPrintServers));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kCrostiniAnsiblePlaybook));
  handlers->AddHandler(
      std::make_unique<AppLaunchAutomationPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<ExternalDataPolicyHandler>(
      key::kPreconfiguredDeskTemplates));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kSessionLocales, nullptr, chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED));
  handlers->AddHandler(
      std::make_unique<ash::platform_keys::KeyPermissionsPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<DefaultGeolocationPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<DeviceLoginScreenGeolocationAccessLevelPolicyHandler>());
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kNoteTakingAppsLockScreenAllowlist,
      prefs::kNoteTakingAppsLockScreenAllowlist, false /*allow_wildcards*/));
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kSecondaryGoogleAccountSigninAllowed,
      ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kUsageTimeLimit, prefs::kUsageTimeLimit, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<ArcServicePolicyHandler>(
      key::kArcBackupRestoreServiceEnabled,
      arc::prefs::kArcBackupRestoreEnabled));
  handlers->AddHandler(std::make_unique<ArcLocationServicePolicyHandler>(
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
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kPerAppTimeLimitsAllowlist, prefs::kPerAppTimeLimitsAllowlistPolicy,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kEduCoexistenceToSVersion, ash::prefs::kEduCoexistenceToSVersion,
      chrome_schema, SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kKerberosAccounts, prefs::kKerberosAccounts, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kNearbyShareAllowed, prefs::kNearbySharingEnabledPrefName));
  handlers->AddHandler(std::make_unique<LacrosAvailabilityPolicyHandler>());
  handlers->AddHandler(std::make_unique<LacrosSelectionPolicyHandler>());
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kFastPairEnabled, ash::prefs::kFastPairEnabled));
  handlers->AddHandler(std::make_unique<arc::ArcPolicyHandler>());
  handlers->AddHandler(std::make_unique<BooleanDisablingPolicyHandler>(
      key::kSystemTerminalSshAllowed,
      crostini::prefs::kTerminalSshAllowedByPolicy));
  handlers->AddHandler(std::make_unique<OsColorModePolicyHandler>());
  handlers->AddHandler(std::make_unique<HelpMeWritePolicyHandler>());
  handlers->AddHandler(
      std::make_unique<bruschetta::BruschettaPolicyHandler>(chrome_schema));
  handlers->AddHandler(
      std::make_unique<bruschetta::BruschettaInstallerPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<DriveFileSyncAvailablePolicyHandler>());
  handlers->AddHandler(std::make_unique<ScreenCaptureLocationPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<ContextualGoogleIntegrationsPoliciesHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kDeviceRestrictionSchedule,
      chromeos::prefs::kDeviceRestrictionSchedule, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kContentTransferEnablementStatus,
      ash::prefs::kGraduationEnablementStatus, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(std::make_unique<HelpMeReadPolicyHandler>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// On most platforms, there is a legacy policy
// kUnsafelyTreatInsecureOriginAsSecure which has been replaced by
// kOverrideSecurityRestrictionsOnInsecureOrigin. The legacy policy was never
// supported on ChromeOS or Android, so on those platforms, simply use the new
// one.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(std::make_unique<SecureOriginPolicyHandler>(
      key::kOverrideSecurityRestrictionsOnInsecureOrigin, chrome_schema));
#else   // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      secure_origin_legacy_policy;
  secure_origin_legacy_policy.push_back(
      std::make_unique<SecureOriginPolicyHandler>(
          key::kUnsafelyTreatInsecureOriginAsSecure, chrome_schema));
  handlers->AddHandler(std::make_unique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(secure_origin_legacy_policy),
      std::make_unique<SecureOriginPolicyHandler>(
          key::kOverrideSecurityRestrictionsOnInsecureOrigin, chrome_schema)));

  handlers->AddHandler(std::make_unique<DiskCacheDirPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingAllowlist,
          extensions::pref_names::kNativeMessagingAllowlist, false));
  handlers->AddHandler(
      std::make_unique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingBlocklist,
          extensions::pref_names::kNativeMessagingBlocklist, true));
  handlers->AddHandler(
      std::make_unique<AutoLaunchProtocolsPolicyHandler>(chrome_schema));
  handlers->AddHandler(std::make_unique<FileSelectionDialogsPolicyHandler>());
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(std::make_unique<SimpleSchemaValidatingPolicyHandler>(
      key::kProfilePickerOnStartupAvailability,
      prefs::kBrowserProfilePickerAvailabilityOnStartup, chrome_schema,
      SCHEMA_ALLOW_UNKNOWN,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers->AddHandler(std::make_unique<extensions::ExtensionListPolicyHandler>(
      key::kExtensionInstallAllowlist,
      extensions::pref_names::kInstallAllowList, false));
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionInstallBlockListPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionInstallForceListPolicyHandler>());
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionURLPatternListPolicyHandler>(
          key::kExtensionInstallSources,
          extensions::pref_names::kAllowedInstallSites));
  handlers->AddHandler(std::make_unique<StringMappingListPolicyHandler>(
      key::kExtensionAllowedTypes, extensions::pref_names::kAllowedTypes,
      base::BindRepeating(GetExtensionAllowedTypesMap)));
  handlers->AddHandler(
      std::make_unique<extensions::ExtensionSettingsPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kExtensionUnpublishedAvailability,
      extensions::pref_names::kExtensionUnpublishedAvailability,
      /*min=*/0, /*max=*/1, /*clamp=*/false));
  handlers->AddHandler(std::make_unique<ExtensionDeveloperModePolicyHandler>());
  handlers->AddHandler(std::make_unique<IntRangePolicyHandler>(
      key::kExtensionManifestV2Availability,
      extensions::pref_names::kManifestV2Availability, /*min=*/0, /*max=*/3,
      /*clamp=*/false));

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  handlers->AddHandler(std::make_unique<PrintPdfAsImageDefaultPolicyHandler>());
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  handlers->AddHandler(std::make_unique<SpellcheckLanguagePolicyHandler>());
  handlers->AddHandler(
      std::make_unique<SpellcheckLanguageBlocklistPolicyHandler>(
          key::kSpellcheckLanguageBlocklist));
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_LINUX)
  handlers->AddHandler(std::make_unique<SimplePolicyHandler>(
      key::kAllowSystemNotifications, prefs::kAllowSystemNotifications,
      base::Value::Type::BOOLEAN));
#endif  // BUILDFLAG(IS_LINUX)

  handlers->AddHandler(std::make_unique<URLSchemeListPolicyHandler>(
      key::kAllHttpAuthSchemesAllowedForOrigins,
      prefs::kAllHttpAuthSchemesAllowedForOrigins));

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      /*legacy_policy_handler=*/std::make_unique<
          first_party_sets::FirstPartySetsOverridesPolicyHandler>(
          key::kFirstPartySetsOverrides, chrome_schema),
      /*new_policy_handler=*/std::make_unique<
          first_party_sets::FirstPartySetsOverridesPolicyHandler>(
          key::kRelatedWebsiteSetsOverrides, chrome_schema)));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(std::make_unique<PrivacySandboxPolicyHandler>());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  handlers->AddHandler(
      std::make_unique<data_controls::DataControlsPolicyHandler>(
          key::kDataControlsRules, data_controls::kDataControlsRulesPref,
          chrome_schema));
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

  handlers->AddHandler(std::make_unique<SimpleDeprecatingPolicyHandler>(
      /*legacy_policy_handler=*/std::make_unique<SimplePolicyHandler>(
          key::kFirstPartySetsEnabled,
          prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
          base::Value::Type::BOOLEAN),
      /*new_policy_handler=*/std::make_unique<SimplePolicyHandler>(
          key::kRelatedWebsiteSetsEnabled,
          prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
          base::Value::Type::BOOLEAN)));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  handlers->AddHandler(std::make_unique<BatterySaverPolicyHandler>());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  handlers->AddHandler(
      std::make_unique<DeviceWeeklyScheduledSuspendPolicyHandler>(
          chrome_schema));
  handlers->AddHandler(std::make_unique<MultiScreenCapturePolicyHandler>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
  handlers->AddHandler(std::make_unique<CloudUserOnlyPolicyHandler>(
      std::make_unique<IntRangePolicyHandler>(key::kChromeDataRegionSetting,
                                              prefs::kChromeDataRegionSetting,
                                              /*min=*/0,
                                              /*max=*/2,
                                              /*clamp=*/false)));
#endif  // !BUILDFLAG(IS_ANDROID)

  std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
      gen_ai_default_policies;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  gen_ai_default_policies.emplace_back(
      key::kHelpMeWriteSettings,
      optimization_guide::prefs::kComposeEnterprisePolicyAllowed);
  gen_ai_default_policies.emplace_back(
      key::kTabOrganizerSettings,
      optimization_guide::prefs::kTabOrganizationEnterprisePolicyAllowed);
  gen_ai_default_policies.emplace_back(
      key::kCreateThemesSettings,
      optimization_guide::prefs::kWallpaperSearchEnterprisePolicyAllowed);
  gen_ai_default_policies.emplace_back(key::kDevToolsGenAiSettings,
                                       prefs::kDevToolsGenAiSettings);
  gen_ai_default_policies.emplace_back(
      key::kHistorySearchSettings,
      optimization_guide::prefs::kHistorySearchEnterprisePolicyAllowed);
  gen_ai_default_policies.emplace_back(
      key::kTabCompareSettings,
      optimization_guide::prefs::kProductSpecificationsEnterprisePolicyAllowed);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS)
  gen_ai_default_policies.emplace_back(key::kGenAIWallpaperSettings,
                                       ash::prefs::kGenAIWallpaperSettings);
  gen_ai_default_policies.emplace_back(key::kGenAIVcBackgroundSettings,
                                       ash::prefs::kGenAIVcBackgroundSettings);
  gen_ai_default_policies.emplace_back(key::kHelpMeReadSettings,
                                       ash::prefs::kHmrManagedSettings);
#endif  // BUILDFLAG(IS_CHROMEOS)
  handlers->AddHandler(std::make_unique<GenAiDefaultSettingsPolicyHandler>(
      std::move(gen_ai_default_policies)));

  return handlers;
}

}  // namespace policy
