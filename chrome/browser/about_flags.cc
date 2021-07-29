// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/performance_hints/performance_hints_features.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/permissions/abusive_origin_notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/site_isolation/about_flags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/assist_ranker/predictor_config_definitions.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browsing_data/core/features.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/contextual_search/core/browser/public.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/public/common/download_features.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/history_clusters/core/memories_features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/lens/lens_features.h"
#include "components/lookalikes/core/features.h"
#include "components/messages/android/messages_feature.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/features.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/permissions/features.h"
#include "components/policy/core/common/features.h"
#include "components/query_tiles/switches.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/search/ntp_features.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/features.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/send_tab_to_self/features.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_isolation/features.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "components/translate/core/common/translate_util.h"
#include "components/ui_devtools/switches.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/webapps/common/switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/fido/features.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "media/midi/midi_switches.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/core/embedder/features.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/serial/serial_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "storage/browser/quota/quota_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/allocator/buildflags.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/webapps/android/features.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/browser_ui/site_settings/android/features.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/external_intents/android/external_intents_features.h"
#include "components/power_scheduler/power_scheduler_features.h"
#else  // OS_ANDROID
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/web_applications/components/preinstalled_app_install_features.h"
#endif  // OS_ANDROID

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_util.h"
#include "components/full_restore/features.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#endif  // OS_MAC

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#endif  // ENABLE_EXTENSIONS

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_features.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif  // USE_OZONE

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/titlebar_config.h"
#endif  // OS_WIN

#if defined(TOOLKIT_VIEWS)
#include "ui/views/animation/installable_ink_drop.h"
#include "ui/views/views_features.h"
#include "ui/views/views_switches.h"
#endif  // defined(TOOLKIT_VIEWS)

using flags_ui::FeatureEntry;
using flags_ui::kDeprecated;
using flags_ui::kOsAndroid;
using flags_ui::kOsCrOS;
using flags_ui::kOsCrOSOwnerOnly;
using flags_ui::kOsLinux;
using flags_ui::kOsMac;
using flags_ui::kOsWin;

namespace about_flags {

namespace {

const unsigned kOsAll = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS;

#if defined(USE_AURA)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS;
#endif  // USE_AURA

#if defined(USE_AURA)
const FeatureEntry::Choice kPullToRefreshChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kPullToRefresh, "0"},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kPullToRefresh, "1"},
    {flag_descriptions::kPullToRefreshEnabledTouchscreen,
     switches::kPullToRefresh, "2"}};
#endif  // USE_AURA

const FeatureEntry::Choice kOverlayStrategiesChoices[] = {
    {flag_descriptions::kOverlayStrategiesDefault, "", ""},
    {flag_descriptions::kOverlayStrategiesNone,
     switches::kEnableHardwareOverlays, ""},
    {flag_descriptions::kOverlayStrategiesUnoccludedFullscreen,
     switches::kEnableHardwareOverlays, "single-fullscreen"},
    {flag_descriptions::kOverlayStrategiesUnoccluded,
     switches::kEnableHardwareOverlays, "single-fullscreen,single-on-top"},
    {flag_descriptions::kOverlayStrategiesOccludedAndUnoccluded,
     switches::kEnableHardwareOverlays,
     "single-fullscreen,single-on-top,underlay"},
};

const FeatureEntry::Choice kTouchTextSelectionStrategyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kTouchSelectionStrategyCharacter,
     blink::switches::kTouchTextSelectionStrategy, "character"},
    {flag_descriptions::kTouchSelectionStrategyDirection,
     blink::switches::kTouchTextSelectionStrategy, "direction"}};

const FeatureEntry::Choice kTraceUploadURL[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flag_descriptions::kTraceUploadUrlChoiceOther, switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,Other"},
    {flag_descriptions::kTraceUploadUrlChoiceEmloading,
     switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,emloading"},
    {flag_descriptions::kTraceUploadUrlChoiceQa, switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,QA"},
    {flag_descriptions::kTraceUploadUrlChoiceTesting, switches::kTraceUploadURL,
     "https://performance-insights.appspot.com/upload?tags=flags,TestingTeam"}};

const FeatureEntry::Choice kLiteVideoDefaultDownlinkBandwidthKbps[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"100", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "100"},
    {"150", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "150"},
    {"200", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "200"},
    {"250", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "250"},
    {"300", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "300"},
    {"350", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "350"},
    {"400", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "400"},
    {"450", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "450"},
    {"500", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "500"},
    {"600", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "600"},
    {"700", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "700"},
    {"800", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "800"},
    {"900", lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps,
     "900"}};

#if defined(OS_WIN)
const FeatureEntry::Choice kUseAngleChoices[] = {
    {flag_descriptions::kUseAngleDefault, "", ""},
    {flag_descriptions::kUseAngleGL, switches::kUseANGLE,
     gl::kANGLEImplementationOpenGLName},
    {flag_descriptions::kUseAngleD3D11, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11Name},
    {flag_descriptions::kUseAngleD3D9, switches::kUseANGLE,
     gl::kANGLEImplementationD3D9Name},
    {flag_descriptions::kUseAngleD3D11on12, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11on12Name}};
#endif

#if BUILDFLAG(ENABLE_VR)
const FeatureEntry::Choice kWebXrForceRuntimeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kWebXrRuntimeChoiceNone, switches::kWebXrForceRuntime,
     switches::kWebXrRuntimeNone},

#if BUILDFLAG(ENABLE_OPENXR)
    {flag_descriptions::kWebXrRuntimeChoiceOpenXR, switches::kWebXrForceRuntime,
     switches::kWebXrRuntimeOpenXr},
#endif  // ENABLE_OPENXR
};
#endif  // ENABLE_VR

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kElasticOverscrollFilterType[] = {
    {features::kElasticOverscrollType, features::kElasticOverscrollTypeFilter}};
const FeatureEntry::FeatureParam kElasticOverscrollTransformType[] = {
    {features::kElasticOverscrollType,
     features::kElasticOverscrollTypeTransform}};

const FeatureEntry::FeatureVariation kElasticOverscrollVariations[] = {
    {"Pixel shader stretch", kElasticOverscrollFilterType,
     base::size(kElasticOverscrollFilterType), nullptr},
    {"Transform stretch", kElasticOverscrollTransformType,
     base::size(kElasticOverscrollTransformType), nullptr}};

const FeatureEntry::Choice kReaderModeHeuristicsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kReaderModeHeuristicsMarkup,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kOGArticle},
    {flag_descriptions::kReaderModeHeuristicsAdaboost,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kAdaBoost},
    {flag_descriptions::kReaderModeHeuristicsAlwaysOn,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kAlwaysTrue},
    {flag_descriptions::kReaderModeHeuristicsAlwaysOff,
     switches::kReaderModeHeuristics, switches::reader_mode_heuristics::kNone},
    {flag_descriptions::kReaderModeHeuristicsAllArticles,
     switches::kReaderModeHeuristics,
     switches::reader_mode_heuristics::kAllArticles},
};

const FeatureEntry::Choice kForceUpdateMenuTypeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kUpdateMenuTypeNone, switches::kForceUpdateMenuType,
     "none"},
    {flag_descriptions::kUpdateMenuTypeUpdateAvailable,
     switches::kForceUpdateMenuType, "update_available"},
    {flag_descriptions::kUpdateMenuTypeUnsupportedOSVersion,
     switches::kForceUpdateMenuType, "unsupported_os_version"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateSuccess,
     switches::kForceUpdateMenuType, "inline_update_success"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDialogCanceled,
     switches::kForceUpdateMenuType, "inline_update_dialog_canceled"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDialogFailed,
     switches::kForceUpdateMenuType, "inline_update_dialog_failed"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDownloadFailed,
     switches::kForceUpdateMenuType, "inline_update_download_failed"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateDownloadCanceled,
     switches::kForceUpdateMenuType, "inline_update_download_canceled"},
    {flag_descriptions::kUpdateMenuTypeInlineUpdateInstallFailed,
     switches::kForceUpdateMenuType, "inline_update_install_failed"},
};
#else  // !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kReaderModeOfferInSettings[] = {
    {switches::kReaderModeDiscoverabilityParamName,
     switches::kReaderModeOfferInSettings}};

const FeatureEntry::FeatureVariation kReaderModeDiscoverabilityVariations[] = {
    {"available in settings", kReaderModeOfferInSettings,
     base::size(kReaderModeOfferInSettings), nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureVariation kAdaptiveButtonInTopToolbarVariations[] = {
    {
        "Always None",
        (FeatureEntry::FeatureParam[]){{"mode", "always-none"}},
        1,
        nullptr,
    },
    {
        "Always New Tab",
        (FeatureEntry::FeatureParam[]){{"mode", "always-new-tab"}},
        1,
        nullptr,
    },
    {
        "Always Share",
        (FeatureEntry::FeatureParam[]){{"mode", "always-share"}},
        1,
        nullptr,
    },
    {
        "Always Voice",
        (FeatureEntry::FeatureParam[]){{"mode", "always-voice"}},
        1,
        nullptr,
    },
};

const FeatureEntry::FeatureVariation
    kAdaptiveButtonInTopToolbarCustomizationVariations[] = {
        {
            "New Tab",
            (FeatureEntry::FeatureParam[]){
                {"default_segment", "new-tab"},
                {"ignore_segmentation_results", "true"}},
            1,
            nullptr,
        },
        {
            "Share",
            (FeatureEntry::FeatureParam[]){
                {"default_segment", "share"},
                {"ignore_segmentation_results", "true"}},
            1,
            nullptr,
        },
        {
            "Voice",
            (FeatureEntry::FeatureParam[]){
                {"default_segment", "voice"},
                {"ignore_segmentation_results", "true"}},
            1,
            nullptr,
        },
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kHideDismissButton[] = {
    {"dismiss_button", "hide"}};

const FeatureEntry::FeatureParam kSuppressBottomSheet[] = {
    {"consecutive_active_dismissal_limit", "3"}};

const FeatureEntry::FeatureVariation kMobileIdentityConsistencyVariations[] = {
    {"Hide Dismiss Button", kHideDismissButton, base::size(kHideDismissButton),
     nullptr},
    {"Suppress Bottom Sheet", kSuppressBottomSheet,
     base::size(kSuppressBottomSheet), nullptr},
};
#endif  // OS_ANDROID

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kForceDark_SimpleHsl[] = {
    {"inversion_method", "hsl_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SimpleCielab[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SimpleRgb[] = {
    {"inversion_method", "rgb_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveImageInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"text_lightness_threshold", "256"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveElementInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"text_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveGeneralInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"text_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureParam kForceDark_IncreaseTextContrast[] = {
    {"increase_text_contrast", "true"}};

const FeatureEntry::FeatureVariation kForceDarkVariations[] = {
    {"with simple HSL-based inversion", kForceDark_SimpleHsl,
     base::size(kForceDark_SimpleHsl), nullptr},
    {"with simple CIELAB-based inversion", kForceDark_SimpleCielab,
     base::size(kForceDark_SimpleCielab), nullptr},
    {"with simple RGB-based inversion", kForceDark_SimpleRgb,
     base::size(kForceDark_SimpleRgb), nullptr},
    {"with selective image inversion", kForceDark_SelectiveImageInversion,
     base::size(kForceDark_SelectiveImageInversion), nullptr},
    {"with selective inversion of non-image elements",
     kForceDark_SelectiveElementInversion,
     base::size(kForceDark_SelectiveElementInversion), nullptr},
    {"with selective inversion of everything",
     kForceDark_SelectiveGeneralInversion,
     base::size(kForceDark_SelectiveGeneralInversion), nullptr},
    {"with increased text contrast", kForceDark_IncreaseTextContrast,
     base::size(kForceDark_IncreaseTextContrast), nullptr}};
#endif  // !OS_CHROMEOS

const FeatureEntry::FeatureParam kMBIModeLegacy[] = {{"mode", "legacy"}};
const FeatureEntry::FeatureParam kMBIModeEnabledPerRenderProcessHost[] = {
    {"mode", "per_render_process_host"}};
const FeatureEntry::FeatureParam kMBIModeEnabledPerSiteInstance[] = {
    {"mode", "per_site_instance"}};

const FeatureEntry::FeatureVariation kMBIModeVariations[] = {
    {"legacy mode", kMBIModeLegacy, base::size(kMBIModeLegacy), nullptr},
    {"per render process host", kMBIModeEnabledPerRenderProcessHost,
     base::size(kMBIModeEnabledPerRenderProcessHost), nullptr},
    {"per site instance", kMBIModeEnabledPerSiteInstance,
     base::size(kMBIModeEnabledPerSiteInstance), nullptr}};

const FeatureEntry::FeatureParam kIntensiveWakeUpThrottlingAfter10Seconds[] = {
    {blink::features::kIntensiveWakeUpThrottling_GracePeriodSeconds_Name,
     "10"}};

const FeatureEntry::FeatureVariation kIntensiveWakeUpThrottlingVariations[] = {
    {"10 seconds after a tab is hidden (facilitates testing)",
     kIntensiveWakeUpThrottlingAfter10Seconds,
     base::size(kIntensiveWakeUpThrottlingAfter10Seconds), nullptr},
};

const FeatureEntry::FeatureParam kFencedFramesImplementationTypeShadowDOM[] = {
    {"implementation_type", "shadow_dom"}};
const FeatureEntry::FeatureParam kFencedFramesImplementationTypeMPArch[] = {
    {"implementation_type", "mparch"}};

const FeatureEntry::FeatureVariation
    kFencedFramesImplementationTypeVariations[] = {
        {"with ShadowDOM", kFencedFramesImplementationTypeShadowDOM,
         base::size(kFencedFramesImplementationTypeShadowDOM), nullptr},
        {"with multiple page architecture",
         kFencedFramesImplementationTypeMPArch,
         base::size(kFencedFramesImplementationTypeMPArch), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_Immediate[] = {
    {"baseline_tab_suggestions", "true"},
    {"baseline_close_tab_suggestions", "true"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_4Hours[] = {
    {"close_tab_suggestions_stale_time_ms", "14400000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_8Hours[] = {
    {"close_tab_suggestions_stale_time_ms", "28800000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_7Days[] = {
    {"close_tab_suggestions_stale_time_ms", "604800000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsTimeSiteEngagement[] = {
    {"close_tab_min_num_tabs", "5"},
    {"close_tab_features_time_last_used_enabled", "true"},
    {"close_tab_features_time_last_used_transform", "MEAN_VARIANCE"},
    {"close_tab_features_time_last_used_threshold", "0.5"},
    {"close_tab_features_site_engagement_enabled", "true"},
    {"close_tab_features_site_engagement_threshold", "90.0"},
};
const FeatureEntry::FeatureParam kGroupAndCloseTabSuggestions_Immediate[] = {
    {"baseline_tab_suggestions", "true"},
    {"baseline_group_tab_suggestions", "true"},
    {"baseline_close_tab_suggestions", "true"}};

const FeatureEntry::FeatureVariation kCloseTabSuggestionsStaleVariations[] = {
    {"Close Immediate", kCloseTabSuggestionsStale_Immediate,
     base::size(kCloseTabSuggestionsStale_Immediate), nullptr},
    {"Group+Close Immediate", kGroupAndCloseTabSuggestions_Immediate,
     base::size(kGroupAndCloseTabSuggestions_Immediate), nullptr},
    {"4 hours", kCloseTabSuggestionsStale_4Hours,
     base::size(kCloseTabSuggestionsStale_4Hours), nullptr},
    {"8 hours", kCloseTabSuggestionsStale_8Hours,
     base::size(kCloseTabSuggestionsStale_8Hours), nullptr},
    {"7 days", kCloseTabSuggestionsStale_7Days,
     base::size(kCloseTabSuggestionsStale_7Days), nullptr},
    {"Time & Site Engagement", kCloseTabSuggestionsTimeSiteEngagement,
     base::size(kCloseTabSuggestionsTimeSiteEngagement), nullptr},
};

const FeatureEntry::FeatureParam kWebNoteStylizeRandomizeParam[] = {
    {"randomize_order", "true"}};
const FeatureEntry::FeatureVariation kWebNoteStylizeVariations[] = {
    {"With Randomized Order", kWebNoteStylizeRandomizeParam,
     base::size(kWebNoteStylizeRandomizeParam), nullptr}};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableGpuRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableGpuRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableGpuRasterization, ""},
};

const FeatureEntry::Choice kEnableOopRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableOopRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableOopRasterization, ""},
};

const FeatureEntry::Choice kExtensionContentVerificationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kExtensionContentVerificationBootstrap,
     switches::kExtensionContentVerification,
     switches::kExtensionContentVerificationBootstrap},
    {flag_descriptions::kExtensionContentVerificationEnforce,
     switches::kExtensionContentVerification,
     switches::kExtensionContentVerificationEnforce},
    {flag_descriptions::kExtensionContentVerificationEnforceStrict,
     switches::kExtensionContentVerification,
     switches::kExtensionContentVerificationEnforceStrict},
};

const FeatureEntry::Choice kTopChromeTouchUiChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceAutomatic, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiAuto},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiDisabled},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiEnabled}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kArcUseHighMemoryDalvikProfileInternalName[] =
    "arc-use-high-memory-dalvik-profile";
const char kLacrosAvailabilityIgnoreInternalName[] =
    "lacros-availability-ignore";
const char kLacrosPrimaryInternalName[] = "lacros-primary";
const char kLacrosSupportInternalName[] = "lacros-support";
const char kLacrosStabilityInternalName[] = "lacros-stability";
const char kWebAppsCrosapiInternalName[] = "web-apps-crosapi";

const FeatureEntry::Choice kLacrosStabilityChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kLacrosStabilityLeastStableDescription,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityLeastStable},
    {flag_descriptions::kLacrosStabilityLessStableDescription,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityLessStable},
    {flag_descriptions::kLacrosStabilityMoreStableDescription,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityMoreStable},
};

const char kLacrosSelectionInternalName[] = "lacros-selection";

const FeatureEntry::Choice kLacrosSelectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kLacrosSelectionStatefulDescription,
     crosapi::browser_util::kLacrosSelectionSwitch,
     crosapi::browser_util::kLacrosSelectionStateful},
    {flag_descriptions::kLacrosSelectionRootfsDescription,
     crosapi::browser_util::kLacrosSelectionSwitch,
     crosapi::browser_util::kLacrosSelectionRootfs},
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kCrosRegionsModeChoices[] = {
    {flag_descriptions::kCrosRegionsModeDefault, "", ""},
    {flag_descriptions::kCrosRegionsModeOverride,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeOverride},
    {flag_descriptions::kCrosRegionsModeHide,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeHide},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const FeatureEntry::Choice kForceUIDirectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceDirectionLtr, switches::kForceUIDirection,
     switches::kForceDirectionLTR},
    {flag_descriptions::kForceDirectionRtl, switches::kForceUIDirection,
     switches::kForceDirectionRTL},
};

const FeatureEntry::Choice kForceTextDirectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceDirectionLtr, switches::kForceTextDirection,
     switches::kForceDirectionLTR},
    {flag_descriptions::kForceDirectionRtl, switches::kForceTextDirection,
     switches::kForceDirectionRTL},
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam
    kDesktopPWAsAttentionBadgingCrOSApiAndNotifications[] = {
        {"badge-source",
         switches::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications}};
const FeatureEntry::FeatureParam
    kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications[] = {
        {"badge-source",
         switches::kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications}};
const FeatureEntry::FeatureParam kDesktopPWAsAttentionBadgingCrOSApiOnly[] = {
    {"badge-source", switches::kDesktopPWAsAttentionBadgingCrOSApiOnly}};
const FeatureEntry::FeatureParam
    kDesktopPWAsAttentionBadgingCrOSNotificationsOnly[] = {
        {"badge-source",
         switches::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly}};

const FeatureEntry::FeatureVariation
    kDesktopPWAsAttentionBadgingCrOSVariations[] = {
        {flag_descriptions::kDesktopPWAsAttentionBadgingCrOSApiAndNotifications,
         kDesktopPWAsAttentionBadgingCrOSApiAndNotifications,
         base::size(kDesktopPWAsAttentionBadgingCrOSApiAndNotifications),
         nullptr},
        {flag_descriptions::
             kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications,
         kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications,
         base::size(kDesktopPWAsAttentionBadgingCrOSApiOverridesNotifications),
         nullptr},
        {flag_descriptions::kDesktopPWAsAttentionBadgingCrOSApiOnly,
         kDesktopPWAsAttentionBadgingCrOSApiOnly,
         base::size(kDesktopPWAsAttentionBadgingCrOSApiOnly), nullptr},
        {flag_descriptions::kDesktopPWAsAttentionBadgingCrOSNotificationsOnly,
         kDesktopPWAsAttentionBadgingCrOSNotificationsOnly,
         base::size(kDesktopPWAsAttentionBadgingCrOSNotificationsOnly),
         nullptr},
};

const FeatureEntry::Choice kSchedulerConfigurationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kSchedulerConfigurationConservative,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationConservative},
    {flag_descriptions::kSchedulerConfigurationPerformance,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationPerformance},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kCompactSuggestions_SemicompactVariant[] = {
    {"omnibox_compact_suggestions_variant", "semi-compact"}};

const FeatureEntry::FeatureVariation kCompactSuggestionsVariations[] = {
    {"- Semi-compact", kCompactSuggestions_SemicompactVariant,
     base::size(kCompactSuggestions_SemicompactVariant), nullptr},
};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableUseZoomForDSFChoices[] = {
    {flag_descriptions::kEnableUseZoomForDsfChoiceDefault, "", ""},
    {flag_descriptions::kEnableUseZoomForDsfChoiceEnabled,
     switches::kEnableUseZoomForDSF, "true"},
    {flag_descriptions::kEnableUseZoomForDsfChoiceDisabled,
     switches::kEnableUseZoomForDSF, "false"},
};

const FeatureEntry::Choice kSiteIsolationOptOutChoices[] = {
    {flag_descriptions::kSiteIsolationOptOutChoiceDefault, "", ""},
    {flag_descriptions::kSiteIsolationOptOutChoiceOptOut,
     switches::kDisableSiteIsolation, ""},
};

const FeatureEntry::Choice kForceColorProfileChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceColorProfileSRGB,
     switches::kForceDisplayColorProfile, "srgb"},
    {flag_descriptions::kForceColorProfileP3,
     switches::kForceDisplayColorProfile, "display-p3-d65"},
    {flag_descriptions::kForceColorProfileColorSpin,
     switches::kForceDisplayColorProfile, "color-spin-gamma24"},
    {flag_descriptions::kForceColorProfileSCRGBLinear,
     switches::kForceDisplayColorProfile, "scrgb-linear"},
    {flag_descriptions::kForceColorProfileHDR10,
     switches::kForceDisplayColorProfile, "hdr10"},
};

const FeatureEntry::Choice kForceEffectiveConnectionTypeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kEffectiveConnectionTypeUnknownDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeUnknown},
    {flag_descriptions::kEffectiveConnectionTypeOfflineDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeOffline},
    {flag_descriptions::kEffectiveConnectionTypeSlow2GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeSlow2G},
    {flag_descriptions::kEffectiveConnectionTypeSlow2GOnCellularDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionTypeSlow2GOnCellular},
    {flag_descriptions::kEffectiveConnectionType2GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionType2G},
    {flag_descriptions::kEffectiveConnectionType3GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionType3G},
    {flag_descriptions::kEffectiveConnectionType4GDescription,
     network::switches::kForceEffectiveConnectionType,
     net::kEffectiveConnectionType4G},
};

// Ensure that all effective connection types returned by Network Quality
// Estimator (NQE) are also exposed via flags.
static_assert(net::EFFECTIVE_CONNECTION_TYPE_LAST + 2 ==
                  base::size(kForceEffectiveConnectionTypeChoices),
              "ECT enum value is not handled.");
static_assert(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN == 0,
              "ECT enum value is not handled.");
static_assert(net::EFFECTIVE_CONNECTION_TYPE_4G + 1 ==
                  net::EFFECTIVE_CONNECTION_TYPE_LAST,
              "ECT enum value is not handled.");

const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitNone[] = {
    {"max_srp_prefetches", "-1"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitZero[] = {
    {"max_srp_prefetches", "0"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitOne[] = {
    {"max_srp_prefetches", "1"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitTwo[] = {
    {"max_srp_prefetches", "2"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitThree[] = {
    {"max_srp_prefetches", "3"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitFour[] = {
    {"max_srp_prefetches", "4"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitFive[] = {
    {"max_srp_prefetches", "5"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitTen[] = {
    {"max_srp_prefetches", "10"}};
const FeatureEntry::FeatureParam kIsolatedPrerenderPrefetchLimitFifteen[] = {
    {"max_srp_prefetches", "15"}};

const FeatureEntry::FeatureVariation
    kIsolatedPrerenderFeatureWithPrefetchLimit[] = {
        {"Unlimited Prefetches", kIsolatedPrerenderPrefetchLimitNone,
         base::size(kIsolatedPrerenderPrefetchLimitNone), nullptr},
        {"Zero Prefetches", kIsolatedPrerenderPrefetchLimitZero,
         base::size(kIsolatedPrerenderPrefetchLimitZero), nullptr},
        {"One Prefetch", kIsolatedPrerenderPrefetchLimitOne,
         base::size(kIsolatedPrerenderPrefetchLimitOne), nullptr},
        {"Two Prefetches", kIsolatedPrerenderPrefetchLimitTwo,
         base::size(kIsolatedPrerenderPrefetchLimitTwo), nullptr},
        {"Three Prefetches", kIsolatedPrerenderPrefetchLimitThree,
         base::size(kIsolatedPrerenderPrefetchLimitThree), nullptr},
        {"Four Prefetches", kIsolatedPrerenderPrefetchLimitFour,
         base::size(kIsolatedPrerenderPrefetchLimitFour), nullptr},
        {"Five Prefetches", kIsolatedPrerenderPrefetchLimitFive,
         base::size(kIsolatedPrerenderPrefetchLimitFive), nullptr},
        {"Ten Prefetches", kIsolatedPrerenderPrefetchLimitTen,
         base::size(kIsolatedPrerenderPrefetchLimitTen), nullptr},
        {"Fifteen Prefetches", kIsolatedPrerenderPrefetchLimitFifteen,
         base::size(kIsolatedPrerenderPrefetchLimitFifteen), nullptr},
};

const FeatureEntry::Choice kMemlogModeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flag_descriptions::kMemlogModeMinimal, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeMinimal},
    {flag_descriptions::kMemlogModeAll, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeAll},
    {flag_descriptions::kMemlogModeBrowser, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeBrowser},
    {flag_descriptions::kMemlogModeGpu, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeGpu},
    {flag_descriptions::kMemlogModeAllRenderers, heap_profiling::kMemlogMode,
     heap_profiling::kMemlogModeAllRenderers},
    {flag_descriptions::kMemlogModeRendererSampling,
     heap_profiling::kMemlogMode, heap_profiling::kMemlogModeRendererSampling},
};

const FeatureEntry::Choice kMemlogStackModeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMemlogStackModeNative,
     heap_profiling::kMemlogStackMode, heap_profiling::kMemlogStackModeNative},
    {flag_descriptions::kMemlogStackModeNativeWithThreadNames,
     heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeNativeWithThreadNames},
};

const FeatureEntry::Choice kMemlogSamplingRateChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMemlogSamplingRate10KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate10KB},
    {flag_descriptions::kMemlogSamplingRate50KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate50KB},
    {flag_descriptions::kMemlogSamplingRate100KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate100KB},
    {flag_descriptions::kMemlogSamplingRate500KB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate500KB},
    {flag_descriptions::kMemlogSamplingRate1MB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate1MB},
    {flag_descriptions::kMemlogSamplingRate5MB,
     heap_profiling::kMemlogSamplingRate,
     heap_profiling::kMemlogSamplingRate5MB},
};

const FeatureEntry::FeatureVariation kMemoriesVariations[] = {
    {
        "Limit 1000, On-Device",
        (FeatureEntry::FeatureParam[]){{"MemoriesMaxVisitsToCluster", "1000"}},
        1,
        nullptr,
    },
    {
        "Limit 200, Remote Exp. A",
        (FeatureEntry::FeatureParam[]){
            {"MemoriesExperimentName", "A"},
            {"MemoriesMaxVisitsToCluster", "200"},
            {"MemoriesOnDeviceClusteringBackend", "false"},
        },
        3,
        nullptr,
    },
    {
        "Limit 200, Remote Exp. B",
        (FeatureEntry::FeatureParam[]){
            {"MemoriesExperimentName", "B"},
            {"MemoriesMaxVisitsToCluster", "200"},
            {"MemoriesOnDeviceClusteringBackend", "false"},
        },
        3,
        nullptr,
    },
    {
        "Limit 200, Remote Exp. C",
        (FeatureEntry::FeatureParam[]){
            {"MemoriesExperimentName", "C"},
            {"MemoriesMaxVisitsToCluster", "200"},
            {"MemoriesOnDeviceClusteringBackend", "false"},
        },
        3,
        nullptr,
    },
    {
        "Limit 10k, Remote",
        (FeatureEntry::FeatureParam[]){
            {"MemoriesMaxVisitsToCluster", "10000"},
            {"MemoriesOnDeviceClusteringBackend", "false"},
        },
        2,
        nullptr,
    },
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
const FeatureEntry::FeatureParam kOmniboxDocumentProviderServerScoring[] = {
    {"DocumentUseServerScore", "true"},
    {"DocumentUseClientScore", "false"},
    {"DocumentCapScorePerRank", "false"},
    {"DocumentBoostOwned", "false"},
};
const FeatureEntry::FeatureParam
    kOmniboxDocumentProviderServerScoringCappedByRank[] = {
        {"DocumentUseServerScore", "true"},
        {"DocumentUseClientScore", "false"},
        {"DocumentCapScorePerRank", "true"},
        {"DocumentBoostOwned", "true"},
};
const FeatureEntry::FeatureParam kOmniboxDocumentProviderClientScoring[] = {
    {"DocumentUseServerScore", "false"},
    {"DocumentUseClientScore", "true"},
    {"DocumentCapScorePerRank", "false"},
    {"DocumentBoostOwned", "false"},
};
const FeatureEntry::FeatureParam
    kOmniboxDocumentProviderServerAndClientScoring[] = {
        {"DocumentUseServerScore", "true"},
        {"DocumentUseClientScore", "true"},
        {"DocumentCapScorePerRank", "false"},
        {"DocumentBoostOwned", "false"},
};

const FeatureEntry::FeatureVariation kOmniboxDocumentProviderVariations[] = {
    {"server scores", kOmniboxDocumentProviderServerScoring,
     base::size(kOmniboxDocumentProviderServerScoring), nullptr},
    {"server scores capped by rank",
     kOmniboxDocumentProviderServerScoringCappedByRank,
     base::size(kOmniboxDocumentProviderServerScoringCappedByRank), nullptr},
    {"client scores", kOmniboxDocumentProviderClientScoring,
     base::size(kOmniboxDocumentProviderClientScoring), nullptr},
    {"server and client scores", kOmniboxDocumentProviderServerAndClientScoring,
     base::size(kOmniboxDocumentProviderServerAndClientScoring), nullptr}};

// 3 permutations of the 2 rich autocompletion params:
// - Title AC: Autocompletes suggestions when the input matches the title.
//   E.g. Space Sh | [ttle - Wikipedia] (en.wikipedia.org/wiki/Space_Shuttle)
// - Non-Prefix AC: Autocompletes suggestions when the input is not necessarily
//   a prefix.
//   E.g. [en.wikipe dia.org/] wiki/Spac | [e_Shuttle] (Space Shuttle -
//   Wikipedia)
const FeatureEntry::FeatureVariation kOmniboxRichAutocompletionVariations[] = {
    {
        "Title AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteTitles", "true"}},
        1,
        nullptr,
    },
    {
        "Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"}},
        1,
        nullptr,
    },
    {
        "Title AC & Non-Prefix AC",
        (FeatureEntry::FeatureParam[]){
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"}},
        2,
        nullptr,
    }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionMinCharVariations[] = {
        {
            "Title 0 / Non Prefix 0",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "0"}},
            2,
            nullptr,
        },
        {
            "Title 0 / Non Prefix 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "3"}},
            2,
            nullptr,
        },
        {
            "Title 0 / Non Prefix 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            2,
            nullptr,
        },
        {
            "Title 3 / Non Prefix 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "3"}},
            2,
            nullptr,
        },
        {
            "Title 3 / Non Prefix 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            2,
            nullptr,
        },
        {
            "Title 5 / Non Prefix 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitlesMinChar", "5"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            2,
            nullptr,
        }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionShowAdditionalTextVariations[] = {
        {
            "Show Additional Text",
            (FeatureEntry::FeatureParam[]){},
            0,
            nullptr,
        },
        {
            "Hide Additional Text",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteShowAdditionalText", "false"}},
            1,
            nullptr,
        }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionSplitVariations[] = {
        {
            "Titles & URLs, min char 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitUrlCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "5"}},
            3,
            nullptr,
        },
        {
            "Titles & URLs, min char 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitUrlCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "3"}},
            3,
            nullptr,
        },
        {
            "Titles, min char 5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "5"}},
            2,
            nullptr,
        },
        {
            "Titles, min char 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionSplitTitleCompletion", "true"},
                {"RichAutocompletionSplitCompletionMinChar", "3"}},
            2,
            nullptr,
        }};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPreferUrlsOverPrefixesVariations[] = {
        {
            "Prefer prefixes",
            (FeatureEntry::FeatureParam[]){},
            0,
            nullptr,
        },
        {
            "Prefer URLs",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompletePreferUrlsOverPrefixes",
                 "true"}},
            1,
            nullptr,
        }};

// A limited number of combinations of the above variations that are most
// promising.
const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPromisingVariations[] = {
        {
            "Aggressive - Title, Non-Prefix, min 0/0",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteNonPrefixAll", "true"}},
            2,
            nullptr,
        },
        {
            "Aggressive Moderate - Title, Non-Prefix, min 3/5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            4,
            nullptr,
        },
        {
            "Conservative Moderate - Title, Shortcut Non-Prefix, min 3/5",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteNonPrefixShortcutProvider",
                 "true"},
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
                {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}},
            4,
            nullptr,
        },
        {
            "Conservative - Title, min 3",
            (FeatureEntry::FeatureParam[]){
                {"RichAutocompletionAutocompleteTitles", "true"},
                {"RichAutocompletionAutocompleteTitlesMinChar", "3"}},
            2,
            nullptr,
        },
};

const FeatureEntry::FeatureVariation kOmniboxBookmarkPathsVariations[] = {
    {
        "Default UI (Title - URL)",
        (FeatureEntry::FeatureParam[]){},
        0,
        nullptr,
    },
    {
        "Replace title (Path/Title - URL)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiReplaceTitle", "true"}},
        1,
        nullptr,
    },
    {
        "Replace URL (Title - Path)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiReplaceUrl", "true"}},
        1,
        nullptr,
    },
    {
        "Append after title (Title : Path - URL)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiAppendAfterTitle", "true"}},
        1,
        nullptr,
    },
    {
        "Dynamic Replace URL (Title - Path|URL)",
        (FeatureEntry::FeatureParam[]){
            {"OmniboxBookmarkPathsUiDynamicReplaceUrl", "true"}},
        1,
        nullptr,
    },
};
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN)

const FeatureEntry::FeatureVariation
    kOmniboxOnFocusSuggestionsContextualWebVariations[] = {
        {"GOC Only", {}, 0, "t3317583"},
        {"pSuggest Only", {}, 0, "t3318055"},
        {"GOC, pSuggest Fallback", {}, 0, "t3317692"},
        {"GOC, pSuggest Backfill", {}, 0, "t3317694"},
        {"GOC, Default Hidden", {}, 0, "t3317834"},
};

const FeatureEntry::FeatureVariation kMaxZeroSuggestMatchesVariations[] = {
    {
        "5",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "5"}},
        1,
        nullptr,
    },
    {
        "6",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "6"}},
        1,
        nullptr,
    },
    {
        "7",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "7"}},
        1,
        nullptr,
    },
    {
        "8",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "8"}},
        1,
        nullptr,
    },
    {
        "9",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "9"}},
        1,
        nullptr,
    },
    {
        "10",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "10"}},
        1,
        nullptr,
    },
    {
        "11",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "11"}},
        1,
        nullptr,
    },
    {
        "12",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "12"}},
        1,
        nullptr,
    },
    {
        "13",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "13"}},
        1,
        nullptr,
    },
    {
        "14",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "14"}},
        1,
        nullptr,
    },
    {
        "15",
        (FeatureEntry::FeatureParam[]){{"MaxZeroSuggestMatches", "15"}},
        1,
        nullptr,
    },
};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches7[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "7"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches9[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "9"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         base::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         base::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         base::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         base::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"7 matches", kOmniboxUIMaxAutocompleteMatches7,
         base::size(kOmniboxUIMaxAutocompleteMatches7), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         base::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"9 matches", kOmniboxUIMaxAutocompleteMatches9,
         base::size(kOmniboxUIMaxAutocompleteMatches9), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         base::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         base::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::FeatureParam
    kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout3s[] = {
        {omnibox::kDefaultTypedNavigationsToHttpsTimeoutParam, "3s"}};
const FeatureEntry::FeatureParam
    kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout10s[] = {
        {omnibox::kDefaultTypedNavigationsToHttpsTimeoutParam, "10s"}};
const FeatureEntry::FeatureVariation
    kOmniboxDefaultTypedNavigationsToHttpsVariations[] = {
        {"3 second timeout",
         kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout3s,
         base::size(kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout3s),
         nullptr},
        {"10 second timeout",
         kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout10s,
         base::size(kOmniboxDefaultTypedNavigationsToHttpsVariationsTimeout10s),
         nullptr},
};

const FeatureEntry::FeatureParam kOmniboxMaxURLMatches2[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "2"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches3[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches4[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches5[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches6[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "6"}};

const FeatureEntry::FeatureVariation kOmniboxMaxURLMatchesVariations[] = {
    {"2 matches", kOmniboxMaxURLMatches2, base::size(kOmniboxMaxURLMatches2),
     nullptr},
    {"3 matches", kOmniboxMaxURLMatches3, base::size(kOmniboxMaxURLMatches3),
     nullptr},
    {"4 matches", kOmniboxMaxURLMatches4, base::size(kOmniboxMaxURLMatches4),
     nullptr},
    {"5 matches", kOmniboxMaxURLMatches5, base::size(kOmniboxMaxURLMatches5),
     nullptr},
    {"6 matches", kOmniboxMaxURLMatches6, base::size(kOmniboxMaxURLMatches6),
     nullptr}};

const FeatureEntry::FeatureVariation
    kOmniboxDynamicMaxAutocompleteVariations[] = {
        {
            "9 suggestions if 0 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "0"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}},
            2,
            nullptr,
        },
        {
            "9 suggestions if 1 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "1"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}},
            2,
            nullptr,
        },
        {
            "9 suggestions if 2 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "2"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}},
            2,
            nullptr,
        },
        {
            "10 suggestions if 0 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "0"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}},
            2,
            nullptr,
        },
        {
            "10 suggestions if 1 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "1"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}},
            2,
            nullptr,
        },
        {
            "10 suggestions if 2 or less URLs",
            (FeatureEntry::FeatureParam[]){
                {"OmniboxDynamicMaxAutocompleteUrlCutoff", "2"},
                {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}},
            2,
            nullptr,
        }};

const FeatureEntry::FeatureParam kMinimumTabWidthSettingPinned[] = {
    {features::kMinimumTabWidthFeatureParameterName, "54"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingMedium[] = {
    {features::kMinimumTabWidthFeatureParameterName, "72"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingLarge[] = {
    {features::kMinimumTabWidthFeatureParameterName, "140"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingFull[] = {
    {features::kMinimumTabWidthFeatureParameterName, "256"}};

const FeatureEntry::FeatureVariation kTabScrollingVariations[] = {
    {" - tabs shrink to pinned tab width", kMinimumTabWidthSettingPinned,
     base::size(kMinimumTabWidthSettingPinned), nullptr},
    {" - tabs shrink to a medium width", kMinimumTabWidthSettingMedium,
     base::size(kMinimumTabWidthSettingMedium), nullptr},
    {" - tabs shrink to a large width", kMinimumTabWidthSettingLarge,
     base::size(kMinimumTabWidthSettingLarge), nullptr},
    {" - tabs don't shrink", kMinimumTabWidthSettingFull,
     base::size(kMinimumTabWidthSettingFull), nullptr}};

const FeatureEntry::FeatureParam kTabHoverCardImagesOptimizationCaptureSpeed[] =
    {{features::kTabHoverCardImagesNotReadyDelayParameterName, "0"},
     {features::kTabHoverCardImagesLoadingDelayParameterName, "0"},
     {features::kTabHoverCardImagesLoadedDelayParameterName, "0"}};
const FeatureEntry::FeatureParam
    kTabHoverCardImagesOptimizationResourceUsage[] = {
        {features::kTabHoverCardImagesNotReadyDelayParameterName, "800"},
        {features::kTabHoverCardImagesLoadingDelayParameterName, "300"},
        {features::kTabHoverCardImagesLoadedDelayParameterName, "300"}};
const FeatureEntry::FeatureParam
    kTabHoverCardImagesImmediatePlaceholderCrossfade[] = {
        {features::kTabHoverCardImagesNotReadyDelayParameterName, "500"},
        {features::kTabHoverCardImagesLoadingDelayParameterName, "100"},
        {features::kTabHoverCardImagesLoadedDelayParameterName, "0"},
        {features::kTabHoverCardImagesCrossfadePreviewAtParameterName, "0.0"}};
const FeatureEntry::FeatureParam
    kTabHoverCardImagesEarlySlidePlaceholderCrossfade[] = {
        {features::kTabHoverCardImagesNotReadyDelayParameterName, "500"},
        {features::kTabHoverCardImagesLoadingDelayParameterName, "100"},
        {features::kTabHoverCardImagesLoadedDelayParameterName, "0"},
        {features::kTabHoverCardImagesCrossfadePreviewAtParameterName, "0.25"}};
const FeatureEntry::FeatureParam
    kTabHoverCardImagesMidSlidePlaceholderCrossfade[] = {
        {features::kTabHoverCardImagesNotReadyDelayParameterName, "500"},
        {features::kTabHoverCardImagesLoadingDelayParameterName, "100"},
        {features::kTabHoverCardImagesLoadedDelayParameterName, "0"},
        {features::kTabHoverCardImagesCrossfadePreviewAtParameterName, "0.5"}};
const FeatureEntry::FeatureParam kTabHoverCardImagesLatePlaceholderCrossfade[] =
    {{features::kTabHoverCardImagesNotReadyDelayParameterName, "500"},
     {features::kTabHoverCardImagesLoadingDelayParameterName, "100"},
     {features::kTabHoverCardImagesLoadedDelayParameterName, "0"},
     {features::kTabHoverCardImagesCrossfadePreviewAtParameterName, "1.0"}};

const FeatureEntry::FeatureVariation kTabHoverCardImagesVariations[] = {
    {" capture speed", kTabHoverCardImagesOptimizationCaptureSpeed,
     base::size(kTabHoverCardImagesOptimizationCaptureSpeed), nullptr},
    {" resource usage", kTabHoverCardImagesOptimizationResourceUsage,
     base::size(kTabHoverCardImagesOptimizationResourceUsage), nullptr},
    {" immediate placeholder crossfade",
     kTabHoverCardImagesImmediatePlaceholderCrossfade,
     base::size(kTabHoverCardImagesImmediatePlaceholderCrossfade), nullptr},
    {" early-transition placeholder crossfade",
     kTabHoverCardImagesEarlySlidePlaceholderCrossfade,
     base::size(kTabHoverCardImagesEarlySlidePlaceholderCrossfade), nullptr},
    {" mid-transition placeholder crossfade",
     kTabHoverCardImagesMidSlidePlaceholderCrossfade,
     base::size(kTabHoverCardImagesMidSlidePlaceholderCrossfade), nullptr},
    {" placeholder crossfade on land",
     kTabHoverCardImagesLatePlaceholderCrossfade,
     base::size(kTabHoverCardImagesImmediatePlaceholderCrossfade), nullptr}};

const FeatureEntry::FeatureParam kPromoBrowserCommandUnknownCommandParam[] = {
    {features::kPromoBrowserCommandIdParam, "0"}};
const FeatureEntry::FeatureParam
    kPromoBrowserCommandOpenSafetyCheckCommandParam[] = {
        {features::kPromoBrowserCommandIdParam, "1"}};
const FeatureEntry::FeatureParam
    kPromoBrowserCommandOpenSafeBrowsingSettingsEnhancedProtectionCommandParam
        [] = {{features::kPromoBrowserCommandIdParam, "2"}};
const FeatureEntry::FeatureVariation kPromoBrowserCommandsVariations[] = {
    {"- Unknown Command", kPromoBrowserCommandUnknownCommandParam,
     base::size(kPromoBrowserCommandUnknownCommandParam), nullptr},
    {"- Open Safety Check", kPromoBrowserCommandOpenSafetyCheckCommandParam,
     base::size(kPromoBrowserCommandOpenSafetyCheckCommandParam), nullptr},
    {"- Open Safe Browsing Enhanced Protection Settings",
     kPromoBrowserCommandOpenSafeBrowsingSettingsEnhancedProtectionCommandParam,
     base::size(
         kPromoBrowserCommandOpenSafeBrowsingSettingsEnhancedProtectionCommandParam),
     nullptr}};
#if !defined(OS_ANDROID)

const FeatureEntry::FeatureParam kNtpChromeCartModuleFakeData[] = {
    {ntp_features::kNtpChromeCartModuleDataParam, "fake"},
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, "true"}};
const FeatureEntry::FeatureParam kNtpChromeCartModuleAbandonedCartDiscount[] = {
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, "true"},
    {"partner-merchant-pattern",
     "(electronicexpress.com|zazzle.com|wish.com|homesquare.com)"}};
const FeatureEntry::FeatureParam kNtpChromeCartModuleHeuristicsImprovement[] = {
    {ntp_features::kNtpChromeCartModuleHeuristicsImprovementParam, "true"}};
const FeatureEntry::FeatureVariation kNtpChromeCartModuleVariations[] = {
    {"- Fake Data And Discount", kNtpChromeCartModuleFakeData,
     base::size(kNtpChromeCartModuleFakeData), nullptr},
    {"- Abandoned Cart Discount", kNtpChromeCartModuleAbandonedCartDiscount,
     base::size(kNtpChromeCartModuleAbandonedCartDiscount), nullptr},
    {"- Heuristics Improvement", kNtpChromeCartModuleHeuristicsImprovement,
     base::size(kNtpChromeCartModuleHeuristicsImprovement), nullptr},
};

const FeatureEntry::FeatureParam kNtpRecipeTasksModuleFakeData[] = {
    {ntp_features::kNtpRecipeTasksModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpRecipeTasksModuleVariations[] = {
    {"- Fake Data", kNtpRecipeTasksModuleFakeData,
     base::size(kNtpRecipeTasksModuleFakeData), nullptr},
};

const FeatureEntry::FeatureParam kNtpShoppingTasksModuleFakeData[] = {
    {ntp_features::kNtpShoppingTasksModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpShoppingTasksModuleVariations[] = {
    {"- Fake Data", kNtpShoppingTasksModuleFakeData,
     base::size(kNtpShoppingTasksModuleFakeData),
     "t3329139" /* variation_id */},
};

const FeatureEntry::FeatureParam kNtpDriveModuleFakeData[] = {
    {ntp_features::kNtpDriveModuleDataParam, "fake"}};
const FeatureEntry::FeatureParam kNtpDriveModuleManagedUsersOnly[] = {
    {ntp_features::kNtpDriveModuleManagedUsersOnlyParam, "true"}};
const FeatureEntry::FeatureVariation kNtpDriveModuleVariations[] = {
    {"- Fake Data", kNtpDriveModuleFakeData,
     base::size(kNtpDriveModuleFakeData), nullptr},
    {"- Managed Users Only", kNtpDriveModuleManagedUsersOnly,
     base::size(kNtpDriveModuleManagedUsersOnly), nullptr},
};
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kEnterpriseRealtimeExtensionRequestParam[] = {
    {"with_erp", "true"}};
const FeatureEntry::FeatureVariation
    kEnterpriseRealtimeExtensionRequestVariation[] = {
        {"with encrypted reporting pipeline",
         kEnterpriseRealtimeExtensionRequestParam,
         base::size(kEnterpriseRealtimeExtensionRequestParam), nullptr}};
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishHeuristic[] = {
    {language::kOverrideModelKey, language::kOverrideModelHeuristicValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishGeo[] = {
    {language::kOverrideModelKey, language::kOverrideModelGeoValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishBackoff[] = {
    {language::kOverrideModelKey, language::kOverrideModelDefaultValue},
    {language::kEnforceRankerKey, "false"},
    {language::kBackoffThresholdKey, "0"}};
const FeatureEntry::FeatureVariation
    kTranslateForceTriggerOnEnglishVariations[] = {
        {"(Heuristic model without Ranker)",
         kTranslateForceTriggerOnEnglishHeuristic,
         base::size(kTranslateForceTriggerOnEnglishHeuristic), nullptr},
        {"(Geo model without Ranker)", kTranslateForceTriggerOnEnglishGeo,
         base::size(kTranslateForceTriggerOnEnglishGeo), nullptr},
        {"(Zero threshold)", kTranslateForceTriggerOnEnglishBackoff,
         base::size(kTranslateForceTriggerOnEnglishBackoff), nullptr}};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kOverridePrefsForHrefTranslateForceAuto[] = {
    {translate::kForceAutoTranslateKey, "true"}};

const FeatureEntry::FeatureVariation
    kOverrideLanguagePrefsForHrefTranslateVariations[] = {
        {"(Force automatic translation of blocked languages for hrefTranslate)",
         kOverridePrefsForHrefTranslateForceAuto,
         base::size(kOverridePrefsForHrefTranslateForceAuto), nullptr}};

const FeatureEntry::FeatureVariation
    kOverrideSitePrefsForHrefTranslateVariations[] = {
        {"(Force automatic translation of blocked sites for hrefTranslate)",
         kOverridePrefsForHrefTranslateForceAuto,
         base::size(kOverridePrefsForHrefTranslateForceAuto), nullptr}};

const FeatureEntry::FeatureParam
    kOverrideUnsupportedPageLanguageForHrefTranslateForceAuto[] = {
        {"force-auto-translate-for-unsupported-page-language", "true"}};

const FeatureEntry::FeatureVariation
    kOverrideUnsupportedPageLanguageForHrefTranslateVariations[] = {
        {"(Force automatic translation of pages with unknown language for "
         "hrefTranslate)",
         kOverrideUnsupportedPageLanguageForHrefTranslateForceAuto,
         base::size(kOverrideUnsupportedPageLanguageForHrefTranslateForceAuto),
         nullptr}};

const FeatureEntry::FeatureParam
    kOverrideSimilarLanguagesForHrefTranslateForceAuto[] = {
        {"force-auto-translate-for-similar-languages", "true"}};

const FeatureEntry::FeatureVariation
    kOverrideSimilarLanguagesForHrefTranslateVariations[] = {
        {"(Force automatic translation of pages with the same language as the "
         "target language for hrefTranslate)",
         kOverrideSimilarLanguagesForHrefTranslateForceAuto,
         base::size(kOverrideSimilarLanguagesForHrefTranslateForceAuto),
         nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kExploreSitesExperimental = {
    chrome::android::explore_sites::kExploreSitesVariationParameterName,
    chrome::android::explore_sites::kExploreSitesVariationExperimental};
const FeatureEntry::FeatureParam kExploreSitesDenseTitleBottom[] = {
    {chrome::android::explore_sites::kExploreSitesDenseVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesDenseVariationDenseTitleBottom},
};
const FeatureEntry::FeatureParam kExploreSitesDenseTitleRight[] = {
    {chrome::android::explore_sites::kExploreSitesDenseVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesDenseVariationDenseTitleRight},
};
const FeatureEntry::FeatureVariation kExploreSitesVariations[] = {
    {"Experimental", &kExploreSitesExperimental, 1, nullptr},
    {"Dense Title Bottom", kExploreSitesDenseTitleBottom,
     base::size(kExploreSitesDenseTitleBottom), nullptr},
    {"Dense Title Right", kExploreSitesDenseTitleRight,
     base::size(kExploreSitesDenseTitleRight), nullptr}};
const FeatureEntry::FeatureParam kLongpressResolvePreserveTap = {
    contextual_search::kLongpressResolveParamName,
    contextual_search::kLongpressResolvePreserveTap};
const FeatureEntry::FeatureVariation kLongpressResolveVariations[] = {
    {"and preserve Tap behavior", &kLongpressResolvePreserveTap, 1, nullptr},
};

const FeatureEntry::FeatureParam kRelatedSearchesUrl = {"stamp", "1Ru"};
const FeatureEntry::FeatureParam kRelatedSearchesContent = {"stamp", "1Rc"};
const FeatureEntry::FeatureVariation kRelatedSearchesVariations[] = {
    {"from URL", &kRelatedSearchesUrl, 1, nullptr},
    {"from content", &kRelatedSearchesContent, 1, nullptr},
};
const FeatureEntry::FeatureParam kRelatedSearchesUiVerbose = {"verbosity", "v"};
const FeatureEntry::FeatureParam kRelatedSearchesUiExtreme = {"verbosity", "x"};
const FeatureEntry::FeatureVariation kRelatedSearchesUiVariations[] = {
    {"verbose", &kRelatedSearchesUiVerbose, 1, nullptr},
    {"extreme", &kRelatedSearchesUiExtreme, 1, nullptr},
};

const FeatureEntry::FeatureParam kRelatedSearchesInBarNoShowDefaultChip = {
    "default_query_chip", "false"};
const FeatureEntry::FeatureParam kRelatedSearchesInBarShowDefaultChip = {
    "default_query_chip", "true"};
const FeatureEntry::FeatureParam
    kRelatedSearchesInBarShowDefaultChipWith110SpEllipsis[] = {
        {"default_query_chip", "true"},
        {"default_query_max_width_sp", "110"}};
const FeatureEntry::FeatureParam
    kRelatedSearchesInBarShowDefaultChipWith115SpEllipsis[] = {
        {"default_query_chip", "true"},
        {"default_query_max_width_sp", "115"}};
const FeatureEntry::FeatureParam
    kRelatedSearchesInBarShowDefaultChipWith120SpEllipsis[] = {
        {"default_query_chip", "true"},
        {"default_query_max_width_sp", "120"}};
const FeatureEntry::FeatureVariation kRelatedSearchesInBarVariations[] = {
    {"without default query chip", &kRelatedSearchesInBarNoShowDefaultChip, 1,
     nullptr},
    {"with default query chip", &kRelatedSearchesInBarShowDefaultChip, 1,
     nullptr},
    {"with 110sp ellipsized default query chip",
     kRelatedSearchesInBarShowDefaultChipWith110SpEllipsis,
     base::size(kRelatedSearchesInBarShowDefaultChipWith110SpEllipsis),
     nullptr},
    {"with 115sp ellipsized default query chip",
     kRelatedSearchesInBarShowDefaultChipWith115SpEllipsis,
     base::size(kRelatedSearchesInBarShowDefaultChipWith115SpEllipsis),
     nullptr},
    {"with 120sp ellipsized default query chip",
     kRelatedSearchesInBarShowDefaultChipWith120SpEllipsis,
     base::size(kRelatedSearchesInBarShowDefaultChipWith120SpEllipsis),
     nullptr},
};

const FeatureEntry::FeatureParam kRelatedSearchesAlternateUxNoShowDefaultChip =
    {"default_query_chip", "false"};
const FeatureEntry::FeatureParam kRelatedSearchesAlternateUxShowDefaultChip = {
    "default_query_chip", "true"};
const FeatureEntry::FeatureParam
    kRelatedSearchesAlternateUxShowDefaultChipWith110SpEllipsis[] = {
        {"default_query_chip", "true"},
        {"default_query_max_width_sp", "110"}};
const FeatureEntry::FeatureParam
    kRelatedSearchesAlternateUxShowDefaultChipWith115SpEllipsis[] = {
        {"default_query_chip", "true"},
        {"default_query_max_width_sp", "115"}};
const FeatureEntry::FeatureParam
    kRelatedSearchesAlternateUxShowDefaultChipWith120SpEllipsis[] = {
        {"default_query_chip", "true"},
        {"default_query_max_width_sp", "120"}};
const FeatureEntry::FeatureVariation kRelatedSearchesAlternateUxVariations[] = {
    {"without default query chip",
     &kRelatedSearchesAlternateUxNoShowDefaultChip, 1, nullptr},
    {"with default query chip", &kRelatedSearchesAlternateUxShowDefaultChip, 1,
     nullptr},
    {"with 110sp ellipsized default query chip",
     kRelatedSearchesAlternateUxShowDefaultChipWith110SpEllipsis,
     base::size(kRelatedSearchesAlternateUxShowDefaultChipWith110SpEllipsis),
     nullptr},
    {"with 115sp ellipsized default query chip",
     kRelatedSearchesAlternateUxShowDefaultChipWith115SpEllipsis,
     base::size(kRelatedSearchesAlternateUxShowDefaultChipWith115SpEllipsis),
     nullptr},
    {"with 120sp ellipsized default query chip",
     kRelatedSearchesAlternateUxShowDefaultChipWith120SpEllipsis,
     base::size(kRelatedSearchesAlternateUxShowDefaultChipWith120SpEllipsis),
     nullptr},
};

#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kResamplingInputEventsLSQEnabled[] = {
    {"predictor", features::kPredictorNameLsq}};
const FeatureEntry::FeatureParam kResamplingInputEventsKalmanEnabled[] = {
    {"predictor", features::kPredictorNameKalman}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearFirstEnabled[] = {
    {"predictor", features::kPredictorNameLinearFirst}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearSecondEnabled[] = {
    {"predictor", features::kPredictorNameLinearSecond}};
const FeatureEntry::FeatureParam
    kResamplingInputEventsLinearResamplingEnabled[] = {
        {"predictor", features::kPredictorNameLinearResampling}};

const FeatureEntry::FeatureVariation kResamplingInputEventsFeatureVariations[] =
    {{features::kPredictorNameLsq, kResamplingInputEventsLSQEnabled,
      base::size(kResamplingInputEventsLSQEnabled), nullptr},
     {features::kPredictorNameKalman, kResamplingInputEventsKalmanEnabled,
      base::size(kResamplingInputEventsKalmanEnabled), nullptr},
     {features::kPredictorNameLinearFirst,
      kResamplingInputEventsLinearFirstEnabled,
      base::size(kResamplingInputEventsLinearFirstEnabled), nullptr},
     {features::kPredictorNameLinearSecond,
      kResamplingInputEventsLinearSecondEnabled,
      base::size(kResamplingInputEventsLinearSecondEnabled), nullptr},
     {features::kPredictorNameLinearResampling,
      kResamplingInputEventsLinearResamplingEnabled,
      base::size(kResamplingInputEventsLinearResamplingEnabled), nullptr}};

const FeatureEntry::FeatureParam
    kResamplingScrollEventsPredictionTimeBasedEnabled[] = {
        {"mode", features::kPredictionTypeTimeBased},
        {"latency", features::kPredictionTypeDefaultTime}};
const FeatureEntry::FeatureParam
    kResamplingScrollEventsPredictionFramesBasedEnabled[] = {
        {"mode", features::kPredictionTypeFramesBased},
        {"latency", features::kPredictionTypeDefaultFramesRatio}};
const FeatureEntry::FeatureVariation
    kResamplingScrollEventsExperimentalPredictionVariations[] = {
        {features::kPredictionTypeTimeBased,
         kResamplingScrollEventsPredictionTimeBasedEnabled,
         base::size(kResamplingScrollEventsPredictionTimeBasedEnabled),
         nullptr},
        {features::kPredictionTypeFramesBased,
         kResamplingScrollEventsPredictionFramesBasedEnabled,
         base::size(kResamplingScrollEventsPredictionFramesBasedEnabled),
         nullptr}};

const FeatureEntry::FeatureParam kFilteringPredictionEmptyFilterEnabled[] = {
    {"filter", features::kFilterNameEmpty}};
const FeatureEntry::FeatureParam kFilteringPredictionOneEuroFilterEnabled[] = {
    {"filter", features::kFilterNameOneEuro}};

const FeatureEntry::FeatureVariation kFilteringPredictionFeatureVariations[] = {
    {features::kFilterNameEmpty, kFilteringPredictionEmptyFilterEnabled,
     base::size(kFilteringPredictionEmptyFilterEnabled), nullptr},
    {features::kFilterNameOneEuro, kFilteringPredictionOneEuroFilterEnabled,
     base::size(kFilteringPredictionOneEuroFilterEnabled), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_Immediate[] = {
    {"tab_switcher_on_return_time_ms", "0"}};
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_1Minute[] = {
    {"tab_switcher_on_return_time_ms", "60000"}};
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_30Minutes[] = {
    {"tab_switcher_on_return_time_ms", "1800000"}};
const FeatureEntry::FeatureParam kTabSwitcherOnReturn_60Minutes[] = {
    {"tab_switcher_on_return_time_ms", "3600000"}};
const FeatureEntry::FeatureVariation kTabSwitcherOnReturnVariations[] = {
    {"Immediate", kTabSwitcherOnReturn_Immediate,
     base::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"1 minute", kTabSwitcherOnReturn_1Minute,
     base::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"30 minutes", kTabSwitcherOnReturn_30Minutes,
     base::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"60 minutes", kTabSwitcherOnReturn_60Minutes,
     base::size(kTabSwitcherOnReturn_60Minutes), nullptr},
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabVariation[] = {
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabTile[] = {
    {"tab_grid_layout_android_new_tab_tile", "NewTabTile"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_TallNTV[] = {
    {"thumbnail_aspect_ratio", "0.85"},
    {"allow_to_refetch", "true"},
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"enable_launch_polish", "true"},
    {"enable_launch_bug_fix", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_SearchChip[] = {
    {"enable_search_term_chip", "true"}};

const FeatureEntry::FeatureParam kCommercePriceTracking_PriceAlerts[] = {
    {"enable_price_tracking", "true"},
    {"price_tracking_with_optimization_guide", "false"}};

const FeatureEntry::FeatureParam
    kCommercePriceTracking_PriceAlerts_WithOptimizationGuide[] = {
        {"enable_price_tracking", "true"},
        {"price_tracking_with_optimization_guide", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_TabGroupAutoCreation[] =
    {{"enable_tab_group_auto_creation", "false"}};

const FeatureEntry::FeatureParam kCommercePriceTracking_PriceNotifications[] = {
    {"enable_price_notification", "true"}};

const FeatureEntry::FeatureVariation kTabGridLayoutAndroidVariations[] = {
    {"New Tab Variation", kTabGridLayoutAndroid_NewTabVariation,
     base::size(kTabGridLayoutAndroid_NewTabVariation), nullptr},
    {"New Tab Tile", kTabGridLayoutAndroid_NewTabTile,
     base::size(kTabGridLayoutAndroid_NewTabTile), nullptr},
    {"Tall NTV", kTabGridLayoutAndroid_TallNTV,
     base::size(kTabGridLayoutAndroid_TallNTV), nullptr},
    {"Search term chip", kTabGridLayoutAndroid_SearchChip,
     base::size(kTabGridLayoutAndroid_SearchChip), nullptr},
    {"Without auto group", kTabGridLayoutAndroid_TabGroupAutoCreation,
     base::size(kTabGridLayoutAndroid_TabGroupAutoCreation), nullptr},
};

const FeatureEntry::FeatureVariation kCommercePriceTrackingAndroidVariations[] =
    {
        {"Price alerts", kCommercePriceTracking_PriceAlerts,
         base::size(kCommercePriceTracking_PriceAlerts), nullptr},
        {"Price alerts with OptimizationGuide",
         kCommercePriceTracking_PriceAlerts_WithOptimizationGuide,
         base::size(kCommercePriceTracking_PriceAlerts_WithOptimizationGuide),
         nullptr},
        {"Price notifications", kCommercePriceTracking_PriceNotifications,
         base::size(kCommercePriceTracking_PriceNotifications), nullptr},
};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface[] = {
    {"start_surface_variation", "single"},
    {"show_tabs_in_mru_order", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurfaceFinale[] = {
    {"start_surface_variation", "single"},
    {"omnibox_focused_on_new_tab", "true"},
    {"home_button_on_grid_tab_switcher", "true"},
    {"new_home_surface_from_home_button", "hide_tab_switcher_only"},
    {"hide_switch_when_no_incognito_tabs", "true"},
    {"show_tabs_in_mru_order", "true"},
    {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_SingleSurfaceFinale_NTPTilesOnOmnibox[] = {
        {"start_surface_variation", "single"},
        {"omnibox_focused_on_new_tab", "true"},
        {"show_ntp_tiles_on_omnibox", "true"},
        {"home_button_on_grid_tab_switcher", "true"},
        {"new_home_surface_from_home_button", "hide_mv_tiles_and_tab_switcher"},
        {"hide_switch_when_no_incognito_tabs", "true"},
        {"show_tabs_in_mru_order", "true"},
        {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"exclude_mv_tiles", "true"},
    {"open_ntp_instead_of_start", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2Finale[] =
    {{"start_surface_variation", "single"},
     {"show_last_active_tab_only", "true"},
     {"omnibox_focused_on_new_tab", "true"},
     {"home_button_on_grid_tab_switcher", "true"},
     {"new_home_surface_from_home_button", "hide_tab_switcher_only"},
     {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_SingleSurface_V2Finale_NTPTilesOnOmnibox[] = {
        {"start_surface_variation", "single"},
        {"show_last_active_tab_only", "true"},
        {"omnibox_focused_on_new_tab", "true"},
        {"show_ntp_tiles_on_omnibox", "true"},
        {"home_button_on_grid_tab_switcher", "true"},
        {"new_home_surface_from_home_button", "hide_mv_tiles_and_tab_switcher"},
        {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurfaceSingleTab[] =
    {{"start_surface_variation", "single"},
     {"show_last_active_tab_only", "true"},
     {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureVariation kStartSurfaceAndroidVariations[] = {
    {"Single Surface", kStartSurfaceAndroid_SingleSurface,
     base::size(kStartSurfaceAndroid_SingleSurface), nullptr},
    {"Single Surface Finale", kStartSurfaceAndroid_SingleSurfaceFinale,
     base::size(kStartSurfaceAndroid_SingleSurfaceFinale), nullptr},
    {"Single Surface Finale + NTP tiles on Omnibox",
     kStartSurfaceAndroid_SingleSurfaceFinale_NTPTilesOnOmnibox,
     base::size(kStartSurfaceAndroid_SingleSurfaceFinale_NTPTilesOnOmnibox),
     nullptr},
    {"Single Surface V2", kStartSurfaceAndroid_SingleSurface_V2,
     base::size(kStartSurfaceAndroid_SingleSurface_V2), nullptr},
    {"Single Surface V2 Finale", kStartSurfaceAndroid_SingleSurface_V2Finale,
     base::size(kStartSurfaceAndroid_SingleSurface_V2Finale), nullptr},
    {"Single Surface V2 Finale + NTP tiles on Omnibox",
     kStartSurfaceAndroid_SingleSurface_V2Finale_NTPTilesOnOmnibox,
     base::size(kStartSurfaceAndroid_SingleSurface_V2Finale_NTPTilesOnOmnibox),
     nullptr},
    {"Single Surface + Single Tab", kStartSurfaceAndroid_SingleSurfaceSingleTab,
     base::size(kStartSurfaceAndroid_SingleSurfaceSingleTab), nullptr},
};

const FeatureEntry::FeatureParam kWebFeed_accelerator[] = {
    {"recommendation_style", "accelerator"}};

const FeatureEntry::FeatureParam kWebFeed_IPH[] = {
    {"recommendation_style", "IPH"}};

const FeatureEntry::FeatureVariation kWebFeedVariations[] = {
    {"accelerator recommendations", kWebFeed_accelerator,
     base::size(kWebFeed_accelerator), nullptr},
    {"IPH recommendations", kWebFeed_IPH, base::size(kWebFeed_IPH), nullptr},
};

const FeatureEntry::FeatureParam kConditionalTabStripAndroid_Immediate[] = {
    {"conditional_tab_strip_session_time_ms", "0"}};
const FeatureEntry::FeatureParam kConditionalTabStripAndroid_60Minutes[] = {
    {"conditional_tab_strip_session_time_ms", "3600000"}};
const FeatureEntry::FeatureVariation kConditionalTabStripAndroidVariations[] = {
    {"Immediate", kConditionalTabStripAndroid_Immediate,
     base::size(kConditionalTabStripAndroid_Immediate), nullptr},
    {"60 minutes", kConditionalTabStripAndroid_60Minutes,
     base::size(kConditionalTabStripAndroid_60Minutes), nullptr},
};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kAddToHomescreen_UseTextBubble[] = {
    {"use_text_bubble", "true"}};
const FeatureEntry::FeatureParam kAddToHomescreen_UseMessage[] = {
    {"use_message", "true"}};

const FeatureEntry::FeatureVariation kAddToHomescreenIPHVariations[] = {
    {"Use Text Bubble", kAddToHomescreen_UseTextBubble,
     base::size(kAddToHomescreen_UseTextBubble), nullptr},
    {"Use Message", kAddToHomescreen_UseMessage,
     base::size(kAddToHomescreen_UseMessage), nullptr}};
#endif

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowAll[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowAll}};
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowOne[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowOne}};

const FeatureEntry::FeatureVariation
    kAutofillUseMobileLabelDisambiguationVariations[] = {
        {"(show all)", kAutofillUseMobileLabelDisambiguationShowAll,
         base::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         base::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kHomepagePromoCardLarge[] = {
    {"promo-card-variation", "Large"}};
const FeatureEntry::FeatureParam kHomepagePromoCardCompact[] = {
    {"promo-card-variation", "Compact"}};
const FeatureEntry::FeatureParam kHomepagePromoCardSlim[] = {
    {"promo-card-variation", "Slim"}};
const FeatureEntry::FeatureParam kHomepagePromoCardSuppressing[] = {
    {"promo-card-variation", "Compact"},
    {"suppressing_sign_in_promo", "true"}};

const FeatureEntry::FeatureVariation kHomepagePromoCardVariations[] = {
    {"Large", kHomepagePromoCardLarge, base::size(kHomepagePromoCardLarge),
     nullptr},
    {"Compact", kHomepagePromoCardCompact,
     base::size(kHomepagePromoCardCompact), nullptr},
    {"Slim", kHomepagePromoCardSlim, base::size(kHomepagePromoCardSlim),
     nullptr},
    {"Compact_SuppressingSignInPromo", kHomepagePromoCardSuppressing,
     base::size(kHomepagePromoCardSuppressing), nullptr}};
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kLensCameraAssistedSearchLensButtonStart[] = {
    {"searchBoxStartVariantForLensCameraAssistedSearch", "true"}};

const FeatureEntry::FeatureParam kLensCameraAssistedSearchLensButtonEnd[] = {
    {"searchBoxStartVariantForLensCameraAssistedSearch", "false"}};

const FeatureEntry::FeatureParam
    kLensCameraAssistedSkipAgsaVersionCheckEnabled[] = {
        {"skipAgsaVersionCheck", "true"}};

const FeatureEntry::FeatureParam
    kLensCameraAssistedSkipAgsaVersionCheckDisabled[] = {
        {"skipAgsaVersionCheck", "false"}};

const FeatureEntry::FeatureVariation kLensCameraAssistedSearchVariations[] = {
    {"(Lens then Mic)", kLensCameraAssistedSearchLensButtonStart,
     base::size(kLensCameraAssistedSearchLensButtonStart), nullptr},
    {"(Mic then Lens)", kLensCameraAssistedSearchLensButtonEnd,
     base::size(kLensCameraAssistedSearchLensButtonEnd), nullptr},
    {"(without AGSA version check)",
     kLensCameraAssistedSkipAgsaVersionCheckEnabled,
     base::size(kLensCameraAssistedSkipAgsaVersionCheckEnabled), nullptr},
    {"(with AGSA version check )",
     kLensCameraAssistedSkipAgsaVersionCheckDisabled,
     base::size(kLensCameraAssistedSkipAgsaVersionCheckDisabled), nullptr}};

const FeatureEntry::FeatureParam kLensContextMenuTranslateHideRemoveIcon[] = {
    {"hideChipRemoveIcon", "true"}};

const FeatureEntry::FeatureVariation kLensContextMenuTranslateVariations[] = {
    {"(Hide Remove Icon)", kLensContextMenuTranslateHideRemoveIcon,
     base::size(kLensContextMenuTranslateHideRemoveIcon), nullptr},
};

#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kLazyFrameLoadingAutomatic[] = {
    {"automatic-lazy-load-frames-enabled", "true"},
    {"restrict-lazy-load-frames-to-data-saver-only", "false"},
};

const FeatureEntry::FeatureVariation kLazyFrameLoadingVariations[] = {
    {"(Automatically lazily load where safe even if not marked "
     "'loading=lazy')",
     kLazyFrameLoadingAutomatic, base::size(kLazyFrameLoadingAutomatic),
     nullptr}};

const FeatureEntry::FeatureParam kLazyImageLoadingAutomatic[] = {
    {"automatic-lazy-load-images-enabled", "true"},
    {"restrict-lazy-load-images-to-data-saver-only", "false"},
};

const FeatureEntry::FeatureVariation kLazyImageLoadingVariations[] = {
    {"(Automatically lazily load where safe even if not marked "
     "'loading=lazy')",
     kLazyImageLoadingAutomatic, base::size(kLazyImageLoadingAutomatic),
     nullptr}};

const FeatureEntry::Choice kNotificationSchedulerChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::
         kNotificationSchedulerImmediateBackgroundTaskDescription,
     notifications::switches::kNotificationSchedulerImmediateBackgroundTask,
     ""},
};

#if defined(OS_ANDROID)

const FeatureEntry::FeatureParam kIphMicToolbarGenericMessage[] = {
    {"generic_message", "true"}};
const FeatureEntry::FeatureParam kIphMicToolbarExampleQuery[] = {
    {"generic_message", "false"}};
const FeatureEntry::FeatureVariation kIphMicToolbarVariations[] = {
    {"generic message", kIphMicToolbarGenericMessage,
     base::size(kIphMicToolbarGenericMessage), nullptr},
    {"example query", kIphMicToolbarExampleQuery,
     base::size(kIphMicToolbarExampleQuery), nullptr},

};

const FeatureEntry::FeatureParam kOmniboxAssistantVoiceSearchGreyMic[] = {
    {"min_agsa_version", "10.95"},
    {"colorful_mic", "false"}};

const FeatureEntry::FeatureParam kOmniboxAssistantVoiceSearchColorfulMic[] = {
    {"min_agsa_version", "10.95"},
    {"colorful_mic", "true"}};

const FeatureEntry::FeatureParam
    kOmniboxAssistantVoiceSearchNoMultiAccountCheck[] = {
        {"min_agsa_version", "10.95"},
        {"colorful_mic", "true"},
        {"enable_multi_account_check", "false"}};

const FeatureEntry::FeatureVariation kOmniboxAssistantVoiceSearchVariations[] =
    {
        {"(grey mic)", kOmniboxAssistantVoiceSearchGreyMic,
         base::size(kOmniboxAssistantVoiceSearchGreyMic), nullptr},
        {"(colorful mic)", kOmniboxAssistantVoiceSearchColorfulMic,
         base::size(kOmniboxAssistantVoiceSearchColorfulMic), nullptr},
        {"(no account check)", kOmniboxAssistantVoiceSearchNoMultiAccountCheck,
         base::size(kOmniboxAssistantVoiceSearchNoMultiAccountCheck), nullptr},
};

const FeatureEntry::FeatureParam
    kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails[] = {
        {"animate_thumbnails", "true"}};
const FeatureEntry::FeatureVariation
    kPhotoPickerVideoSupportFeatureVariations[] = {
        {"(with animated thumbnails)",
         kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails,
         base::size(kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails),
         nullptr}};

// Request Desktop Site on Tablet by default variations.
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets100[] = {
    {"screen_width_dp", "100"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets600[] = {
    {"screen_width_dp", "600"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets768[] = {
    {"screen_width_dp", "768"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets960[] = {
    {"screen_width_dp", "960"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets1024[] = {
    {"screen_width_dp", "1024"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets1280[] = {
    {"screen_width_dp", "1280"},
    {"enabled", "true"}};
const FeatureEntry::FeatureParam kRequestDesktopSiteForTablets1920[] = {
    {"screen_width_dp", "1920"},
    {"enabled", "true"}};
const FeatureEntry::FeatureVariation kRequestDesktopSiteForTabletsVariations[] =
    {{"for 100dp+ screens", kRequestDesktopSiteForTablets100,
      base::size(kRequestDesktopSiteForTablets100), nullptr},
     {"for 600dp+ screens", kRequestDesktopSiteForTablets600,
      base::size(kRequestDesktopSiteForTablets600), nullptr},
     {"for 768dp+ screens", kRequestDesktopSiteForTablets768,
      base::size(kRequestDesktopSiteForTablets768), nullptr},
     {"for 960dp+ screens", kRequestDesktopSiteForTablets960,
      base::size(kRequestDesktopSiteForTablets960), nullptr},
     {"for 1024dp+ screens", kRequestDesktopSiteForTablets1024,
      base::size(kRequestDesktopSiteForTablets1024), nullptr},
     {"for 1280dp+ screens", kRequestDesktopSiteForTablets1280,
      base::size(kRequestDesktopSiteForTablets1280), nullptr},
     {"for 1920dp+ screens", kRequestDesktopSiteForTablets1920,
      base::size(kRequestDesktopSiteForTablets1920), nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureVariation
    kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations[] = {
        {
            "relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            2,
            nullptr,
        },
        {
            "no-delay-relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "0"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            3,
            nullptr,
        }};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam
    kQuietNotificationPromptsWithAdaptiveActivation[] = {
        {QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
         "true"},
        {QuietNotificationPermissionUiConfig::kEnableAbusiveRequestBlocking,
         "true"},
        {QuietNotificationPermissionUiConfig::kEnableAbusiveRequestWarning,
         "true"},
        {QuietNotificationPermissionUiConfig::kEnableCrowdDenyTriggering,
         "true"},
        {QuietNotificationPermissionUiConfig::kCrowdDenyHoldBackChance, "0"}};

// The default "Enabled" option has the semantics of showing the quiet UI
// (animated location bar indicator on Desktop, and mini-infobars on Android),
// but only when the user directly turns it on in Settings. In addition to that,
// expose an option to also enable adaptively turning on the quiet UI after
// three consecutive denies or based on crowd deny verdicts.
const FeatureEntry::FeatureVariation kQuietNotificationPromptsVariations[] = {
    {"(with adaptive activation)",
     kQuietNotificationPromptsWithAdaptiveActivation,
     base::size(kQuietNotificationPromptsWithAdaptiveActivation), nullptr},
};

// TODO(crbug.com/991082,1015377): Remove after proper support for back-forward
// cache is implemented.
const FeatureEntry::FeatureParam kBackForwardCache_ForceCaching[] = {
    {"TimeToLiveInBackForwardCacheInSeconds", "300"},
    {"should_ignore_blocklists", "true"},
    {"enable_same_site", "true"}};

// With this, back-forward cache will be enabled on eligible pages when doing
// same-site navigations (instead of only cross-site navigations).
const FeatureEntry::FeatureParam kBackForwardCache_SameSite[] = {
    {"enable_same_site", "true"}};

const FeatureEntry::FeatureVariation kBackForwardCacheVariations[] = {
    {"same-site support (experimental)", kBackForwardCache_SameSite,
     base::size(kBackForwardCache_SameSite), nullptr},
    {"force caching all pages (experimental)", kBackForwardCache_ForceCaching,
     base::size(kBackForwardCache_ForceCaching), nullptr},
};

const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2250[] = {
        {"lcp-limit-in-ms", "2250"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2000[] = {
        {"lcp-limit-in-ms", "2000"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1750[] = {
        {"lcp-limit-in-ms", "1750"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1500[] = {
        {"lcp-limit-in-ms", "1500"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1250[] = {
        {"lcp-limit-in-ms", "1250"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1000[] = {
        {"lcp-limit-in-ms", "1000"},
        {"intervention-mode", "failure"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2250[] = {
        {"lcp-limit-in-ms", "2250"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2000[] = {
        {"lcp-limit-in-ms", "2000"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1750[] = {
        {"lcp-limit-in-ms", "1750"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1500[] = {
        {"lcp-limit-in-ms", "1500"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1250[] = {
        {"lcp-limit-in-ms", "1250"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureParam
    kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1000[] = {
        {"lcp-limit-in-ms", "1000"},
        {"intervention-mode", "swap"}};
const FeatureEntry::FeatureVariation
    kAlignFontDisplayAutoTimeoutWithLCPGoalVariations[] = {
        {"switch to failure after 2250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2250),
         nullptr},
        {"switch to failure after 2000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure2000),
         nullptr},
        {"switch to failure after 1750ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1750,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1750),
         nullptr},
        {"switch to failure after 1500ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1500,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1500),
         nullptr},
        {"switch to failure after 1250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1250),
         nullptr},
        {"switch to failure after 1000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalFailure1000),
         nullptr},
        {"switch to swap after 2250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2250), nullptr},
        {"switch to swap after 2000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap2000), nullptr},
        {"switch to swap after 1750ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1750,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1750), nullptr},
        {"switch to swap after 1500ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1500,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1500), nullptr},
        {"switch to swap after 1250ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1250,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1250), nullptr},
        {"switch to swap after 1000ms since navigation",
         kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1000,
         base::size(kAlignFontDisplayAutoTimeoutWithLCPGoalSwap1000), nullptr},
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kEnableCrOSActionRecorderChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {ash::switches::kCrOSActionRecorderWithHash,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderWithHash},
    {ash::switches::kCrOSActionRecorderWithoutHash,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderWithoutHash},
    {ash::switches::kCrOSActionRecorderCopyToDownloadDir,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderCopyToDownloadDir},
    {ash::switches::kCrOSActionRecorderDisabled,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderDisabled},
    {ash::switches::kCrOSActionRecorderStructuredDisabled,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderStructuredDisabled},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
const FeatureEntry::Choice kWebOtpBackendChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kWebOtpBackendSmsVerification, switches::kWebOtpBackend,
     switches::kWebOtpBackendSmsVerification},
    {flag_descriptions::kWebOtpBackendUserConsent, switches::kWebOtpBackend,
     switches::kWebOtpBackendUserConsent},
    {flag_descriptions::kWebOtpBackendAuto, switches::kWebOtpBackend,
     switches::kWebOtpBackendAuto},
};

const FeatureEntry::Choice kQueryTilesCountryChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kQueryTilesCountryCodeUS,
     query_tiles::switches::kQueryTilesCountryCode, "US"},
    {flag_descriptions::kQueryTilesCountryCodeIndia,
     query_tiles::switches::kQueryTilesCountryCode, "IN"},
    {flag_descriptions::kQueryTilesCountryCodeBrazil,
     query_tiles::switches::kQueryTilesCountryCode, "BR"},
    {flag_descriptions::kQueryTilesCountryCodeNigeria,
     query_tiles::switches::kQueryTilesCountryCode, "NG"},
    {flag_descriptions::kQueryTilesCountryCodeIndonesia,
     query_tiles::switches::kQueryTilesCountryCode, "ID"},
};

#endif  // defined(OS_ANDROID)

// The choices for --enable-experimental-cookie-features. This really should
// just be a SINGLE_VALUE_TYPE, but it is misleading to have the choices be
// labeled "Disabled"/"Enabled". So instead this is made to be a
// MULTI_VALUE_TYPE with choices "Default"/"Enabled".
const FeatureEntry::Choice kEnableExperimentalCookieFeaturesChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableExperimentalCookieFeatures, ""},
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kFrameThrottleFpsChoices[] = {
    {flag_descriptions::kFrameThrottleFpsDefault, "", ""},
    {flag_descriptions::kFrameThrottleFps5, ash::switches::kFrameThrottleFps,
     "5"},
    {flag_descriptions::kFrameThrottleFps10, ash::switches::kFrameThrottleFps,
     "10"},
    {flag_descriptions::kFrameThrottleFps15, ash::switches::kFrameThrottleFps,
     "15"},
    {flag_descriptions::kFrameThrottleFps20, ash::switches::kFrameThrottleFps,
     "20"},
    {flag_descriptions::kFrameThrottleFps25, ash::switches::kFrameThrottleFps,
     "25"},
    {flag_descriptions::kFrameThrottleFps30, ash::switches::kFrameThrottleFps,
     "30"}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const FeatureEntry::Choice kDrawPredictedPointsChoices[] = {
    {flag_descriptions::kDrawPredictedPointsDefault, "", ""},
    {flag_descriptions::kDraw1PredictedPoint12Ms,
     switches::kDrawPredictedInkPoint, switches::kDraw1Point12Ms},
    {flag_descriptions::kDraw2PredictedPoints6Ms,
     switches::kDrawPredictedInkPoint, switches::kDraw2Points6Ms},
    {flag_descriptions::kDraw1PredictedPoint6Ms,
     switches::kDrawPredictedInkPoint, switches::kDraw1Point6Ms},
    {flag_descriptions::kDraw2PredictedPoints3Ms,
     switches::kDrawPredictedInkPoint, switches::kDraw2Points3Ms}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kForceControlFaceAeChoices[] = {
    {"Default", "", ""},
    {"Enable", media::switches::kForceControlFaceAe, "enable"},
    {"Disable", media::switches::kForceControlFaceAe, "disable"}};
#endif

#if defined(OS_ANDROID)
// The variations of --password-change-in-settings.
const FeatureEntry::FeatureParam
    kPasswordChangeInSettingsVariationWithForcedWarningForEverySite[] = {
        {password_manager::features::
             kPasswordChangeInSettingsWithForcedWarningForEverySite,
         "true"}};

const FeatureEntry::FeatureVariation
    kPasswordChangeInSettingsFeatureVariations[] = {
        {"Force leak warnings for every site in settings.",
         kPasswordChangeInSettingsVariationWithForcedWarningForEverySite,
         base::size(
             kPasswordChangeInSettingsVariationWithForcedWarningForEverySite),
         nullptr}};

// The variations of --password-change-support.
const FeatureEntry::FeatureParam
    kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission[] = {
        {password_manager::features::
             kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission,
         "true"}};

const FeatureEntry::FeatureVariation kPasswordChangeFeatureVariations[] = {
    {"Force dialog after every successful form submission.",
     kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission,
     base::size(
         kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission),
     nullptr}};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kAssistantBetterOnboardingInternalName[] =
    "enable-assistant-better-onboarding";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
const FeatureEntry::FeatureParam
    kSendWebUIJavaScriptErrorReportsVariationSendToStaging[] = {
        {features::kSendWebUIJavaScriptErrorReportsSendToProductionVariation,
         "false"}};

const FeatureEntry::FeatureVariation
    kSendWebUIJavaScriptErrorReportsVariations[] = {
        {"Send reports to staging server.",
         kSendWebUIJavaScriptErrorReportsVariationSendToStaging,
         base::size(kSendWebUIJavaScriptErrorReportsVariationSendToStaging),
         nullptr}};
#endif

#if defined(OS_ANDROID)
// The variations of --metrics-settings-android.
const FeatureEntry::FeatureParam kMetricsSettingsAndroidAlternativeOne[] = {
    {"fre", "1"}};

const FeatureEntry::FeatureParam kMetricsSettingsAndroidAlternativeTwo[] = {
    {"fre", "2"}};

const FeatureEntry::FeatureVariation kMetricsSettingsAndroidVariations[] = {
    {"Alternative FRE 1", kMetricsSettingsAndroidAlternativeOne,
     base::size(kMetricsSettingsAndroidAlternativeOne), nullptr},
    {"Alternative FRE 2", kMetricsSettingsAndroidAlternativeTwo,
     base::size(kMetricsSettingsAndroidAlternativeTwo), nullptr},
};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// SCT Auditing feature variations.
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateNone[] = {
    {"sampling_rate", "0.0"}};
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateAlternativeOne[] = {
    {"sampling_rate", "0.0001"}};
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateAlternativeTwo[] = {
    {"sampling_rate", "0.001"}};

const FeatureEntry::FeatureVariation kSCTAuditingVariations[] = {
    {"Sampling rate 0%", kSCTAuditingSamplingRateNone,
     base::size(kSCTAuditingSamplingRateNone), nullptr},
    {"Sampling rate 0.01%", kSCTAuditingSamplingRateAlternativeOne,
     base::size(kSCTAuditingSamplingRateAlternativeOne), nullptr},
    {"Sampling rate 0.1%", kSCTAuditingSamplingRateAlternativeTwo,
     base::size(kSCTAuditingSamplingRateAlternativeTwo), nullptr},
};
#endif  // !defined(OS_ANDROID)

const FeatureEntry::FeatureParam kCheckOfflineCapabilityWarnOnly[] = {
    {"check_mode", "warn_only"}};
const FeatureEntry::FeatureParam kCheckOfflineCapabilityEnforce[] = {
    {"check_mode", "enforce"}};

const FeatureEntry::FeatureVariation kCheckOfflineCapabilityVariations[] = {
    {"Warn-only", kCheckOfflineCapabilityWarnOnly,
     base::size(kCheckOfflineCapabilityWarnOnly), nullptr},
    {"Enforce", kCheckOfflineCapabilityEnforce,
     base::size(kCheckOfflineCapabilityEnforce), nullptr},
};

const FeatureEntry::FeatureParam kSubresourceRedirectPublicImageHints[] = {
    {"enable_public_image_hints_based_compression", "true"},
    {"enable_subresource_server_redirect", "true"},
    {"enable_login_robots_based_compression", "false"},
};

const FeatureEntry::FeatureParam
    kSubresourceRedirectLoginRobotsBasedCompression[] = {
        {"enable_login_robots_based_compression", "true"},
        {"enable_subresource_server_redirect", "true"},
        {"enable_public_image_hints_based_compression", "false"},
};

const FeatureEntry::FeatureVariation kSubresourceRedirectVariations[] = {
    {"Public image hints based compression",
     kSubresourceRedirectPublicImageHints,
     base::size(kSubresourceRedirectPublicImageHints), nullptr},
    {"robots.txt allowed image compression in non logged-in pages",
     kSubresourceRedirectLoginRobotsBasedCompression,
     base::size(kSubresourceRedirectLoginRobotsBasedCompression), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureVariation
    kOmniboxRichEntitiesInLauncherVariations[] = {
        {"with linked Suggest experiment", {}, 0, "t4461027"},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kCategoricalSearch_Unranked[] = {
    {"ranking", "none"}};

const FeatureEntry::FeatureVariation kCategoricalSearchVariations[] = {
    {"Unranked", kCategoricalSearch_Unranked,
     base::size(kCategoricalSearch_Unranked), nullptr}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

constexpr FeatureEntry::FeatureParam kPlatformProvidedTrustTokenIssuance[] = {
    {"PlatformProvidedTrustTokenIssuance", "true"}};

constexpr FeatureEntry::FeatureVariation
    kPlatformProvidedTrustTokensVariations[] = {
        {"with platform-provided trust token issuance",
         kPlatformProvidedTrustTokenIssuance,
         base::size(kPlatformProvidedTrustTokenIssuance), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kWallpaperWebUIInternalName[] = "wallpaper-webui";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
const FeatureEntry::FeatureParam kPaintPreviewStartupWithAccessibility[] = {
    {"has_accessibility_support", "true"}};

const FeatureEntry::FeatureVariation kPaintPreviewStartupVariations[] = {
    {"with accessibility support", kPaintPreviewStartupWithAccessibility,
     base::size(kPaintPreviewStartupWithAccessibility), nullptr}};
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kBorealisDiskManagementInternalName[] =
    "borealis-disk-management";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
// The variations of Continuous Search.
const FeatureEntry::FeatureParam kContinuousSearchAfterSecondSrp[] = {
    {"trigger_mode", "1"}};

const FeatureEntry::FeatureParam kContinuousSearchOnReverseScroll[] = {
    {"trigger_mode", "2"}};

const FeatureEntry::FeatureParam kContinuousSearchPermanentDismissal[] = {
    {"permanent_dismissal_threshold", "3"}};

const FeatureEntry::FeatureVariation kContinuousSearchFeatureVariations[] = {
    {"show after second SRP", kContinuousSearchAfterSecondSrp,
     base::size(kContinuousSearchAfterSecondSrp), nullptr},
    {"show on reverse scroll", kContinuousSearchOnReverseScroll,
     base::size(kContinuousSearchOnReverseScroll), nullptr},
    {"with permanent dismissal", kContinuousSearchPermanentDismissal,
     base::size(kContinuousSearchPermanentDismissal), nullptr}};
#endif  // defined(OS_ANDROID)

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the entry is the internal name.
//
// To add a new entry, add to the end of kFeatureEntries. There are two
// distinct types of entries:
// . SINGLE_VALUE: entry is either on or off. Use the SINGLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option). To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// See the documentation of FeatureEntry for details on the fields.
//
// Usage of about:flags is logged on startup via the "Launch.FlagsAtStartup"
// UMA histogram. This histogram shows the number of startups with a given flag
// enabled. If you'd like to see user counts instead, make sure to switch to
// "count users" view on the dashboard. When adding new entries, the enum
// "LoginCustomFlags" must be updated in histograms/enums.xml. See note in
// enums.xml and don't forget to run AboutFlagsHistogramTest unit test to
// calculate and verify checksum.
//
// When adding a new choice, add it to the end of the list.
const FeatureEntry kFeatureEntries[] = {
// Include generated flags for flag unexpiry; see //docs/flag_expiry.md and
// //tools/flags/generate_unexpire_flags.py.
#include "build/chromeos_buildflags.h"
#include "chrome/browser/unexpire_flags_gen.inc"
    {"ignore-gpu-blocklist", flag_descriptions::kIgnoreGpuBlocklistName,
     flag_descriptions::kIgnoreGpuBlocklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlocklist)},
    {"disable-accelerated-2d-canvas",
     flag_descriptions::kAccelerated2dCanvasName,
     flag_descriptions::kAccelerated2dCanvasDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)},
    {"overlay-strategies", flag_descriptions::kOverlayStrategiesName,
     flag_descriptions::kOverlayStrategiesDescription, kOsAll,
     MULTI_VALUE_TYPE(kOverlayStrategiesChoices)},
    {"tint-composited-content", flag_descriptions::kTintCompositedContentName,
     flag_descriptions::kTintCompositedContentDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kTintCompositedContent)},
    {"show-overdraw-feedback", flag_descriptions::kShowOverdrawFeedbackName,
     flag_descriptions::kShowOverdrawFeedbackDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kShowOverdrawFeedback)},
    {"ui-disable-partial-swap", flag_descriptions::kUiPartialSwapName,
     flag_descriptions::kUiPartialSwapDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kUIDisablePartialSwap)},
    {"enable-webrtc-capture-multi-channel-audio-processing",
     flag_descriptions::kWebrtcCaptureMultiChannelApmName,
     flag_descriptions::kWebrtcCaptureMultiChannelApmDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcEnableCaptureMultiChannelApm)},
    {"disable-webrtc-hw-decoding", flag_descriptions::kWebrtcHwDecodingName,
     flag_descriptions::kWebrtcHwDecodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", flag_descriptions::kWebrtcHwEncodingName,
     flag_descriptions::kWebrtcHwEncodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
#if !defined(OS_ANDROID)
    {"enable-reader-mode", flag_descriptions::kEnableReaderModeName,
     flag_descriptions::kEnableReaderModeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(dom_distiller::kReaderMode,
                                    kReaderModeDiscoverabilityVariations,
                                    "ReaderMode")},
#endif  // !defined(OS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-aura-window-subtree-capture",
     flag_descriptions::kAuraWindowSubtreeCaptureName,
     flag_descriptions::kAuraWindowSubtreeCaptureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAuraWindowSubtreeCapture)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(WEBRTC_USE_PIPEWIRE)
    {"enable-webrtc-pipewire-capturer",
     flag_descriptions::kWebrtcPipeWireCapturerName,
     flag_descriptions::kWebrtcPipeWireCapturerDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebRtcPipeWireCapturer)},
#endif  // defined(WEBRTC_USE_PIPEWIRE)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"send-webui-javascript-error-reports",
     flag_descriptions::kSendWebUIJavaScriptErrorReportsName,
     flag_descriptions::kSendWebUIJavaScriptErrorReportsDescription,
     kOsLinux | kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kSendWebUIJavaScriptErrorReports,
         kSendWebUIJavaScriptErrorReportsVariations,
         "SendWebUIJavaScriptErrorReportsVariations")},
#endif
#if !defined(OS_ANDROID)
    {"enable-webrtc-remote-event-log",
     flag_descriptions::kWebRtcRemoteEventLogName,
     flag_descriptions::kWebRtcRemoteEventLogDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebRtcRemoteEventLog)},
#endif
    {"enable-webrtc-srtp-aes-gcm", flag_descriptions::kWebrtcSrtpAesGcmName,
     flag_descriptions::kWebrtcSrtpAesGcmDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcSrtpAesGcm)},
    {"enable-webrtc-hybrid-agc", flag_descriptions::kWebrtcHybridAgcName,
     flag_descriptions::kWebrtcHybridAgcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcHybridAgc)},
    {"enable-webrtc-analog-agc-clipping-control",
     flag_descriptions::kWebrtcAnalogAgcClippingControlName,
     flag_descriptions::kWebrtcAnalogAgcClippingControlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebRtcAnalogAgcClippingControl)},
    {"enable-webrtc-hide-local-ips-with-mdns",
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsName,
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsDecription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcHideLocalIpsWithMdns)},
    {"enable-webrtc-use-min-max-vea-dimensions",
     flag_descriptions::kWebrtcUseMinMaxVEADimensionsName,
     flag_descriptions::kWebrtcUseMinMaxVEADimensionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcUseMinMaxVEADimensions)},
#if BUILDFLAG(ENABLE_NACL)
    {"enable-nacl", flag_descriptions::kNaclName,
     flag_descriptions::kNaclDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
#endif  // ENABLE_NACL
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extensions-on-chrome-urls",
     flag_descriptions::kExtensionsOnChromeUrlsName,
     flag_descriptions::kExtensionsOnChromeUrlsDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif  // ENABLE_EXTENSIONS
    {"colr-v1-fonts", flag_descriptions::kCOLRV1FontsName,
     flag_descriptions::kCOLRV1FontsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCOLRV1Fonts)},
    {"enable-container-queries", flag_descriptions::kCSSContainerQueriesName,
     flag_descriptions::kCSSContainerQueriesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCSSContainerQueries)},
#if defined(OS_ANDROID)
    {"contextual-search-debug", flag_descriptions::kContextualSearchDebugName,
     flag_descriptions::kContextualSearchDebugDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDebug)},
    {"contextual-search-force-caption",
     flag_descriptions::kContextualSearchForceCaptionName,
     flag_descriptions::kContextualSearchForceCaptionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchForceCaption)},
    {"contextual-search-literal-search-tap",
     flag_descriptions::kContextualSearchLiteralSearchTapName,
     flag_descriptions::kContextualSearchLiteralSearchTapDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchLiteralSearchTap)},
    {"contextual-search-longpress-resolve",
     flag_descriptions::kContextualSearchLongpressResolveName,
     flag_descriptions::kContextualSearchLongpressResolveDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextualSearchLongpressResolve,
         kLongpressResolveVariations,
         "ContextualSearchLongpressResolve")},
    {"contextual-search-ml-tap-suppression",
     flag_descriptions::kContextualSearchMlTapSuppressionName,
     flag_descriptions::kContextualSearchMlTapSuppressionDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchMlTapSuppression)},
    {"contextual-search-ranker-query",
     flag_descriptions::kContextualSearchRankerQueryName,
     flag_descriptions::kContextualSearchRankerQueryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(assist_ranker::kContextualSearchRankerQuery)},
    {"contextual-search-second-tap",
     flag_descriptions::kContextualSearchSecondTapName,
     flag_descriptions::kContextualSearchSecondTapDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchSecondTap)},
    {"contextual-search-twv-impl",
     flag_descriptions::kContextualSearchThinWebViewImplementationName,
     flag_descriptions::kContextualSearchThinWebViewImplementationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kContextualSearchThinWebViewImplementation)},
    {"contextual-search-translations",
     flag_descriptions::kContextualSearchTranslationsName,
     flag_descriptions::kContextualSearchTranslationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchTranslations)},
    {"explore-sites", flag_descriptions::kExploreSitesName,
     flag_descriptions::kExploreSitesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kExploreSites,
                                    kExploreSitesVariations,
                                    "ExploreSites InitialCountries")},
    {"related-searches", flag_descriptions::kRelatedSearchesName,
     flag_descriptions::kRelatedSearchesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kRelatedSearches,
                                    kRelatedSearchesVariations,
                                    "RelatedSearches")},
    {"related-searches-ui", flag_descriptions::kRelatedSearchesUiName,
     flag_descriptions::kRelatedSearchesUiDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kRelatedSearchesUi,
                                    kRelatedSearchesUiVariations,
                                    "RelatedSearchesUi")},
    {"related-searches-in-bar", flag_descriptions::kRelatedSearchesInBarName,
     flag_descriptions::kRelatedSearchesInBarDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kRelatedSearchesInBar,
                                    kRelatedSearchesInBarVariations,
                                    "RelatedSearchesInBar")},
    {"related-searches-alternate-ux",
     flag_descriptions::kRelatedSearchesAlternateUxName,
     flag_descriptions::kRelatedSearchesAlternateUxDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kRelatedSearchesAlternateUx,
         kRelatedSearchesAlternateUxVariations,
         "RelatedSearchesAlternateUx")},
    {"related-searches-simplified-ux",
     flag_descriptions::kRelatedSearchesSimplifiedUxName,
     flag_descriptions::kRelatedSearchesSimplifiedUxDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRelatedSearchesSimplifiedUx)},
    {"single-touch-select", flag_descriptions::kSingleTouchSelectName,
     flag_descriptions::kSingleTouchSelectDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSingleTouchSelect)},
    {"bento-offline", flag_descriptions::kBentoOfflineName,
     flag_descriptions::kBentoOfflineDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBentoOffline)},
#endif  // OS_ANDROID
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillShowTypePredictions)},
    {"smooth-scrolling", flag_descriptions::kSmoothScrollingName,
     flag_descriptions::kSmoothScrollingDescription,
     // Mac has a separate implementation with its own setting to disable.
     kOsLinux | kOsCrOS | kOsWin | kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSmoothScrolling,
                               switches::kDisableSmoothScrolling)},
    {"sms-receiver-cross-device", flag_descriptions::kWebOTPCrossDeviceName,
     flag_descriptions::kWebOTPCrossDeviceDescription, kOsAll,
     FEATURE_VALUE_TYPE(kWebOTPCrossDevice)},
    {"fractional-scroll-offsets",
     flag_descriptions::kFractionalScrollOffsetsName,
     flag_descriptions::kFractionalScrollOffsetsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFractionalScrollOffsets)},
#if defined(USE_AURA)
    {"overlay-scrollbars", flag_descriptions::kOverlayScrollbarsName,
     flag_descriptions::kOverlayScrollbarsDescription,
     // Uses the system preference on Mac (a different implementation).
     // On Android, this is always enabled.
     kOsAura, FEATURE_VALUE_TYPE(features::kOverlayScrollbar)},
#endif  // USE_AURA
    {"enable-quic", flag_descriptions::kQuicName,
     flag_descriptions::kQuicDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableQuic, switches::kDisableQuic)},
    {"disable-javascript-harmony-shipping",
     flag_descriptions::kJavascriptHarmonyShippingName,
     flag_descriptions::kJavascriptHarmonyShippingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableJavaScriptHarmonyShipping)},
    {"enable-javascript-harmony", flag_descriptions::kJavascriptHarmonyName,
     flag_descriptions::kJavascriptHarmonyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kJavaScriptHarmony)},
    {"enable-experimental-webassembly-features",
     flag_descriptions::kExperimentalWebAssemblyFeaturesName,
     flag_descriptions::kExperimentalWebAssemblyFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebAssemblyFeatures)},
    {"enable-webassembly-baseline", flag_descriptions::kEnableWasmBaselineName,
     flag_descriptions::kEnableWasmBaselineDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyBaseline)},
    {"enable-webassembly-lazy-compilation",
     flag_descriptions::kEnableWasmLazyCompilationName,
     flag_descriptions::kEnableWasmLazyCompilationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyLazyCompilation)},
    {"enable-webassembly-simd", flag_descriptions::kEnableWasmSimdName,
     flag_descriptions::kEnableWasmSimdDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblySimd)},
    {"enable-webassembly-tiering", flag_descriptions::kEnableWasmTieringName,
     flag_descriptions::kEnableWasmTieringDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyTiering)},
    {"enable-future-v8-vm-features", flag_descriptions::kV8VmFutureName,
     flag_descriptions::kV8VmFutureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8VmFuture)},
    {"enable-gpu-rasterization", flag_descriptions::kGpuRasterizationName,
     flag_descriptions::kGpuRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"enable-oop-rasterization", flag_descriptions::kOopRasterizationName,
     flag_descriptions::kOopRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableOopRasterizationChoices)},
    {"enable-oop-rasterization-ddl",
     flag_descriptions::kOopRasterizationDDLName,
     flag_descriptions::kOopRasterizationDDLDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kOopRasterizationDDL)},
    {"enable-experimental-web-platform-features",
     flag_descriptions::kExperimentalWebPlatformFeaturesName,
     flag_descriptions::kExperimentalWebPlatformFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
#if BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_ANDROID)
    {"enable-android-spellchecker",
     flag_descriptions::kEnableAndroidSpellcheckerDescription,
     flag_descriptions::kEnableAndroidSpellcheckerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(spellcheck::kAndroidSpellCheckerNonLowEnd)},
#endif  // ENABLE_SPELLCHECK && OS_ANDROID
    {"top-chrome-touch-ui", flag_descriptions::kTopChromeTouchUiName,
     flag_descriptions::kTopChromeTouchUiDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTopChromeTouchUiChoices)},
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
    {"webui-tab-strip", flag_descriptions::kWebUITabStripName,
     flag_descriptions::kWebUITabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStrip)},
    {"webui-tab-strip-ntb-in-tab-strip",
     flag_descriptions::kWebUITabStripNTBInTabStripName,
     flag_descriptions::kWebUITabStripNTBInTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStripNewTabButtonInTabStrip)},
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
    {
        "webui-tab-strip-tab-drag-integration",
        flag_descriptions::kWebUITabStripTabDragIntegrationName,
        flag_descriptions::kWebUITabStripTabDragIntegrationDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(ash::features::kWebUITabStripTabDragIntegration),
    },
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"disable-explicit-dma-fences",
     flag_descriptions::kDisableExplicitDmaFencesName,
     flag_descriptions::kDisableExplicitDmaFencesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableExplicitDmaFences)},
    // TODO(crbug.com/1012846): Remove this flag and provision when HDR is fully
    //  supported on ChromeOS.
    {"use-hdr-transfer-function",
     flag_descriptions::kUseHDRTransferFunctionName,
     flag_descriptions::kUseHDRTransferFunctionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kUseHDRTransferFunction)},
    {"account-management-flows-v2",
     flag_descriptions::kAccountManagementFlowsV2Name,
     flag_descriptions::kAccountManagementFlowsV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAccountManagementFlowsV2)},
    {"dark-light-mode", flag_descriptions::kDarkLightTestName,
     flag_descriptions::kDarkLightTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDarkLightMode)},
    {"screen-capture", flag_descriptions::kScreenCaptureTestName,
     flag_descriptions::kScreenCaptureTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCaptureMode)},
    {"ash-bento-bar", flag_descriptions::kBentoBarName,
     flag_descriptions::kBentoBarDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBentoBar)},
    {"ash-overview-button", flag_descriptions::kOverviewButtonName,
     flag_descriptions::kOverviewButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOverviewButton)},
    {"ash-window-follow-cursor-multi-display",
     flag_descriptions::kWindowsFollowCursorName,
     flag_descriptions::kWindowsFollowCursorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWindowsFollowCursor)},
    {"ash-limit-alt-tab-to-active-desk",
     flag_descriptions::kLimitAltTabToActiveDeskName,
     flag_descriptions::kLimitAltTabToActiveDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLimitAltTabToActiveDesk)},
    {"ash-limit-shelf-items-to-active-desk",
     flag_descriptions::kLimitShelfItemsToActiveDeskName,
     flag_descriptions::kLimitShelfItemsToActiveDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPerDeskShelf)},
    {"ash-enable-unified-desktop",
     flag_descriptions::kAshEnableUnifiedDesktopName,
     flag_descriptions::kAshEnableUnifiedDesktopDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUnifiedDesktop)},
    {"enable-compositing-based-throttling",
     flag_descriptions::kCompositingBasedThrottling,
     flag_descriptions::kCompositingBasedThrottlingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCompositingBasedThrottling)},
    {"bluetooth-aggressive-appearance-filter",
     flag_descriptions::kBluetoothAggressiveAppearanceFilterName,
     flag_descriptions::kBluetoothAggressiveAppearanceFilterDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kBluetoothAggressiveAppearanceFilter)},
    {"bluetooth-fix-a2dp-packet-size",
     flag_descriptions::kBluetoothFixA2dpPacketSizeName,
     flag_descriptions::kBluetoothFixA2dpPacketSizeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothFixA2dpPacketSize)},
    {"bluetooth-revamp", flag_descriptions::kBluetoothRevampName,
     flag_descriptions::kBluetoothRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothRevamp)},
    {"bluetooth-wbs-dogfood", flag_descriptions::kBluetoothWbsDogfoodName,
     flag_descriptions::kBluetoothWbsDogfoodDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothWbsDogfood)},
    {"button-arc-network-diagnostics",
     flag_descriptions::kButtonARCNetworkDiagnosticsName,
     flag_descriptions::kButtonARCNetworkDiagnosticsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kButtonARCNetworkDiagnostics)},
    {"cellular-allow-per-network-roaming",
     flag_descriptions::kCellularAllowPerNetworkRoamingName,
     flag_descriptions::kCellularAllowPerNetworkRoamingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularAllowPerNetworkRoaming)},
    {"cellular-forbid-attach-apn",
     flag_descriptions::kCellularForbidAttachApnName,
     flag_descriptions::kCellularForbidAttachApnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularForbidAttachApn)},
    {"cellular-use-attach-apn", flag_descriptions::kCellularUseAttachApnName,
     flag_descriptions::kCellularUseAttachApnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularUseAttachApn)},
    {"cellular-use-external-euicc",
     flag_descriptions::kCellularUseExternalEuiccName,
     flag_descriptions::kCellularUseExternalEuiccDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularUseExternalEuicc)},
    {"cryptauth-v2-device-activity-status",
     flag_descriptions::kCryptAuthV2DeviceActivityStatusName,
     flag_descriptions::kCryptAuthV2DeviceActivityStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceActivityStatus)},
    {"cryptauth-v2-device-activity-status-use-connectivity",
     flag_descriptions::kCryptAuthV2DeviceActivityStatusUseConnectivityName,
     flag_descriptions::
         kCryptAuthV2DeviceActivityStatusUseConnectivityDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kCryptAuthV2DeviceActivityStatusUseConnectivity)},
    {"cryptauth-v2-devicesync", flag_descriptions::kCryptAuthV2DeviceSyncName,
     flag_descriptions::kCryptAuthV2DeviceSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceSync)},
    {"cryptauth-v2-enrollment", flag_descriptions::kCryptAuthV2EnrollmentName,
     flag_descriptions::kCryptAuthV2EnrollmentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2Enrollment)},
    {"disable-cryptauth-v1-devicesync",
     flag_descriptions::kDisableCryptAuthV1DeviceSyncName,
     flag_descriptions::kDisableCryptAuthV1DeviceSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableCryptAuthV1DeviceSync)},
    {"disable-idle-sockets-close-on-memory-pressure",
     flag_descriptions::kDisableIdleSocketsCloseOnMemoryPressureName,
     flag_descriptions::kDisableIdleSocketsCloseOnMemoryPressureDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kDisableIdleSocketsCloseOnMemoryPressure)},
    {"disable-office-editing-component-app",
     flag_descriptions::kDisableOfficeEditingComponentAppName,
     flag_descriptions::kDisableOfficeEditingComponentAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableOfficeEditingComponentApp)},
    {"updated_cellular_activation_ui",
     flag_descriptions::kUpdatedCellularActivationUiName,
     flag_descriptions::kUpdatedCellularActivationUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUpdatedCellularActivationUi)},
    {"use_messages_staging_url", flag_descriptions::kUseMessagesStagingUrlName,
     flag_descriptions::kUseMessagesStagingUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseMessagesStagingUrl)},
    {"use-custom-messages-domain",
     flag_descriptions::kUseCustomMessagesDomainName,
     flag_descriptions::kUseCustomMessagesDomainDescription, kOsCrOS,
     ORIGIN_LIST_VALUE_TYPE(switches::kCustomAndroidMessagesDomain, "")},
    {"disable-cancel-all-touches",
     flag_descriptions::kDisableCancelAllTouchesName,
     flag_descriptions::kDisableCancelAllTouchesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableCancelAllTouches)},
    {
        "enable-background-blur",
        flag_descriptions::kEnableBackgroundBlurName,
        flag_descriptions::kEnableBackgroundBlurDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(ash::features::kEnableBackgroundBlur),
    },
    {"enable-notification-indicator",
     flag_descriptions::kNotificationIndicatorName,
     flag_descriptions::kNotificationIndicatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNotificationIndicator)},
    {kLacrosSupportInternalName, flag_descriptions::kLacrosSupportName,
     flag_descriptions::kLacrosSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosSupport)},
    {kLacrosStabilityInternalName, flag_descriptions::kLacrosStabilityName,
     flag_descriptions::kLacrosStabilityDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosStabilityChoices)},
    {kLacrosSelectionInternalName, flag_descriptions::kLacrosSelectionName,
     flag_descriptions::kLacrosSelectionDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosSelectionChoices)},
    {kWebAppsCrosapiInternalName, flag_descriptions::kWebAppsCrosapiName,
     flag_descriptions::kWebAppsCrosapiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebAppsCrosapi)},
    {kLacrosPrimaryInternalName, flag_descriptions::kLacrosPrimaryName,
     flag_descriptions::kLacrosPrimaryDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosPrimary)},
    {kLacrosAvailabilityIgnoreInternalName,
     flag_descriptions::kLacrosAvailabilityIgnoreName,
     flag_descriptions::kLacrosAvailabilityIgnoreDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kLacrosAvailabilityIgnore)},
    {"list-all-display-modes", flag_descriptions::kListAllDisplayModesName,
     flag_descriptions::kListAllDisplayModesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kListAllDisplayModes)},
    {"enable-hardware_mirror-mode",
     flag_descriptions::kEnableHardwareMirrorModeName,
     flag_descriptions::kEnableHardwareMirrorModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kEnableHardwareMirrorMode)},
    {"enable-dns-proxy", flag_descriptions::kEnableDnsProxyName,
     flag_descriptions::kEnableDnsProxyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableDnsProxy)},
    {"dns-proxy-enable-doh", flag_descriptions::kDnsProxyEnableDOHName,
     flag_descriptions::kDnsProxyEnableDOHDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(::features::kDnsProxyEnableDOH)},
    {"instant-tethering", flag_descriptions::kTetherName,
     flag_descriptions::kTetherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kInstantTethering)},
    {
        "new-shortcut-mapping",
        flag_descriptions::kEnableNewShortcutMappingName,
        flag_descriptions::kEnableNewShortcutMappingDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(features::kNewShortcutMapping),
    },
    {"improved-keyboard-shortcuts",
     flag_descriptions::kImprovedKeyboardShortcutsName,
     flag_descriptions::kImprovedKeyboardShortcutsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kImprovedKeyboardShortcuts)},
    {"deprecate-alt-click", flag_descriptions::kDeprecateAltClickName,
     flag_descriptions::kDeprecateAltClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDeprecateAltClick)},
    {"deprecate-alt-based-six-pack",
     flag_descriptions::kDeprecateAltBasedSixPackName,
     flag_descriptions::kDeprecateAltBasedSixPackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDeprecateAltBasedSixPack)},
    {"shelf-hide-buttons-in-tablet",
     flag_descriptions::kHideShelfControlsInTabletModeName,
     flag_descriptions::kHideShelfControlsInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideShelfControlsInTabletMode)},
    {"shelf-hover-previews", flag_descriptions::kShelfHoverPreviewsName,
     flag_descriptions::kShelfHoverPreviewsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kShelfHoverPreviews)},
    {"show-bluetooth-debug-log-toggle",
     flag_descriptions::kShowBluetoothDebugLogToggleName,
     flag_descriptions::kShowBluetoothDebugLogToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShowBluetoothDebugLogToggle)},
    {"show-taps", flag_descriptions::kShowTapsName,
     flag_descriptions::kShowTapsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShowTaps)},
    {"show-touch-hud", flag_descriptions::kShowTouchHudName,
     flag_descriptions::kShowTouchHudDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {"trim-on-memory-pressure", flag_descriptions::kTrimOnMemoryPressureName,
     flag_descriptions::kTrimOnMemoryPressureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kTrimOnMemoryPressure)},
    {"stylus-battery-status", flag_descriptions::kStylusBatteryStatusName,
     flag_descriptions::kStylusBatteryStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kStylusBatteryStatus)},
    {"wake-on-wifi-allowed", flag_descriptions::kWakeOnWifiAllowedName,
     flag_descriptions::kWakeOnWifiAllowedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kWakeOnWifiAllowed)},
    {"microphone-mute-notifications",
     flag_descriptions::kMicrophoneMuteNotificationsName,
     flag_descriptions::kMicrophoneMuteNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMicMuteNotifications)},
    {"microphone-mute-switch-device",
     flag_descriptions::kMicrophoneMuteSwitchDeviceName,
     flag_descriptions::kMicrophoneMuteSwitchDeviceDescription, kOsCrOS,
     SINGLE_VALUE_TYPE("enable-microphone-mute-switch-device")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS)
    // TODO(b/180051795): remove kOsLinux when lacros-chrome switches to
    // kOsCrOS.
    {"deprecate-low-usage-codecs",
     flag_descriptions::kDeprecateLowUsageCodecsName,
     flag_descriptions::kDeprecateLowUsageCodecsDescription, kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(media::kDeprecateLowUsageCodecs)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX)
    {
        "enable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsLinux,
        FEATURE_VALUE_TYPE(media::kVaapiVideoDecodeLinux),
    },
#else
    // TODO(b/180051795): remove kOsLinux when lacros-chrome switches to
    // kOsCrOS.
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid | kOsLinux,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
#endif  // defined(OS_LINUX)
    {
        "disable-accelerated-video-encode",
        flag_descriptions::kAcceleratedVideoEncodeName,
        flag_descriptions::kAcceleratedVideoEncodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoEncode),
    },
    {
        "enable-media-internals",
        flag_descriptions::kEnableMediaInternalsName,
        flag_descriptions::kEnableMediaInternalsDescription,
        kOsAll,
        FEATURE_VALUE_TYPE(media::kEnableMediaInternals),
    },
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {
        "zero-copy-video-capture",
        flag_descriptions::kZeroCopyVideoCaptureName,
        flag_descriptions::kZeroCopyVideoCaptureDescription,
        kOsCrOS,
        ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
            switches::kVideoCaptureUseGpuMemoryBuffer,
            "1",
            switches::kDisableVideoCaptureUseGpuMemoryBuffer,
            "1"),
    },
    {
        "ash-debug-shortcuts",
        flag_descriptions::kDebugShortcutsName,
        flag_descriptions::kDebugShortcutsDescription,
        kOsAll,
        SINGLE_VALUE_TYPE(ash::switches::kAshDebugShortcuts),
    },
    {"ui-slow-animations", flag_descriptions::kUiSlowAnimationsName,
     flag_descriptions::kUiSlowAnimationsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kUISlowAnimations)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_WIN)
    {
        "zero-copy-video-capture",
        flag_descriptions::kZeroCopyVideoCaptureName,
        flag_descriptions::kZeroCopyVideoCaptureDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kMediaFoundationD3D11VideoCapture),
    },
#endif  // defined(OS_WIN)
    {"debug-packed-apps", flag_descriptions::kDebugPackedAppName,
     flag_descriptions::kDebugPackedAppDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"use-lookalikes-for-navigation-suggestions",
     flag_descriptions::kUseLookalikesForNavigationSuggestionsName,
     flag_descriptions::kUseLookalikesForNavigationSuggestionsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(net::features::kUseLookalikesForNavigationSuggestions)},
    {"username-first-flow", flag_descriptions::kUsernameFirstFlowName,
     flag_descriptions::kUsernameFirstFlowDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kUsernameFirstFlow)},
    {"username-first-flow-filling",
     flag_descriptions::kUsernameFirstFlowFillingName,
     flag_descriptions::kUsernameFirstFlowFillingDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kUsernameFirstFlowFilling)},
    {"enable-show-autofill-signatures",
     flag_descriptions::kShowAutofillSignaturesName,
     flag_descriptions::kShowAutofillSignaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillSignatures)},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription,
     kOsAndroid | kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
    {"enable-web-bluetooth-new-permissions-backend",
     flag_descriptions::kWebBluetoothNewPermissionsBackendName,
     flag_descriptions::kWebBluetoothNewPermissionsBackendDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebBluetoothNewPermissionsBackend)},
#if defined(USE_AURA)
    {"overscroll-history-navigation",
     flag_descriptions::kOverscrollHistoryNavigationName,
     flag_descriptions::kOverscrollHistoryNavigationDescription, kOsAura,
     FEATURE_VALUE_TYPE(features::kOverscrollHistoryNavigation)},
    {"pull-to-refresh", flag_descriptions::kPullToRefreshName,
     flag_descriptions::kPullToRefreshDescription, kOsAura,
     MULTI_VALUE_TYPE(kPullToRefreshChoices)},
#endif  // USE_AURA
    {"enable-touch-drag-drop", flag_descriptions::kTouchDragDropName,
     flag_descriptions::kTouchDragDropDescription, kOsWin | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchDragDrop,
                               switches::kDisableTouchDragDrop)},
    {"touch-selection-strategy", flag_descriptions::kTouchSelectionStrategyName,
     flag_descriptions::kTouchSelectionStrategyDescription,
     kOsAndroid,  // TODO(mfomitchev): Add CrOS/Win/Linux support soon.
     MULTI_VALUE_TYPE(kTouchTextSelectionStrategyChoices)},
    {"trace-upload-url", flag_descriptions::kTraceUploadUrlName,
     flag_descriptions::kTraceUploadUrlDescription, kOsAll,
     MULTI_VALUE_TYPE(kTraceUploadURL)},
    {"enable-suggestions-with-substring-match",
     flag_descriptions::kSuggestionsWithSubStringMatchName,
     flag_descriptions::kSuggestionsWithSubStringMatchDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillTokenPrefixMatching)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-virtual-keyboard", flag_descriptions::kVirtualKeyboardName,
     flag_descriptions::kVirtualKeyboardDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
    {"disable-virtual-keyboard",
     flag_descriptions::kVirtualKeyboardDisabledName,
     flag_descriptions::kVirtualKeyboardDisabledDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kDisableVirtualKeyboard)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     flag_descriptions::kDeviceDiscoveryNotificationsName,
     flag_descriptions::kDeviceDiscoveryNotificationsDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications)},
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"enable-webgl-developer-extensions",
     flag_descriptions::kWebglDeveloperExtensionsName,
     flag_descriptions::kWebglDeveloperExtensionsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDeveloperExtensions)},
    {"enable-webgl-draft-extensions",
     flag_descriptions::kWebglDraftExtensionsName,
     flag_descriptions::kWebglDraftExtensionsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
    {"enable-zero-copy", flag_descriptions::kZeroCopyName,
     flag_descriptions::kZeroCopyDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(blink::switches::kEnableZeroCopy,
                               blink::switches::kDisableZeroCopy)},
    {"enable-vulkan", flag_descriptions::kEnableVulkanName,
     flag_descriptions::kEnableVulkanDescription,
     kOsWin | kOsLinux | kOsAndroid, FEATURE_VALUE_TYPE(features::kVulkan)},
#if defined(OS_MAC)
    {"disable-hosted-app-shim-creation",
     flag_descriptions::kHostedAppShimCreationName,
     flag_descriptions::kHostedAppShimCreationDescription, kOsMac,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableHostedAppShimCreation)},
#endif  // OS_MAC
#if defined(OS_ANDROID)
    {"translate-force-trigger-on-english",
     flag_descriptions::kTranslateForceTriggerOnEnglishName,
     flag_descriptions::kTranslateForceTriggerOnEnglishDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(language::kOverrideTranslateTriggerInIndia,
                                    kTranslateForceTriggerOnEnglishVariations,
                                    "OverrideTranslateTriggerInIndia")},
    {"translate-assist-content", flag_descriptions::kTranslateAssistContentName,
     flag_descriptions::kTranslateAssistContentDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kTranslateAssistContent)},
    {"translate-intent", flag_descriptions::kTranslateIntentName,
     flag_descriptions::kTranslateIntentDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kTranslateIntent)},
#endif  // OS_ANDROID

    {"override-language-prefs-for-href-translate",
     flag_descriptions::kOverrideLanguagePrefsForHrefTranslateName,
     flag_descriptions::kOverrideLanguagePrefsForHrefTranslateDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         translate::kOverrideLanguagePrefsForHrefTranslate,
         kOverrideLanguagePrefsForHrefTranslateVariations,
         "OverrideLanguagePrefsForHrefTranslate")},
    {"override-site-prefs-for-href-translate",
     flag_descriptions::kOverrideSitePrefsForHrefTranslateName,
     flag_descriptions::kOverrideSitePrefsForHrefTranslateDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         translate::kOverrideSitePrefsForHrefTranslate,
         kOverrideSitePrefsForHrefTranslateVariations,
         "OverrideSitePrefsForHrefTranslate")},
    {"override-unsupported-page-language-for-href-translate",
     flag_descriptions::kOverrideUnsupportedPageLanguageForHrefTranslateName,
     flag_descriptions::
         kOverrideUnsupportedPageLanguageForHrefTranslateDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         translate::kOverrideUnsupportedPageLanguageForHrefTranslate,
         kOverrideUnsupportedPageLanguageForHrefTranslateVariations,
         "OverrideUnsupportedPageLanguageForHrefTranslate")},
    {"override-similar-languages-for-href-translate",
     flag_descriptions::kOverrideSimilarLanguagesForHrefTranslateName,
     flag_descriptions::kOverrideSimilarLanguagesForHrefTranslateDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         translate::kOverrideSimilarLanguagesForHrefTranslate,
         kOverrideSimilarLanguagesForHrefTranslateVariations,
         "OverrideSimilarLanguagesForHrefTranslate")},

#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-system-notifications",
     flag_descriptions::kNotificationsSystemFlagName,
     flag_descriptions::kNotificationsSystemFlagDescription,
     kOsMac | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kSystemNotifications)},
#endif  // BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_ANDROID)
    {"adaptive-button-in-top-toolbar",
     flag_descriptions::kAdaptiveButtonInTopToolbarName,
     flag_descriptions::kAdaptiveButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kAdaptiveButtonInTopToolbar,
         kAdaptiveButtonInTopToolbarVariations,
         "OptionalToolbarButton")},
    {"adaptive-button-in-top-toolbar-customization",
     flag_descriptions::kAdaptiveButtonInTopToolbarCustomizationName,
     flag_descriptions::kAdaptiveButtonInTopToolbarCustomizationDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kAdaptiveButtonInTopToolbarCustomization,
         kAdaptiveButtonInTopToolbarCustomizationVariations,
         "OptionalToolbarButtonCustomization")},
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
    {"voice-button-in-top-toolbar",
     flag_descriptions::kVoiceButtonInTopToolbarName,
     flag_descriptions::kVoiceButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kVoiceButtonInTopToolbar)},
    {"assistant-explicit-voice-consent",
     flag_descriptions::kAssistantExplicitVoiceConsentName,
     flag_descriptions::kAssistantExplicitVoiceConsentDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantExplicitVoiceConsent)},
    {"assistant-intent-page-url",
     flag_descriptions::kAssistantIntentPageUrlName,
     flag_descriptions::kAssistantIntentPageUrlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantIntentPageUrl)},
    {"assistant-intent-translate-info",
     flag_descriptions::kAssistantIntentTranslateInfoName,
     flag_descriptions::kAssistantIntentTranslateInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantIntentTranslateInfo)},
    {"assistant-voice-consent-taps-counter",
     flag_descriptions::kAssistantVoiceConstentTapsCounterName,
     flag_descriptions::kAssistantVoiceConstentTapsCounterDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantVoiceConsentTapsCounter)},
    {"share-button-in-top-toolbar",
     flag_descriptions::kShareButtonInTopToolbarName,
     flag_descriptions::kShareButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShareButtonInTopToolbar)},
    {"chrome-share-highlights-android",
     flag_descriptions::kChromeShareHighlightsAndroidName,
     flag_descriptions::kChromeShareHighlightsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareHighlightsAndroid)},
    {"chrome-share-long-screenshot",
     flag_descriptions::kChromeShareLongScreenshotName,
     flag_descriptions::kChromeShareLongScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareLongScreenshot)},
    {"chrome-share-screenshot", flag_descriptions::kChromeShareScreenshotName,
     flag_descriptions::kChromeShareScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeShareScreenshot)},
    {"chrome-sharing-hub", flag_descriptions::kChromeSharingHubName,
     flag_descriptions::kChromeSharingHubDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeSharingHub)},
    {"webnotes-stylize", flag_descriptions::kWebNotesStylizeName,
     flag_descriptions::kWebNotesStylizeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(content_creation::kWebNotesStylizeEnabled,
                                    kWebNoteStylizeVariations,
                                    "WebNotesStylize")},
    {"sharing-hub-link-toggle", flag_descriptions::kSharingHubLinkToggleName,
     flag_descriptions::kSharingHubLinkToggleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSharingHubLinkToggle)},
#endif  // OS_ANDROID
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeChoiceName,
     flag_descriptions::kInProductHelpDemoModeChoiceDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"disable-threaded-scrolling", flag_descriptions::kThreadedScrollingName,
     flag_descriptions::kThreadedScrollingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(blink::switches::kDisableThreadedScrolling)},
    {"extension-content-verification",
     flag_descriptions::kExtensionContentVerificationName,
     flag_descriptions::kExtensionContentVerificationDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
    {"preemptive-link-to-text-generation",
     flag_descriptions::kPreemptiveLinkToTextGenerationName,
     flag_descriptions::kPreemptiveLinkToTextGenerationDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kPreemptiveLinkToTextGeneration)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"keyboard-based-display-arrangement-in-settings",
     flag_descriptions::kKeyboardBasedDisplayArrangementInSettingsName,
     flag_descriptions::kKeyboardBasedDisplayArrangementInSettingsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kKeyboardBasedDisplayArrangementInSettings)},
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crostini-use-dlc", flag_descriptions::kCrostiniUseDlcName,
     flag_descriptions::kCrostiniUseDlcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseDlc)},
    {"pluginvm-fullscreen", flag_descriptions::kPluginVmFullscreenName,
     flag_descriptions::kPluginVmFullscreenDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPluginVmFullscreen)},
    {"vm-status-page", flag_descriptions::kVmStatusPageName,
     flag_descriptions::kVmStatusPageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVmStatusPage)},
    {"crostini-reset-lxd-db", flag_descriptions::kCrostiniResetLxdDbName,
     flag_descriptions::kCrostiniResetLxdDbDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniResetLxdDb)},
#if BUILDFLAG(USE_TCMALLOC)
    {"dynamic-tcmalloc-tuning", flag_descriptions::kDynamicTcmallocName,
     flag_descriptions::kDynamicTcmallocDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kDynamicTcmallocTuning)},
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if (defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_ANDROID)) && \
    !defined(OS_NACL)
    {"mojo-linux-sharedmem", flag_descriptions::kMojoLinuxChannelSharedMemName,
     flag_descriptions::kMojoLinuxChannelSharedMemDescription,
     kOsCrOS | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(mojo::core::kMojoLinuxChannelSharedMem)},
#endif
#if defined(OS_ANDROID)
    {"enable-site-isolation-for-password-sites",
     flag_descriptions::kSiteIsolationForPasswordSitesName,
     flag_descriptions::kSiteIsolationForPasswordSitesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         site_isolation::features::kSiteIsolationForPasswordSites)},
    {"enable-site-per-process", flag_descriptions::kStrictSiteIsolationName,
     flag_descriptions::kStrictSiteIsolationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
    {"enable-process-sharing-with-default-site-instances",
     flag_descriptions::kProcessSharingWithDefaultSiteInstancesName,
     flag_descriptions::kProcessSharingWithDefaultSiteInstancesDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kProcessSharingWithDefaultSiteInstances)},
    {"enable-process-sharing-with-strict-site-instances",
     flag_descriptions::kProcessSharingWithStrictSiteInstancesName,
     flag_descriptions::kProcessSharingWithStrictSiteInstancesDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kProcessSharingWithStrictSiteInstances)},
#endif
    {"isolate-origins", flag_descriptions::kIsolateOriginsName,
     flag_descriptions::kIsolateOriginsDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kIsolateOrigins, "")},
    {about_flags::kSiteIsolationTrialOptOutInternalName,
     flag_descriptions::kSiteIsolationOptOutName,
     flag_descriptions::kSiteIsolationOptOutDescription, kOsAll,
     MULTI_VALUE_TYPE(kSiteIsolationOptOutChoices)},
    {"isolation-by-default", flag_descriptions::kIsolationByDefaultName,
     flag_descriptions::kIsolationByDefaultDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIsolationByDefault)},
    {"enable-use-zoom-for-dsf", flag_descriptions::kEnableUseZoomForDsfName,
     flag_descriptions::kEnableUseZoomForDsfDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableUseZoomForDSFChoices)},
    {"enable-subresource-redirect",
     flag_descriptions::kEnableSubresourceRedirectName,
     flag_descriptions::kEnableSubresourceRedirectDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kSubresourceRedirect,
                                    kSubresourceRedirectVariations,
                                    "SubresourceRedirect")},
    {"enable-login-detection", flag_descriptions::kEnableLoginDetectionName,
     flag_descriptions::kEnableLoginDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(login_detection::kLoginDetection)},
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
    {"enable-save-data", flag_descriptions::kEnableSaveDataName,
     flag_descriptions::kEnableSaveDataDescription, kOsCrOS | kOsLinux,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxy)},
    {"enable-navigation-predictor",
     flag_descriptions::kEnableNavigationPredictorName,
     flag_descriptions::kEnableNavigationPredictorDescription,
     kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(blink::features::kNavigationPredictor)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || OS_LINUX
    {"enable-preconnect-to-search",
     flag_descriptions::kEnablePreconnectToSearchName,
     flag_descriptions::kEnablePreconnectToSearchDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPreconnectToSearch)},
    {"enable-google-srp-isolated-prerender-probing",
     flag_descriptions::kEnableSRPIsolatedPrerenderProbingName,
     flag_descriptions::kEnableSRPIsolatedPrerenderProbingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kIsolatePrerendersMustProbeOrigin)},
    {"enable-google-srp-isolated-prerenders",
     flag_descriptions::kEnableSRPIsolatedPrerendersName,
     flag_descriptions::kEnableSRPIsolatedPrerendersDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kIsolatePrerenders,
                                    kIsolatedPrerenderFeatureWithPrefetchLimit,
                                    "Prefetch Limit")},
    {"enable-google-srp-isolated-prerender-nsp",
     flag_descriptions::kEnableSRPIsolatedPrerendersNSPName,
     flag_descriptions::kEnableSRPIsolatedPrerendersNSPDescription, kOsAll,
     SINGLE_VALUE_TYPE(kIsolatedPrerenderEnableNSPCmdLineFlag)},
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     SINGLE_VALUE_TYPE(webapps::switches::kBypassAppBannerEngagementChecks)},
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    {"allow-default-web-app-migration-for-chrome-os-managed-users",
     flag_descriptions::kAllowDefaultWebAppMigrationForChromeOsManagedUsersName,
     flag_descriptions::
         kAllowDefaultWebAppMigrationForChromeOsManagedUsersDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         web_app::kAllowDefaultWebAppMigrationForChromeOsManagedUsers)},
    {"enable-default-chat-web-app", flag_descriptions::kDefaultChatWebAppName,
     flag_descriptions::kDefaultChatWebAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(web_app::kDefaultChatWebApp)},
    {"enable-default-meet-web-app", flag_descriptions::kDefaultMeetWebAppName,
     flag_descriptions::kDefaultMeetWebAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(web_app::kDefaultMeetWebApp)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(https://crbug.com/1069293): Add macOS and Linux implementations.
    {"enable-desktop-pwas-app-icon-shortcuts-menu",
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuName,
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsAppIconShortcutsMenu)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desktop-pwas-attention-badging-cros",
     flag_descriptions::kDesktopPWAsAttentionBadgingCrOSName,
     flag_descriptions::kDesktopPWAsAttentionBadgingCrOSDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kDesktopPWAsAttentionBadgingCrOS,
                                    kDesktopPWAsAttentionBadgingCrOSVariations,
                                    "DesktopPWAsAttentionBadgingCrOS")},
#endif
    {"enable-desktop-pwas-prefix-app-name-in-window-title",
     flag_descriptions::kDesktopPWAsPrefixAppNameInWindowTitleName,
     flag_descriptions::kDesktopPWAsPrefixAppNameInWindowTitleDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(features::kPrefixWebAppWindowsWithAppName)},
    {"enable-desktop-pwas-remove-status-bar",
     flag_descriptions::kDesktopPWAsRemoveStatusBarName,
     flag_descriptions::kDesktopPWAsRemoveStatusBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRemoveStatusBarInWebApps)},
    {"enable-desktop-pwas-elided-extensions-menu",
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuName,
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsElidedExtensionsMenu)},
    {"enable-desktop-pwas-flash-app-name-instead-of-origin",
     flag_descriptions::kDesktopPWAsFlashAppNameInsteadOfOriginName,
     flag_descriptions::kDesktopPWAsFlashAppNameInsteadOfOriginDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsFlashAppNameInsteadOfOrigin)},
    {"enable-desktop-pwas-notification-icon-and-title",
     flag_descriptions::kDesktopPWAsNotificationIconAndTitleName,
     flag_descriptions::kDesktopPWAsNotificationIconAndTitleDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsNotificationIconAndTitle)},
    {"enable-desktop-pwas-tab-strip",
     flag_descriptions::kDesktopPWAsTabStripName,
     flag_descriptions::kDesktopPWAsTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStrip)},
    {"enable-desktop-pwas-tab-strip-link-capturing",
     flag_descriptions::kDesktopPWAsTabStripLinkCapturingName,
     flag_descriptions::kDesktopPWAsTabStripLinkCapturingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStripLinkCapturing)},
    {"enable-desktop-pwas-tab-strip-settings",
     flag_descriptions::kDesktopPWAsTabStripSettingsName,
     flag_descriptions::kDesktopPWAsTabStripSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStripSettings)},
    {"enable-desktop-pwas-link-capturing",
     flag_descriptions::kDesktopPWAsLinkCapturingName,
     flag_descriptions::kDesktopPWAsLinkCapturingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableLinkCapturing)},
    {"enable-desktop-pwas-manifest-id",
     flag_descriptions::kDesktopPWAsManifestIdName,
     flag_descriptions::kDesktopPWAsManifestIdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableManifestId)},
    {"enable-desktop-pwas-run-on-os-login",
     flag_descriptions::kDesktopPWAsRunOnOsLoginName,
     flag_descriptions::kDesktopPWAsRunOnOsLoginDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsRunOnOsLogin)},
    {"enable-desktop-pwas-sub-apps", flag_descriptions::kDesktopPWAsSubAppsName,
     flag_descriptions::kDesktopPWAsSubAppsDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsSubApps)},
    {"enable-desktop-pwas-protocol-handling",
     flag_descriptions::kDesktopPWAsProtocolHandlingName,
     flag_descriptions::kDesktopPWAsProtocolHandlingDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableProtocolHandlers)},
    {"enable-desktop-pwas-url-handling",
     flag_descriptions::kDesktopPWAsUrlHandlingName,
     flag_descriptions::kDesktopPWAsUrlHandlingDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableUrlHandlers)},
    {"enable-desktop-pwas-window-controls-overlay",
     flag_descriptions::kDesktopPWAsWindowControlsOverlayName,
     flag_descriptions::kDesktopPWAsWindowControlsOverlayDescription,
     kOsWin | kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(features::kWebAppWindowControlsOverlay)},
    {"enable-desktop-pwas-web-bundles",
     flag_descriptions::kDesktopPWAsWebBundlesName,
     flag_descriptions::kDesktopPWAsWebBundlesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsWebBundles)},
    {"record-web-app-debug-info", flag_descriptions::kRecordWebAppDebugInfoName,
     flag_descriptions::kRecordWebAppDebugInfoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRecordWebAppDebugInfo)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
#if !defined(OS_ANDROID)
    {"media-router-cast-allow-all-ips",
     flag_descriptions::kMediaRouterCastAllowAllIPsName,
     flag_descriptions::kMediaRouterCastAllowAllIPsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastAllowAllIPsFeature)},
    {"cast-media-route-provider",
     flag_descriptions::kCastMediaRouteProviderName,
     flag_descriptions::kCastMediaRouteProviderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastMediaRouteProvider)},
    {"global-media-controls-cast-start-stop",
     flag_descriptions::kGlobalMediaControlsCastStartStopName,
     flag_descriptions::kGlobalMediaControlsCastStartStopDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media_router::kGlobalMediaControlsCastStartStop)},
    {"allow-all-sites-to-initiate-mirroring",
     flag_descriptions::kAllowAllSitesToInitiateMirroringName,
     flag_descriptions::kAllowAllSitesToInitiateMirroringDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kAllowAllSitesToInitiateMirroring)},
    {"enable-migrate-default-chrome-app-to-web-apps-gsuite",
     flag_descriptions::kEnableMigrateDefaultChromeAppToWebAppsGSuiteName,
     flag_descriptions::
         kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(web_app::kMigrateDefaultChromeAppToWebAppsGSuite)},
    {"enable-migrate-default-chrome-app-to-web-apps-non-gsuite",
     flag_descriptions::kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName,
     flag_descriptions::
         kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(web_app::kMigrateDefaultChromeAppToWebAppsNonGSuite)},
#endif  // !OS_ANDROID
#if defined(OS_ANDROID)
    {"autofill-keyboard-accessory-view",
     flag_descriptions::kAutofillAccessoryViewName,
     flag_descriptions::kAutofillAccessoryViewDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillKeyboardAccessory)},
#endif  // OS_ANDROID
#if defined(OS_MAC)
    {"mac-syscall-sandbox", flag_descriptions::kMacSyscallSandboxName,
     flag_descriptions::kMacSyscallSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSyscallSandbox)},
#endif  // OS_MAC
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN) || defined(OS_MAC)
    {"web-share", flag_descriptions::kWebShareName,
     flag_descriptions::kWebShareDescription, kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebShare)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || OS_WIN || OS_MAC
#if BUILDFLAG(ENABLE_VR)
    {"webxr-incubations", flag_descriptions::kWebXrIncubationsName,
     flag_descriptions::kWebXrIncubationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(device::features::kWebXrIncubations)},
    {"webxr-runtime", flag_descriptions::kWebXrForceRuntimeName,
     flag_descriptions::kWebXrForceRuntimeDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kWebXrForceRuntimeChoices)},
#endif  // ENABLE_VR
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"disable-accelerated-mjpeg-decode",
     flag_descriptions::kAcceleratedMjpegDecodeName,
     flag_descriptions::kAcceleratedMjpegDecodeDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {"system-keyboard-lock", flag_descriptions::kSystemKeyboardLockName,
     flag_descriptions::kSystemKeyboardLockDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemKeyboardLock)},
#if defined(OS_ANDROID)
    {"add-to-homescreen-iph", flag_descriptions::kAddToHomescreenIPHName,
     flag_descriptions::kAddToHomescreenIPHDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAddToHomescreenIPH,
                                    kAddToHomescreenIPHVariations,
                                    "AddToHomescreen")},
    {"offline-pages-live-page-sharing",
     flag_descriptions::kOfflinePagesLivePageSharingName,
     flag_descriptions::kOfflinePagesLivePageSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesLivePageSharingFeature)},
    {"offline-indicator-v2", flag_descriptions::kOfflineIndicatorV2Name,
     flag_descriptions::kOfflineIndicatorV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOfflineIndicatorV2)},
    {"query-tiles", flag_descriptions::kQueryTilesName,
     flag_descriptions::kQueryTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTiles)},
    {"query-tiles-ntp", flag_descriptions::kQueryTilesNTPName,
     flag_descriptions::kQueryTilesNTPDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesInNTP)},
    {"query-tiles-omnibox", flag_descriptions::kQueryTilesOmniboxName,
     flag_descriptions::kQueryTilesOmniboxDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesInOmnibox)},
    {"query-tiles-local-ordering",
     flag_descriptions::kQueryTilesLocalOrderingName,
     flag_descriptions::kQueryTilesLocalOrderingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesLocalOrdering)},
    {"query-tiles-single-tier", flag_descriptions::kQueryTilesSingleTierName,
     flag_descriptions::kQueryTilesSingleTierDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesSingleTier)},
    {"query-tiles-enable-query-editing",
     flag_descriptions::kQueryTilesEnableQueryEditingName,
     flag_descriptions::kQueryTilesEnableQueryEditingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesEnableQueryEditing)},
    {"query-tiles-enable-trending",
     flag_descriptions::kQueryTilesEnableTrendingName,
     flag_descriptions::kQueryTilesEnableTrendingDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesEnableTrending)},
    {"query-tiles-country-code", flag_descriptions::kQueryTilesCountryCode,
     flag_descriptions::kQueryTilesCountryCodeDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kQueryTilesCountryChoices)},
    {"query-tiles-instant-fetch",
     flag_descriptions::kQueryTilesInstantFetchName,
     flag_descriptions::kQueryTilesInstantFetchDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(
         query_tiles::switches::kQueryTilesInstantBackgroundTask)},
    {"query-tiles-more-trending",
     flag_descriptions::kQueryTilesMoreTrendingName,
     flag_descriptions::kQueryTilesMoreTrendingDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesMoreTrending)},
    {"query-tiles-rank-tiles", flag_descriptions::kQueryTilesRankTilesName,
     flag_descriptions::kQueryTilesRankTilesDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(query_tiles::switches::kQueryTilesRankTiles)},
    {"query-tiles-swap-trending",
     flag_descriptions::kQueryTilesSwapTrendingName,
     flag_descriptions::kQueryTilesSwapTrendingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         query_tiles::features::kQueryTilesRemoveTrendingTilesAfterInactivity)},
    {"video-tutorials", flag_descriptions::kVideoTutorialsName,
     flag_descriptions::kVideoTutorialsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(video_tutorials::features::kVideoTutorials)},
    {"video-tutorials-instant-fetch",
     flag_descriptions::kVideoTutorialsInstantFetchName,
     flag_descriptions::kVideoTutorialsInstantFetchDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(video_tutorials::switches::kVideoTutorialsInstantFetch)},
    {"android-picture-in-picture-api",
     flag_descriptions::kAndroidPictureInPictureAPIName,
     flag_descriptions::kAndroidPictureInPictureAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(media::kPictureInPictureAPI)},
    {"reengagement-notification",
     flag_descriptions::kReengagementNotificationName,
     flag_descriptions::kReengagementNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReengagementNotification)},
    {"toolbar-iph-android", flag_descriptions::kToolbarIphAndroidName,
     flag_descriptions::kToolbarIphAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kToolbarIphAndroid)},
    {"toolbar-mic-iph-android", flag_descriptions::kToolbarMicIphAndroidName,
     flag_descriptions::kToolbarMicIphAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kToolbarMicIphAndroid,
                                    kIphMicToolbarVariations,
                                    "ToolbarMicIphAndroid")},
    {"theme-refactor-android", flag_descriptions::kThemeRefactorAndroidName,
     flag_descriptions::kThemeRefactorAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kThemeRefactorAndroid)},
#endif  // OS_ANDROID
    {"disallow-doc-written-script-loads",
     flag_descriptions::kDisallowDocWrittenScriptsUiName,
     flag_descriptions::kDisallowDocWrittenScriptsUiDescription, kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         blink::switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=true",
         blink::switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=false")},
    {"document-transition", flag_descriptions::kDocumentTransitionName,
     flag_descriptions::kDocumentTransitionDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDocumentTransition)},
#if defined(OS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // OS_WIN
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-regions-mode", flag_descriptions::kCrosRegionsModeName,
     flag_descriptions::kCrosRegionsModeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCrosRegionsModeChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
#endif  // TOOLKIT_VIEWS || OS_ANDROID
    {"force-ui-direction", flag_descriptions::kForceUiDirectionName,
     flag_descriptions::kForceUiDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceUIDirectionChoices)},
    {"force-text-direction", flag_descriptions::kForceTextDirectionName,
     flag_descriptions::kForceTextDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceTextDirectionChoices)},
#if defined(OS_ANDROID)
    {"force-update-menu-type", flag_descriptions::kUpdateMenuTypeName,
     flag_descriptions::kUpdateMenuTypeDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kForceUpdateMenuTypeChoices)},
    {"enable-inline-update-flow", flag_descriptions::kInlineUpdateFlowName,
     flag_descriptions::kInlineUpdateFlowDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInlineUpdateFlow)},
    {"update-menu-item-custom-summary",
     flag_descriptions::kUpdateMenuItemCustomSummaryName,
     flag_descriptions::kUpdateMenuItemCustomSummaryDescription, kOsAndroid,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kForceShowUpdateMenuItemCustomSummary,
         "Custom Summary")},
    {"force-show-update-menu-badge", flag_descriptions::kUpdateMenuBadgeName,
     flag_descriptions::kUpdateMenuBadgeDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kForceShowUpdateMenuBadge)},
    {"set-market-url-for-testing",
     flag_descriptions::kSetMarketUrlForTestingName,
     flag_descriptions::kSetMarketUrlForTestingDescription, kOsAndroid,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kMarketUrlForTesting,
                                 "https://play.google.com/store/apps/"
                                 "details?id=com.android.chrome")},
#endif  // OS_ANDROID
    {"enable-tls13-early-data", flag_descriptions::kEnableTLS13EarlyDataName,
     flag_descriptions::kEnableTLS13EarlyDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableTLS13EarlyData)},
    {"post-quantum-cecpq2", flag_descriptions::kPostQuantumCECPQ2Name,
     flag_descriptions::kPostQuantumCECPQ2Description, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPostQuantumCECPQ2)},
#if defined(OS_ANDROID)
    {"interest-feed-v2", flag_descriptions::kInterestFeedV2Name,
     flag_descriptions::kInterestFeedV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2)},
    {"feed-interactive-refresh", flag_descriptions::kFeedInteractiveRefreshName,
     flag_descriptions::kFeedInteractiveRefreshDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedInteractiveRefresh)},
    {"feed-v2-hearts", flag_descriptions::kInterestFeedV2HeartsName,
     flag_descriptions::kInterestFeedV2HeartsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2Hearts)},
    {"feed-v2-autoplay", flag_descriptions::kInterestFeedV2AutoplayName,
     flag_descriptions::kInterestFeedV2AutoplayDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2Autoplay)},
    {"web-feed", flag_descriptions::kWebFeedName,
     flag_descriptions::kWebFeedDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(feed::kWebFeed,
                                    kWebFeedVariations,
                                    "WebFeed")},
    {"xsurface-metrics-reporting",
     flag_descriptions::kXsurfaceMetricsReportingName,
     flag_descriptions::kXsurfaceMetricsReportingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kWebFeed)},
    {"interest-feed-v1-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV1ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV1ClickAndViewActionsConditionalUploadDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV1ClicksAndViewsConditionalUpload)},
    {"interest-feed-v2-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV2ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV2ClickAndViewActionsConditionalUploadDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2ClicksAndViewsConditionalUpload)},
#endif  // OS_ANDROID
    {"PasswordImport", flag_descriptions::kPasswordImportName,
     flag_descriptions::kPasswordImportDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordImport)},
    {"enable-force-dark", flag_descriptions::kForceWebContentsDarkModeName,
     flag_descriptions::kForceWebContentsDarkModeDescription, kOsAll,
#if BUILDFLAG(IS_CHROMEOS_ASH)
     // TODO(https://crbug.com/1011696): Investigate crash reports and
     // re-enable variations for ChromeOS.
     FEATURE_VALUE_TYPE(blink::features::kForceWebContentsDarkMode)},
#else
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kForceWebContentsDarkMode,
                                    kForceDarkVariations,
                                    "ForceDarkVariations")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_ANDROID)
    {"enable-android-layout-change-tab-reparenting",
     flag_descriptions::kAndroidLayoutChangeTabReparentingName,
     flag_descriptions::kAndroidLayoutChangeTabReparentingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidLayoutChangeTabReparenting)},
#endif  // OS_ANDROID
    {"enable-experimental-accessibility-language-detection",
     flag_descriptions::kExperimentalAccessibilityLanguageDetectionName,
     flag_descriptions::kExperimentalAccessibilityLanguageDetectionDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityLanguageDetection)},
    {"enable-experimental-accessibility-language-detection-dynamic",
     flag_descriptions::kExperimentalAccessibilityLanguageDetectionDynamicName,
     flag_descriptions::
         kExperimentalAccessibilityLanguageDetectionDynamicDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic)},
    {"enable-aria-element-reflection",
     flag_descriptions::kAriaElementReflectionName,
     flag_descriptions::kAriaElementReflectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAriaElementReflection)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-cros-ime-assist-autocorrect",
     flag_descriptions::kImeAssistAutocorrectName,
     flag_descriptions::kImeAssistAutocorrectDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistAutoCorrect)},
    {"enable-cros-ime-assist-emoji-enhanced",
     flag_descriptions::kImeAssistEmojiEnhancedName,
     flag_descriptions::kImeAssistEmojiEnhancedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistEmojiEnhanced)},
    {"enable-cros-ime-assist-multi-word",
     flag_descriptions::kImeAssistMultiWordName,
     flag_descriptions::kImeAssistMultiWordDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistMultiWord)},
    {"enable-cros-ime-assist-multi-word-expanded",
     flag_descriptions::kImeAssistMultiWordExpandedName,
     flag_descriptions::kImeAssistMultiWordExpandedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistMultiWordExpanded)},
    {"enable-cros-ime-assist-personal-info",
     flag_descriptions::kImeAssistPersonalInfoName,
     flag_descriptions::kImeAssistPersonalInfoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistPersonalInfo)},
    {"enable-cros-ime-emoji-suggest-addition",
     flag_descriptions::kImeEmojiSuggestAdditionName,
     flag_descriptions::kImeEmojiSuggestAdditionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEmojiSuggestAddition)},
    {"enable-cros-ime-mozc-proto", flag_descriptions::kImeMozcProtoName,
     flag_descriptions::kImeMozcProtoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeMozcProto)},
    {"enable-cros-ime-service-decoder",
     flag_descriptions::kImeServiceDecoderName,
     flag_descriptions::kImeServiceDecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeMojoDecoder)},
    {"enable-cros-ime-system-emoji-picker",
     flag_descriptions::kImeSystemEmojiPickerName,
     flag_descriptions::kImeSystemEmojiPickerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPicker)},
    {"enable-cros-ime-system-emoji-picker-clipboard",
     flag_descriptions::kImeSystemEmojiPickerClipboardName,
     flag_descriptions::kImeSystemEmojiPickerClipboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPickerClipboard)},
    {"enable-cros-language-settings-update-2",
     flag_descriptions::kCrosLanguageSettingsUpdate2Name,
     flag_descriptions::kCrosLanguageSettingsUpdate2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLanguageSettingsUpdate2)},
    {"enable-cros-language-settings-ime-options-in-settings",
     flag_descriptions::kCrosLanguageSettingsImeOptionsInSettingsName,
     flag_descriptions::kCrosLanguageSettingsImeOptionsInSettingsDescription,
     kOsCrOS, FEATURE_VALUE_TYPE(chromeos::features::kImeOptionsInSettings)},
    {"enable-cros-multilingual-typing",
     flag_descriptions::kMultilingualTypingName,
     flag_descriptions::kMultilingualTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMultilingualTyping)},
    {"enable-cros-on-device-grammar-check",
     flag_descriptions::kCrosOnDeviceGrammarCheckName,
     flag_descriptions::kCrosOnDeviceGrammarCheckDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOnDeviceGrammarCheck)},
    {"enable-cros-system-latin-physical-typing",
     flag_descriptions::kSystemLatinPhysicalTypingName,
     flag_descriptions::kSystemLatinPhysicalTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSystemLatinPhysicalTyping)},
    {"enable-cros-virtual-keyboard-bordered-key",
     flag_descriptions::kVirtualKeyboardBorderedKeyName,
     flag_descriptions::kVirtualKeyboardBorderedKeyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardBorderedKey)},
    {"enable-cros-virtual-keyboard-multipaste",
     flag_descriptions::kVirtualKeyboardMultipasteName,
     flag_descriptions::kVirtualKeyboardMultipasteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardMultipaste)},
    {"enable-experimental-accessibility-dictation-extension",
     flag_descriptions::kExperimentalAccessibilityDictationExtensionName,
     flag_descriptions::kExperimentalAccessibilityDictationExtensionDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilityDictationExtension)},
    {"enable-experimental-accessibility-dictation-listening",
     flag_descriptions::kExperimentalAccessibilityDictationListeningName,
     flag_descriptions::kExperimentalAccessibilityDictationListeningDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kExperimentalAccessibilityDictationListening)},
    {"enable-experimental-accessibility-dictation-offline",
     flag_descriptions::kExperimentalAccessibilityDictationOfflineName,
     flag_descriptions::kExperimentalAccessibilityDictationOfflineDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kExperimentalAccessibilityDictationOffline)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
    {"enable-switch-access-point-scanning",
     flag_descriptions::kSwitchAccessPointScanningName,
     flag_descriptions::kSwitchAccessPointScanningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableSwitchAccessPointScanning)},
    {"enable-experimental-accessibility-switch-access-setup-guide",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessSetupGuideName,
     flag_descriptions::
         kExperimentalAccessibilitySwitchAccessSetupGuideDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessSetupGuide)},
    {"enable-experimental-kernel-vm-support",
     flag_descriptions::kKernelnextVMsName,
     flag_descriptions::kKernelnextVMsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kKernelnextVMs)},
    {"enable-magnifier-panning-improvements",
     flag_descriptions::kMagnifierPanningImprovementsName,
     flag_descriptions::kMagnifierPanningImprovementsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMagnifierPanningImprovements)},
    {"enable-magnifier-continuous-mouse-following-mode-setting",
     flag_descriptions::kMagnifierContinuousMouseFollowingModeSettingName,
     flag_descriptions::
         kMagnifierContinuousMouseFollowingModeSettingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kMagnifierContinuousMouseFollowingModeSetting)},
    {"enable-system-proxy-for-system-services",
     flag_descriptions::kSystemProxyForSystemServicesName,
     flag_descriptions::kSystemProxyForSystemServicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSystemProxyForSystemServices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_MAC)
    {"enable-immersive-fullscreen-toolbar",
     flag_descriptions::kImmersiveFullscreenName,
     flag_descriptions::kImmersiveFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kImmersiveFullscreen)},
#endif  // OS_MAC
    {"enable-web-payments-experimental-features",
     flag_descriptions::kWebPaymentsExperimentalFeaturesName,
     flag_descriptions::kWebPaymentsExperimentalFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsExperimentalFeatures)},
    {"enable-web-payments-minimal-ui",
     flag_descriptions::kWebPaymentsMinimalUIName,
     flag_descriptions::kWebPaymentsMinimalUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebPaymentsMinimalUI)},
    {"enable-debug-for-store-billing",
     flag_descriptions::kAppStoreBillingDebugName,
     flag_descriptions::kAppStoreBillingDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kAppStoreBillingDebug)},
    {"enable-debug-for-secure-payment-confirmation",
     flag_descriptions::kSecurePaymentConfirmationDebugName,
     flag_descriptions::kSecurePaymentConfirmationDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSecurePaymentConfirmationDebug)},
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-custom-tabs-experiment",
     flag_descriptions::kArcCustomTabsExperimentName,
     flag_descriptions::kArcCustomTabsExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kCustomTabsExperimentFeature)},
    {"arc-documents-provider-unknown-size",
     flag_descriptions::kArcDocumentsProviderUnknownSizeName,
     flag_descriptions::kArcDocumentsProviderUnknownSizeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kDocumentsProviderUnknownSizeFeature)},
    {"arc-enable-usap", flag_descriptions::kArcEnableUsapName,
     flag_descriptions::kArcEnableUsapDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUsap)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-image-copy-paste-compat",
     flag_descriptions::kArcImageCopyPasteCompatName,
     flag_descriptions::kArcImageCopyPasteCompatDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kImageCopyPasteCompatFeature)},
    {"arc-keyboard-shortcut-helper-integration",
     flag_descriptions::kArcKeyboardShortcutHelperIntegrationName,
     flag_descriptions::kArcKeyboardShortcutHelperIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kKeyboardShortcutHelperIntegrationFeature)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
    {"arc-native-bridge-64bit-support-experiment",
     flag_descriptions::kArcNativeBridge64BitSupportExperimentName,
     flag_descriptions::kArcNativeBridge64BitSupportExperimentDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridge64BitSupportExperimentFeature)},
    {"arc-rt-vcpu-dual-core", flag_descriptions::kArcRtVcpuDualCoreName,
     flag_descriptions::kArcRtVcpuDualCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuDualCore)},
    {"arc-rt-vcpu-quad-core", flag_descriptions::kArcRtVcpuQuadCoreName,
     flag_descriptions::kArcRtVcpuQuadCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuQuadCore)},
    {kArcUseHighMemoryDalvikProfileInternalName,
     flag_descriptions::kArcUseHighMemoryDalvikProfileName,
     flag_descriptions::kArcUseHighMemoryDalvikProfileDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUseHighMemoryDalvikProfile)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-generic-sensor-extra-classes",
     flag_descriptions::kEnableGenericSensorExtraClassesName,
     flag_descriptions::kEnableGenericSensorExtraClassesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensorExtraClasses)},
    {"expensive-background-timer-throttling",
     flag_descriptions::kExpensiveBackgroundTimerThrottlingName,
     flag_descriptions::kExpensiveBackgroundTimerThrottlingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExpensiveBackgroundTimerThrottling)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {ui_devtools::switches::kEnableUiDevTools,
     flag_descriptions::kUiDevToolsName,
     flag_descriptions::kUiDevToolsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ui_devtools::switches::kEnableUiDevTools)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"enable-autofill-manual-fallback",
     flag_descriptions::kAutofillManualFallbackAndroidName,
     flag_descriptions::kAutofillManualFallbackAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillManualFallbackAndroid)},

    {"enable-autofill-refresh-style",
     flag_descriptions::kEnableAutofillRefreshStyleName,
     flag_descriptions::kEnableAutofillRefreshStyleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRefreshStyleAndroid)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchCalibrationSetting)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"prefer-constant-frame-rate",
     flag_descriptions::kPreferConstantFrameRateName,
     flag_descriptions::kPreferConstantFrameRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPreferConstantFrameRate)},
    {"force-control-face-ae", flag_descriptions::kForceControlFaceAeName,
     flag_descriptions::kForceControlFaceAeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kForceControlFaceAeChoices)},
    {"crostini-gpu-support", flag_descriptions::kCrostiniGpuSupportName,
     flag_descriptions::kCrostiniGpuSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniGpuSupport)},
    {"disable-camera-frame-rotation-at-source",
     flag_descriptions::kDisableCameraFrameRotationAtSourceName,
     flag_descriptions::kDisableCameraFrameRotationAtSourceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::features::kDisableCameraFrameRotationAtSource)},
    {"drive-fs-bidirectional-native-messaging",
     flag_descriptions::kDriveFsBidirectionalNativeMessagingName,
     flag_descriptions::kDriveFsBidirectionalNativeMessagingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kDriveFsBidirectionalNativeMessaging)},
    {"files-archivemount", flag_descriptions::kFilesArchivemountName,
     flag_descriptions::kFilesArchivemountDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesArchivemount)},
    {"files-banner-framework", flag_descriptions::kFilesBannerFrameworkName,
     flag_descriptions::kFilesBannerFrameworkDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesBannerFramework)},
    {"files-filters-in-recents", flag_descriptions::kFiltersInRecentsName,
     flag_descriptions::kFiltersInRecentsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFiltersInRecents)},
    {"files-single-partition-format",
     flag_descriptions::kFilesSinglePartitionFormatName,
     flag_descriptions::kFilesSinglePartitionFormatDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesSinglePartitionFormat)},
    {"files-swa", flag_descriptions::kFilesSWAName,
     flag_descriptions::kFilesSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesSWA)},
    {"files-trash", flag_descriptions::kFilesTrashName,
     flag_descriptions::kFilesTrashDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesTrash)},
    {"files-zip-mount", flag_descriptions::kFilesZipMountName,
     flag_descriptions::kFilesZipMountDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipMount)},
    {"files-zip-pack", flag_descriptions::kFilesZipPackName,
     flag_descriptions::kFilesZipPackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipPack)},
    {"files-zip-unpack", flag_descriptions::kFilesZipUnpackName,
     flag_descriptions::kFilesZipUnpackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesZipUnpack)},
    {"force-spectre-v2-mitigation",
     flag_descriptions::kForceSpectreVariant2MitigationName,
     flag_descriptions::kForceSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         sandbox::policy::features::kForceSpectreVariant2Mitigation)},
    {"spectre-v2-mitigation", flag_descriptions::kSpectreVariant2MitigationName,
     flag_descriptions::kSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(sandbox::policy::features::kSpectreVariant2Mitigation)},
    {"eche-swa", flag_descriptions::kEcheSWAName,
     flag_descriptions::kEcheSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWA)},
    {"eche-swa-resizing", flag_descriptions::kEcheSWAResizingName,
     flag_descriptions::kEcheSWAResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWAResizing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"gdi-text-printing", flag_descriptions::kGdiTextPrinting,
     flag_descriptions::kGdiTextPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kGdiTextPrinting)},
#endif  // defined(OS_WIN)

#if defined(OS_MAC)
    {"new-usb-backend", flag_descriptions::kNewUsbBackendName,
     flag_descriptions::kNewUsbBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(device::kNewUsbBackend)},
#endif  // defined(OS_MAC)

#if defined(OS_ANDROID)
    {"omnibox-adaptive-suggestions-count",
     flag_descriptions::kOmniboxAdaptiveSuggestionsCountName,
     flag_descriptions::kOmniboxAdaptiveSuggestionsCountDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kAdaptiveSuggestionsCount)},
    {"omnibox-assistant-voice-search",
     flag_descriptions::kOmniboxAssistantVoiceSearchName,
     flag_descriptions::kOmniboxAssistantVoiceSearchDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxAssistantVoiceSearch,
                                    kOmniboxAssistantVoiceSearchVariations,
                                    "OmniboxAssistantVoiceSearch")},
    {"omnibox-compact-suggestions",
     flag_descriptions::kOmniboxCompactSuggestionsName,
     flag_descriptions::kOmniboxCompactSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kCompactSuggestions,
                                    kCompactSuggestionsVariations,
                                    "OmniboxCompactSuggestions")},
    {"omnibox-most-visited-tiles",
     flag_descriptions::kOmniboxMostVisitedTilesName,
     flag_descriptions::kOmniboxMostVisitedTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTiles)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-clipboard-suggestions-content-hidden",
     flag_descriptions::kClipboardSuggestionContentHiddenName,
     flag_descriptions::kClipboardSuggestionContentHiddenDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kClipboardSuggestionContentHidden)},
#endif  // defined(OS_ANDROID)

    {"omnibox-clobber-triggers-contextual-web-zero-suggest",
     flag_descriptions::kOmniboxClobberTriggersContextualWebZeroSuggestName,
     flag_descriptions::
         kOmniboxClobberTriggersContextualWebZeroSuggestDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kClobberTriggersContextualWebZeroSuggest,
         // On-clobber has the same variations and forcing IDs as on-focus.
         kOmniboxOnFocusSuggestionsContextualWebVariations,
         "OmniboxGoogleOnContent")},

    {"omnibox-on-device-head-suggestions-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProviderIncognito)},

#if defined(OS_ANDROID)
    {"omnibox-on-device-head-suggestions-non-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderNonIncognito,
         kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadNonIncognitoTuningMobile")},
#endif  // defined(OS_ANDROID)

    {"omnibox-on-focus-suggestions-contextual-web",
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebName,
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnFocusSuggestionsContextualWeb,
         kOmniboxOnFocusSuggestionsContextualWebVariations,
         "OmniboxGoogleOnContent")},

    {"omnibox-on-focus-suggestions-contextual-web-allow-srp",
     flag_descriptions::kOmniboxOnFocusSuggestionsContextualWebAllowSRPName,
     flag_descriptions::
         kOmniboxOnFocusSuggestionsContextualWebAllowSRPDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOnFocusSuggestionsContextualWebAllowSRP)},

    {"omnibox-experimental-suggest-scoring",
     flag_descriptions::kOmniboxExperimentalSuggestScoringName,
     flag_descriptions::kOmniboxExperimentalSuggestScoringDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxExperimentalSuggestScoring)},

    {"omnibox-trending-zero-prefix-suggestions-on-ntp",
     flag_descriptions::kOmniboxTrendingZeroPrefixSuggestionsOnNTPName,
     flag_descriptions::kOmniboxTrendingZeroPrefixSuggestionsOnNTPDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTrendingZeroPrefixSuggestionsOnNTP)},

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
    {"omnibox-experimental-keyword-mode",
     flag_descriptions::kOmniboxExperimentalKeywordModeName,
     flag_descriptions::kOmniboxExperimentalKeywordModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kExperimentalKeywordMode)},
    {"omnibox-short-bookmark-suggestions",
     flag_descriptions::kOmniboxShortBookmarkSuggestionsName,
     flag_descriptions::kOmniboxShortBookmarkSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kShortBookmarkSuggestions)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-pedals-batch2", flag_descriptions::kOmniboxPedalsBatch2Name,
     flag_descriptions::kOmniboxPedalsBatch2Description, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsBatch2)},
    {"omnibox-pedals-batch2-nonenglish",
     flag_descriptions::kOmniboxPedalsBatch2NonEnglishName,
     flag_descriptions::kOmniboxPedalsBatch2NonEnglishDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsBatch2NonEnglish)},
    {"omnibox-pedals-batch3", flag_descriptions::kOmniboxPedalsBatch3Name,
     flag_descriptions::kOmniboxPedalsBatch3Description, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsBatch3)},
    {"omnibox-pedals-default-icon-colored",
     flag_descriptions::kOmniboxPedalsDefaultIconColoredName,
     flag_descriptions::kOmniboxPedalsDefaultIconColoredDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsDefaultIconColored)},
    {"omnibox-keyword-search-button",
     flag_descriptions::kOmniboxKeywordSearchButtonName,
     flag_descriptions::kOmniboxKeywordSearchButtonDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxKeywordSearchButton)},
    {"omnibox-refined-focus-state",
     flag_descriptions::kOmniboxRefinedFocusStateName,
     flag_descriptions::kOmniboxRefinedFocusStateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxRefinedFocusState)},
    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescriptions, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kDocumentProvider,
         kOmniboxDocumentProviderVariations,
         "OmniboxDocumentProviderNonDogfoodExperiments")},
    {"omnibox-rich-autocompletion",
     flag_descriptions::kOmniboxRichAutocompletionName,
     flag_descriptions::kOmniboxRichAutocompletionDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-min-char",
     flag_descriptions::kOmniboxRichAutocompletionMinCharName,
     flag_descriptions::kOmniboxRichAutocompletionMinCharDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionMinCharVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-show-additional-text",
     flag_descriptions::kOmniboxRichAutocompletionShowAdditionalTextName,
     flag_descriptions::kOmniboxRichAutocompletionShowAdditionalTextDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionShowAdditionalTextVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-split",
     flag_descriptions::kOmniboxRichAutocompletionSplitName,
     flag_descriptions::kOmniboxRichAutocompletionSplitDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kOmniboxRichAutocompletionSplitVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-prefer-urls-over-prefixes",
     flag_descriptions::kOmniboxRichAutocompletionPreferUrlsOverPrefixesName,
     flag_descriptions::
         kOmniboxRichAutocompletionPreferUrlsOverPrefixesDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionPreferUrlsOverPrefixesVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-promising",
     flag_descriptions::kOmniboxRichAutocompletionPromisingName,
     flag_descriptions::kOmniboxRichAutocompletionPromisingDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionPromisingVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-bookmark-paths", flag_descriptions::kOmniboxBookmarkPathsName,
     flag_descriptions::kOmniboxBookmarkPathsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kBookmarkPaths,
                                    kOmniboxBookmarkPathsVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-disable-cgi-param-matching",
     flag_descriptions::kOmniboxDisableCGIParamMatchingName,
     flag_descriptions::kOmniboxDisableCGIParamMatchingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDisableCGIParamMatching)},
    {"omnibox-keyword-space-triggering-setting",
     flag_descriptions::kOmniboxKeywordSpaceTriggeringSettingName,
     flag_descriptions::kOmniboxKeywordSpaceTriggeringSettingDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(omnibox::kKeywordSpaceTriggeringSetting)},
    {"omnibox-active-search-engines",
     flag_descriptions::kOmniboxActiveSearchEnginesName,
     flag_descriptions::kOmniboxActiveSearchEnginesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kActiveSearchEngines)},
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"scheduler-configuration", flag_descriptions::kSchedulerConfigurationName,
     flag_descriptions::kSchedulerConfigurationDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kSchedulerConfigurationChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"enable-command-line-on-non-rooted-devices",
     flag_descriptions::kEnableCommandLineOnNonRootedName,
     flag_descriptions::kEnableCommandLineOnNoRootedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCommandLineOnNonRooted)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"request-desktop-site-for-tablets",
     flag_descriptions::kRequestDesktopSiteForTabletsName,
     flag_descriptions::kRequestDesktopSiteForTabletsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kRequestDesktopSiteForTablets,
                                    kRequestDesktopSiteForTabletsVariations,
                                    "RequestDesktopSiteForTablets")},
    {"app-menu-mobile-site-option",
     flag_descriptions::kAppMenuMobileSiteOptionName,
     flag_descriptions::kAppMenuMobileSiteOptionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAppMenuMobileSiteOption)},
#endif  // OS_ANDROID

    {"force-color-profile", flag_descriptions::kForceColorProfileName,
     flag_descriptions::kForceColorProfileDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceColorProfileChoices)},

    {"force-effective-connection-type",
     flag_descriptions::kForceEffectiveConnectionTypeName,
     flag_descriptions::kForceEffectiveConnectionTypeDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceEffectiveConnectionTypeChoices)},

    {"forced-colors", flag_descriptions::kForcedColorsName,
     flag_descriptions::kForcedColorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kForcedColors)},

#if defined(OS_ANDROID)
    {"dynamic-color-gamut", flag_descriptions::kDynamicColorGamutName,
     flag_descriptions::kDynamicColorGamutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDynamicColorGamut)},
#endif

    {"memlog", flag_descriptions::kMemlogName,
     flag_descriptions::kMemlogDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogModeChoices)},

    {"memlog-sampling-rate", flag_descriptions::kMemlogSamplingRateName,
     flag_descriptions::kMemlogSamplingRateDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogSamplingRateChoices)},

    {"memlog-stack-mode", flag_descriptions::kMemlogStackModeName,
     flag_descriptions::kMemlogStackModeDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogStackModeChoices)},

    {"omnibox-default-typed-navigations-to-https",
     flag_descriptions::kOmniboxDefaultTypedNavigationsToHttpsName,
     flag_descriptions::kOmniboxDefaultTypedNavigationsToHttpsDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kDefaultTypedNavigationsToHttps,
         kOmniboxDefaultTypedNavigationsToHttpsVariations,
         "OmniboxDefaultTypedNavigationsToHttps")},

    {"omnibox-max-zero-suggest-matches",
     flag_descriptions::kOmniboxMaxZeroSuggestMatchesName,
     flag_descriptions::kOmniboxMaxZeroSuggestMatchesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMaxZeroSuggestMatches,
                                    kMaxZeroSuggestMatchesVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxBundledExperimentV1")},

    {"omnibox-max-url-matches", flag_descriptions::kOmniboxMaxURLMatchesName,
     flag_descriptions::kOmniboxMaxURLMatchesDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxMaxURLMatches,
                                    kOmniboxMaxURLMatchesVariations,
                                    "OmniboxMaxURLMatchesVariations")},

    {"omnibox-dynamic-max-autocomplete",
     flag_descriptions::kOmniboxDynamicMaxAutocompleteName,
     flag_descriptions::kOmniboxDynamicMaxAutocompleteDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kDynamicMaxAutocomplete,
                                    kOmniboxDynamicMaxAutocompleteVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-webui-omnibox-popup",
     flag_descriptions::kOmniboxWebUIOmniboxPopupName,
     flag_descriptions::kOmniboxWebUIOmniboxPopupDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kWebUIOmniboxPopup)},

    {"memories", flag_descriptions::kMemoriesName,
     flag_descriptions::kMemoriesDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history_clusters::kMemories,
                                    kMemoriesVariations,
                                    "Memories")},

    {"memories-debug", flag_descriptions::kMemoriesDebugName,
     flag_descriptions::kMemoriesDebugDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(history_clusters::kDebug)},

    {"search-prefetch", flag_descriptions::kEnableSearchPrefetchName,
     flag_descriptions::kEnableSearchPrefetchDescription, kOsAll,
     SINGLE_VALUE_TYPE(kSearchPrefetchServiceCommandLineFlag)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"handwriting-gesture-editing",
     flag_descriptions::kHandwritingGestureEditingName,
     flag_descriptions::kHandwritingGestureEditingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHandwritingGestureEditing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"block-insecure-private-network-requests",
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsName,
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBlockInsecurePrivateNetworkRequests)},

    {"cross-origin-embedder-policy-credentialless",
     flag_descriptions::kCrossOriginEmbedderPolicyCredentiallessName,
     flag_descriptions::kCrossOriginEmbedderPolicyCredentiallessDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         network::features::kCrossOriginEmbedderPolicyCredentialless)},

    {"disable-keepalive-fetch", flag_descriptions::kDisableKeepaliveFetchName,
     flag_descriptions::kDisableKeepaliveFetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kDisableKeepaliveFetch)},

    {"mbi-mode", flag_descriptions::kMBIModeName,
     flag_descriptions::kMBIModeDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kMBIMode,
                                    kMBIModeVariations,
                                    "MBIMode")},

    {"intensive-wake-up-throttling",
     flag_descriptions::kIntensiveWakeUpThrottlingName,
     flag_descriptions::kIntensiveWakeUpThrottlingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kIntensiveWakeUpThrottling,
                                    kIntensiveWakeUpThrottlingVariations,
                                    "IntensiveWakeUpThrottling")},

#if defined(OS_ANDROID)
    {"omnibox-spare-renderer", flag_descriptions::kOmniboxSpareRendererName,
     flag_descriptions::kOmniboxSpareRendererDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSpareRenderer)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"double-tap-to-zoom-in-tablet-mode",
     flag_descriptions::kDoubleTapToZoomInTabletModeName,
     flag_descriptions::kDoubleTapToZoomInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDoubleTapToZoomInTabletMode)},

    {"quick-settings-pwa-notifications",
     flag_descriptions::kQuickSettingsPWANotificationsName,
     flag_descriptions::kQuickSettingsPWANotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kQuickSettingsPWANotifications)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {flag_descriptions::kReadLaterFlagId, flag_descriptions::kReadLaterName,
     flag_descriptions::kReadLaterDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(reading_list::switches::kReadLater)},

    {"read-later-new-badge-promo",
     flag_descriptions::kReadLaterNewBadgePromoName,
     flag_descriptions::kReadLaterNewBadgePromoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadLaterNewBadgePromo)},

#ifdef OS_ANDROID
    {"read-later-reminder-notification",
     flag_descriptions::kReadLaterReminderNotificationName,
     flag_descriptions::kReadLaterReminderNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         reading_list::switches::kReadLaterReminderNotification)},

    {"bookmark-bottom-sheet", flag_descriptions::kBookmarkBottomSheetName,
     flag_descriptions::kBookmarkBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBookmarkBottomSheet)},
#endif

    {"tab-groups-auto-create", flag_descriptions::kTabGroupsAutoCreateName,
     flag_descriptions::kTabGroupsAutoCreateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsAutoCreate)},

    {"tab-groups-collapse-freezing",
     flag_descriptions::kTabGroupsCollapseFreezingName,
     flag_descriptions::kTabGroupsCollapseFreezingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsCollapseFreezing)},

    {"tab-groups-feedback", flag_descriptions::kTabGroupsFeedbackName,
     flag_descriptions::kTabGroupsFeedbackDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsFeedback)},

    {"tab-groups-new-badge-promo",
     flag_descriptions::kTabGroupsNewBadgePromoName,
     flag_descriptions::kTabGroupsNewBadgePromoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsNewBadgePromo)},

    {"tab-groups-save", flag_descriptions::kTabGroupsSaveName,
     flag_descriptions::kTabGroupsSaveDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsSave)},

    {"new-tabstrip-animation", flag_descriptions::kNewTabstripAnimationName,
     flag_descriptions::kNewTabstripAnimationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNewTabstripAnimation)},

    {flag_descriptions::kScrollableTabStripFlagId,
     flag_descriptions::kScrollableTabStripName,
     flag_descriptions::kScrollableTabStripDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kScrollableTabStrip,
                                    kTabScrollingVariations,
                                    "TabScrolling")},

    {"scrollable-tabstrip-buttons",
     flag_descriptions::kScrollableTabStripButtonsName,
     flag_descriptions::kScrollableTabStripButtonsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kScrollableTabStripButtons)},

    {flag_descriptions::kSidePanelFlagId, flag_descriptions::kSidePanelName,
     flag_descriptions::kSidePanelDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanel)},

    {"tab-outlines-in-low-contrast-themes",
     flag_descriptions::kTabOutlinesInLowContrastThemesName,
     flag_descriptions::kTabOutlinesInLowContrastThemesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabOutlinesInLowContrastThemes)},

    {"prominent-dark-mode-active-tab-title",
     flag_descriptions::kProminentDarkModeActiveTabTitleName,
     flag_descriptions::kProminentDarkModeActiveTabTitleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kProminentDarkModeActiveTabTitle)},

    {"promo-browser-commands", flag_descriptions::kPromoBrowserCommandsName,
     flag_descriptions::kPromoBrowserCommandsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kPromoBrowserCommands,
                                    kPromoBrowserCommandsVariations,
                                    "PromoBrowserCommands")},
#if defined(OS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"webui-feedback", flag_descriptions::kWebuiFeedbackName,
     flag_descriptions::kWebuiFeedbackDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIFeedback)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
    {"ntp-cache-one-google-bar", flag_descriptions::kNtpCacheOneGoogleBarName,
     flag_descriptions::kNtpCacheOneGoogleBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCacheOneGoogleBar)},

    {"ntp-repeatable-queries", flag_descriptions::kNtpRepeatableQueriesName,
     flag_descriptions::kNtpRepeatableQueriesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpRepeatableQueries)},

    {"ntp-modules", flag_descriptions::kNtpModulesName,
     flag_descriptions::kNtpModulesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kModules)},

    {"ntp-drive-module", flag_descriptions::kNtpDriveModuleName,
     flag_descriptions::kNtpDriveModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpDriveModule,
                                    kNtpDriveModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-recipe-tasks-module", flag_descriptions::kNtpRecipeTasksModuleName,
     flag_descriptions::kNtpRecipeTasksModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpRecipeTasksModule,
                                    kNtpRecipeTasksModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-shopping-tasks-module",
     flag_descriptions::kNtpShoppingTasksModuleName,
     flag_descriptions::kNtpShoppingTasksModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpShoppingTasksModule,
                                    kNtpShoppingTasksModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-chrome-cart-module", flag_descriptions::kNtpChromeCartModuleName,
     flag_descriptions::kNtpChromeCartModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpChromeCartModule,
                                    kNtpChromeCartModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-modules-drag-and-drop", flag_descriptions::kNtpModulesDragAndDropName,
     flag_descriptions::kNtpModulesDragAndDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesDragAndDrop)},

    {"ntp-modules-redesigned", flag_descriptions::kNtpModulesRedesignedName,
     flag_descriptions::kNtpModulesRedesignedDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesRedesigned)},

    {"ntp-realbox-suggestion-answers",
     flag_descriptions::kNtpRealboxSuggestionAnswersName,
     flag_descriptions::kNtpRealboxSuggestionAnswersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kNtpRealboxSuggestionAnswers)},
#endif  // !defined(OS_ANDROID)

#if defined(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, kOsWin,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // defined(DCHECK_IS_CONFIGURABLE)

    {"enable-pixel-canvas-recording",
     flag_descriptions::kEnablePixelCanvasRecordingName,
     flag_descriptions::kEnablePixelCanvasRecordingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnablePixelCanvasRecording)},

    {"enable-parallel-downloading", flag_descriptions::kParallelDownloadingName,
     flag_descriptions::kParallelDownloadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kParallelDownloading)},

    {"enable-pointer-lock-options", flag_descriptions::kPointerLockOptionsName,
     flag_descriptions::kPointerLockOptionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPointerLockOptions)},

#if defined(OS_ANDROID)
    {"enable-async-dns", flag_descriptions::kAsyncDnsName,
     flag_descriptions::kAsyncDnsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAsyncDns)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"download-auto-resumption-native",
     flag_descriptions::kDownloadAutoResumptionNativeName,
     flag_descriptions::kDownloadAutoResumptionNativeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadAutoResumptionNative)},

    {"download-later", flag_descriptions::kDownloadLaterName,
     flag_descriptions::kDownloadLaterDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadLater)},

    {"download-later-debug-on-wifi",
     flag_descriptions::kDownloadLaterDebugOnWifiName,
     flag_descriptions::kDownloadLaterDebugOnWifiNameDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(download::switches::kDownloadLaterDebugOnWifi)},
#endif

    {"enable-new-download-backend",
     flag_descriptions::kEnableNewDownloadBackendName,
     flag_descriptions::kEnableNewDownloadBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kUseDownloadOfflineContentProvider)},

#if defined(OS_ANDROID)
    {"screen-capture-android", flag_descriptions::kUserMediaScreenCapturingName,
     flag_descriptions::kUserMediaScreenCapturingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUserMediaScreenCapturing)},
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"chrome-tips-in-main-menu", flag_descriptions::kChromeTipsInMainMenuName,
     flag_descriptions::kChromeTipsInMainMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeTipsInMainMenu)},

    {"chrome-tips-in-main-menu-new-badge",
     flag_descriptions::kChromeTipsInMainMenuNewBadgeName,
     flag_descriptions::kChromeTipsInMainMenuNewBadgeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeTipsInMainMenuNewBadge)},
#endif

    {"tab-hover-card-images", flag_descriptions::kTabHoverCardImagesName,
     flag_descriptions::kTabHoverCardImagesDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabHoverCardImages,
                                    kTabHoverCardImagesVariations,
                                    "TabHoverCardImages")},

    {"enable-storage-pressure-event",
     flag_descriptions::kStoragePressureEventName,
     flag_descriptions::kStoragePressureEventDescription, kOsAll,
     FEATURE_VALUE_TYPE(storage::features::kStoragePressureEvent)},

    {"installed-apps-in-cbd", flag_descriptions::kInstalledAppsInCbdName,
     flag_descriptions::kInstalledAppsInCbdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kInstalledAppsInCbd)},

    {"enable-network-logging-to-file",
     flag_descriptions::kEnableNetworkLoggingToFileName,
     flag_descriptions::kEnableNetworkLoggingToFileDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kLogNetLog)},

    {"enable-web-authentication-cable-v2-support",
     flag_descriptions::kEnableWebAuthenticationCableV2SupportName,
     flag_descriptions::kEnableWebAuthenticationCableV2SupportDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(device::kWebAuthCableSecondFactor)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-web-authentication-chromeos-authenticator",
     flag_descriptions::kEnableWebAuthenticationChromeOSAuthenticatorName,
     flag_descriptions::
         kEnableWebAuthenticationChromeOSAuthenticatorDescription,
     kOsCrOS, FEATURE_VALUE_TYPE(device::kWebAuthCrosPlatformAuthenticator)},
#endif
#if BUILDFLAG(ENABLE_PDF)
    {"accessible-pdf-form", flag_descriptions::kAccessiblePDFFormName,
     flag_descriptions::kAccessiblePDFFormDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kAccessiblePDFForm)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
#if defined(OS_MAC)
    {"cups-ipp-printing-backend",
     flag_descriptions::kCupsIppPrintingBackendName,
     flag_descriptions::kCupsIppPrintingBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(printing::features::kCupsIppPrintingBackend)},
#endif  // defined(OS_MAC)

#if defined(OS_WIN)
    {"print-with-reduced-rasterization",
     flag_descriptions::kPrintWithReducedRasterizationName,
     flag_descriptions::kPrintWithReducedRasterizationDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kPrintWithReducedRasterization)},

    {"use-xps-for-printing", flag_descriptions::kUseXpsForPrintingName,
     flag_descriptions::kUseXpsForPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kUseXpsForPrinting)},

    {"use-xps-for-printing-from-pdf",
     flag_descriptions::kUseXpsForPrintingFromPdfName,
     flag_descriptions::kUseXpsForPrintingFromPdfDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kUseXpsForPrintingFromPdf)},
#endif  // defined(OS_WIN)
#endif  // BUILDFLAG(ENABLE_PRINTING)

    {"autofill-profile-client-validation",
     flag_descriptions::kAutofillProfileClientValidationName,
     flag_descriptions::kAutofillProfileClientValidationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillProfileClientValidation)},

    {"autofill-profile-server-validation",
     flag_descriptions::kAutofillProfileServerValidationName,
     flag_descriptions::kAutofillProfileServerValidationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillProfileServerValidation)},

#if defined(OS_WIN)
    {"enable-windows-gaming-input-data-fetcher",
     flag_descriptions::kEnableWindowsGamingInputDataFetcherName,
     flag_descriptions::kEnableWindowsGamingInputDataFetcherDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableWindowsGamingInputDataFetcher)},
#endif

#if defined(OS_ANDROID)
    {"enable-start-surface", flag_descriptions::kStartSurfaceAndroidName,
     flag_descriptions::kStartSurfaceAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kStartSurfaceAndroid,
                                    kStartSurfaceAndroidVariations,
                                    "ChromeStart")},

    {"enable-instant-start", flag_descriptions::kInstantStartName,
     flag_descriptions::kInstantStartDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInstantStart)},

    {"enable-close-tab-suggestions",
     flag_descriptions::kCloseTabSuggestionsName,
     flag_descriptions::kCloseTabSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCloseTabSuggestions,
                                    kCloseTabSuggestionsStaleVariations,
                                    "CloseSuggestionsTab")},

    {"enable-critical-persisted-tab-data",
     flag_descriptions::kCriticalPersistedTabDataName,
     flag_descriptions::kCriticalPersistedTabDataDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCriticalPersistedTabData)},

    {"enable-store-hours", flag_descriptions::kStoreHoursAndroidName,
     flag_descriptions::kStoreHoursAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kStoreHoursAndroid)},

    {"enable-tab-grid-layout", flag_descriptions::kTabGridLayoutAndroidName,
     flag_descriptions::kTabGridLayoutAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabGridLayoutAndroid,
                                    kTabGridLayoutAndroidVariations,
                                    "TabGridLayoutAndroid")},

    {"enable-commerce-merchant-viewer",
     flag_descriptions::kCommerceMerchantViewerAndroidName,
     flag_descriptions::kCommerceMerchantViewerAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kCommerceMerchantViewer)},

    {"enable-commerce-price-tracking",
     flag_descriptions::kCommercePriceTrackingAndroidName,
     flag_descriptions::kCommercePriceTrackingAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kCommercePriceTracking,
                                    kCommercePriceTrackingAndroidVariations,
                                    "CommercePriceTrackingAndroid")},

    {"enable-tab-groups", flag_descriptions::kTabGroupsAndroidName,
     flag_descriptions::kTabGroupsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsAndroid)},

    {"enable-tab-groups-continuation",
     flag_descriptions::kTabGroupsContinuationAndroidName,
     flag_descriptions::kTabGroupsContinuationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsContinuationAndroid)},

    {"enable-tab-groups-ui-improvements",
     flag_descriptions::kTabGroupsUiImprovementsAndroidName,
     flag_descriptions::kTabGroupsUiImprovementsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsUiImprovementsAndroid)},

    {"enable-tab-switcher-on-return",
     flag_descriptions::kTabSwitcherOnReturnName,
     flag_descriptions::kTabSwitcherOnReturnDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabSwitcherOnReturn,
                                    kTabSwitcherOnReturnVariations,
                                    "ChromeStart")},

    {"enable-tab-to-gts-animation",
     flag_descriptions::kTabToGTSAnimationAndroidName,
     flag_descriptions::kTabToGTSAnimationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabToGTSAnimation)},

    {"enable-tab-engagement-reporting",
     flag_descriptions::kTabEngagementReportingName,
     flag_descriptions::kTabEngagementReportingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabEngagementReportingAndroid)},

    {"enable-conditional-tabstrip",
     flag_descriptions::kConditionalTabStripAndroidName,
     flag_descriptions::kConditionalTabStripAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kConditionalTabStripAndroid,
         kConditionalTabStripAndroidVariations,
         "ConditioanlTabStrip")},

    {"enable-quick-action-search-widget-android",
     flag_descriptions::kQuickActionSearchWidgetAndroidName,
     flag_descriptions::kQuickActionSearchWidgetAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kQuickActionSearchWidgetAndroid)},

#endif  // OS_ANDROID

    {"enable-layout-ng", flag_descriptions::kEnableLayoutNGName,
     flag_descriptions::kEnableLayoutNGDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLayoutNG)},

    {"enable-table-ng", flag_descriptions::kEnableLayoutNGTableName,
     flag_descriptions::kEnableLayoutNGTableDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLayoutNGTable)},

    {"enable-lazy-image-loading",
     flag_descriptions::kEnableLazyImageLoadingName,
     flag_descriptions::kEnableLazyImageLoadingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kLazyImageLoading,
                                    kLazyImageLoadingVariations,
                                    "LazyLoad")},

    {"enable-lazy-frame-loading",
     flag_descriptions::kEnableLazyFrameLoadingName,
     flag_descriptions::kEnableLazyFrameLoadingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kLazyFrameLoading,
                                    kLazyFrameLoadingVariations,
                                    "LazyLoad")},

    {"autofill-enable-toolbar-status-chip",
     flag_descriptions::kAutofillEnableToolbarStatusChipName,
     flag_descriptions::kAutofillEnableToolbarStatusChipDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableToolbarStatusChip)},

    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         network::switches::kUnsafelyTreatInsecureOriginAsSecure,
         "")},

    {"treat-unsafe-downloads-as-active-content",
     flag_descriptions::kTreatUnsafeDownloadsAsActiveName,
     flag_descriptions::kTreatUnsafeDownloadsAsActiveDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kTreatUnsafeDownloadsAsActive)},

    {"detect-target-embedding-lookalikes",
     flag_descriptions::kDetectTargetEmbeddingLookalikesName,
     flag_descriptions::kDetectTargetEmbeddingLookalikesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         lookalikes::features::kDetectTargetEmbeddingLookalikes)},

    {"disable-process-reuse", flag_descriptions::kDisableProcessReuse,
     flag_descriptions::kDisableProcessReuseDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDisableProcessReuse)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-app-data-search", flag_descriptions::kEnableAppDataSearchName,
     flag_descriptions::kEnableAppDataSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppDataSearch)},

    {"enable-app-grid-ghost", flag_descriptions::kEnableAppGridGhostName,
     flag_descriptions::kEnableAppGridGhostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppGridGhost)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
    {"enable-accessibility-live-caption",
     flag_descriptions::kEnableAccessibilityLiveCaptionName,
     flag_descriptions::kEnableAccessibilityLiveCaptionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kLiveCaption)},
    {"enable-accessibility-live-caption-soda",
     flag_descriptions::kEnableAccessibilityLiveCaptionSodaName,
     flag_descriptions::kEnableAccessibilityLiveCaptionSodaDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(media::kUseSodaForLiveCaption)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"cct-incognito", flag_descriptions::kCCTIncognitoName,
     flag_descriptions::kCCTIncognitoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognito)},
#endif

#if defined(OS_ANDROID)
    {"cct-incognito-available-to-third-party",
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyName,
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognitoAvailableToThirdParty)},
#endif

#if defined(OS_ANDROID)
    {"enable-use-aaudio-driver", flag_descriptions::kEnableUseAaudioDriverName,
     flag_descriptions::kEnableUseAaudioDriverDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUseAAudioDriver)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enforce-system-aec", flag_descriptions::kCrOSEnforceSystemAecName,
     flag_descriptions::kCrOSEnforceSystemAecDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAec)},
    {"enforce-system-aec-agc", flag_descriptions::kCrOSEnforceSystemAecAgcName,
     flag_descriptions::kCrOSEnforceSystemAecAgcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAecAgc)},
    {"enforce-system-aec-ns-agc",
     flag_descriptions::kCrOSEnforceSystemAecNsAgcName,
     flag_descriptions::kCrOSEnforceSystemAecNsAgcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAecNsAgc)},
    {"enforce-system-aec-ns", flag_descriptions::kCrOSEnforceSystemAecNsName,
     flag_descriptions::kCrOSEnforceSystemAecNsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAecNs)},
#endif

    {"enable-sxg-subresource-prefetching",
     flag_descriptions::kEnableSignedExchangeSubresourcePrefetchName,
     flag_descriptions::kEnableSignedExchangeSubresourcePrefetchDescription,
     kOsAll, FEATURE_VALUE_TYPE(features::kSignedExchangeSubresourcePrefetch)},

    {"enable-sxg-prefetch-cache-for-navigations",
     flag_descriptions::kEnableSignedExchangePrefetchCacheForNavigationsName,
     flag_descriptions::
         kEnableSignedExchangePrefetchCacheForNavigationsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kSignedExchangePrefetchCacheForNavigations)},

    {"enable-autofill-account-wallet-storage",
     flag_descriptions::kEnableAutofillAccountWalletStorageName,
     flag_descriptions::kEnableAutofillAccountWalletStorageDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableAccountWalletStorage)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-zero-state-app-reinstall-suggestions",
     flag_descriptions::kEnableAppReinstallZeroStateName,
     flag_descriptions::kEnableAppReinstallZeroStateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppReinstallZeroState)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"reduce-horizontal-fling-velocity",
     flag_descriptions::kReduceHorizontalFlingVelocityName,
     flag_descriptions::kReduceHorizontalFlingVelocityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kReduceHorizontalFlingVelocity)},

    {"enable-resampling-input-events",
     flag_descriptions::kEnableResamplingInputEventsName,
     flag_descriptions::kEnableResamplingInputEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kResamplingInputEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingInputEvents")},

    {"enable-resampling-scroll-events",
     flag_descriptions::kEnableResamplingScrollEventsName,
     flag_descriptions::kEnableResamplingScrollEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kResamplingScrollEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingScrollEvents")},

    // Should only be available if kResamplingScrollEvents is on, and using
    // linear resampling.
    {"enable-resampling-scroll-events-experimental-prediction",
     flag_descriptions::kEnableResamplingScrollEventsExperimentalPredictionName,
     flag_descriptions::
         kEnableResamplingScrollEventsExperimentalPredictionDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ::features::kResamplingScrollEventsExperimentalPrediction,
         kResamplingScrollEventsExperimentalPredictionVariations,
         "ResamplingScrollEventsExperimentalLatency")},

    {"enable-filtering-scroll-events",
     flag_descriptions::kFilteringScrollPredictionName,
     flag_descriptions::kFilteringScrollPredictionDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kFilteringScrollPrediction,
                                    kFilteringPredictionFeatureVariations,
                                    "FilteringScrollPrediction")},

    {"compositor-threaded-scrollbar-scrolling",
     flag_descriptions::kCompositorThreadedScrollbarScrollingName,
     flag_descriptions::kCompositorThreadedScrollbarScrollingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kCompositorThreadedScrollbarScrolling)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-vaapi-jpeg-image-decode-acceleration",
     flag_descriptions::kVaapiJpegImageDecodeAccelerationName,
     flag_descriptions::kVaapiJpegImageDecodeAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVaapiJpegImageDecodeAcceleration)},

    {"enable-vaapi-webp-image-decode-acceleration",
     flag_descriptions::kVaapiWebPImageDecodeAccelerationName,
     flag_descriptions::kVaapiWebPImageDecodeAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVaapiWebPImageDecodeAcceleration)},
#endif

#if defined(OS_WIN)
    {"calculate-native-win-occlusion",
     flag_descriptions::kCalculateNativeWinOcclusionName,
     flag_descriptions::kCalculateNativeWinOcclusionDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kCalculateNativeWinOcclusion)},
#endif  // OS_WIN

#if !defined(OS_ANDROID)
    {"happiness-tracking-surveys-for-desktop-demo",
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktopDemo)},

    {"happiness-tracking-surveys-for-desktop-privacy-sandbox",
     flag_descriptions::kHappinessTrackingSurveysForDesktopPrivacySandboxName,
     flag_descriptions::
         kHappinessTrackingSurveysForDesktopPrivacySandboxDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::kHappinessTrackingSurveysForDesktopPrivacySandbox)},

    {"happiness-tracking-surveys-for-desktop-settings",
     flag_descriptions::kHappinessTrackingSurveysForDesktopSettingsName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopSettingsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktopSettings)},

    {"happiness-tracking-surveys-for-desktop-settings-privacy",
     flag_descriptions::kHappinessTrackingSurveysForDesktopSettingsPrivacyName,
     flag_descriptions::
         kHappinessTrackingSurveysForDesktopSettingsPrivacyDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::kHappinessTrackingSurveysForDesktopSettingsPrivacy)},
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"enable-sign-in-profile-creation",
     flag_descriptions::kSignInProfileCreationName,
     flag_descriptions::kSignInProfileCreationDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kSignInProfileCreation)},

    {"enable-sign-in-profile-creation-enterprise",
     flag_descriptions::kSignInProfileCreationEnterpriseName,
     flag_descriptions::kSignInProfileCreationEnterpriseDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kSignInProfileCreationEnterprise)},
#endif

    {"destroy-profile-on-browser-close",
     flag_descriptions::kDestroyProfileOnBrowserCloseName,
     flag_descriptions::kDestroyProfileOnBrowserCloseDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kDestroyProfileOnBrowserClose)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-arc-unified-audio-focus",
     flag_descriptions::kEnableArcUnifiedAudioFocusName,
     flag_descriptions::kEnableArcUnifiedAudioFocusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUnifiedAudioFocusFeature)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescription, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoices)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-dsp", flag_descriptions::kEnableGoogleAssistantDspName,
     flag_descriptions::kEnableGoogleAssistantDspDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kEnableDspHotword)},

    {"enable-assistant-app-support",
     flag_descriptions::kEnableAssistantAppSupportName,
     flag_descriptions::kEnableAssistantAppSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantAppSupport)},

    {"enable-quick-answers", flag_descriptions::kEnableQuickAnswersName,
     flag_descriptions::kEnableQuickAnswersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswers)},

    {"enable-quick-answers-on-editable-text",
     flag_descriptions::kEnableQuickAnswersOnEditableTextName,
     flag_descriptions::kEnableQuickAnswersOnEditableTextDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersOnEditableText)},

    {"enable-quick-answers-text-annotator",
     flag_descriptions::kEnableQuickAnswersTextAnnotatorName,
     flag_descriptions::kEnableQuickAnswersTextAnnotatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTextAnnotator)},

    {"enable-quick-answers-translation",
     flag_descriptions::kEnableQuickAnswersTranslationName,
     flag_descriptions::kEnableQuickAnswersTranslationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTranslation)},

    {"enable-quick-answers-translation-cloud-api",
     flag_descriptions::kEnableQuickAnswersTranslationCloudAPIName,
     flag_descriptions::kEnableQuickAnswersTranslationCloudAPIDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersTranslationCloudAPI)},

    {"enable-quick-answers-v2", flag_descriptions::kEnableQuickAnswersV2Name,
     flag_descriptions::kEnableQuickAnswersV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersV2)},

    {kAssistantBetterOnboardingInternalName,
     flag_descriptions::kEnableAssistantBetterOnboardingName,
     flag_descriptions::kEnableAssistantBetterOnboardingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kAssistantBetterOnboarding)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"new-window-app-menu", flag_descriptions::kNewWindowAppMenuName,
     flag_descriptions::kNewWindowAppMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNewWindowAppMenu)},

    {"instance-switcher", flag_descriptions::kInstanceSwitcherName,
     flag_descriptions::kInstanceSwitcherDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInstanceSwitcher)},
#endif  // defined(OS_ANDROID)

    {"enable-gamepad-button-axis-events",
     flag_descriptions::kEnableGamepadButtonAxisEventsName,
     flag_descriptions::kEnableGamepadButtonAxisEventsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableGamepadButtonAxisEvents)},

    {"restrict-gamepad-access", flag_descriptions::kRestrictGamepadAccessName,
     flag_descriptions::kRestrictGamepadAccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRestrictGamepadAccess)},

    {"shared-clipboard-ui", flag_descriptions::kSharedClipboardUIName,
     flag_descriptions::kSharedClipboardUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharedClipboardUI)},

    {"sharing-prefer-vapid", flag_descriptions::kSharingPreferVapidName,
     flag_descriptions::kSharingPreferVapidDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingPreferVapid)},

    {"sharing-send-via-sync", flag_descriptions::kSharingSendViaSyncName,
     flag_descriptions::kSharingSendViaSyncDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingSendViaSync)},

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
    {"sharing-hub-desktop-app-menu",
     flag_descriptions::kSharingHubDesktopAppMenuName,
     flag_descriptions::kSharingHubDesktopAppMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(sharing_hub::kSharingHubDesktopAppMenu)},
    {"sharing-hub-desktop-omnibox",
     flag_descriptions::kSharingHubDesktopOmniboxName,
     flag_descriptions::kSharingHubDesktopOmniboxDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(sharing_hub::kSharingHubDesktopOmnibox)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ash-enable-pip-rounded-corners",
     flag_descriptions::kAshEnablePipRoundedCornersName,
     flag_descriptions::kAshEnablePipRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipRoundedCorners)},

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-stereo-input",
     flag_descriptions::kEnableGoogleAssistantStereoInputName,
     flag_descriptions::kEnableGoogleAssistantStereoInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableStereoAudioInput)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-audio-focus-enforcement",
     flag_descriptions::kEnableAudioFocusEnforcementName,
     flag_descriptions::kEnableAudioFocusEnforcementDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_session::features::kAudioFocusEnforcement)},
    {"enable-media-session-service",
     flag_descriptions::kEnableMediaSessionServiceName,
     flag_descriptions::kEnableMediaSessionServiceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_session::features::kMediaSessionService)},
    {"enable-gpu-service-logging",
     flag_descriptions::kEnableGpuServiceLoggingName,
     flag_descriptions::kEnableGpuServiceLoggingDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableGPUServiceLogging)},

#if !defined(OS_ANDROID)
    {"hardware-media-key-handling",
     flag_descriptions::kHardwareMediaKeyHandling,
     flag_descriptions::kHardwareMediaKeyHandlingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kHardwareMediaKeyHandling)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"app-management-intent-settings",
     flag_descriptions::kAppManagementIntentSettingsName,
     flag_descriptions::kAppManagementIntentSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppManagementIntentSettings)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"app-service-adaptive-icon",
     flag_descriptions::kAppServiceAdaptiveIconName,
     flag_descriptions::kAppServiceAdaptiveIconDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceAdaptiveIcon)},

    {"app-service-external-protocol",
     flag_descriptions::kAppServiceExternalProtocolName,
     flag_descriptions::kAppServiceExternalProtocolDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceExternalProtocol)},

    {"arc-ghost-window", flag_descriptions::kArcGhostWindowName,
     flag_descriptions::kArcGhostWindowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kArcGhostWindow)},

    {"arc-resize-lock", flag_descriptions::kArcResizeLockName,
     flag_descriptions::kArcResizeLockDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kArcResizeLock)},

    {"full-restore", flag_descriptions::kFullRestoreName,
     flag_descriptions::kFullRestoreDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kFullRestore)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},

    {"intent-picker-pwa-persistence",
     flag_descriptions::kIntentPickerPWAPersistenceName,
     flag_descriptions::kIntentPickerPWAPersistenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kIntentPickerPWAPersistence)},

    {"intent-handling-sharing", flag_descriptions::kIntentHandlingSharingName,
     flag_descriptions::kIntentHandlingSharingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kIntentHandlingSharing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"d3d11-video-decoder", flag_descriptions::kD3D11VideoDecoderName,
     flag_descriptions::kD3D11VideoDecoderDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kD3D11VideoDecoder)},
#endif

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    // TODO(b/180051795): remove kOsLinux when lacros-chrome switches to
    // kOsCrOS.
    {"chromeos-direct-video-decoder",
     flag_descriptions::kChromeOSDirectVideoDecoderName,
     flag_descriptions::kChromeOSDirectVideoDecoderDescription,
     kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(media::kUseChromeOSDirectVideoDecoder)},
#endif

#if defined(OS_ANDROID)
    {"deprecate-menagerie-api", flag_descriptions::kDeprecateMenagerieAPIName,
     flag_descriptions::kDeprecateMenagerieAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kDeprecateMenagerieAPI)},
    {"wipe-data-on-child-account-signin",
     flag_descriptions::kWipeDataOnChildAccountSigninName,
     flag_descriptions::kWipeDataOnChildAccountSigninDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kWipeDataOnChildAccountSignin)},
    {"mobile-identity-consistency",
     flag_descriptions::kMobileIdentityConsistencyName,
     flag_descriptions::kMobileIdentityConsistencyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(signin::kMobileIdentityConsistency)},
    {"mobile-identity-consistency-var",
     flag_descriptions::kMobileIdentityConsistencyVarName,
     flag_descriptions::kMobileIdentityConsistencyVarDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(signin::kMobileIdentityConsistencyVar,
                                    kMobileIdentityConsistencyVariations,
                                    "MobileIdentityConsistencyVar")},
    {"mobile-identity-consistency-fre",
     flag_descriptions::kMobileIdentityConsistencyFREName,
     flag_descriptions::kMobileIdentityConsistencyFREDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(signin::kMobileIdentityConsistencyFRE)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
#endif  // defined(OS_ANDROID)

    {"autofill-use-improved-label-disambiguation",
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationName,
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseImprovedLabelDisambiguation)},

    {"file-handling-api", flag_descriptions::kFileHandlingAPIName,
     flag_descriptions::kFileHandlingAPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFileHandlingAPI)},

    {"file-handling-icons", flag_descriptions::kFileHandlingIconsName,
     flag_descriptions::kFileHandlingIconsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFileHandlingIcons)},

#if defined(TOOLKIT_VIEWS)
    {"installable-ink-drop", flag_descriptions::kInstallableInkDropName,
     flag_descriptions::kInstallableInkDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::kInstallableInkDropFeature)},
#endif  // defined(TOOLKIT_VIEWS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-launcher-integration",
     flag_descriptions::kEnableAssistantLauncherIntegrationName,
     flag_descriptions::kEnableAssistantLauncherIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAssistantSearch)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"strict-extension-isolation",
     flag_descriptions::kStrictExtensionIsolationName,
     flag_descriptions::kStrictExtensionIsolationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(extensions_features::kStrictExtensionIsolation)},
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    {"strict-origin-isolation", flag_descriptions::kStrictOriginIsolationName,
     flag_descriptions::kStrictOriginIsolationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kStrictOriginIsolation)},

#if defined(OS_ANDROID)
    {"enable-logging-js-console-messages",
     flag_descriptions::kLogJsConsoleMessagesName,
     flag_descriptions::kLogJsConsoleMessagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLogJsConsoleMessages)},
#endif  // OS_ANDROID

    {"enable-skia-renderer", flag_descriptions::kSkiaRendererName,
     flag_descriptions::kSkiaRendererDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUseSkiaRenderer)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"allow-disable-mouse-acceleration",
     flag_descriptions::kAllowDisableMouseAccelerationName,
     flag_descriptions::kAllowDisableMouseAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowDisableMouseAcceleration)},

    {"allow-repeated-updates", flag_descriptions::kAllowRepeatedUpdatesName,
     flag_descriptions::kAllowRepeatedUpdatesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowRepeatedUpdates)},

    {"allow-scroll-settings", flag_descriptions::kAllowScrollSettingsName,
     flag_descriptions::kAllowScrollSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowScrollSettings)},

    {"bluetooth-advertisement-monitoring",
     flag_descriptions::kBluetoothAdvertisementMonitoringName,
     flag_descriptions::kBluetoothAdvertisementMonitoringDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBluetoothAdvertisementMonitoring)},

    {"enable-neural-stylus-palm-rejection",
     flag_descriptions::kEnableNeuralStylusPalmRejectionName,
     flag_descriptions::kEnableNeuralStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableNeuralPalmDetectionFilter)},

    {"enable-os-feedback", flag_descriptions::kEnableOsFeedbackName,
     flag_descriptions::kEnableOsFeedbackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOsFeedback)},

    {"enable-palm-max-touch-major",
     flag_descriptions::kEnablePalmOnMaxTouchMajorName,
     flag_descriptions::kEnablePalmOnMaxTouchMajorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmOnMaxTouchMajor)},

    {"enable-palm-tool-type-palm",
     flag_descriptions::kEnablePalmOnToolTypePalmName,
     flag_descriptions::kEnablePalmOnToolTypePalmName, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmOnToolTypePalm)},

    {"enable-pci-guard-ui", flag_descriptions::kEnablePciguardUiName,
     flag_descriptions::kEnablePciguardUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnablePciguardUi)},

    {"enable-heuristic-stylus-palm-rejection",
     flag_descriptions::kEnableHeuristicStylusPalmRejectionName,
     flag_descriptions::kEnableHeuristicStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeuristicPalmDetectionFilter)},

    {"enable-hide-arc-media-notifications",
     flag_descriptions::kHideArcMediaNotificationsName,
     flag_descriptions::kHideArcMediaNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideArcMediaNotifications)},

    {"reduce-display-notifications",
     flag_descriptions::kReduceDisplayNotificationsName,
     flag_descriptions::kReduceDisplayNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kReduceDisplayNotifications)},

    {"use-search-click-for-right-click",
     flag_descriptions::kUseSearchClickForRightClickName,
     flag_descriptions::kUseSearchClickForRightClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseSearchClickForRightClick)},

    {"show-metered-toggle", flag_descriptions::kMeteredShowToggleName,
     flag_descriptions::kMeteredShowToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMeteredShowToggle)},

    {"wifi-sync-allow-deletes", flag_descriptions::kWifiSyncAllowDeletesName,
     flag_descriptions::kWifiSyncAllowDeletesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kWifiSyncAllowDeletes)},

    {"wifi-sync-android", flag_descriptions::kWifiSyncAndroidName,
     flag_descriptions::kWifiSyncAndroidDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kWifiSyncAndroid)},

    {"display-identification", flag_descriptions::kDisplayIdentificationName,
     flag_descriptions::kDisplayIdentificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayIdentification)},

    {"display-alignment-assistance",
     flag_descriptions::kDisplayAlignmentAssistanceName,
     flag_descriptions::kDisplayAlignmentAssistanceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayAlignAssist)},

    {"diagnostics-app", flag_descriptions::kDiagnosticsAppName,
     flag_descriptions::kDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDiagnosticsApp)},

    {"diagnostics-app-navigation",
     flag_descriptions::kDiagnosticsAppNavigationName,
     flag_descriptions::kDiagnosticsAppNavigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDiagnosticsAppNavigation)},

    {"enable-hostname-setting", flag_descriptions::kEnableHostnameSettingName,
     flag_descriptions::kEnableHostnameSettingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableHostnameSetting)},

    {"webui-dark-mode", flag_descriptions::kWebuiDarkModeName,
     flag_descriptions::kWebuiDarkModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebUIDarkMode)},

    {"select-to-speak-navigation-control",
     flag_descriptions::kSelectToSpeakNavigationControlName,
     flag_descriptions::kSelectToSpeakNavigationControlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSelectToSpeakNavigationControl)},

    {"enable-networking-in-diagnostics-app",
     flag_descriptions::kEnableNetworkingInDiagnosticsAppName,
     flag_descriptions::kEnableNetworkingInDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableNetworkingInDiagnosticsApp)},

    {"enable-oauth-ipp", flag_descriptions::kEnableOAuthIppName,
     flag_descriptions::kEnableOAuthIppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableOAuthIpp)},

    {"enable-shortcut-customization-app",
     flag_descriptions::kEnableShortcutCustomizationAppName,
     flag_descriptions::kEnableShortcutCustomizationAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kShortcutCustomizationApp)},

    {"enhanced-network-voices", flag_descriptions::kEnhancedNetworkVoicesName,
     flag_descriptions::kEnhancedNetworkVoicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnhancedNetworkVoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-extended-sync-promos-capability",
     flag_descriptions::kEnableExtendedSyncPromosCapabilityName,
     flag_descriptions::kEnableExtendedSyncPromosCapabilityDescription,
     flags_ui::kOsAndroid, FEATURE_VALUE_TYPE(switches::kMinorModeSupport)},

    {"enable-fenced-frames", flag_descriptions::kEnableFencedFramesName,
     flag_descriptions::kEnableFencedFramesDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kFencedFrames,
                                    kFencedFramesImplementationTypeVariations,
                                    "FencedFrames")},

    {"enable-portals", flag_descriptions::kEnablePortalsName,
     flag_descriptions::kEnablePortalsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPortals)},
    {"enable-portals-cross-origin",
     flag_descriptions::kEnablePortalsCrossOriginName,
     flag_descriptions::kEnablePortalsCrossOriginDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPortalsCrossOrigin)},
    {"enable-autofill-credit-card-authentication",
     flag_descriptions::kEnableAutofillCreditCardAuthenticationName,
     flag_descriptions::kEnableAutofillCreditCardAuthenticationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardAuthentication)},

    {"storage-access-api", flag_descriptions::kStorageAccessAPIName,
     flag_descriptions::kStorageAccessAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kStorageAccessAPI)},

    {"enable-removing-all-third-party-cookies",
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesName,
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableRemovingAllThirdPartyCookies)},

#if defined(OS_MAC)
    {"enterprise-reporting-api-keychain-recreation",
     flag_descriptions::kEnterpriseReportingApiKeychainRecreationName,
     flag_descriptions::kEnterpriseReportingApiKeychainRecreationDescription,
     kOsMac,
     FEATURE_VALUE_TYPE(features::kEnterpriseReportingApiKeychainRecreation)},
#endif  // defined(OS_MAC)

#if !defined(OS_ANDROID)
    {"enterprise-realtime-extension-request",
     flag_descriptions::kEnterpriseRealtimeExtensionRequestName,
     flag_descriptions::kEnterpriseRealtimeExtensionRequestDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kEnterpriseRealtimeExtensionRequest,
         kEnterpriseRealtimeExtensionRequestVariation,
         "EnterpriseRealtimeExtensionRequest")},
#endif  // !defined(OS_ANDROID)

    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription, kOsMac | kOsLinux | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},
    {"enable-unsafe-webgpu-service",
     flag_descriptions::kUnsafeWebGPUServiceName,
     flag_descriptions::kUnsafeWebGPUServiceDescription,
     kOsMac | kOsLinux | kOsWin, FEATURE_VALUE_TYPE(features::kWebGPUService)},

    {"enable-unsafe-fast-js-calls", flag_descriptions::kUnsafeFastJSCallsName,
     flag_descriptions::kUnsafeFastJSCallsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeFastJSCalls)},

#if defined(OS_ANDROID)
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
#endif  // defined(OS_ANDROID)

    {"autofill-prune-suggestions",
     flag_descriptions::kAutofillPruneSuggestionsName,
     flag_descriptions::kAutofillPruneSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPruneSuggestions)},

    {"allow-sync-xhr-in-page-dismissal",
     flag_descriptions::kAllowSyncXHRInPageDismissalName,
     flag_descriptions::kAllowSyncXHRInPageDismissalDescription,
     kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(blink::features::kAllowSyncXHRInPageDismissal)},

    {"enable-sync-requires-policies-loaded",
     flag_descriptions::kEnableSyncRequiresPoliciesLoadedName,
     flag_descriptions::kEnableSyncRequiresPoliciesLoadedDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncRequiresPoliciesLoaded)},

    {"enable-policy-blocklist-throttle-requires-policies-loaded",
     flag_descriptions::
         kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedName,
     flag_descriptions::
         kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         policy::features::kPolicyBlocklistThrottleRequiresPoliciesLoaded)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"auto-screen-brightness", flag_descriptions::kAutoScreenBrightnessName,
     flag_descriptions::kAutoScreenBrightnessDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAutoScreenBrightness)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"privacy-sandbox-settings", flag_descriptions::kPrivacySandboxSettingsName,
     flag_descriptions::kPrivacySandboxSettingsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacySandboxSettings)},

    {"privacy-sandbox-settings-2",
     flag_descriptions::kPrivacySandboxSettings2Name,
     flag_descriptions::kPrivacySandboxSettings2Description,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacySandboxSettings2)},

#if defined(OS_ANDROID)
    {"metrics-settings-android", flag_descriptions::kMetricsSettingsAndroidName,
     flag_descriptions::kMetricsSettingsAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kMetricsSettingsAndroid,
                                    kMetricsSettingsAndroidVariations,
                                    "MetricsSettingsAndroid")},
#endif

    {"search-history-link", flag_descriptions::kSearchHistoryLinkName,
     flag_descriptions::kSearchHistoryLinkDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSearchHistoryLink)},

#if defined(OS_ANDROID)
    {"safe-browsing-client-side-detection-android",
     flag_descriptions::kSafeBrowsingClientSideDetectionAndroidName,
     flag_descriptions::kSafeBrowsingClientSideDetectionAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(safe_browsing::kClientSideDetectionForAndroid)},

    {"safe-browsing-enhanced-protection-promo-android",
     flag_descriptions::kEnhancedProtectionPromoAndroidName,
     flag_descriptions::kEnhancedProtectionPromoAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEnhancedProtectionPromoCard)},

    {"safe-browsing-passwordcheck-integration-for-saved-passwords-android",
     flag_descriptions::
         kSafeBrowsingPasswordCheckIntegrationForSavedPasswordsAndroidName,
     flag_descriptions::
         kSafeBrowsingPasswordCheckIntegrationForSavedPasswordsAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         safe_browsing::
             kSafeBrowsingPasswordCheckIntegrationForSavedPasswordsAndroid)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"gesture-properties-dbus-service",
     flag_descriptions::kEnableGesturePropertiesDBusServiceName,
     flag_descriptions::kEnableGesturePropertiesDBusServiceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGesturePropertiesDBusService)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"ev-details-in-page-info", flag_descriptions::kEvDetailsInPageInfoName,
     flag_descriptions::kEvDetailsInPageInfoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEvDetailsInPageInfo)},

    {"enable-autofill-credit-card-upload-feedback",
     flag_descriptions::kEnableAutofillCreditCardUploadFeedbackName,
     flag_descriptions::kEnableAutofillCreditCardUploadFeedbackDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardUploadFeedback)},

    {"font-access", flag_descriptions::kFontAccessAPIName,
     flag_descriptions::kFontAccessAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFontAccess)},

    {"font-access-persistent", flag_descriptions::kFontAccessPersistentName,
     flag_descriptions::kFontAccessPersistentDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFontAccessPersistent)},

    {"mouse-subframe-no-implicit-capture",
     flag_descriptions::kMouseSubframeNoImplicitCaptureName,
     flag_descriptions::kMouseSubframeNoImplicitCaptureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kMouseSubframeNoImplicitCapture)},

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"global-media-controls", flag_descriptions::kGlobalMediaControlsName,
     flag_descriptions::kGlobalMediaControlsDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControls)},

    {"global-media-controls-for-cast",
     flag_descriptions::kGlobalMediaControlsForCastName,
     flag_descriptions::kGlobalMediaControlsForCastDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsForCast)},

    {"global-media-controls-for-chromeos",
     flag_descriptions::kGlobalMediaControlsForChromeOSName,
     flag_descriptions::kGlobalMediaControlsForChromeOSDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsForChromeOS)},

    {"global-media-controls-modern-ui",
     flag_descriptions::kGlobalMediaControlsModernUIName,
     flag_descriptions::kGlobalMediaControlsModernUIDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsModernUI)},

    {"global-media-controls-picture-in-picture",
     flag_descriptions::kGlobalMediaControlsPictureInPictureName,
     flag_descriptions::kGlobalMediaControlsPictureInPictureDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsPictureInPicture)},

    {"global-media-controls-seamless-transfer",
     flag_descriptions::kGlobalMediaControlsSeamlessTransferName,
     flag_descriptions::kGlobalMediaControlsSeamlessTransferDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsSeamlessTransfer)},

    {"global-media-controls-overlay-controls",
     flag_descriptions::kGlobalMediaControlsOverlayControlsName,
     flag_descriptions::kGlobalMediaControlsOverlayControlsDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsOverlayControls)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

    {"safety-tips", flag_descriptions::kSafetyTipName,
     flag_descriptions::kSafetyTipDescription, kOsAll,
     FEATURE_VALUE_TYPE(security_state::features::kSafetyTipUI)},

    {"turn-off-streaming-media-caching-on-battery",
     flag_descriptions::kTurnOffStreamingMediaCachingOnBatteryName,
     flag_descriptions::kTurnOffStreamingMediaCachingOnBatteryDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTurnOffStreamingMediaCachingOnBattery)},

    {"turn-off-streaming-media-caching-always",
     flag_descriptions::kTurnOffStreamingMediaCachingAlwaysName,
     flag_descriptions::kTurnOffStreamingMediaCachingAlwaysDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTurnOffStreamingMediaCachingAlways)},

    {"enable-cooperative-scheduling",
     flag_descriptions::kCooperativeSchedulingName,
     flag_descriptions::kCooperativeSchedulingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCooperativeScheduling)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-routines",
     flag_descriptions::kEnableAssistantRoutinesName,
     flag_descriptions::kEnableAssistantRoutinesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantRoutines)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"notification-scheduler", flag_descriptions::kNotificationSchedulerName,
     flag_descriptions::kNotificationSchedulerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kNotificationScheduleService)},

    {"notification-scheduler-debug-options",
     flag_descriptions::kNotificationSchedulerDebugOptionName,
     flag_descriptions::kNotificationSchedulerDebugOptionDescription,
     kOsAndroid, MULTI_VALUE_TYPE(kNotificationSchedulerChoices)},

#if defined(OS_ANDROID)

    {"use-notification-compat-builder",
     flag_descriptions::kUseNotificationCompatBuilderName,
     flag_descriptions::kUseNotificationCompatBuilderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUseNotificationCompatBuilder)},

    {"debug-chime-notification",
     flag_descriptions::kChimeAlwaysShowNotificationName,
     flag_descriptions::kChimeAlwaysShowNotificationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(notifications::switches::kDebugChimeNotification)},

    {"use-chime-android-sdk", flag_descriptions::kChimeAndroidSdkName,
     flag_descriptions::kChimeAndroidSdkDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kUseChimeAndroidSdk)},

#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"lock-screen-media-controls",
     flag_descriptions::kLockScreenMediaControlsName,
     flag_descriptions::kLockScreenMediaControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenMediaControls)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"contextual-nudges", flag_descriptions::kContextualNudgesName,
     flag_descriptions::kContextualNudgesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kContextualNudges)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"settings-app-notification-settings",
     flag_descriptions::kSettingsAppNotificationSettingsName,
     flag_descriptions::kSettingsAppNotificationSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSettingsAppNotificationSettings)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"decode-jpeg-images-to-yuv",
     flag_descriptions::kDecodeJpeg420ImagesToYUVName,
     flag_descriptions::kDecodeJpeg420ImagesToYUVDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDecodeJpeg420ImagesToYUV)},

    {"decode-webp-images-to-yuv",
     flag_descriptions::kDecodeLossyWebPImagesToYUVName,
     flag_descriptions::kDecodeLossyWebPImagesToYUVDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDecodeLossyWebPImagesToYUV)},

    {"dns-httpssvc", flag_descriptions::kDnsHttpssvcName,
     flag_descriptions::kDnsHttpssvcDescription,
     kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kDnsHttpssvc)},

    {"dns-over-https", flag_descriptions::kDnsOverHttpsName,
     flag_descriptions::kDnsOverHttpsDescription,
     kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDnsOverHttps)},

    {"web-bundles", flag_descriptions::kWebBundlesName,
     flag_descriptions::kWebBundlesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebBundles)},

#if defined(OS_ANDROID)
    {"web-otp-backend", flag_descriptions::kWebOtpBackendName,
     flag_descriptions::kWebOtpBackendDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kWebOtpBackendChoices)},

    {"darken-websites-checkbox-in-themes-setting",
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingName,
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kDarkenWebsitesCheckboxInThemesSetting)},
#endif  // defined(OS_ANDROID)

    {"enable-autofill-upi-vpa", flag_descriptions::kAutofillSaveAndFillVPAName,
     flag_descriptions::kAutofillSaveAndFillVPADescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveAndFillVPA)},

    {"quiet-notification-prompts",
     flag_descriptions::kQuietNotificationPromptsName,
     flag_descriptions::kQuietNotificationPromptsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kQuietNotificationPrompts,
                                    kQuietNotificationPromptsVariations,
                                    "QuietNotificationPrompts")},
    {"abusive-notification-permission-revocation",
     flag_descriptions::kAbusiveNotificationPermissionRevocationName,
     flag_descriptions::kAbusiveNotificationPermissionRevocationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kAbusiveNotificationPermissionRevocation)},

#if defined(OS_ANDROID)
    {"context-menu-google-lens-chip",
     flag_descriptions::kContextMenuGoogleLensChipName,
     flag_descriptions::kContextMenuGoogleLensChipDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuGoogleLensChip)},

    {"context-menu-search-with-google-lens",
     flag_descriptions::kContextMenuSearchWithGoogleLensName,
     flag_descriptions::kContextMenuSearchWithGoogleLensDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuSearchWithGoogleLens)},

    {"context-menu-shop-with-google-lens",
     flag_descriptions::kContextMenuShopWithGoogleLensName,
     flag_descriptions::kContextMenuShopWithGoogleLensDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuShopWithGoogleLens)},

    {"context-menu-search-and-shop-with-google-lens",
     flag_descriptions::kContextMenuSearchAndShopWithGoogleLensName,
     flag_descriptions::kContextMenuSearchAndShopWithGoogleLensDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kContextMenuSearchAndShopWithGoogleLens)},

    {"context-menu-translate-with-google-lens",
     flag_descriptions::kContextMenuTranslateWithGoogleLensName,
     flag_descriptions::kContextMenuTranslateWithGoogleLensDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextMenuTranslateWithGoogleLens,
         kLensContextMenuTranslateVariations,
         "LensContextMenuTranslate")},

    {"google-lens-sdk-intent", flag_descriptions::kGoogleLensSdkIntentName,
     flag_descriptions::kGoogleLensSdkIntentDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kGoogleLensSdkIntent)},

    {"lens-camera-assisted-search",
     flag_descriptions::kLensCameraAssistedSearchName,
     flag_descriptions::kLensCameraAssistedSearchDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kLensCameraAssistedSearch,
                                    kLensCameraAssistedSearchVariations,
                                    "OmniboxAssistantVoiceSearch")},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-suggested-files", flag_descriptions::kEnableSuggestedFilesName,
     flag_descriptions::kEnableSuggestedFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableSuggestedFiles)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-always-return-cloud-tokenized-card",
     flag_descriptions::kAutofillAlwaysReturnCloudTokenizedCardName,
     flag_descriptions::kAutofillAlwaysReturnCloudTokenizedCardDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAlwaysReturnCloudTokenizedCard)},

    {"back-forward-cache", flag_descriptions::kBackForwardCacheName,
     flag_descriptions::kBackForwardCacheDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kBackForwardCache,
                                    kBackForwardCacheVariations,
                                    "BackForwardCache")},

    {"impulse-scroll-animations",
     flag_descriptions::kImpulseScrollAnimationsName,
     flag_descriptions::kImpulseScrollAnimationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kImpulseScrollAnimations)},

    {"percent-based-scrolling", flag_descriptions::kPercentBasedScrollingName,
     flag_descriptions::kPercentBasedScrollingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPercentBasedScrolling)},

    {"scroll-unification", flag_descriptions::kScrollUnificationName,
     flag_descriptions::kScrollUnificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kScrollUnification)},

#if defined(OS_WIN)
    {"elastic-overscroll", flag_descriptions::kElasticOverscrollName,
     flag_descriptions::kElasticOverscrollDescription, kOsWin | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kElasticOverscroll)},
#elif defined(OS_ANDROID)
    {"elastic-overscroll", flag_descriptions::kElasticOverscrollName,
     flag_descriptions::kElasticOverscrollDescription, kOsWin | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kElasticOverscroll,
                                    kElasticOverscrollVariations,
                                    "ElasticOverscroll")},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-aec", flag_descriptions::kEnableGoogleAssistantAecName,
     flag_descriptions::kEnableGoogleAssistantAecDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantAudioEraser)},
#endif

#if defined(OS_WIN)
    {"enable-winrt-geolocation-implementation",
     flag_descriptions::kWinrtGeolocationImplementationName,
     flag_descriptions::kWinrtGeolocationImplementationDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWinrtGeolocationImplementation)},
#endif

#if defined(OS_MAC)
    {"enable-core-location-backend",
     flag_descriptions::kMacCoreLocationBackendName,
     flag_descriptions::kMacCoreLocationBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacCoreLocationBackend)},
#endif

#if defined(OS_MAC)
    {"enable-core-location-implementation",
     flag_descriptions::kMacCoreLocationImplementationName,
     flag_descriptions::kMacCoreLocationImplementationDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacCoreLocationImplementation)},
#endif

#if !defined(OS_ANDROID)
    {"mute-notification-snooze-action",
     flag_descriptions::kMuteNotificationSnoozeActionName,
     flag_descriptions::kMuteNotificationSnoozeActionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMuteNotificationSnoozeAction)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_MAC)
    {"enable-new-mac-notification-api",
     flag_descriptions::kNewMacNotificationAPIName,
     flag_descriptions::kNewMacNotificationAPIDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kNewMacNotificationAPI)},
    {"notifications-via-helper-app",
     flag_descriptions::kNotificationsViaHelperAppName,
     flag_descriptions::kNotificationsViaHelperAppDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kNotificationsViaHelperApp)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"exo-gamepad-vibration", flag_descriptions::kExoGamepadVibrationName,
     flag_descriptions::kExoGamepadVibrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGamepadVibration)},
    {"exo-lock-notification", flag_descriptions::kExoLockNotificationName,
     flag_descriptions::kExoLockNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoLockNotification)},
    {"exo-ordinal-motion", flag_descriptions::kExoOrdinalMotionName,
     flag_descriptions::kExoOrdinalMotionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoOrdinalMotion)},
    {"exo-pointer-lock", flag_descriptions::kExoPointerLockName,
     flag_descriptions::kExoPointerLockDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoPointerLock)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
    {"metal", flag_descriptions::kMetalName,
     flag_descriptions::kMetalDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMetal)},
    {"screentime", flag_descriptions::kScreenTimeName,
     flag_descriptions::kScreenTimeDescription, kOsMac,
     FEATURE_VALUE_TYPE(screentime::kScreenTime)},
#endif

    {"enable-de-jelly", flag_descriptions::kEnableDeJellyName,
     flag_descriptions::kEnableDeJellyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableDeJelly)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-cros-action-recorder",
     flag_descriptions::kEnableCrOSActionRecorderName,
     flag_descriptions::kEnableCrOSActionRecorderDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kEnableCrOSActionRecorderChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-heavy-ad-intervention",
     flag_descriptions::kHeavyAdInterventionName,
     flag_descriptions::kHeavyAdInterventionDescription, kOsAll,
     FEATURE_VALUE_TYPE(heavy_ad_intervention::features::kHeavyAdIntervention)},

    {"heavy-ad-privacy-mitigations",
     flag_descriptions::kHeavyAdPrivacyMitigationsName,
     flag_descriptions::kHeavyAdPrivacyMitigationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         heavy_ad_intervention::features::kHeavyAdPrivacyMitigations)},

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    {"enable-ftp", flag_descriptions::kEnableFtpName,
     flag_descriptions::kEnableFtpDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kFtpProtocol)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crosh-swa", flag_descriptions::kCroshSWAName,
     flag_descriptions::kCroshSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCroshSWA)},
    {"crostini-use-buster-image",
     flag_descriptions::kCrostiniUseBusterImageName,
     flag_descriptions::kCrostiniUseBusterImageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseBusterImage)},
    {"crostini-disk-resizing", flag_descriptions::kCrostiniDiskResizingName,
     flag_descriptions::kCrostiniDiskResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniDiskResizing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"homepage-promo-card", flag_descriptions::kHomepagePromoCardName,
     flag_descriptions::kHomepagePromoCardDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kHomepagePromoCard,
                                    kHomepagePromoCardVariations,
                                    "HomepagePromoAndroid")},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"split-settings-sync", flag_descriptions::kSplitSettingsSyncName,
     flag_descriptions::kSplitSettingsSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSplitSettingsSync)},
    {"os-settings-app-notifications-page",
     flag_descriptions::kOsSettingsAppNotificationsPageName,
     flag_descriptions::kOsSettingsAppNotificationsPageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOsSettingsAppNotificationsPage)},
    {"os-settings-deep-linking", flag_descriptions::kOsSettingsDeepLinkingName,
     flag_descriptions::kOsSettingsDeepLinkingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOsSettingsDeepLinking)},
    {"help-app-background-page", flag_descriptions::kHelpAppBackgroundPageName,
     flag_descriptions::kHelpAppBackgroundPageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHelpAppBackgroundPage)},
    {"help-app-discover-tab", flag_descriptions::kHelpAppDiscoverTabName,
     flag_descriptions::kHelpAppDiscoverTabDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHelpAppDiscoverTab)},
    {"help-app-launcher-search", flag_descriptions::kHelpAppLauncherSearchName,
     flag_descriptions::kHelpAppLauncherSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHelpAppLauncherSearch)},
    {"help-app-search-service-integration",
     flag_descriptions::kHelpAppSearchServiceIntegrationName,
     flag_descriptions::kHelpAppSearchServiceIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHelpAppSearchServiceIntegration)},
    {"media-app-annotation", flag_descriptions::kMediaAppAnnotationName,
     flag_descriptions::kMediaAppAnnotationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppAnnotation)},
    {"media-app-display-exif", flag_descriptions::kMediaAppDisplayExifName,
     flag_descriptions::kMediaAppDisplayExifDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppAnnotation)},
    {"media-app-handles-pdf", flag_descriptions::kMediaAppHandlesPdfName,
     flag_descriptions::kMediaAppHandlesPdfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppHandlesPdf)},
    {"release-notes-notification",
     flag_descriptions::kReleaseNotesNotificationName,
     flag_descriptions::kReleaseNotesNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kReleaseNotesNotification)},
    {"release-notes-notification-all-channels",
     flag_descriptions::kReleaseNotesNotificationAllChannelsName,
     flag_descriptions::kReleaseNotesNotificationAllChannelsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kReleaseNotesNotificationAllChannels)},
    {"use-stork-smds-server-address",
     flag_descriptions::kUseStorkSmdsServerAddressName,
     flag_descriptions::kUseStorkSmdsServerAddressDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseStorkSmdsServerAddress)},
    {"use-wallpaper-staging-url",
     flag_descriptions::kUseWallpaperStagingUrlName,
     flag_descriptions::kUseWallpaperStagingUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseWallpaperStagingUrl)},
    {"semantic-colors-debug-override",
     flag_descriptions::kSemanticColorsDebugOverrideName,
     flag_descriptions::kSemanticColorsDebugOverrideDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSemanticColorsDebugOverride)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-virtual-card",
     flag_descriptions::kAutofillEnableVirtualCardName,
     flag_descriptions::kAutofillEnableVirtualCardDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVirtualCard)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"account-id-migration", flag_descriptions::kAccountIdMigrationName,
     flag_descriptions::kAccountIdMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(switches::kAccountIdMigration)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // TODO(https://crbug.com/1032161): Implement and enable for ChromeOS.
    {"raw-clipboard", flag_descriptions::kRawClipboardName,
     flag_descriptions::kRawClipboardDescription, kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(blink::features::kRawClipboard)},

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
    {"paint-preview-demo", flag_descriptions::kPaintPreviewDemoName,
     flag_descriptions::kPaintPreviewDemoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(paint_preview::kPaintPreviewDemo)},
    {"paint-preview-startup", flag_descriptions::kPaintPreviewStartupName,
     flag_descriptions::kPaintPreviewStartupDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(paint_preview::kPaintPreviewShowOnStartup,
                                    kPaintPreviewStartupVariations,
                                    "PaintPreviewShowOnStartup")},
#endif  // ENABLE_PAINT_PREVIEW && defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"block-external-form-redirects-no-gesture",
     flag_descriptions::kIntentBlockExternalFormRedirectsNoGestureName,
     flag_descriptions::kIntentBlockExternalFormRedirectsNoGestureDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         external_intents::kIntentBlockExternalFormRedirectsNoGesture)},
    {"recover-from-never-save-android",
     flag_descriptions::kRecoverFromNeverSaveAndroidName,
     flag_descriptions::kRecoverFromNeverSaveAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kRecoverFromNeverSaveAndroid)},
    {"photo-picker-video-support",
     flag_descriptions::kPhotoPickerVideoSupportName,
     flag_descriptions::kPhotoPickerVideoSupportDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         photo_picker::features::kPhotoPickerVideoSupport,
         kPhotoPickerVideoSupportFeatureVariations,
         "PhotoPickerVideoSupportFeatureVariations")},
#endif  // defined(OS_ANDROID)

    {"reduce-user-agent", flag_descriptions::kReduceUserAgentName,
     flag_descriptions::kReduceUserAgentDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kReduceUserAgent)},

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-user-data-snapshot", flag_descriptions::kUserDataSnapshotName,
     flag_descriptions::kUserDataSnapshotDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kUserDataSnapshot)},
#endif

#if defined(OS_WIN)
    {"run-video-capture-service-in-browser",
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessName,
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(features::kRunVideoCaptureServiceInBrowserProcess)},
#endif  // defined(OS_WIN)

    {"legacy-tls-enforced", flag_descriptions::kLegacyTLSEnforcedName,
     flag_descriptions::kLegacyTLSEnforcedDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kLegacyTLSEnforced)},

    {"double-buffer-compositing",
     flag_descriptions::kDoubleBufferCompositingName,
     flag_descriptions::kDoubleBufferCompositingDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDoubleBufferCompositing)},

#if defined(OS_ANDROID)
    {"password-change-in-settings",
     flag_descriptions::kPasswordChangeInSettingsName,
     flag_descriptions::kPasswordChangeInSettingsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kPasswordChangeInSettings,
         kPasswordChangeInSettingsFeatureVariations,
         "PasswordChangeAndroid")},
    {"password-scripts-fetching",
     flag_descriptions::kPasswordScriptsFetchingName,
     flag_descriptions::kPasswordScriptsFetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordScriptsFetching)},
    {"password-change-support", flag_descriptions::kPasswordChangeName,
     flag_descriptions::kPasswordChangeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(password_manager::features::kPasswordChange,
                                    kPasswordChangeFeatureVariations,
                                    "PasswordChangeFeatureVariations")},

    {"context-menu-performance-info-and-remote-hints-fetching",
     flag_descriptions::kContextMenuPerformanceInfoAndRemoteHintFetchingName,
     flag_descriptions::
         kContextMenuPerformanceInfoAndRemoteHintFetchingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(optimization_guide::features::
                            kContextMenuPerformanceInfoAndRemoteHintFetching)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"page-info-discoverability",
     flag_descriptions::kPageInfoDiscoverabilityName,
     flag_descriptions::kPageInfoDiscoverabilityDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoDiscoverability)},
    {"page-info-history", flag_descriptions::kPageInfoHistoryName,
     flag_descriptions::kPageInfoHistoryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistory)},
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"page-info-version-2-desktop", flag_descriptions::kPageInfoV2DesktopName,
     flag_descriptions::kPageInfoV2DesktopDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kPageInfoV2Desktop)},
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enhanced_clipboard", flag_descriptions::kEnhancedClipboardName,
     flag_descriptions::kEnhancedClipboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kClipboardHistory)},
    {"enhanced_clipboard_nudge_session_reset",
     flag_descriptions::kEnhancedClipboardNudgeSessionResetName,
     flag_descriptions::kEnhancedClipboardNudgeSessionResetDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kClipboardHistoryNudgeSessionReset)},
    {"enhanced_clipboard_screenshot_nudge",
     flag_descriptions::kEnhancedClipboardScreenshotNudgeName,
     flag_descriptions::kEnhancedClipboardScreenshotNudgeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kClipboardHistoryScreenshotNudge)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
    {"enable-media-foundation-video-capture",
     flag_descriptions::kEnableMediaFoundationVideoCaptureName,
     flag_descriptions::kEnableMediaFoundationVideoCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kMediaFoundationVideoCapture)},
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"scan-app-media-link", flag_descriptions::kScanAppMediaLinkName,
     flag_descriptions::kScanAppMediaLinkDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanAppMediaLink)},
    {"scan-app-multi-page-scan", flag_descriptions::kScanAppMultiPageScanName,
     flag_descriptions::kScanAppMultiPageScanDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanAppMultiPageScan)},
    {"scan-app-searchable-pdf", flag_descriptions::kScanAppSearchablePdfName,
     flag_descriptions::kScanAppSearchablePdfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanAppSearchablePdf)},
    {"scan-app-sticky-settings", flag_descriptions::kScanAppStickySettingsName,
     flag_descriptions::kScanAppStickySettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanAppStickySettings)},
    {"avatar-toolbar-button", flag_descriptions::kAvatarToolbarButtonName,
     flag_descriptions::kAvatarToolbarButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAvatarToolbarButton)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"color-provider-redirection",
     flag_descriptions::kColorProviderRedirectionName,
     flag_descriptions::kColorProviderRedirectionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kColorProviderRedirection)},

    {"trust-tokens", flag_descriptions::kTrustTokensName,
     flag_descriptions::kTrustTokensDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(network::features::kTrustTokens,
                                    kPlatformProvidedTrustTokensVariations,
                                    "TrustTokenOriginTrial")},

#if defined(OS_ANDROID)
    {"android-partner-customization-phenotype",
     flag_descriptions::kAndroidPartnerCustomizationPhenotypeName,
     flag_descriptions::kAndroidPartnerCustomizationPhenotypeDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kAndroidPartnerCustomizationPhenotype)},
#endif  // defined(OS_ANDROID)

    {"media-history", flag_descriptions::kMediaHistoryName,
     flag_descriptions::kMediaHistoryDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kUseMediaHistoryStore)},

#if !defined(OS_ANDROID)
    {"copy-link-to-text", flag_descriptions::kCopyLinkToTextName,
     flag_descriptions::kCopyLinkToTextDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCopyLinkToText)},
#endif  // !defined(OS_ANDROID)

    {"shared-highlighting-use-blocklist",
     flag_descriptions::kSharedHighlightingUseBlocklistName,
     flag_descriptions::kSharedHighlightingUseBlocklistDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingUseBlocklist)},
    {"shared-highlighting-v2", flag_descriptions::kSharedHighlightingV2Name,
     flag_descriptions::kSharedHighlightingV2Description, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingV2)},
    {"shared-highlighting-amp", flag_descriptions::kSharedHighlightingAmpName,
     flag_descriptions::kSharedHighlightingAmpDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingAmp)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"shimless-rma-flow", flag_descriptions::kShimlessRMAFlowName,
     flag_descriptions::kShimlessRMAFlowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShimlessRMAFlow)},
    {"nearby-keep-alive-fix", flag_descriptions::kNearbyKeepAliveFixName,
     flag_descriptions::kNearbyKeepAliveFixDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNearbyKeepAliveFix)},
    {"nearby-sharing", flag_descriptions::kNearbySharingName,
     flag_descriptions::kNearbySharingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharing)},
    {"nearby-sharing-background-scanning",
     flag_descriptions::kNearbySharingBackgroundScanningName,
     flag_descriptions::kNearbySharingBackgroundScanningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingBackgroundScanning)},
    {"nearby-sharing-device-contacts",
     flag_descriptions::kNearbySharingDeviceContactsName,
     flag_descriptions::kNearbySharingDeviceContactsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingDeviceContacts)},
    {"nearby-sharing-webrtc", flag_descriptions::kNearbySharingWebRtcName,
     flag_descriptions::kNearbySharingWebRtcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingWebRtc)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"app-cache", flag_descriptions::kAppCacheName,
     flag_descriptions::kAppCacheDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAppCache)},

    {"align-font-display-auto-lcp",
     flag_descriptions::kAlignFontDisplayAutoTimeoutWithLCPGoalName,
     flag_descriptions::kAlignFontDisplayAutoTimeoutWithLCPGoalDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kAlignFontDisplayAutoTimeoutWithLCPGoal,
         kAlignFontDisplayAutoTimeoutWithLCPGoalVariations,
         "AlignFontDisplayAutoTimeoutWithLCPGoal")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-palm-suppression", flag_descriptions::kEnablePalmSuppressionName,
     flag_descriptions::kEnablePalmSuppressionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmSuppression)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-experimental-cookie-features",
     flag_descriptions::kEnableExperimentalCookieFeaturesName,
     flag_descriptions::kEnableExperimentalCookieFeaturesDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableExperimentalCookieFeaturesChoices)},

    {"autofill-enable-google-issued-card",
     flag_descriptions::kAutofillEnableGoogleIssuedCardName,
     flag_descriptions::kAutofillEnableGoogleIssuedCardDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableGoogleIssuedCard)},

    {"permission-chip", flag_descriptions::kPermissionChipName,
     flag_descriptions::kPermissionChipDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(permissions::features::kPermissionChip)},
    {"permission-chip-gesture",
     flag_descriptions::kPermissionChipGestureSensitiveName,
     flag_descriptions::kPermissionChipGestureSensitiveDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         permissions::features::kPermissionChipGestureSensitive)},
    {"permission-chip-request-type",
     flag_descriptions::kPermissionChipRequestTypeSensitiveName,
     flag_descriptions::kPermissionChipRequestTypeSensitiveDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         permissions::features::kPermissionChipRequestTypeSensitive)},
    {"permission-quiet-chip", flag_descriptions::kPermissionQuietChipName,
     flag_descriptions::kPermissionQuietChipDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(permissions::features::kPermissionQuietChip)},

    {"canvas-2d-layers", flag_descriptions::kCanvas2DLayersName,
     flag_descriptions::kCanvas2DLayersDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableCanvas2DLayers)},

    {"new-canvas-2d-api", flag_descriptions::kNewCanvas2DAPIName,
     flag_descriptions::kNewCanvas2DAPIDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNewCanvas2DAPI)},

    {"enable-translate-sub-frames",
     flag_descriptions::kEnableTranslateSubFramesName,
     flag_descriptions::kEnableTranslateSubFramesDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTranslateSubFrames)},

    {"service-worker-subresource-filter",
     flag_descriptions::kServiceWorkerSubresourceFilterName,
     flag_descriptions::kServiceWorkerSubresourceFilterDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kServiceWorkerSubresourceFilter)},

    {"conversion-measurement-api",
     flag_descriptions::kConversionMeasurementApiName,
     flag_descriptions::kConversionMeasurementApiDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kConversionMeasurement)},
    {"conversion-measurement-debug-mode",
     flag_descriptions::kConversionMeasurementDebugModeName,
     flag_descriptions::kConversionMeasurementDebugModeDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kConversionsDebugMode)},

    {"client-storage-access-context-auditing",
     flag_descriptions::kClientStorageAccessContextAuditingName,
     flag_descriptions::kClientStorageAccessContextAuditingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClientStorageAccessContextAuditing)},

#if defined(OS_WIN)
    {"safety-check-chrome-cleaner-child",
     flag_descriptions::kSafetyCheckChromeCleanerChildName,
     flag_descriptions::kSafetyCheckChromeCleanerChildDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kSafetyCheckChromeCleanerChild)},
#endif  // !defined(OS_WIN)

#if defined(OS_WIN)
    {"chrome-cleanup-scan-completed-notification",
     flag_descriptions::kChromeCleanupScanCompletedNotificationName,
     flag_descriptions::kChromeCleanupScanCompletedNotificationDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(features::kChromeCleanupScanCompletedNotification)},
#endif  // !defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"productivity-launcher", flag_descriptions::kAppListBubbleName,
     flag_descriptions::kAppListBubbleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAppListBubble)},
    {"enable-launcher-app-paging",
     flag_descriptions::kNewDragSpecInLauncherName,
     flag_descriptions::kNewDragSpecInLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kNewDragSpecInLauncher)},
    {"cdm-factory-daemon", flag_descriptions::kCdmFactoryDaemonName,
     flag_descriptions::kCdmFactoryDaemonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCdmFactoryDaemon)},
    {"shelf-drag-to-pin", flag_descriptions::kShelfDragToPinName,
     flag_descriptions::kShelfDragToPinDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragUnpinnedAppToPin)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"categorical-search", flag_descriptions::kCategoricalSearchName,
     flag_descriptions::kCategoricalSearchName, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(app_list_features::kCategoricalSearch,
                                    kCategoricalSearchVariations,
                                    "LauncherCategoricalSearch")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-offers-in-downstream",
     flag_descriptions::kAutofillEnableOffersInDownstreamName,
     flag_descriptions::kAutofillEnableOffersInDownstreamDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOffersInDownstream)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-sharesheet", flag_descriptions::kSharesheetName,
     flag_descriptions::kSharesheetDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSharesheet)},

    {"enable-sharesheet-content-previews",
     flag_descriptions::kSharesheetContentPreviewsName,
     flag_descriptions::kSharesheetContentPreviewsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSharesheetContentPreviews)},

    {"chromeos-sharing-hub", flag_descriptions::kChromeOSSharingHubName,
     flag_descriptions::kChromeOSSharingHubDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kChromeOSSharingHub)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"schemeful-same-site", flag_descriptions::kSchemefulSameSiteName,
     flag_descriptions::kSchemefulSameSiteDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kSchemefulSameSite)},

    {"enable-bluetooth-spp-in-serial-api",
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiName,
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiDescription,
     kOsMac,
     SINGLE_VALUE_TYPE(switches::kEnableBluetoothSerialPortProfileInSerialApi)},

    {"enable-lite-video", flag_descriptions::kLiteVideoName,
     flag_descriptions::kLiteVideoDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLiteVideo)},

    {"lite-video-default-downlink-bandwidth-kbps",
     flag_descriptions::kLiteVideoDownlinkBandwidthKbpsName,
     flag_descriptions::kLiteVideoDownlinkBandwidthKbpsDescription, kOsAll,
     MULTI_VALUE_TYPE(kLiteVideoDefaultDownlinkBandwidthKbps)},

    {"lite-video-force-override-decision",
     flag_descriptions::kLiteVideoForceOverrideDecisionName,
     flag_descriptions::kLiteVideoForceOverrideDecisionDescription, kOsAll,
     SINGLE_VALUE_TYPE(lite_video::switches::kLiteVideoForceOverrideDecision)},

    {"edit-passwords-in-settings",
     flag_descriptions::kEditPasswordsInSettingsName,
     flag_descriptions::kEditPasswordsInSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kEditPasswordsInSettings)},

    {"mixed-forms-disable-autofill",
     flag_descriptions::kMixedFormsDisableAutofillName,
     flag_descriptions::kMixedFormsDisableAutofillDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPreventMixedFormsFilling)},

    {"mixed-forms-interstitial", flag_descriptions::kMixedFormsInterstitialName,
     flag_descriptions::kMixedFormsInterstitialDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         security_interstitials::kInsecureFormSubmissionInterstitial)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"frame-throttle-fps", flag_descriptions::kFrameThrottleFpsName,
     flag_descriptions::kFrameThrottleFpsDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kFrameThrottleFpsChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"filling-passwords-from-any-origin",
     flag_descriptions::kFillingPasswordsFromAnyOriginName,
     flag_descriptions::kFillingPasswordsFromAnyOriginDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingPasswordsFromAnyOrigin)},
#endif  // OS_ANDROID

#if defined(OS_WIN)
    {"enable-incognito-shortcut-on-desktop",
     flag_descriptions::kEnableIncognitoShortcutOnDesktopName,
     flag_descriptions::kEnableIncognitoShortcutOnDesktopDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableIncognitoShortcutOnDesktop)},
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
    {"incognito-brand-consistency-for-android",
     flag_descriptions::kIncognitoBrandConsistencyForAndroidName,
     flag_descriptions::kIncognitoBrandConsistencyForAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kIncognitoBrandConsistencyForAndroid)},
#endif

#if defined(OS_MAC) || defined(OS_WIN) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"incognito-brand-consistency-for-desktop",
     flag_descriptions::kIncognitoBrandConsistencyForDesktopName,
     flag_descriptions::kIncognitoBrandConsistencyForDesktopDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIncognitoBrandConsistencyForDesktop)},

    {"incognito-clear-browsing-data-dialog-for-desktop",
     flag_descriptions::kIncognitoClearBrowsingDataDialogForDesktopName,
     flag_descriptions::kIncognitoClearBrowsingDataDialogForDesktopDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIncognitoClearBrowsingDataDialogForDesktop)},

    {"inherit-native-theme-from-parent-widget",
     flag_descriptions::kInheritNativeThemeFromParentWidgetName,
     flag_descriptions::kInheritNativeThemeFromParentWidgetDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(views::features::kInheritNativeThemeFromParentWidget)},
#endif

    {"content-settings-redesign",
     flag_descriptions::kContentSettingsRedesignName,
     flag_descriptions::kContentSettingsRedesignDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kContentSettingsRedesign)},

#if defined(OS_ANDROID)
    {"cpu-affinity-restrict-to-little-cores",
     flag_descriptions::kCpuAffinityRestrictToLittleCoresName,
     flag_descriptions::kCpuAffinityRestrictToLittleCoresDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         power_scheduler::features::kCpuAffinityRestrictToLittleCores)},

    {"enable-surface-control", flag_descriptions::kAndroidSurfaceControlName,
     flag_descriptions::kAndroidSurfaceControlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidSurfaceControl)},

    {"enable-image-reader", flag_descriptions::kAImageReaderName,
     flag_descriptions::kAImageReaderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAImageReader)},
#endif  // OS_ANDROID

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-auto-select", flag_descriptions::kEnableAutoSelectName,
     flag_descriptions::kEnableAutoSelectDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(blink::features::kCrOSAutoSelect)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"smart-suggestion-for-large-downloads",
     flag_descriptions::kSmartSuggestionForLargeDownloadsName,
     flag_descriptions::kSmartSuggestionForLargeDownloadsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kSmartSuggestionForLargeDownloads)},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_JXL_DECODER)
    {"enable-jxl", flag_descriptions::kEnableJXLName,
     flag_descriptions::kEnableJXLDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kJXL)},
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

    {"window-naming", flag_descriptions::kWindowNamingName,
     flag_descriptions::kWindowNamingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWindowNaming)},

#if defined(OS_ANDROID)
    {"messages-for-android-infrastructure",
     flag_descriptions::kMessagesForAndroidInfrastructureName,
     flag_descriptions::kMessagesForAndroidInfrastructureDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidInfrastructure)},
    {"messages-for-android-passwords",
     flag_descriptions::kMessagesForAndroidPasswordsName,
     flag_descriptions::kMessagesForAndroidPasswordsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPasswords)},
    {"messages-for-android-popup-blocked",
     flag_descriptions::kMessagesForAndroidPopupBlockedName,
     flag_descriptions::kMessagesForAndroidPopupBlockedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPopupBlocked)},
    {"messages-for-android-reader-mode",
     flag_descriptions::kMessagesForAndroidReaderModeName,
     flag_descriptions::kMessagesForAndroidReaderModeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidReaderMode)},
    {"messages-for-android-safety-tip",
     flag_descriptions::kMessagesForAndroidSafetyTipName,
     flag_descriptions::kMessagesForAndroidSafetyTipDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidSafetyTip)},
    {"messages-for-android-save-card",
     flag_descriptions::kMessagesForAndroidSaveCardName,
     flag_descriptions::kMessagesForAndroidSaveCardDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidSaveCard)},
    {"messages-for-android-update-password",
     flag_descriptions::kMessagesForAndroidUpdatePasswordName,
     flag_descriptions::kMessagesForAndroidUpdatePasswordDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidUpdatePassword)},
#endif

#if defined(OS_ANDROID)
    {"android-detailed-language-settings",
     flag_descriptions::kAndroidDetailedLanguageSettingsName,
     flag_descriptions::kAndroidDetailedLanguageSettingsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kDetailedLanguageSettings)},
    {"android-force-app-language-prompt",
     flag_descriptions::kAndroidForceAppLanguagePromptName,
     flag_descriptions::kAndroidForceAppLanguagePromptDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kForceAppLanguagePrompt)},
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX)
    {"commander", flag_descriptions::kCommanderName,
     flag_descriptions::kCommanderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCommander)},

    {"desktop-restructured-language-settings",
     flag_descriptions::kDesktopRestructuredLanguageSettingsName,
     flag_descriptions::kDesktopRestructuredLanguageSettingsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(language::kDesktopRestructuredLanguageSettings)},

    {"desktop-detailed-language-settings",
     flag_descriptions::kDesktopDetailedLanguageSettingsName,
     flag_descriptions::kDesktopDetailedLanguageSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(language::kDesktopDetailedLanguageSettings)},
#endif

    {"pwa-update-dialog-for-name-and-icon",
     flag_descriptions::kPwaUpdateDialogForNameAndIconName,
     flag_descriptions::kPwaUpdateDialogForNameAndIconDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPwaUpdateDialogForNameAndIcon)},

    {"sync-autofill-wallet-offer-data",
     flag_descriptions::kSyncAutofillWalletOfferDataName,
     flag_descriptions::kSyncAutofillWalletOfferDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncAutofillWalletOfferData)},

#if (defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
     defined(OS_CHROMEOS)) &&                                   \
    BUILDFLAG(ENABLE_PRINTING)
    {"enable-oop-print-drivers", flag_descriptions::kEnableOopPrintDriversName,
     flag_descriptions::kEnableOopPrintDriversDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(printing::features::kEnableOopPrintDrivers)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"omnibox-rich-entities-in-launcher",
     flag_descriptions::kOmniboxRichEntitiesInLauncherName,
     flag_descriptions::kOmniboxRichEntitiesInLauncherDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         app_list_features::kEnableOmniboxRichEntities,
         kOmniboxRichEntitiesInLauncherVariations,
         "OmniboxRichEntitiesInLauncher")},
#endif

#if defined(OS_ANDROID)
    {"decouple-sync-from-android-auto-sync",
     flag_descriptions::kDecoupleSyncFromAndroidAutoSyncName,
     flag_descriptions::kDecoupleSyncFromAndroidAutoSyncDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kDecoupleSyncFromAndroidMasterSync)},
#endif  // defined(OS_ANDROID)

    {"enable-browsing-data-lifetime-manager",
     flag_descriptions::kEnableBrowsingDataLifetimeManagerName,
     flag_descriptions::kEnableBrowsingDataLifetimeManagerDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableBrowsingDataLifetimeManager)},

#if defined(OS_ANDROID)
    {"wallet-requires-first-sync-setup",
     flag_descriptions::kWalletRequiresFirstSyncSetupCompleteName,
     flag_descriptions::kWalletRequiresFirstSyncSetupCompleteDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kWalletRequiresFirstSyncSetupComplete)},
#endif  // defined(OS_ANDROID)

    {"privacy-advisor", flag_descriptions::kPrivacyAdvisorName,
     flag_descriptions::kPrivacyAdvisorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrivacyAdvisor)},

#if defined(TOOLKIT_VIEWS)
    {"desktop-in-product-help-snooze",
     flag_descriptions::kDesktopInProductHelpSnoozeName,
     flag_descriptions::kDesktopInProductHelpSnoozeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHDesktopSnoozeFeature)},
#endif  // defined(TOOLKIT_VIEWS)

    {"animated-image-resume", flag_descriptions::kAnimatedImageResumeName,
     flag_descriptions::kAnimatedImageResumeDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kAnimatedImageResume)},

#if !defined(OS_ANDROID)
    {"sct-auditing", flag_descriptions::kSCTAuditingName,
     flag_descriptions::kSCTAuditingDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kSCTAuditing,
                                    kSCTAuditingVariations,
                                    "SCTAuditingVariations")},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"enable-autofill-password-account-indicator-footer",
     flag_descriptions::
         kEnableAutofillPasswordInfoBarAccountIndicationFooterName,
     flag_descriptions::
         kEnableAutofillPasswordInfoBarAccountIndicationFooterDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnablePasswordInfoBarAccountIndicationFooter)},

    {"incognito-screenshot", flag_descriptions::kIncognitoScreenshotName,
     flag_descriptions::kIncognitoScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kIncognitoScreenshot)},
#endif
    {"use-first-party-set", flag_descriptions::kUseFirstPartySetName,
     flag_descriptions::kUseFirstPartySetDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(network::switches::kUseFirstPartySet, "")},

    {"check-offline-capability", flag_descriptions::kCheckOfflineCapabilityName,
     flag_descriptions::kCheckOfflineCapabilityDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kCheckOfflineCapability,
                                    kCheckOfflineCapabilityVariations,
                                    "CheckOfflineCapability")},
#if defined(OS_ANDROID)
    {"enable-autofill-save-card-info-bar-account-indication-footer",
     flag_descriptions::
         kEnableAutofillSaveCardInfoBarAccountIndicationFooterName,
     flag_descriptions::
         kEnableAutofillSaveCardInfoBarAccountIndicationFooterDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableSaveCardInfoBarAccountIndicationFooter)},
#endif
    {"detect-form-submission-on-form-clear",
     flag_descriptions::kDetectFormSubmissionOnFormClearName,
     flag_descriptions::kDetectFormSubmissionOnFormClearDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kDetectFormSubmissionOnFormClear)},

#if defined(OS_ANDROID)
    {"enable-autofill-infobar-account-indication-footer-for-single-account-"
     "users",
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersName,
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers)},
    {"enable-autofill-infobar-account-indication-footer-for-sync-users",
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersName,
     flag_descriptions::
         kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers)},
#endif

    {"permission-predictions", flag_descriptions::kPermissionPredictionsName,
     flag_descriptions::kPermissionPredictionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPermissionPredictions)},

    {"show-performance-metrics-hud",
     flag_descriptions::kShowPerformanceMetricsHudName,
     flag_descriptions::kShowPerformanceMetricsHudDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHudDisplayForPerformanceMetrics)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"disable-buffer-bw-compression",
     flag_descriptions::kDisableBufferBWCompressionName,
     flag_descriptions::kDisableBufferBWCompressionDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableBufferBWCompression)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-prerender2", flag_descriptions::kPrerender2Name,
     flag_descriptions::kPrerender2Description, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPrerender2)},

#if defined(OS_ANDROID)
    {"enable-swipe-to-move-cursor", flag_descriptions::kSwipeToMoveCursorName,
     flag_descriptions::kSwipeToMoveCursorDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSwipeToMoveCursor)},
#endif  // defined(OS_ANDROID)

    {"safety-check-weak-passwords",
     flag_descriptions::kSafetyCheckWeakPasswordsName,
     flag_descriptions::kSafetyCheckWeakPasswordsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSafetyCheckWeakPasswords)},

#if !defined(OS_ANDROID)
    {"settings-landing-page-redesign",
     flag_descriptions::kSettingsLandingPageRedesignName,
     flag_descriptions::kSettingsLandingPageRedesignDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSettingsLandingPageRedesign)},

    {"webui-branding-update", flag_descriptions::kWebUIBrandingUpdateName,
     flag_descriptions::kWebUIBrandingUpdateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIBrandingUpdate)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"continuous-search", flag_descriptions::kContinuousSearchName,
     flag_descriptions::kContinuousSearchDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kContinuousSearch,
                                    kContinuousSearchFeatureVariations,
                                    "ContinuousSearchVariations")},

    {"enable-experimental-accessibility-labels",
     flag_descriptions::kExperimentalAccessibilityLabelsName,
     flag_descriptions::kExperimentalAccessibilityLabelsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kExperimentalAccessibilityLabels)},
#endif  // defined(OS_ANDROID)

    {"chrome-labs", flag_descriptions::kChromeLabsName,
     flag_descriptions::kChromeLabsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeLabs)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"launcher-search-normalization",
     flag_descriptions::kEnableLauncherSearchNormalizationName,
     flag_descriptions::kEnableLauncherSearchNormalizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableLauncherSearchNormalization)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-first-party-sets", flag_descriptions::kEnableFirstPartySetsName,
     flag_descriptions::kEnableFirstPartySetsDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kFirstPartySets)},

#if defined(OS_ANDROID)
    {"autofill-enable-offers-in-clank-keyboard-accessory",
     flag_descriptions::kAutofillEnableOffersInClankKeyboardAccessoryName,
     flag_descriptions::
         kAutofillEnableOffersInClankKeyboardAccessoryDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableOffersInClankKeyboardAccessory)},
#endif

#if BUILDFLAG(ENABLE_PDF)
    {"pdf-unseasoned", flag_descriptions::kPdfUnseasonedName,
     flag_descriptions::kPdfUnseasonedDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfUnseasoned)},
    {"pdf-xfa-forms", flag_descriptions::kPdfXfaFormsName,
     flag_descriptions::kPdfXfaFormsDescription, kOsAll,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfXfaSupport)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if defined(OS_ANDROID)
    {"actionable-content-settings",
     flag_descriptions::kActionableContentSettingsName,
     flag_descriptions::kActionableContentSettingsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(browser_ui::kActionableContentSettings)},
#endif

    {"send-tab-to-self-when-signed-in",
     flag_descriptions::kSendTabToSelfWhenSignedInName,
     flag_descriptions::kSendTabToSelfWhenSignedInDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfWhenSignedIn)},

    {"send-tab-to-self-v2", flag_descriptions::kSendTabToSelfV2Name,
     flag_descriptions::kSendTabToSelfV2Description, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfV2)},

#if defined(OS_ANDROID)
    {"mobile-pwa-install-use-bottom-sheet",
     flag_descriptions::kMobilePwaInstallUseBottomSheetName,
     flag_descriptions::kMobilePwaInstallUseBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(webapps::features::kPwaInstallUseBottomSheet)},
#endif

    {"text-fragment-color-change",
     flag_descriptions::kTextFragmentColorChangeName,
     flag_descriptions::kTextFragmentColorChangeDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kTextFragmentColorChange)},

#if defined(OS_WIN)
    {"raw-audio-capture", flag_descriptions::kRawAudioCaptureName,
     flag_descriptions::kRawAudioCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kWasapiRawAudioCapture)},
#endif  // defined(OS_MAC)

    {"enable-managed-configuration-web-api",
     flag_descriptions::kEnableManagedConfigurationWebApiName,
     flag_descriptions::kEnableManagedConfigurationWebApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(blink::features::kManagedConfiguration)},

    {"enable-restricted-web-apis",
     flag_descriptions::kEnableRestrictedWebApisName,
     flag_descriptions::kEnableRestrictedWebApisDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableRestrictedWebApis)},

    {"clear-cross-site-cross-browsing-context-group-window-name",
     flag_descriptions::kClearCrossSiteCrossBrowsingContextGroupWindowNameName,
     flag_descriptions::
         kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kClearCrossSiteCrossBrowsingContextGroupWindowName)},

    {"autofill-enable-offer-notification",
     flag_descriptions::kAutofillEnableOfferNotificationName,
     flag_descriptions::kAutofillEnableOfferNotificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOfferNotification)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kWallpaperWebUIInternalName, flag_descriptions::kWallpaperWebUIName,
     flag_descriptions::kWallpaperWebUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperWebUI)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS)
    // TODO(b/180051795): remove kOsLinux when lacros-chrome switches to
    // kOsCrOS.
    {"enable-vaapi-av1-decode-acceleration",
     flag_descriptions::kVaapiAV1DecoderName,
     flag_descriptions::kVaapiAV1DecoderDescription, kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(media::kVaapiAV1Decoder)},
#endif  // defined(OS_CHROMEOS)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-vaapi-vp9-kSVC-encode-acceleration",
     flag_descriptions::kVaapiVP9kSVCEncoderName,
     flag_descriptions::kVaapiVP9kSVCEncoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kVaapiVp9kSVCHWEncoding)},
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC)
    {
        "ui-debug-tools",
        flag_descriptions::kUIDebugToolsName,
        flag_descriptions::kUIDebugToolsDescription,
        kOsWin | kOsLinux | kOsMac,
        FEATURE_VALUE_TYPE(features::kUIDebugTools),
    },
#endif
    {"http-cache-partitioning",
     flag_descriptions::kSplitCacheByNetworkIsolationKeyName,
     flag_descriptions::kSplitCacheByNetworkIsolationKeyDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kSplitCacheByNetworkIsolationKey)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-show-date-in-tray", flag_descriptions::kShowDateInTrayName,
     flag_descriptions::kShowDateInTrayDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShowDateInTrayButton)},
#endif

    {"autofill-address-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptName,
     flag_descriptions::kEnableAutofillAddressSavePromptDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAddressProfileSavePrompt)},

    {"detected-source-language-option",
     flag_descriptions::kDetectedSourceLanguageOptionName,
     flag_descriptions::kDetectedSourceLanguageOptionDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(language::kDetectedSourceLanguageOption)},

#if defined(OS_ANDROID)
    {"content-languages-in-language-picker",
     flag_descriptions::kContentLanguagesInLanguagePickerName,
     flag_descriptions::kContentLanguagesInLanguagePickerName, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kContentLanguagesInLanguagePicker)},
#endif

    {"filling-across-affiliated-websites",
     flag_descriptions::kFillingAcrossAffiliatedWebsitesName,
     flag_descriptions::kFillingAcrossAffiliatedWebsitesDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingAcrossAffiliatedWebsites)},

    {"draw-predicted-ink-point", flag_descriptions::kDrawPredictedPointsName,
     flag_descriptions::kDrawPredictedPointsDescription, kOsAll,
     MULTI_VALUE_TYPE(kDrawPredictedPointsChoices)},

    {"enable-tflite-language-detection",
     flag_descriptions::kTFLiteLanguageDetectionName,
     flag_descriptions::kTFLiteLanguageDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionEnabled)},

    {"optimization-guide-model-downloading",
     flag_descriptions::kOptimizationGuideModelDownloadingName,
     flag_descriptions::kOptimizationGuideModelDownloadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         optimization_guide::features::kOptimizationGuideModelDownloading)},

#if defined(OS_ANDROID)
    {"optimization-guide-push-notifications",
     flag_descriptions::kOptimizationGuideModelPushNotificationName,
     flag_descriptions::kOptimizationGuideModelPushNotificationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(optimization_guide::features::kPushNotifications)},
#endif

    {"media-session-webrtc", flag_descriptions::kMediaSessionWebRTCName,
     flag_descriptions::kMediaSessionWebRTCDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kMediaSessionWebRTC)},

    {"webid", flag_descriptions::kWebIdName,
     flag_descriptions::kWebIdDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebID)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"bluetooth-sessionized-metrics",
     flag_descriptions::kBluetoothSessionizedMetricsName,
     flag_descriptions::kBluetoothSessionizedMetricsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(metrics::structured::kBluetoothSessionizedMetrics)},
#endif

#if defined(OS_LINUX) && defined(USE_OZONE)
    {"use-ozone-platform", flag_descriptions::kUseOzonePlatformName,
     flag_descriptions::kUseOzonePlatformDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kUseOzonePlatform)},
#endif

    {"subframe-shutdown-delay", flag_descriptions::kSubframeShutdownDelayName,
     flag_descriptions::kSubframeShutdownDelayDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSubframeShutdownDelay)},

    {"composite-after-paint", flag_descriptions::kCompositeAfterPaintName,
     flag_descriptions::kCompositeAfterPaintDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCompositeAfterPaint)},
    {"autofill-parse-merchant-promo-code-fields",
     flag_descriptions::kAutofillParseMerchantPromoCodeFieldsName,
     flag_descriptions::kAutofillParseMerchantPromoCodeFieldsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillParseMerchantPromoCodeFields)},

    {"autofill-enable-offer-notification-cross-tab-tracking",
     flag_descriptions::kAutofillEnableOfferNotificationCrossTabTrackingName,
     flag_descriptions::
         kAutofillEnableOfferNotificationCrossTabTrackingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableOfferNotificationCrossTabTracking)},

    {"autofill-fix-offer-in-incognito",
     flag_descriptions::kAutofillFixOfferInIncognitoName,
     flag_descriptions::kAutofillFixOfferInIncognitoDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillFixOfferInIncognito)},

    {"sanitizer-api", flag_descriptions::kSanitizerApiName,
     flag_descriptions::kSanitizerApiDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kSanitizerAPI)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"productivity-reorder-apps", flag_descriptions::kLauncherAppSortName,
     flag_descriptions::kLauncherAppSortDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLauncherAppSort)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desktop-pwas-app-icon-shortcuts-menu-ui",
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuUIName,
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsAppIconShortcutsMenuUI)},

    {"enable-input-event-logging",
     flag_descriptions::kEnableInputEventLoggingName,
     flag_descriptions::kEnableInputEventLoggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableInputEventLogging)},
#endif

    {"muting-compromised-credentials",
     flag_descriptions::kMutingCompromisedCredentialsName,
     flag_descriptions::kMutingCompromisedCredentialsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kMutingCompromisedCredentials)},

    {"autofill-enable-merchant-bound-virtual-cards",
     flag_descriptions::kAutofillEnableMerchantBoundVirtualCardsName,
     flag_descriptions::kAutofillEnableMerchantBoundVirtualCardsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableMerchantBoundVirtualCards)},

    {"autofill-enable-sticky-manual-fallback-for-cards",
     flag_descriptions::kAutofillEnableStickyManualFallbackForCardsName,
     flag_descriptions::kAutofillEnableStickyManualFallbackForCardsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableStickyManualFallbackForCards)},

    {"autofill-auto-trigger-manual-fallback-for-cards",
     flag_descriptions::kAutofillAutoTriggerManualFallbackForCardsName,
     flag_descriptions::kAutofillAutoTriggerManualFallbackForCardsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAutoTriggerManualFallbackForCards)},

    {"autofill-suggest-virtual-cards-on-incomplete-form",
     flag_descriptions::kAutofillSuggestVirtualCardsOnIncompleteFormName,
     flag_descriptions::kAutofillSuggestVirtualCardsOnIncompleteFormDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSuggestVirtualCardsOnIncompleteForm)},

    {flag_descriptions::kEnableLensRegionSearchFlagId,
     flag_descriptions::kEnableLensRegionSearchName,
     flag_descriptions::kEnableLensRegionSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(lens::features::kLensRegionSearch)},

    {"enable-penetrating-image-selection",
     flag_descriptions::kEnablePenetratingImageSelectionName,
     flag_descriptions::kEnablePenetratingImageSelectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEnablePenetratingImageSelection)},

#if defined(OS_ANDROID)
    {"biometric-reauth-password-filling",
     flag_descriptions::kBiometricReauthForPasswordFillingName,
     flag_descriptions::kBiometricReauthForPasswordFillingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kBiometricTouchToFill)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"launcher-query-highlighting",
     flag_descriptions::kLauncherQueryHighlightingName,
     flag_descriptions::kLauncherQueryHighlightingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kLauncherQueryHighlighting)},

    {"enable-input-noise-cancellation-ui",
     flag_descriptions::kEnableInputNoiseCancellationUiName,
     flag_descriptions::kEnableInputNoiseCancellationUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableInputNoiseCancellationUi)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"update-history-entry-points-in-incognito",
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoName,
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUpdateHistoryEntryPointsInIncognito)},

    {"enable-throttle-display-none-and-visibility-hidden-cross-origin-iframes",
     flag_descriptions::
         kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesName,
     flag_descriptions::
         kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         blink::features::
             kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframes)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-input-in-diagnostics-app",
     flag_descriptions::kEnableInputInDiagnosticsAppName,
     flag_descriptions::kEnableInputInDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableInputInDiagnosticsApp)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"link-doctor-deprecation-android",
     flag_descriptions::kLinkDoctorDeprecationAndroidName,
     flag_descriptions::kLinkDoctorDeprecationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLinkDoctorDeprecationAndroid)},
#endif

#if defined(TOOLKIT_VIEWS)
    {"download-shelf-webui", flag_descriptions::kDownloadShelfWebUI,
     flag_descriptions::kDownloadShelfWebUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIDownloadShelf)},
#endif  // defined(TOOLKIT_VIEWS)

    {"playback-speed-button", flag_descriptions::kPlaybackSpeedButtonName,
     flag_descriptions::kPlaybackSpeedButtonDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kPlaybackSpeedButton)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-generated-webapks", flag_descriptions::kEnableGeneratedWebApksName,
     flag_descriptions::kEnableGeneratedWebApksDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWebApkGenerator)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"use-passthrough-command-decoder",
     flag_descriptions::kUsePassthroughCommandDecoderName,
     flag_descriptions::kUsePassthroughCommandDecoderDescription,
     kOsMac | kOsLinux | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDefaultPassthroughCommandDecoder)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"focus-follows-cursor", flag_descriptions::kFocusFollowsCursorName,
     flag_descriptions::kFocusFollowsCursorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(::features::kFocusFollowsCursor)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"clipboard-custom-formats", flag_descriptions::kClipboardCustomFormatsName,
     flag_descriptions::kClipboardCustomFormatsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kClipboardCustomFormats)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"performant-split-view-resizing",
     flag_descriptions::kPerformantSplitViewResizing,
     flag_descriptions::kPerformantSplitViewResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPerformantSplitViewResizing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"privacy-review", flag_descriptions::kPrivacyReviewName,
     flag_descriptions::kPrivacyReviewDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrivacyReview)},

#if defined(OS_ANDROID)
    {"google-mobile-services-passwords",
     flag_descriptions::kUnifiedPasswordManagerAndroidName,
     flag_descriptions::kUnifiedPasswordManagerAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kUnifiedPasswordManagerAndroid)},
#endif

    {"extension-workflow-justification",
     flag_descriptions::kExtensionWorkflowJustificationName,
     flag_descriptions::kExtensionWorkflowJustificationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExtensionWorkflowJustification)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-web-app-share", flag_descriptions::kArcWebAppShareName,
     flag_descriptions::kArcWebAppShareDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableWebAppShareFeature)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"tab-restore-sub-menus", flag_descriptions::kTabRestoreSubMenusName,
     flag_descriptions::kTabRestoreSubMenusDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabRestoreSubMenus)},

    {"chrome-whats-new-ui", flag_descriptions::kChromeWhatsNewUIName,
     flag_descriptions::kChromeWhatsNewUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeWhatsNewUI)},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"chrome-whats-new-in-main-menu-new-badge",
     flag_descriptions::kChromeWhatsNewInMainMenuNewBadgeName,
     flag_descriptions::kChromeWhatsNewInMainMenuNewBadgeDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeWhatsNewInMainMenuNewBadge)},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    {"sync-trusted-vault-passphrase-promo",
     flag_descriptions::kSyncTrustedVaultPassphrasePromoName,
     flag_descriptions::kSyncTrustedVaultPassphrasePromoDescription, kOsAll,
     FEATURE_VALUE_TYPE(::switches::kSyncTrustedVaultPassphrasePromo)},

    {"sync-trusted-vault-passphrase-recovery",
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryName,
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryDescription, kOsAll,
     FEATURE_VALUE_TYPE(::switches::kSyncTrustedVaultPassphraseRecovery)},

    {"debug-history-intervention-no-user-activation",
     flag_descriptions::kDebugHistoryInterventionNoUserActivationName,
     flag_descriptions::kDebugHistoryInterventionNoUserActivationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kDebugHistoryInterventionNoUserActivation)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"smart-lock-ui-revamp", flag_descriptions::kSmartLockUIRevampName,
     flag_descriptions::kSmartLockUIRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSmartLockUIRevamp)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-phone-hub-camera-roll", flag_descriptions::kPhoneHubCameraRollName,
     flag_descriptions::kPhoneHubCameraRollDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPhoneHubCameraRoll)},

    {"enable-phone-hub-recent-apps", flag_descriptions::kPhoneHubRecentAppsName,
     flag_descriptions::kPhoneHubRecentAppsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPhoneHubRecentApps)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"sameparty-cookies-considered-first-party",
     flag_descriptions::kSamePartyCookiesConsideredFirstPartyName,
     flag_descriptions::kSamePartyCookiesConsideredFirstPartyDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(net::features::kSamePartyCookiesConsideredFirstParty)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kBorealisDiskManagementInternalName,
     flag_descriptions::kBorealisDiskManagementName,
     flag_descriptions::kBorealisDiskManagementDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisDiskManagement)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"dynamic-color-android", flag_descriptions::kDynamicColorAndroidName,
     flag_descriptions::kDynamicColorAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDynamicColorAndroid)},
#endif  //   defined(OS_ANDROID)

#if defined(OS_WIN)
    {"win-10-tab-search-caption-button",
     flag_descriptions::kWin10TabSearchCaptionButtonName,
     flag_descriptions::kWin10TabSearchCaptionButtonDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWin10TabSearchCaptionButton)},
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-holding-space-in-progress-downloads-integration",
     flag_descriptions::kHoldingSpaceInProgressDownloadsIntegrationName,
     flag_descriptions::kHoldingSpaceInProgressDownloadsIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kHoldingSpaceInProgressDownloadsIntegration)},
#endif

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC) || (defined(OS_ANDROID))
    {"omnibox-updated-connection-security-indicators",
     flag_descriptions::kOmniboxUpdatedConnectionSecurityIndicatorsName,
     flag_descriptions::kOmniboxUpdatedConnectionSecurityIndicatorsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kUpdatedConnectionSecurityIndicators)},
#endif

#if defined(OS_ANDROID)
    {"share-usage-ranking", flag_descriptions::kShareUsageRankingName,
     flag_descriptions::kShareUsageRankingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kShareUsageRanking)},
#endif

    // NOTE: Adding a new flag requires adding a corresponding entry to enum
    // "LoginCustomFlags" in tools/metrics/histograms/enums.xml. See "Flag
    // Histograms" in tools/metrics/histograms/README.md (run the
    // AboutFlagsHistogramTest unit test to verify this process).
};

class FlagsStateSingleton : public flags_ui::FlagsState::Delegate {
 public:
  FlagsStateSingleton()
      : flags_state_(
            std::make_unique<flags_ui::FlagsState>(kFeatureEntries, this)) {}
  FlagsStateSingleton(const FlagsStateSingleton&) = delete;
  FlagsStateSingleton& operator=(const FlagsStateSingleton&) = delete;
  ~FlagsStateSingleton() override = default;

  static FlagsStateSingleton* GetInstance() {
    return base::Singleton<FlagsStateSingleton>::get();
  }

  static flags_ui::FlagsState* GetFlagsState() {
    return GetInstance()->flags_state_.get();
  }

  void RebuildState(const std::vector<flags_ui::FeatureEntry>& entries) {
    flags_state_ = std::make_unique<flags_ui::FlagsState>(entries, this);
  }

  void RestoreDefaultState() {
    flags_state_ =
        std::make_unique<flags_ui::FlagsState>(kFeatureEntries, this);
  }

 private:
  // flags_ui::FlagsState::Delegate:
  bool ShouldExcludeFlag(const flags_ui::FlagsStorage* storage,
                         const FeatureEntry& entry) override {
    return flags::IsFlagExpired(storage, entry.internal_name);
  }

  std::unique_ptr<flags_ui::FlagsState> flags_state_;
};

bool ShouldSkipNonDeprecatedFeatureEntry(const FeatureEntry& entry) {
  return ~entry.supported_platforms & kDeprecated;
}

}  // namespace

bool ShouldSkipConditionalFeatureEntry(const flags_ui::FlagsStorage* storage,
                                       const FeatureEntry& entry) {
  version_info::Channel channel = chrome::GetChannel();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // enable-ui-devtools is only available on for non Stable channels.
  if (!strcmp(ui_devtools::switches::kEnableUiDevTools, entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  if (!strcmp(kLacrosSupportInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosSupportFlagAllowed(channel);
  }

  if (!strcmp(kLacrosStabilityInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosAllowedToBeEnabled(channel);
  }

  if (!strcmp(kLacrosPrimaryInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosPrimaryFlagAllowed(channel);
  }

  if (!strcmp(kWebAppsCrosapiInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosAllowedToBeEnabled(channel);
  }

  // The following flags are only available to teamfooders.
  if (!strcmp(kAssistantBetterOnboardingInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kTeamfoodFlags);
  }

  // wallpaper-webui is only available for Unknown/Canary/Dev channels.
  if (!strcmp(kWallpaperWebUIInternalName, entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

  // Leave the feature only for ARCVM.
  if (!strcmp(kArcUseHighMemoryDalvikProfileInternalName,
              entry.internal_name)) {
    return !arc::IsArcVmEnabled();
  }

  // Only show Borealis flags on enabled devices.
  if (!strcmp(kBorealisDiskManagementInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // data-reduction-proxy-lo-fi and enable-data-reduction-proxy-lite-page
  // are only available for Chromium builds and the Canary/Dev/Beta channels.
  if ((!strcmp("data-reduction-proxy-lo-fi", entry.internal_name) ||
       !strcmp("enable-data-reduction-proxy-lite-page", entry.internal_name)) &&
      channel != version_info::Channel::BETA &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

  // enable-unsafe-webgpu is only available on Dev/Canary channels.
  if (!strcmp("enable-unsafe-webgpu", entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

#if defined(OS_WIN)
  // HDR mode works, but displays everything horribly wrong prior to windows 10.
  if (!strcmp("enable-hdr", entry.internal_name) &&
      base::win::GetVersion() < base::win::Version::WIN10) {
    return true;
  }
#endif  // OS_WIN

  if (!strcmp("dns-over-https", entry.internal_name) &&
      SystemNetworkContextManager::GetInstance() &&
      (SystemNetworkContextManager::GetStubResolverConfigReader()
           ->ShouldDisableDohForManaged() ||
       features::kDnsOverHttpsShowUiParam.Get())) {
    return true;
  }

#if defined(OS_ANDROID)
  if (!strcmp("password-change-support", entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kTeamfoodFlags);
  }
#endif  // OS_ANDROID

  if (flags::IsFlagExpired(storage, entry.internal_name))
    return true;

  return false;
}

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            flags_ui::SentinelsMode sentinels) {
  if (command_line->HasSwitch(switches::kNoExperiments))
    return;

  FlagsStateSingleton::GetFlagsState()->ConvertFlagsToSwitches(
      flags_storage, command_line, sentinels, switches::kEnableFeatures,
      switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  return FlagsStateSingleton::GetFlagsState()
      ->RegisterAllFeatureVariationParameters(flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::Value::ListStorage& supported_entries,
                           base::Value::ListStorage& unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::BindRepeating(&ShouldSkipConditionalFeatureEntry,
                          // Unretained: this callback doesn't outlive this
                          // stack frame.
                          base::Unretained(flags_storage)));
}

void GetFlagFeatureEntriesForDeprecatedPage(
    flags_ui::FlagsStorage* flags_storage,
    flags_ui::FlagAccess access,
    base::Value::ListStorage& supported_entries,
    base::Value::ListStorage& unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::BindRepeating(&ShouldSkipNonDeprecatedFeatureEntry));
}

flags_ui::FlagsState* GetCurrentFlagsState() {
  return FlagsStateSingleton::GetFlagsState();
}

bool IsRestartNeededToCommitChanges() {
  return FlagsStateSingleton::GetFlagsState()->IsRestartNeededToCommitChanges();
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  FlagsStateSingleton::GetFlagsState()->SetFeatureEntryEnabled(
      flags_storage, internal_name, enable);
}

void SetOriginListFlag(const std::string& internal_name,
                       const std::string& value,
                       flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->SetOriginListFlag(internal_name, value,
                                                          flags_storage);
}

void RemoveFlagsSwitches(base::CommandLine::SwitchMap* switch_list) {
  FlagsStateSingleton::GetFlagsState()->RemoveFlagsSwitches(switch_list);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->ResetAllFlags(flags_storage);
}

void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage,
                         const std::string& histogram_name) {
  std::set<std::string> switches;
  std::set<std::string> features;
  FlagsStateSingleton::GetFlagsState()->GetSwitchesAndFeaturesFromFlags(
      flags_storage, &switches, &features);
  flags_ui::ReportAboutFlagsHistogram(histogram_name, switches, features);
}

namespace testing {

std::vector<FeatureEntry>* GetEntriesForTesting() {
  static base::NoDestructor<std::vector<FeatureEntry>> entries;
  return entries.get();
}

void SetFeatureEntries(const std::vector<FeatureEntry>& entries) {
  CHECK(GetEntriesForTesting()->empty());  // IN-TEST
  for (const auto& entry : entries)
    GetEntriesForTesting()->push_back(entry);  // IN-TEST
  FlagsStateSingleton::GetInstance()->RebuildState(
      *GetEntriesForTesting());  // IN-TEST
}

ScopedFeatureEntries::ScopedFeatureEntries(
    const std::vector<flags_ui::FeatureEntry>& entries) {
  SetFeatureEntries(entries);
}

ScopedFeatureEntries::~ScopedFeatureEntries() {
  GetEntriesForTesting()->clear();  // IN-TEST
  // Restore the flag state to the production flags.
  FlagsStateSingleton::GetInstance()->RestoreDefaultState();
}

const FeatureEntry* GetFeatureEntries(size_t* count) {
  if (!GetEntriesForTesting()->empty()) {
    *count = GetEntriesForTesting()->size();
    return GetEntriesForTesting()->data();
  }
  *count = base::size(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing

}  // namespace about_flags
