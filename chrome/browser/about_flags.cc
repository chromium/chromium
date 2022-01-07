// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Instructions for adding new entries to this file:
// https://chromium.googlesource.com/chromium/src/+/main/docs/how_to_add_your_feature_flag.md#step-2_adding-the-feature-flag-to-the-chrome_flags-ui

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

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
#include "chrome/browser/apps/app_discovery_service/app_discovery_features.h"
#include "chrome/browser/ash/android_sms/android_sms_switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/permissions/abusive_origin_notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/share/share_submenu_model.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/site_isolation/about_flags.h"
#include "chrome/browser/ui/app_list/search/search_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ui/wm/features.h"
#include "components/assist_ranker/predictor_config_definitions.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/bookmarks/browser/features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browsing_data/core/features.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/content_settings/core/common/features.h"
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
#include "components/history_clusters/core/features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/lens/lens_features.h"
#include "components/lookalikes/core/features.h"
#include "components/messages/android/messages_feature.h"
#include "components/mirroring/service/mirroring_features.h"
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
#include "components/page_info/core/features.h"
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
#include "device/bluetooth/floss/floss_features.h"
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
#include "media/webrtc/webrtc_features.h"
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
#include "ui/ui_features.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/allocator/buildflags.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/reactions/core/reactions_features.h"
#include "components/external_intents/android/external_intents_features.h"
#include "components/power_scheduler/power_scheduler_features.h"
#include "components/webapps/browser/android/features.h"
#else  // OS_ANDROID
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#endif  // OS_ANDROID

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/app_restore/features.h"
#include "components/metrics/structured/structured_metrics_features.h"  // nogncheck
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#include "chrome/common/webui_url_constants.h"
#endif

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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_features/supervised_user_features.h"
#endif  // ENABLE_SUPERVISED_USERS

#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_switches.h"
#endif  // OS_LINUX || BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/titlebar_config.h"
#include "ui/color/color_switches.h"  // nogncheck
#endif                                // OS_WIN

#if defined(TOOLKIT_VIEWS)
#include "ui/views/views_features.h"
#include "ui/views/views_switches.h"
#endif  // defined(TOOLKIT_VIEWS)

using flags_ui::FeatureEntry;
using flags_ui::kDeprecated;
using flags_ui::kOsAndroid;
using flags_ui::kOsCrOS;
using flags_ui::kOsCrOSOwnerOnly;
using flags_ui::kOsFuchsia;
using flags_ui::kOsLinux;
using flags_ui::kOsMac;
using flags_ui::kOsWin;

namespace about_flags {

namespace {

const unsigned kOsAll =
    kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid | kOsFuchsia;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsFuchsia;

#if defined(USE_AURA)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS | kOsFuchsia;
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

#if defined(OS_WIN)
const FeatureEntry::Choice kUseAngleChoicesWindows[] = {
    {flag_descriptions::kUseAngleDefault, "", ""},
    {flag_descriptions::kUseAngleGL, switches::kUseANGLE,
     gl::kANGLEImplementationOpenGLName},
    {flag_descriptions::kUseAngleD3D11, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11Name},
    {flag_descriptions::kUseAngleD3D9, switches::kUseANGLE,
     gl::kANGLEImplementationD3D9Name},
    {flag_descriptions::kUseAngleD3D11on12, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11on12Name}};
#elif defined(OS_MAC)
const FeatureEntry::Choice kUseAngleChoicesMac[] = {
    {flag_descriptions::kUseAngleDefault, "", ""},
    {flag_descriptions::kUseAngleGL, switches::kUseANGLE,
     gl::kANGLEImplementationOpenGLName},
    {flag_descriptions::kUseAngleMetal, switches::kUseANGLE,
     gl::kANGLEImplementationMetalName}};
#endif

#if defined(OS_LINUX)
const FeatureEntry::Choice kOzonePlatformHintRuntimeChoices[] = {
    {flag_descriptions::kOzonePlatformHintChoiceDefault, "", ""},
    {flag_descriptions::kOzonePlatformHintChoiceAuto,
     switches::kOzonePlatformHint, "auto"},
#if BUILDFLAG(OZONE_PLATFORM_X11)
    {flag_descriptions::kOzonePlatformHintChoiceX11,
     switches::kOzonePlatformHint, "x11"},
#endif
#if BUILDFLAG(OZONE_PLATFORM_WAYLAND)
    {flag_descriptions::kOzonePlatformHintChoiceWayland,
     switches::kOzonePlatformHint, "wayland"},
#endif
};
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
const FeatureEntry::FeatureParam kAdaptiveButton_AlwaysNone[] = {
    {"mode", "always-none"}};
const FeatureEntry::FeatureParam kAdaptiveButton_AlwaysNewTab[] = {
    {"mode", "always-new-tab"}};
const FeatureEntry::FeatureParam kAdaptiveButton_AlwaysShare[] = {
    {"mode", "always-share"}};
const FeatureEntry::FeatureParam kAdaptiveButton_AlwaysVoice[] = {
    {"mode", "always-voice"}};
const FeatureEntry::FeatureVariation kAdaptiveButtonInTopToolbarVariations[] = {
    {"Always None", kAdaptiveButton_AlwaysNone,
     base::size(kAdaptiveButton_AlwaysNone), nullptr},
    {"Always New Tab", kAdaptiveButton_AlwaysNewTab,
     base::size(kAdaptiveButton_AlwaysNewTab), nullptr},
    {"Always Share", kAdaptiveButton_AlwaysShare,
     base::size(kAdaptiveButton_AlwaysShare), nullptr},
    {"Always Voice", kAdaptiveButton_AlwaysVoice,
     base::size(kAdaptiveButton_AlwaysVoice), nullptr},
};

const FeatureEntry::FeatureParam kAdaptiveButtonCustomization_NewTab[] = {
    {"default_segment", "new-tab"},
    {"ignore_segmentation_results", "true"}};
const FeatureEntry::FeatureParam kAdaptiveButtonCustomization_Share[] = {
    {"default_segment", "share"},
    {"ignore_segmentation_results", "true"}};
const FeatureEntry::FeatureParam kAdaptiveButtonCustomization_Voice[] = {
    {"default_segment", "voice"},
    {"ignore_segmentation_results", "true"}};
const FeatureEntry::FeatureVariation
    kAdaptiveButtonInTopToolbarCustomizationVariations[] = {
        {"New Tab", kAdaptiveButtonCustomization_NewTab,
         base::size(kAdaptiveButtonCustomization_NewTab), nullptr},
        {"Share", kAdaptiveButtonCustomization_Share,
         base::size(kAdaptiveButtonCustomization_Share), nullptr},
        {"Voice", kAdaptiveButtonCustomization_Voice,
         base::size(kAdaptiveButtonCustomization_Voice), nullptr},
};
#endif  // OS_ANDROID

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kForceDark_SimpleHsl[] = {
    {"inversion_method", "hsl_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SimpleCielab[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SimpleRgb[] = {
    {"inversion_method", "rgb_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

// Keep in sync with the kForceDark_SelectiveImageInversion
// in aw_feature_entries.cc if you tweak these parameters.
const FeatureEntry::FeatureParam kForceDark_SelectiveImageInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"foreground_lightness_threshold", "255"},
    {"background_lightness_threshold", "0"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveElementInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureParam kForceDark_SelectiveGeneralInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"foreground_lightness_threshold", "150"},
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

const FeatureEntry::FeatureParam kLongScreenshot_AutoscrollDragSlow[] = {
    {"autoscroll", "1"}};
const FeatureEntry::FeatureParam kLongScreenshot_AutoscrollDragQuick[] = {
    {"autoscroll", "2"}};
const FeatureEntry::FeatureVariation kLongScreenshotVariations[] = {
    {"Autoscroll Experiment 1", kLongScreenshot_AutoscrollDragSlow,
     base::size(kLongScreenshot_AutoscrollDragSlow), nullptr},
    {"Autoscroll Experiment 2", kLongScreenshot_AutoscrollDragQuick,
     base::size(kLongScreenshot_AutoscrollDragQuick), nullptr}};

const FeatureEntry::FeatureParam kShowSingleRowMVTiles[] = {
    {"most_visited_max_rows_normal_screen", "1"},
    {"most_visited_max_rows_small_screen", "1"},
    {"small_screen_height_threshold_dp", "700"}};
const FeatureEntry::FeatureParam kShowTwoRowsMVTiles[] = {
    {"most_visited_max_rows_normal_screen", "2"},
    {"most_visited_max_rows_small_screen", "2"},
    {"small_screen_height_threshold_dp", "700"}};
const FeatureEntry::FeatureVariation kQueryTilesVariations[] = {
    {"(show single row of MV tiles)", kShowSingleRowMVTiles,
     base::size(kShowSingleRowMVTiles), nullptr},
    {"(show two rows of MV tiles)", kShowTwoRowsMVTiles,
     base::size(kShowTwoRowsMVTiles), nullptr}};

const FeatureEntry::FeatureParam kDangerousDownloadNoFilledNegativeButton = {
    "filled_negative_button", "false"};
const FeatureEntry::FeatureParam kDangerousDownloadFilledNegativeButton = {
    "filled_negative_button", "true"};
const FeatureEntry::FeatureVariation kDangerousDownloadDialogVariations[] = {
    {"without filled negative button",
     &kDangerousDownloadNoFilledNegativeButton, 1, nullptr},
    {"with filled negative button", &kDangerousDownloadFilledNegativeButton, 1,
     nullptr}};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableGpuRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableGpuRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableGpuRasterization, ""},
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
const char kLacrosAvailabilityIgnoreInternalName[] =
    "lacros-availability-ignore";
const char kLacrosPrimaryInternalName[] = "lacros-primary";
const char kLacrosSupportInternalName[] = "lacros-support";
const char kLacrosStabilityInternalName[] = "lacros-stability";
const char kWebAppsCrosapiInternalName[] = "web-apps-crosapi";
const char kArcVmBalloonPolicyInternalName[] =
    "arc-use-limit-cache-balloon-policy";

const FeatureEntry::Choice kLacrosStabilityChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {crosapi::browser_util::kLacrosStabilityChannelCanary,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityChannelCanary},
    {crosapi::browser_util::kLacrosStabilityChannelDev,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityChannelDev},
    {crosapi::browser_util::kLacrosStabilityChannelBeta,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityChannelBeta},
    {crosapi::browser_util::kLacrosStabilityChannelStable,
     crosapi::browser_util::kLacrosStabilitySwitch,
     crosapi::browser_util::kLacrosStabilityChannelStable},
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

#if BUILDFLAG(ENABLE_NACL)
// Note: This needs to be kept in sync with parsing in
// content/common/zygote/zygote_communication_linux.cc
const FeatureEntry::Choice kVerboseLoggingInNaclChoices[] = {
    {flag_descriptions::kVerboseLoggingInNaclChoiceDefault, "", ""},
    {flag_descriptions::kVerboseLoggingInNaclChoiceLow,
     switches::kVerboseLoggingInNacl, switches::kVerboseLoggingInNaclChoiceLow},
    {flag_descriptions::kVerboseLoggingInNaclChoiceMedium,
     switches::kVerboseLoggingInNacl,
     switches::kVerboseLoggingInNaclChoiceMedium},
    {flag_descriptions::kVerboseLoggingInNaclChoiceHigh,
     switches::kVerboseLoggingInNacl,
     switches::kVerboseLoggingInNaclChoiceHigh},
    {flag_descriptions::kVerboseLoggingInNaclChoiceHighest,
     switches::kVerboseLoggingInNacl,
     switches::kVerboseLoggingInNaclChoiceHighest},
    {flag_descriptions::kVerboseLoggingInNaclChoiceDisabled,
     switches::kVerboseLoggingInNacl,
     switches::kVerboseLoggingInNaclChoiceDisabled},
};
#endif  // ENABLE_NACL

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

const FeatureEntry::FeatureParam kPageContentAnnotationsContentParams[] = {
    {"annotate_title_instead_of_page_content", "false"},
    {"extract_related_searches", "true"},
    {"max_size_for_text_dump_in_bytes", "5120"},
    {"models_to_execute",
     "OPTIMIZATION_TARGET_PAGE_TOPICS,OPTIMIZATION_TARGET_PAGE_ENTITIES"},
    {"write_to_history_service", "true"},
};
const FeatureEntry::FeatureParam kPageContentAnnotationsTitleParams[] = {
    {"annotate_title_instead_of_page_content", "true"},
    {"extract_related_searches", "true"},
    {"max_size_for_text_dump_in_bytes", "5120"},
    {"models_to_execute",
     "OPTIMIZATION_TARGET_PAGE_TOPICS,OPTIMIZATION_TARGET_PAGE_ENTITIES"},
    {"write_to_history_service", "true"},
};
const FeatureEntry::FeatureVariation kPageContentAnnotationsVariations[] = {
    {"All Annotations and Persistence on Content",
     kPageContentAnnotationsContentParams,
     base::size(kPageContentAnnotationsContentParams), nullptr},
    {"All Annotations and Persistence on Title",
     kPageContentAnnotationsTitleParams,
     base::size(kPageContentAnnotationsTitleParams), nullptr},
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN) || defined(OS_FUCHSIA)
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
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionTitle[] = {
    {"RichAutocompletionAutocompleteTitles", "true"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionNonPrefix[] = {
    {"RichAutocompletionAutocompleteNonPrefixAll", "true"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionTitleNonPrefix[] = {
    {"RichAutocompletionAutocompleteTitles", "true"},
    {"RichAutocompletionAutocompleteNonPrefixAll", "true"}};

const FeatureEntry::FeatureVariation kOmniboxRichAutocompletionVariations[] = {
    {"Title AC", kOmniboxRichAutocompletionTitle,
     base::size(kOmniboxRichAutocompletionTitle), nullptr},
    {"Non-Prefix AC", kOmniboxRichAutocompletionNonPrefix,
     base::size(kOmniboxRichAutocompletionNonPrefix), nullptr},
    {"Title AC & Non-Prefix AC", kOmniboxRichAutocompletionTitleNonPrefix,
     base::size(kOmniboxRichAutocompletionTitleNonPrefix), nullptr}};

const FeatureEntry::FeatureParam kOmniboxRichAutocompletionMinChar00[] = {
    {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
    {"RichAutocompletionAutocompleteNonPrefixMinChar", "0"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionMinChar03[] = {
    {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
    {"RichAutocompletionAutocompleteNonPrefixMinChar", "3"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionMinChar05[] = {
    {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
    {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionMinChar33[] = {
    {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
    {"RichAutocompletionAutocompleteNonPrefixMinChar", "3"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionMinChar35[] = {
    {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
    {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionMinChar55[] = {
    {"RichAutocompletionAutocompleteTitlesMinChar", "5"},
    {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionMinCharVariations[] = {
        {"Title 0 / Non Prefix 0", kOmniboxRichAutocompletionMinChar00,
         base::size(kOmniboxRichAutocompletionMinChar00), nullptr},
        {"Title 0 / Non Prefix 3", kOmniboxRichAutocompletionMinChar03,
         base::size(kOmniboxRichAutocompletionMinChar03), nullptr},
        {"Title 0 / Non Prefix 5", kOmniboxRichAutocompletionMinChar05,
         base::size(kOmniboxRichAutocompletionMinChar05), nullptr},
        {"Title 3 / Non Prefix 3", kOmniboxRichAutocompletionMinChar33,
         base::size(kOmniboxRichAutocompletionMinChar33), nullptr},
        {"Title 3 / Non Prefix 5", kOmniboxRichAutocompletionMinChar35,
         base::size(kOmniboxRichAutocompletionMinChar35), nullptr},
        {"Title 5 / Non Prefix 5", kOmniboxRichAutocompletionMinChar55,
         base::size(kOmniboxRichAutocompletionMinChar55), nullptr}};

const FeatureEntry::FeatureParam
    kOmniboxRichAutocompletionAdditionalTextHide[] = {
        {"RichAutocompletionAutocompleteShowAdditionalText", "false"}};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionShowAdditionalTextVariations[] = {
        {"Show Additional Text", {}, 0, nullptr},
        {"Hide Additional Text", kOmniboxRichAutocompletionAdditionalTextHide,
         base::size(kOmniboxRichAutocompletionAdditionalTextHide), nullptr}};

const FeatureEntry::FeatureParam kOmniboxRichAutocompletionTitlesUrls5[] = {
    {"RichAutocompletionSplitTitleCompletion", "true"},
    {"RichAutocompletionSplitUrlCompletion", "true"},
    {"RichAutocompletionSplitCompletionMinChar", "5"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionTitlesUrls3[] = {
    {"RichAutocompletionSplitTitleCompletion", "true"},
    {"RichAutocompletionSplitUrlCompletion", "true"},
    {"RichAutocompletionSplitCompletionMinChar", "3"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionTitles5[] = {
    {"RichAutocompletionSplitTitleCompletion", "true"},
    {"RichAutocompletionSplitCompletionMinChar", "5"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionTitles3[] = {
    {"RichAutocompletionSplitTitleCompletion", "true"},
    {"RichAutocompletionSplitCompletionMinChar", "3"}};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionSplitVariations[] = {
        {"Titles & URLs, min char 5", kOmniboxRichAutocompletionTitlesUrls5,
         base::size(kOmniboxRichAutocompletionTitlesUrls5), nullptr},
        {"Titles & URLs, min char 3", kOmniboxRichAutocompletionTitlesUrls3,
         base::size(kOmniboxRichAutocompletionTitlesUrls3), nullptr},
        {"Titles, min char 5", kOmniboxRichAutocompletionTitles5,
         base::size(kOmniboxRichAutocompletionTitles5), nullptr},
        {"Titles, min char 3", kOmniboxRichAutocompletionTitles3,
         base::size(kOmniboxRichAutocompletionTitles3), nullptr}};

const FeatureEntry::FeatureParam kOmniboxRichAutocompletionPreferUrls[] = {
    {"RichAutocompletionAutocompletePreferUrlsOverPrefixes", "true"}};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPreferUrlsOverPrefixesVariations[] = {
        {"Prefer prefixes", {}, 0, nullptr},
        {"Prefer URLs", kOmniboxRichAutocompletionPreferUrls,
         base::size(kOmniboxRichAutocompletionPreferUrls), nullptr}};

// A limited number of combinations of the above variations that are most
// promising.
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionAggressive[] = {
    {"RichAutocompletionAutocompleteTitles", "true"},
    {"RichAutocompletionAutocompleteNonPrefixAll", "true"}};
const FeatureEntry::FeatureParam
    kOmniboxRichAutocompletionAggressiveModerate[] = {
        {"RichAutocompletionAutocompleteTitles", "true"},
        {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
        {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}};
const FeatureEntry::FeatureParam
    kOmniboxRichAutocompletionConservativeModerate[] = {
        {"RichAutocompletionAutocompleteTitles", "true"},
        {"RichAutocompletionAutocompleteNonPrefixShortcutProvider", "true"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
        {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}};
const FeatureEntry::FeatureParam
    kOmniboxRichAutocompletionConservativeModerate2[] = {
        {"RichAutocompletionAutocompleteTitlesShortcutProvider", "true"},
        {"RichAutocompletionAutocompleteNonPrefixShortcutProvider", "true"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
        {"RichAutocompletionAutocompleteNonPrefixMinChar", "5"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionConservative[] = {
    {"RichAutocompletionAutocompleteTitles", "true"},
    {"RichAutocompletionAutocompleteTitlesMinChar", "3"}};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPromisingVariations[] = {
        {"Aggressive - Title, Non-Prefix, min 0/0",
         kOmniboxRichAutocompletionAggressive,
         base::size(kOmniboxRichAutocompletionAggressive), nullptr},
        {"Aggressive Moderate - Title, Non-Prefix, min 3/5",
         kOmniboxRichAutocompletionAggressiveModerate,
         base::size(kOmniboxRichAutocompletionAggressiveModerate), nullptr},
        {"Conservative Moderate - Title, Shortcut Non-Prefix, min 3/5",
         kOmniboxRichAutocompletionConservativeModerate,
         base::size(kOmniboxRichAutocompletionConservativeModerate), nullptr},
        {"Conservative Moderate 2 - Shortcut Title, Shortcut Non-Prefix, min "
         "3/5",
         kOmniboxRichAutocompletionConservativeModerate2,
         base::size(kOmniboxRichAutocompletionConservativeModerate2), nullptr},
        {"Conservative - Title, min 3", kOmniboxRichAutocompletionConservative,
         base::size(kOmniboxRichAutocompletionConservative), nullptr}};

const FeatureEntry::FeatureParam kOmniboxBookmarkPathsReplaceTitle[] = {
    {"OmniboxBookmarkPathsUiReplaceTitle", "true"}};
const FeatureEntry::FeatureParam kOmniboxBookmarkPathsReplaceUrl[] = {
    {"OmniboxBookmarkPathsUiReplaceUrl", "true"}};
const FeatureEntry::FeatureParam kOmniboxBookmarkPathsAppendAfterTitle[] = {
    {"OmniboxBookmarkPathsUiAppendAfterTitle", "true"}};
const FeatureEntry::FeatureParam kOmniboxBookmarkPathsDynamicReplaceUrl[] = {
    {"OmniboxBookmarkPathsUiDynamicReplaceUrl", "true"}};

const FeatureEntry::FeatureVariation kOmniboxBookmarkPathsVariations[] = {
    {"Default UI (Title - URL)", {}, 0, nullptr},
    {"Replace title (Path/Title - URL)", kOmniboxBookmarkPathsReplaceTitle,
     base::size(kOmniboxBookmarkPathsReplaceTitle), nullptr},
    {"Replace URL (Title - Path)", kOmniboxBookmarkPathsReplaceUrl,
     base::size(kOmniboxBookmarkPathsReplaceUrl), nullptr},
    {"Append after title (Title : Path - URL)",
     kOmniboxBookmarkPathsAppendAfterTitle,
     base::size(kOmniboxBookmarkPathsAppendAfterTitle), nullptr},
    {"Dynamic Replace URL (Title - Path|URL)",
     kOmniboxBookmarkPathsDynamicReplaceUrl,
     base::size(kOmniboxBookmarkPathsDynamicReplaceUrl), nullptr}};
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN) || defined(OS_FUCHSIA)

const FeatureEntry::FeatureVariation
    kOmniboxOnFocusSuggestionsContextualWebVariations[] = {
        {"GOC Only", {}, 0, "t3317583"},
        {"pSuggest Only", {}, 0, "t3318055"},
        {"GOC, pSuggest Fallback", {}, 0, "t3317692"},
        {"GOC, pSuggest Backfill", {}, 0, "t3317694"},
        {"GOC, Default Hidden", {}, 0, "t3317834"},
};

const FeatureEntry::FeatureParam kMaxZeroSuggestMatches5[] = {
    {"MaxZeroSuggestMatches", "5"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches6[] = {
    {"MaxZeroSuggestMatches", "6"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches7[] = {
    {"MaxZeroSuggestMatches", "7"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches8[] = {
    {"MaxZeroSuggestMatches", "8"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches9[] = {
    {"MaxZeroSuggestMatches", "9"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches10[] = {
    {"MaxZeroSuggestMatches", "10"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches11[] = {
    {"MaxZeroSuggestMatches", "11"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches12[] = {
    {"MaxZeroSuggestMatches", "12"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches13[] = {
    {"MaxZeroSuggestMatches", "13"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches14[] = {
    {"MaxZeroSuggestMatches", "14"}};
const FeatureEntry::FeatureParam kMaxZeroSuggestMatches15[] = {
    {"MaxZeroSuggestMatches", "15"}};

const FeatureEntry::FeatureVariation kMaxZeroSuggestMatchesVariations[] = {
    {"5", kMaxZeroSuggestMatches5, base::size(kMaxZeroSuggestMatches5),
     nullptr},
    {"6", kMaxZeroSuggestMatches6, base::size(kMaxZeroSuggestMatches6),
     nullptr},
    {"7", kMaxZeroSuggestMatches7, base::size(kMaxZeroSuggestMatches7),
     nullptr},
    {"8", kMaxZeroSuggestMatches8, base::size(kMaxZeroSuggestMatches8),
     nullptr},
    {"9", kMaxZeroSuggestMatches9, base::size(kMaxZeroSuggestMatches9),
     nullptr},
    {"10", kMaxZeroSuggestMatches10, base::size(kMaxZeroSuggestMatches10),
     nullptr},
    {"11", kMaxZeroSuggestMatches11, base::size(kMaxZeroSuggestMatches11),
     nullptr},
    {"12", kMaxZeroSuggestMatches12, base::size(kMaxZeroSuggestMatches12),
     nullptr},
    {"13", kMaxZeroSuggestMatches13, base::size(kMaxZeroSuggestMatches13),
     nullptr},
    {"14", kMaxZeroSuggestMatches14, base::size(kMaxZeroSuggestMatches14),
     nullptr},
    {"15", kMaxZeroSuggestMatches15, base::size(kMaxZeroSuggestMatches15),
     nullptr}};

constexpr FeatureEntry::FeatureParam kOmniboxZeroSuggestCacheDuration15Secs[] =
    {{"ZeroSuggestCacheDurationSec", "15"}};
constexpr FeatureEntry::FeatureParam
    kOmniboxZeroSuggestCacheDuration15SecsCounterfactual[] = {
        {"ZeroSuggestCacheDurationSec", "15"},
        {"ZeroSuggestCacheCounterfactual", "true"}};
constexpr FeatureEntry::FeatureParam kOmniboxZeroSuggestCacheDuration30Secs[] =
    {{"ZeroSuggestCacheDurationSec", "30"}};
constexpr FeatureEntry::FeatureParam
    kOmniboxZeroSuggestCacheDuration30SecsCounterfactual[] = {
        {"ZeroSuggestCacheDurationSec", "30"},
        {"ZeroSuggestCacheCounterfactual", "true"}};
constexpr FeatureEntry::FeatureParam kOmniboxZeroSuggestCacheDuration60Secs[] =
    {{"ZeroSuggestCacheDurationSec", "60"}};
constexpr FeatureEntry::FeatureParam
    kOmniboxZeroSuggestCacheDuration60SecsCounterfactual[] = {
        {"ZeroSuggestCacheDurationSec", "60"},
        {"ZeroSuggestCacheCounterfactual", "true"}};

constexpr FeatureEntry::FeatureVariation
    kOmniboxZeroSuggestPrefetchingVariations[] = {
        {"15 seconds", kOmniboxZeroSuggestCacheDuration15Secs,
         base::size(kOmniboxZeroSuggestCacheDuration15Secs), nullptr},
        {"15 seconds (counterfactual)",
         kOmniboxZeroSuggestCacheDuration15SecsCounterfactual,
         base::size(kOmniboxZeroSuggestCacheDuration15SecsCounterfactual),
         nullptr},
        {"30 seconds", kOmniboxZeroSuggestCacheDuration30Secs,
         base::size(kOmniboxZeroSuggestCacheDuration30Secs), nullptr},
        {"30 seconds (counterfactual)",
         kOmniboxZeroSuggestCacheDuration30SecsCounterfactual,
         base::size(kOmniboxZeroSuggestCacheDuration30SecsCounterfactual),
         nullptr},
        {"60 seconds", kOmniboxZeroSuggestCacheDuration60Secs,
         base::size(kOmniboxZeroSuggestCacheDuration60Secs), nullptr},
        {"60 seconds (counterfactual)",
         kOmniboxZeroSuggestCacheDuration60SecsCounterfactual,
         base::size(kOmniboxZeroSuggestCacheDuration60SecsCounterfactual),
         nullptr}};

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

const FeatureEntry::FeatureParam kOmniboxDynamicMaxAutocomplete90[] = {
    {"OmniboxDynamicMaxAutocompleteUrlCutoff", "0"},
    {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}};
const FeatureEntry::FeatureParam kOmniboxDynamicMaxAutocomplete91[] = {
    {"OmniboxDynamicMaxAutocompleteUrlCutoff", "1"},
    {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}};
const FeatureEntry::FeatureParam kOmniboxDynamicMaxAutocomplete92[] = {
    {"OmniboxDynamicMaxAutocompleteUrlCutoff", "2"},
    {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "9"}};
const FeatureEntry::FeatureParam kOmniboxDynamicMaxAutocomplete100[] = {
    {"OmniboxDynamicMaxAutocompleteUrlCutoff", "0"},
    {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}};
const FeatureEntry::FeatureParam kOmniboxDynamicMaxAutocomplete101[] = {
    {"OmniboxDynamicMaxAutocompleteUrlCutoff", "1"},
    {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}};
const FeatureEntry::FeatureParam kOmniboxDynamicMaxAutocomplete102[] = {
    {"OmniboxDynamicMaxAutocompleteUrlCutoff", "2"},
    {"OmniboxDynamicMaxAutocompleteIncreasedLimit", "10"}};

const FeatureEntry::FeatureVariation
    kOmniboxDynamicMaxAutocompleteVariations[] = {
        {"9 suggestions if 0 or fewer URLs", kOmniboxDynamicMaxAutocomplete90,
         base::size(kOmniboxDynamicMaxAutocomplete90), nullptr},
        {"9 suggestions if 1 or fewer URLs", kOmniboxDynamicMaxAutocomplete91,
         base::size(kOmniboxDynamicMaxAutocomplete91), nullptr},
        {"9 suggestions if 2 or fewer URLs", kOmniboxDynamicMaxAutocomplete92,
         base::size(kOmniboxDynamicMaxAutocomplete92), nullptr},
        {"10 suggestions if 0 or fewer URLs", kOmniboxDynamicMaxAutocomplete100,
         base::size(kOmniboxDynamicMaxAutocomplete100), nullptr},
        {"10 suggestions if 1 or fewer URLs", kOmniboxDynamicMaxAutocomplete101,
         base::size(kOmniboxDynamicMaxAutocomplete101), nullptr},
        {"10 suggestions if 2 or fewer URLs", kOmniboxDynamicMaxAutocomplete102,
         base::size(kOmniboxDynamicMaxAutocomplete102), nullptr}};

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

const FeatureEntry::FeatureParam kAlsoShowMediaTabsinOpenTabsSection[] = {
    {features::kTabSearchAlsoShowMediaTabsinOpenTabsSectionParameterName,
     "true"}};

const FeatureEntry::FeatureVariation kTabSearchMediaTabsVariations[] = {
    {" - media tabs also shown in open tabs",
     kAlsoShowMediaTabsinOpenTabsSection,
     base::size(kAlsoShowMediaTabsinOpenTabsSection), nullptr}};

const FeatureEntry::FeatureParam kTabSearchSearchThresholdSmall[] = {
    {features::kTabSearchSearchThresholdName, "0.3"}};
const FeatureEntry::FeatureParam kTabSearchSearchThresholdMedium[] = {
    {features::kTabSearchSearchThresholdName, "0.6"}};
const FeatureEntry::FeatureParam kTabSearchSearchThresholdLarge[] = {
    {features::kTabSearchSearchThresholdName, "0.8"}};

const FeatureEntry::FeatureVariation kTabSearchSearchThresholdVariations[] = {
    {" - fuzzy level: small", kTabSearchSearchThresholdSmall,
     base::size(kTabSearchSearchThresholdSmall), nullptr},
    {" - fuzzy level: medium", kTabSearchSearchThresholdMedium,
     base::size(kTabSearchSearchThresholdMedium), nullptr},
    {" - fuzzy level: large", kTabSearchSearchThresholdLarge,
     base::size(kTabSearchSearchThresholdLarge), nullptr}};

const FeatureEntry::FeatureParam kTabHoverCardImagesAlternateFormat[] = {
    {features::kTabHoverCardAlternateFormat, "1"}};

const FeatureEntry::FeatureVariation kTabHoverCardImagesVariations[] = {
    {" alternate hover card format", kTabHoverCardImagesAlternateFormat,
     base::size(kTabHoverCardImagesAlternateFormat), nullptr}};

#if !defined(OS_ANDROID)

const FeatureEntry::FeatureParam kNtpChromeCartModuleFakeData[] = {
    {ntp_features::kNtpChromeCartModuleDataParam, "fake"},
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, "true"}};
const FeatureEntry::FeatureParam kNtpChromeCartModuleAbandonedCartDiscount[] = {
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, "true"},
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam,
     "true"},
    {"partner-merchant-pattern",
     "(electronicexpress.com|zazzle.com|wish.com|homesquare.com|iherb.com|"
     "zappos.com|otterbox.com)"}};
const FeatureEntry::FeatureParam kNtpChromeCartModuleHeuristicsImprovement[] = {
    {ntp_features::kNtpChromeCartModuleHeuristicsImprovementParam, "true"}};
const FeatureEntry::FeatureParam kNtpChromeCartModuleRBDAndCouponDiscount[] = {
    {ntp_features::kNtpChromeCartModuleHeuristicsImprovementParam, "true"},
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, "true"},
    {ntp_features::kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam,
     "true"},
    {"partner-merchant-pattern",
     "(electronicexpress.com|zazzle.com|wish.com|homesquare.com)"},
    {ntp_features::kNtpChromeCartModuleCouponParam, "true"}};
const FeatureEntry::FeatureVariation kNtpChromeCartModuleVariations[] = {
    {"- Fake Data And Discount", kNtpChromeCartModuleFakeData,
     base::size(kNtpChromeCartModuleFakeData), nullptr},
    {"- Abandoned Cart Discount", kNtpChromeCartModuleAbandonedCartDiscount,
     base::size(kNtpChromeCartModuleAbandonedCartDiscount), nullptr},
    {"- Heuristics Improvement", kNtpChromeCartModuleHeuristicsImprovement,
     base::size(kNtpChromeCartModuleHeuristicsImprovement), nullptr},
    {"- RBD and Coupons", kNtpChromeCartModuleRBDAndCouponDiscount,
     base::size(kNtpChromeCartModuleRBDAndCouponDiscount), nullptr},
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
     base::size(kNtpShoppingTasksModuleFakeData), nullptr},
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

const FeatureEntry::FeatureParam kNtpPhotosModuleFakeData0[] = {
    {ntp_features::kNtpPhotosModuleDataParam, "0"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleFakeData1[] = {
    {ntp_features::kNtpPhotosModuleDataParam, "1"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleFakeData2[] = {
    {ntp_features::kNtpPhotosModuleDataParam, "2"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleFakeData3[] = {
    {ntp_features::kNtpPhotosModuleDataParam, "3"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleFakeData4[] = {
    {ntp_features::kNtpPhotosModuleDataParam, "4"}};

const FeatureEntry::FeatureVariation kNtpPhotosModuleVariations[] = {
    {" - Fake memories: 0", kNtpPhotosModuleFakeData0,
     base::size(kNtpPhotosModuleFakeData0), nullptr},
    {" - Fake memories: 1", kNtpPhotosModuleFakeData1,
     base::size(kNtpPhotosModuleFakeData1), nullptr},
    {" - Fake memories: 2", kNtpPhotosModuleFakeData2,
     base::size(kNtpPhotosModuleFakeData2), nullptr},
    {" - Fake memories: 3", kNtpPhotosModuleFakeData3,
     base::size(kNtpPhotosModuleFakeData3), nullptr},
    {" - Fake memories: 4", kNtpPhotosModuleFakeData4,
     base::size(kNtpPhotosModuleFakeData4), nullptr}};

const FeatureEntry::FeatureParam kNtpSafeBrowsingModuleFastCooldown[] = {
    {ntp_features::kNtpSafeBrowsingModuleCooldownPeriodDaysParam, "0.001"},
    {ntp_features::kNtpSafeBrowsingModuleCountMaxParam, "1"}};
const FeatureEntry::FeatureVariation kNtpSafeBrowsingModuleVariations[] = {
    {"(Fast Cooldown)", kNtpSafeBrowsingModuleFastCooldown,
     base::size(kNtpSafeBrowsingModuleFastCooldown), nullptr},
};
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishGeo[] = {
    {language::kOverrideModelKey, language::kOverrideModelGeoValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishBackoff[] = {
    {language::kOverrideModelKey, language::kOverrideModelDefaultValue},
    {language::kEnforceRankerKey, "false"},
    {language::kBackoffThresholdKey, "0"}};
const FeatureEntry::FeatureVariation
    kTranslateForceTriggerOnEnglishVariations[] = {
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

const FeatureEntry::FeatureParam kContextualSearchPromoCardShow3Times = {
    "promo_card_max_shown", "3"};
const FeatureEntry::FeatureParam kContextualSearchPromoCardShow100Times = {
    "promo_card_max_shown", "100"};
const FeatureEntry::FeatureVariation ContextualSearchNewSettingsVariations[] = {
    {"with promo show 3 times", &kContextualSearchPromoCardShow3Times, 1,
     nullptr},
    {"with promo show 100 times", &kContextualSearchPromoCardShow100Times, 1,
     nullptr},
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
        {"price_tracking_with_optimization_guide", "true"},
        {"enable_persisted_tab_data_maintenance", "true"}};

const FeatureEntry::FeatureParam
    kTabGridLayoutAndroid_TabGroupAutoCreation_TabGroupFirst[] = {
        {"enable_tab_group_auto_creation", "false"},
        {"show_open_in_tab_group_menu_item_first", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_TabGroupAutoCreation[] =
    {{"enable_tab_group_auto_creation", "false"},
     {"show_open_in_tab_group_menu_item_first", "false"}};

const FeatureEntry::FeatureParam kCommercePriceTracking_PriceNotifications[] = {
    {"enable_price_tracking", "true"},
    {"price_tracking_with_optimization_guide", "true"},
    {"enable_persisted_tab_data_maintenance", "true"},
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
    {"Without auto group-group first",
     kTabGridLayoutAndroid_TabGroupAutoCreation_TabGroupFirst,
     base::size(kTabGridLayoutAndroid_TabGroupAutoCreation_TabGroupFirst),
     nullptr},
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
    {"tab_count_button_on_start_surface", "true"},
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
        {"tab_count_button_on_start_surface", "true"},
        {"new_home_surface_from_home_button", "hide_mv_tiles_and_tab_switcher"},
        {"hide_switch_when_no_incognito_tabs", "true"},
        {"show_tabs_in_mru_order", "true"},
        {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"open_ntp_instead_of_start", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2Finale[] =
    {{"start_surface_variation", "single"},
     {"show_last_active_tab_only", "true"},
     {"omnibox_focused_on_new_tab", "true"},
     {"home_button_on_grid_tab_switcher", "true"},
     {"tab_count_button_on_start_surface", "true"},
     {"new_home_surface_from_home_button", "hide_tab_switcher_only"},
     {"hide_switch_when_no_incognito_tabs", "true"},
     {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_SingleSurface_V2Finale_NTPTilesOnOmnibox[] = {
        {"start_surface_variation", "single"},
        {"show_last_active_tab_only", "true"},
        {"omnibox_focused_on_new_tab", "true"},
        {"show_ntp_tiles_on_omnibox", "true"},
        {"home_button_on_grid_tab_switcher", "true"},
        {"tab_count_button_on_start_surface", "true"},
        {"hide_switch_when_no_incognito_tabs", "true"},
        {"new_home_surface_from_home_button", "hide_mv_tiles_and_tab_switcher"},
        {"enable_tab_groups_continuation", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurfaceSingleTab[] =
    {{"start_surface_variation", "single"},
     {"show_last_active_tab_only", "true"},
     {"hide_switch_when_no_incognito_tabs", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_CandidateA[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"hide_switch_when_no_incognito_tabs", "true"},
    {"tab_count_button_on_start_surface", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_CandidateA_SyncCheck[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"hide_switch_when_no_incognito_tabs", "true"},
    {"tab_count_button_on_start_surface", "true"},
    {"check_sync_before_show_start_at_startup", "true"}};

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_CandidateA_SigninPromoTimeLimit[] = {
        {"start_surface_variation", "single"},
        {"show_last_active_tab_only", "true"},
        {"hide_switch_when_no_incognito_tabs", "true"},
        {"tab_count_button_on_start_surface", "true"},
        {"sign_in_promo_show_since_last_background_limit_ms", "30000"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_CandidateB[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"hide_switch_when_no_incognito_tabs", "true"},
    {"tab_count_button_on_start_surface", "true"},
    {"open_ntp_instead_of_start", "true"}};

const FeatureEntry::FeatureVariation kStartSurfaceAndroidVariations[] = {
    {"Canidate A", kStartSurfaceAndroid_CandidateA,
     base::size(kStartSurfaceAndroid_CandidateA), nullptr},
    {"Canidate A + Sync check", kStartSurfaceAndroid_CandidateA_SyncCheck,
     base::size(kStartSurfaceAndroid_CandidateA_SyncCheck), nullptr},
    {"Canidate A + Sign in promo backgrounded time limit",
     kStartSurfaceAndroid_CandidateA_SigninPromoTimeLimit,
     base::size(kStartSurfaceAndroid_CandidateA_SigninPromoTimeLimit), nullptr},
    {"Canidate B", kStartSurfaceAndroid_CandidateB,
     base::size(kStartSurfaceAndroid_CandidateB), nullptr},
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
    {"intro_style", "accelerator"}};

const FeatureEntry::FeatureParam kWebFeed_IPH[] = {{"intro_style", "IPH"}};

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

const FeatureEntry::FeatureParam kDynamicColorFull[] = {
    {"dynamic_color_full", "true"}};

const FeatureEntry::FeatureVariation kDynamicColorAndroidVariations[] = {
    {"(Full)", kDynamicColorFull, base::size(kDynamicColorFull), nullptr},
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

const FeatureEntry::FeatureParam kAssistantConsentV2_reprompts_counter[] = {
    {"count", "3"}};

const FeatureEntry::FeatureVariation kAssistantConsentV2_Variations[] = {
    {"Limited Re-prompts", kAssistantConsentV2_reprompts_counter,
     base::size(kAssistantConsentV2_reprompts_counter), nullptr},
};

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

const FeatureEntry::FeatureParam kDrawPredictedPointExperiment1Point12Ms[] = {
    {"predicted_points", features::kDraw1Point12Ms}};
const FeatureEntry::FeatureParam kDrawPredictedPointExperiment2Points6Ms[] = {
    {"predicted_points", features::kDraw2Points6Ms}};
const FeatureEntry::FeatureParam kDrawPredictedPointExperiment1Point6Ms[] = {
    {"predicted_points", features::kDraw1Point6Ms}};
const FeatureEntry::FeatureParam kDrawPredictedPointExperiment2Points3Ms[] = {
    {"predicted_points", features::kDraw2Points3Ms}};

const FeatureEntry::FeatureVariation kDrawPredictedPointVariations[] = {
    {flag_descriptions::kDraw1PredictedPoint12Ms,
     kDrawPredictedPointExperiment1Point12Ms,
     base::size(kDrawPredictedPointExperiment1Point12Ms), nullptr},
    {flag_descriptions::kDraw2PredictedPoints6Ms,
     kDrawPredictedPointExperiment2Points6Ms,
     base::size(kDrawPredictedPointExperiment2Points6Ms), nullptr},
    {flag_descriptions::kDraw1PredictedPoint6Ms,
     kDrawPredictedPointExperiment1Point6Ms,
     base::size(kDrawPredictedPointExperiment1Point6Ms), nullptr},
    {flag_descriptions::kDraw2PredictedPoints3Ms,
     kDrawPredictedPointExperiment2Points3Ms,
     base::size(kDrawPredictedPointExperiment2Points3Ms), nullptr}};

const FeatureEntry::FeatureParam kFedCmVariationInterception[] = {
    {features::kFedCmInterceptionFieldTrialParamName, "true"}};
const FeatureEntry::FeatureVariation kFedCmFeatureVariations[] = {
    {"- with FedCM HTTP filtering (very experimental)",
     kFedCmVariationInterception, base::size(kFedCmVariationInterception),
     nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kForceControlFaceAeChoices[] = {
    {"Default", "", ""},
    {"Enable", media::switches::kForceControlFaceAe, "enable"},
    {"Disable", media::switches::kForceControlFaceAe, "disable"}};

const FeatureEntry::Choice kHdrNetOverrideChoices[] = {
    {"Default", "", ""},
    {"Force enabled", media::switches::kHdrNetOverride,
     media::switches::kHdrNetForceEnabled},
    {"Force disabled", media::switches::kHdrNetOverride,
     media::switches::kHdrNetForceDisabled}};

const FeatureEntry::Choice kAutoFramingOverrideChoices[] = {
    {"Default", "", ""},
    {"Force enabled", media::switches::kAutoFramingOverride,
     media::switches::kAutoFramingForceEnabled},
    {"Force disabled", media::switches::kAutoFramingOverride,
     media::switches::kAutoFramingForceDisabled}};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kCrostiniContainerChoices[] = {
    {"Default", "", ""},
    {"Buster", crostini::kCrostiniContainerFlag, "buster"},
    {"Bullseye", crostini::kCrostiniContainerFlag, "bullseye"},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if defined(OS_ANDROID)
// The variations of ContentLanguagesInLanguagePicker.
const FeatureEntry::FeatureParam
    kContentLanguagesInLanguagePickerDisableObservers[] = {
        {language::kContentLanguagesDisableObserversParam, "true"}};

const FeatureEntry::FeatureVariation
    kContentLanguagesInLanguaePickerVariations[] = {
        {"Without observers", kContentLanguagesInLanguagePickerDisableObservers,
         base::size(kContentLanguagesInLanguagePickerDisableObservers),
         nullptr},
};
#endif  // defined(OS_ANDROID)

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
const FeatureEntry::FeatureParam kCategoricalSearch_Unranked[] = {
    {"ranking", "none"}};

const FeatureEntry::FeatureParam kCategoricalSearch_ByItem[] = {
    {"ranking", "item"}};

const FeatureEntry::FeatureParam kCategoricalSearch_ByUsage[] = {
    {"ranking", "usage"}};

const FeatureEntry::FeatureVariation kCategoricalSearchVariations[] = {
    {"Unranked", kCategoricalSearch_Unranked,
     base::size(kCategoricalSearch_Unranked), nullptr},
    {"By item", kCategoricalSearch_ByItem,
     base::size(kCategoricalSearch_ByItem), nullptr},
    {"By usage", kCategoricalSearch_ByUsage,
     base::size(kCategoricalSearch_ByUsage), nullptr}};

const FeatureEntry::FeatureParam kQuerySearchBurnInPeriod_50ms[] = {
    {"burnin_period", "50"}};
const FeatureEntry::FeatureParam kQuerySearchBurnInPeriod_100ms[] = {
    {"burnin_period", "100"}};
const FeatureEntry::FeatureParam kQuerySearchBurnInPeriod_150ms[] = {
    {"burnin_period", "150"}};
const FeatureEntry::FeatureParam kQuerySearchBurnInPeriod_500ms[] = {
    {"burnin_period", "500"}};
const FeatureEntry::FeatureParam kQuerySearchBurnInPeriod_2000ms[] = {
    {"burnin_period", "2000"}};

const FeatureEntry::FeatureVariation kQuerySearchBurnInPeriodVariations[] = {
    {"50ms", kQuerySearchBurnInPeriod_50ms,
     base::size(kQuerySearchBurnInPeriod_50ms), nullptr},
    {"100ms", kQuerySearchBurnInPeriod_100ms,
     base::size(kQuerySearchBurnInPeriod_100ms), nullptr},
    {"150ms", kQuerySearchBurnInPeriod_150ms,
     base::size(kQuerySearchBurnInPeriod_150ms), nullptr},
    {"500ms", kQuerySearchBurnInPeriod_500ms,
     base::size(kQuerySearchBurnInPeriod_500ms), nullptr},
    {"2000ms", kQuerySearchBurnInPeriod_2000ms,
     base::size(kQuerySearchBurnInPeriod_2000ms), nullptr}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

constexpr FeatureEntry::FeatureParam kPlatformProvidedTrustTokenIssuance[] = {
    {"PlatformProvidedTrustTokenIssuance", "true"}};

constexpr FeatureEntry::FeatureVariation
    kPlatformProvidedTrustTokensVariations[] = {
        {"with platform-provided trust token issuance",
         kPlatformProvidedTrustTokenIssuance,
         base::size(kPlatformProvidedTrustTokenIssuance), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kPersonalizationHubInternalName[] = "personalization-hub";
constexpr char kWallpaperFullScreenPreviewInternalName[] =
    "wallpaper-fullscreen-preview";
constexpr char kWallpaperPerDeskName[] = "per-desk-wallpaper";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
const FeatureEntry::FeatureParam kPaintPreviewStartupWithAccessibility[] = {
    {"has_accessibility_support", "true"}};

const FeatureEntry::FeatureVariation kPaintPreviewStartupVariations[] = {
    {"with accessibility support", kPaintPreviewStartupWithAccessibility,
     base::size(kPaintPreviewStartupWithAccessibility), nullptr}};
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kBorealisBigGlInternalName[] = "borealis-big-gl";
constexpr char kBorealisDiskManagementInternalName[] =
    "borealis-disk-management";
constexpr char kBorealisForceBetaClientInternalName[] =
    "borealis-force-beta-client";
constexpr char kBorealisLinuxModeInternalName[] = "borealis-linux-mode";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
// The variations of Continuous Search.
const FeatureEntry::FeatureParam kContinuousSearchAfterSecondSrp[] = {
    {"trigger_mode", "1"}};

const FeatureEntry::FeatureParam kContinuousSearchOnReverseScroll[] = {
    {"trigger_mode", "2"}};

const FeatureEntry::FeatureParam kContinuousSearchPermanentDismissal[] = {
    {"permanent_dismissal_threshold", "3"}};

const FeatureEntry::FeatureParam kContinuousSearchDoubleRowChip[] = {
    {"show_result_title", "true"}};

const FeatureEntry::FeatureVariation kContinuousSearchFeatureVariations[] = {
    {"show after second SRP", kContinuousSearchAfterSecondSrp,
     base::size(kContinuousSearchAfterSecondSrp), nullptr},
    {"show on reverse scroll", kContinuousSearchOnReverseScroll,
     base::size(kContinuousSearchOnReverseScroll), nullptr},
    {"with permanent dismissal", kContinuousSearchPermanentDismissal,
     base::size(kContinuousSearchPermanentDismissal), nullptr},
    {"with double-row chips", kContinuousSearchDoubleRowChip,
     base::size(kContinuousSearchDoubleRowChip), nullptr}};

const FeatureEntry::FeatureParam kReadLaterUseRootBookmarkAsDefault[] = {
    {"use_root_bookmark_as_default", "true"}};
const FeatureEntry::FeatureParam kReadLaterInAppMenu[] = {
    {"use_root_bookmark_as_default", "true"},
    {"reading_list_in_app_menu", "true"},
    {"allow_bookmark_type_swapping", "true"}};
const FeatureEntry::FeatureParam kReadLaterSemiIntegrated[] = {
    {"use_root_bookmark_as_default", "true"},
    {"allow_bookmark_type_swapping", "true"}};
const FeatureEntry::FeatureParam kReadLaterNoCustomTab[] = {
    {"use_root_bookmark_as_default", "true"},
    {"use_cct", "false"}};

const FeatureEntry::FeatureVariation kReadLaterVariations[] = {
    {"(use root bookmark as default)", kReadLaterUseRootBookmarkAsDefault,
     base::size(kReadLaterUseRootBookmarkAsDefault), nullptr},
    {"(with app menu item)", kReadLaterInAppMenu,
     base::size(kReadLaterInAppMenu), nullptr},
    {"(bookmarks semi-integration)", kReadLaterSemiIntegrated,
     base::size(kReadLaterSemiIntegrated), nullptr},
    {"(no custom tab)", kReadLaterNoCustomTab,
     base::size(kReadLaterNoCustomTab), nullptr}};

const FeatureEntry::FeatureParam kBookmarksRefreshVisuals[] = {
    {"bookmark_visuals_enabled", "true"}};
const FeatureEntry::FeatureParam kBookmarksRefreshAppMenu[] = {
    {"bookmark_in_app_menu", "true"}};
const FeatureEntry::FeatureParam kBookmarksRefreshWithEverything[] = {
    {"bookmark_visuals_enabled", "true"},
    {"bookmark_in_app_menu", "true"}};

const FeatureEntry::FeatureVariation kBookmarksRefreshVariations[] = {
    {"(manager visuals only)", kBookmarksRefreshVisuals,
     base::size(kBookmarksRefreshVisuals), nullptr},
    {"(app menu item only)", kBookmarksRefreshAppMenu,
     base::size(kBookmarksRefreshAppMenu), nullptr},
    {"(everything)", kBookmarksRefreshWithEverything,
     base::size(kBookmarksRefreshWithEverything), nullptr}};

const FeatureEntry::FeatureParam kScrollCaptureInMemory[] = {
    {"in_memory_capture", "true"}};

const FeatureEntry::FeatureVariation kScrollCaptureVariations[] = {
    {"(in memory capture)", kScrollCaptureInMemory,
     base::size(kScrollCaptureInMemory), nullptr}};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kLargeFaviconFromGoogle96[] = {
    {"favicon_size_in_dip", "96"}};
const FeatureEntry::FeatureParam kLargeFaviconFromGoogle128[] = {
    {"favicon_size_in_dip", "128"}};

const FeatureEntry::FeatureVariation kLargeFaviconFromGoogleVariations[] = {
    {"(96dip)", kLargeFaviconFromGoogle96,
     base::size(kLargeFaviconFromGoogle96), nullptr},
    {"(128dip)", kLargeFaviconFromGoogle128,
     base::size(kLargeFaviconFromGoogle128), nullptr}};

const FeatureEntry::Choice kDocumentTransitionSlowdownFactorChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"5", switches::kDocumentTransitionSlowdownFactor, "5"},
    {"10", switches::kDocumentTransitionSlowdownFactor, "10"},
    {"20", switches::kDocumentTransitionSlowdownFactor, "20"},
    {"50", switches::kDocumentTransitionSlowdownFactor, "50"}};

#if defined(OS_WIN)
const FeatureEntry::FeatureParam kWin11StyleMenusAllWindowsVersions[] = {
    {features::kWin11StyleMenuAllWindowsVersionsName, "true"}};

const FeatureEntry::FeatureVariation kWin11StyleMenusVariations[] = {
    {" - All Windows Versions", kWin11StyleMenusAllWindowsVersions,
     base::size(kWin11StyleMenusAllWindowsVersions), nullptr},
};
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Possible configurations for the snooping protection feature.
// Empty params configures the feature to apply a simple threshold to one
// sample.

const FeatureEntry::FeatureParam kSnoopingProtectionPrecision[] = {
    {"filter_config_case", "2"},
    {"count", "2"},
    {"threshold", "40"},
    {"initial_state", "false"}};

const FeatureEntry::FeatureParam kSnoopingProtectionBalance[] = {
    {"filter_config_case", "2"},
    {"count", "2"},
    {"threshold", "0"},
    {"initial_state", "false"}};

const FeatureEntry::FeatureParam kSnoopingProtectionRecall[] = {
    {"filter_config_case", "2"},
    {"count", "2"},
    {"threshold", "-40"},
    {"initial_state", "false"}};

const FeatureEntry::FeatureVariation kSnoopingProtectionVariations[] = {
    {"Slow Precise", kSnoopingProtectionPrecision,
     base::size(kSnoopingProtectionPrecision), nullptr},
    {"Slow Balanced", kSnoopingProtectionBalance,
     base::size(kSnoopingProtectionBalance), nullptr},
    {"Slow Comprehensive", kSnoopingProtectionRecall,
     base::size(kSnoopingProtectionRecall), nullptr}};

const FeatureEntry::FeatureParam kQuickDim120s[] = {{"quick_dim_ms", "120000"}};

const FeatureEntry::FeatureParam kQuickDim45s[] = {{"quick_dim_ms", "45000"}};

const FeatureEntry::FeatureParam kQuickDim10s[] = {{"quick_dim_ms", "10000"}};

const FeatureEntry::FeatureParam kQuickDimInstantly[] = {
    {"quick_dim_ms", "1000"}};

const FeatureEntry::FeatureVariation kQuickDimVariations[] = {
    {"QuickDim120s", kQuickDim120s, base::size(kQuickDim120s), nullptr},
    {"QuickDim45s", kQuickDim45s, base::size(kQuickDim45s), nullptr},
    {"QuickDim10s", kQuickDim10s, base::size(kQuickDim10s), nullptr},
    {"QuickDimInstantly", kQuickDimInstantly, base::size(kQuickDimInstantly),
     nullptr}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kMaxOverlays1 = {features::kMaxOverlaysParam,
                                                  "1"};
const FeatureEntry::FeatureParam kMaxOverlays2 = {features::kMaxOverlaysParam,
                                                  "2"};
const FeatureEntry::FeatureParam kMaxOverlays3 = {features::kMaxOverlaysParam,
                                                  "3"};
const FeatureEntry::FeatureParam kMaxOverlays4 = {features::kMaxOverlaysParam,
                                                  "4"};
const FeatureEntry::FeatureParam kMaxOverlays5 = {features::kMaxOverlaysParam,
                                                  "5"};
const FeatureEntry::FeatureParam kMaxOverlays6 = {features::kMaxOverlaysParam,
                                                  "6"};

const FeatureEntry::FeatureVariation kUseMultipleOverlaysVariations[] = {
    {"1", &kMaxOverlays1, 1, nullptr}, {"2", &kMaxOverlays2, 1, nullptr},
    {"3", &kMaxOverlays3, 1, nullptr}, {"4", &kMaxOverlays4, 1, nullptr},
    {"5", &kMaxOverlays5, 1, nullptr}, {"6", &kMaxOverlays6, 1, nullptr}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#if defined(WEBRTC_USE_PIPEWIRE)
    {"enable-webrtc-pipewire-capturer",
     flag_descriptions::kWebrtcPipeWireCapturerName,
     flag_descriptions::kWebrtcPipeWireCapturerDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebRtcPipeWireCapturer)},
#endif  // defined(WEBRTC_USE_PIPEWIRE)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-lacros-in-web-kiosk", flag_descriptions::kWebKioskEnableLacrosName,
     flag_descriptions::kWebKioskEnableLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebKioskEnableLacros)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
    {"verbose-logging-in-nacl", flag_descriptions::kVerboseLoggingInNaclName,
     flag_descriptions::kVerboseLoggingInNaclDescription, kOsAll,
     MULTI_VALUE_TYPE(kVerboseLoggingInNaclChoices)},
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
    {"contextual-search-delayed-intelligence",
     flag_descriptions::kContextualSearchDelayedIntelligenceName,
     flag_descriptions::kContextualSearchDelayedIntelligenceDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDelayedIntelligence)},
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
    {"contextual-search-new-settings",
     flag_descriptions::KContextualSearchNewSettingsName,
     flag_descriptions::KContextualSearchNewSettingsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::KContextualSearchNewSettings,
         ContextualSearchNewSettingsVariations,
         "ContextualSearchNewSettings")},
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
    {"contextual-triggers-selection-handles",
     flag_descriptions::kContextualTriggersSelectionHandlesName,
     flag_descriptions::kContextualTriggersSelectionHandlesDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualTriggersSelectionHandles)},
    {"contextual-triggers-selection-menu",
     flag_descriptions::kContextualTriggersSelectionMenuName,
     flag_descriptions::kContextualTriggersSelectionMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualTriggersSelectionMenu)},
    {"contextual-triggers-selection-size",
     flag_descriptions::kContextualTriggersSelectionSizeName,
     flag_descriptions::kContextualTriggersSelectionSizeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualTriggersSelectionSize)},
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
#endif  // OS_ANDROID
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillShowTypePredictions)},
    {"autofill-center-aligned-suggestions",
     flag_descriptions::kAutofillCenterAligngedSuggestionsName,
     flag_descriptions::kAutofillCenterAligngedSuggestionsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCenterAlignedSuggestions)},
    {"autofill-visual-improvements-for-suggestion-ui",
     flag_descriptions::kAutofillVisualImprovementsForSuggestionUiName,
     flag_descriptions::kAutofillVisualImprovementsForSuggestionUiDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillVisualImprovementsForSuggestionUi)},
    {"autofill-type-specific-popup-width",
     flag_descriptions::kAutofillTypeSpecificPopupWidthName,
     flag_descriptions::kAutofillTypeSpecificPopupWidthDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillTypeSpecificPopupWidth)},
    {"autofill-use-consistent-popup-settings-icons",
     flag_descriptions::kAutofillUseConsistentPopupSettingsIconsName,
     flag_descriptions::kAutofillUseConsistentPopupSettingsIconsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseConsistentPopupSettingsIcons)},
    {"smooth-scrolling", flag_descriptions::kSmoothScrollingName,
     flag_descriptions::kSmoothScrollingDescription,
     // Mac has a separate implementation with its own setting to disable.
     kOsLinux | kOsCrOS | kOsWin | kOsAndroid | kOsFuchsia,
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
    {"enable-webassembly-tiering", flag_descriptions::kEnableWasmTieringName,
     flag_descriptions::kEnableWasmTieringDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyTiering)},
    {"enable-future-v8-vm-features", flag_descriptions::kV8VmFutureName,
     flag_descriptions::kV8VmFutureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8VmFuture)},
    {"enable-gpu-rasterization", flag_descriptions::kGpuRasterizationName,
     flag_descriptions::kGpuRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"enable-oop-rasterization-ddl",
     flag_descriptions::kOopRasterizationDDLName,
     flag_descriptions::kOopRasterizationDDLDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kOopRasterizationDDL)},
    {"enable-experimental-web-platform-features",
     flag_descriptions::kExperimentalWebPlatformFeaturesName,
     flag_descriptions::kExperimentalWebPlatformFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
    {"top-chrome-touch-ui", flag_descriptions::kTopChromeTouchUiName,
     flag_descriptions::kTopChromeTouchUiDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTopChromeTouchUiChoices)},
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
    {flag_descriptions::kWebUITabStripFlagId,
     flag_descriptions::kWebUITabStripName,
     flag_descriptions::kWebUITabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStrip)},

    {"webui-tab-strip-context-menu-after-tap",
     flag_descriptions::kWebUITabStripContextMenuAfterTapName,
     flag_descriptions::kWebUITabStripContextMenuAfterTapDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStripContextMenuAfterTap)},
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
    {"vertical-snap", flag_descriptions::kVerticalSnapName,
     flag_descriptions::kVerticalSnapDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::wm::features::kVerticalSnap)},
    {"ash-advanced-screen-capture-settings",
     flag_descriptions::kImprovedScreenCaptureSettingsName,
     flag_descriptions::kImprovedScreenCaptureSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImprovedScreenCaptureSettings)},
    {"ash-bento-bar", flag_descriptions::kBentoBarName,
     flag_descriptions::kBentoBarDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBentoBar)},
    {"ash-drag-window-to-new-desk", flag_descriptions::kDragWindowToNewDeskName,
     flag_descriptions::kDragWindowToNewDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragWindowToNewDesk)},
    {"ash-overview-button", flag_descriptions::kOverviewButtonName,
     flag_descriptions::kOverviewButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOverviewButton)},
    {"ash-window-follow-cursor-multi-display",
     flag_descriptions::kWindowsFollowCursorName,
     flag_descriptions::kWindowsFollowCursorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWindowsFollowCursor)},
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
    {"bluetooth-use-floss", flag_descriptions::kBluetoothUseFlossName,
     flag_descriptions::kBluetoothUseFlossDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(floss::features::kFlossEnabled)},
    {"button-arc-network-diagnostics",
     flag_descriptions::kButtonARCNetworkDiagnosticsName,
     flag_descriptions::kButtonARCNetworkDiagnosticsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kButtonARCNetworkDiagnostics)},
    {"calendar-view", flag_descriptions::kCalendarViewName,
     flag_descriptions::kCalendarViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCalendarView)},
    {"cellular-bypass-esim-installation-connectivity-check",
     flag_descriptions::kCellularBypassESimInstallationConnectivityCheckName,
     flag_descriptions::
         kCellularBypassESimInstallationConnectivityCheckDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kCellularBypassESimInstallationConnectivityCheck)},
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
    {"cryptauth-v2-dedup-device-last-activity-time",
     flag_descriptions::kCryptAuthV2DedupDeviceLastActivityTimeName,
     flag_descriptions::kCryptAuthV2DedupDeviceLastActivityTimeDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kCryptAuthV2DedupDeviceLastActivityTime)},
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
    {"quick-settings-network-revamp",
     flag_descriptions::kQuickSettingsNetworkRevampName,
     flag_descriptions::kQuickSettingsNetworkRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickSettingsNetworkRevamp)},
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
    {"enable-default-calculator-web-app",
     flag_descriptions::kDefaultCalculatorWebAppName,
     flag_descriptions::kDefaultCalculatorWebAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(web_app::kDefaultCalculatorWebApp)},
    {"enable-notifications-revamp", flag_descriptions::kNotificationsRevampName,
     flag_descriptions::kNotificationsRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNotificationsRefresh)},
    {kLacrosSupportInternalName, flag_descriptions::kLacrosSupportName,
     flag_descriptions::kLacrosSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosSupport)},
    {kLacrosStabilityInternalName, flag_descriptions::kLacrosStabilityName,
     flag_descriptions::kLacrosStabilityDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosStabilityChoices)},
    {"lacros-profile-migration-for-any-user",
     flag_descriptions::kLacrosProfileMigrationForAnyUserName,
     flag_descriptions::kLacrosProfileMigrationForAnyUserDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosProfileMigrationForAnyUser)},
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
    {"enable-wireguard", flag_descriptions::kEnableWireGuardName,
     flag_descriptions::kEnableWireGuardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableWireGuard)},
    {"esim-policy", flag_descriptions::kESimPolicyName,
     flag_descriptions::kESimPolicyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kESimPolicy)},
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
    {"improved-desks-keyboard-shortcuts",
     flag_descriptions::kImprovedDesksKeyboardShortcutsName,
     flag_descriptions::kImprovedDesksKeyboardShortcutsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImprovedDesksKeyboardShortcuts)},
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
    {"show-feedback-report-questionnaire",
     flag_descriptions::kShowFeedbackReportQuestionnaireName,
     flag_descriptions::kShowFeedbackReportQuestionnaireDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShowFeedbackReportQuestionnaire)},
    {"wifi-connect-mac-address-randomization",
     flag_descriptions::kWifiConnectMacAddressRandomizationName,
     flag_descriptions::kWifiConnectMacAddressRandomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kWifiConnectMacAddressRandomization)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS)
    {"dark-light-mode", flag_descriptions::kDarkLightTestName,
     flag_descriptions::kDarkLightTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDarkLightMode)},
    // TODO(b/180051795): remove kOsLinux when lacros-chrome switches to
    // kOsCrOS.
    {"deprecate-low-usage-codecs",
     flag_descriptions::kDeprecateLowUsageCodecsName,
     flag_descriptions::kDeprecateLowUsageCodecsDescription, kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(media::kDeprecateLowUsageCodecs)},
    {"enable-tts-lacros-support",
     flag_descriptions::kEnableTtsLacrosSupportName,
     flag_descriptions::kEnableTtsLacrosSupportDescription, kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(chromeos::kLacrosTtsSupport)},
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
        kOsMac | kOsWin | kOsCrOS | kOsAndroid | kOsLinux | kOsFuchsia,
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
#if defined(OS_WIN)
    {
        "enable-hardware-secure-decryption",
        flag_descriptions::kHardwareSecureDecryptionName,
        flag_descriptions::kHardwareSecureDecryptionDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kHardwareSecureDecryption),
    },
    {
        "enable-hardware-secure-decryption-experiment",
        flag_descriptions::kHardwareSecureDecryptionExperimentName,
        flag_descriptions::kHardwareSecureDecryptionExperimentDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kHardwareSecureDecryptionExperiment),
    },
    {
        "enable-media-foundation-clear",
        flag_descriptions::kMediaFoundationClearName,
        flag_descriptions::kMediaFoundationClearDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kMediaFoundationClearPlayback),
    },
#endif
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
    {"username-first-flow", flag_descriptions::kUsernameFirstFlowName,
     flag_descriptions::kUsernameFirstFlowDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kUsernameFirstFlow)},
    {"username-first-flow-fallback-crowdsourcing",
     flag_descriptions::kUsernameFirstFlowFallbackCrowdsourcingName,
     flag_descriptions::kUsernameFirstFlowFallbackCrowdsourcingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kUsernameFirstFlowFallbackCrowdsourcing)},
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
    {"enable-webusb-device-detection",
     flag_descriptions::kWebUsbDeviceDetectionName,
     flag_descriptions::kWebUsbDeviceDetectionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUsbDeviceDetection)},
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
         chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2,
         kAdaptiveButtonInTopToolbarCustomizationVariations,
         "OptionalToolbarButtonCustomization")},
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
    {"voice-button-in-top-toolbar",
     flag_descriptions::kVoiceButtonInTopToolbarName,
     flag_descriptions::kVoiceButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kVoiceButtonInTopToolbar)},
    {"assistant-consent-simplified-text",
     flag_descriptions::kAssistantConsentSimplifiedTextName,
     flag_descriptions::kAssistantConsentSimplifiedTextDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantConsentSimplifiedText)},
    {"assistant-consent-v2", flag_descriptions::kAssistantConsentV2Name,
     flag_descriptions::kAssistantConsentV2Description, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAssistantConsentV2,
                                    kAssistantConsentV2_Variations,
                                    "AssistantConsentV2")},
    {"assistant-intent-page-url",
     flag_descriptions::kAssistantIntentPageUrlName,
     flag_descriptions::kAssistantIntentPageUrlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantIntentPageUrl)},
    {"assistant-intent-translate-info",
     flag_descriptions::kAssistantIntentTranslateInfoName,
     flag_descriptions::kAssistantIntentTranslateInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantIntentTranslateInfo)},
    {"share-button-in-top-toolbar",
     flag_descriptions::kShareButtonInTopToolbarName,
     flag_descriptions::kShareButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShareButtonInTopToolbar)},
    {"chrome-share-long-screenshot",
     flag_descriptions::kChromeShareLongScreenshotName,
     flag_descriptions::kChromeShareLongScreenshotDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kChromeShareLongScreenshot,
                                    kLongScreenshotVariations,
                                    "ChromeShareLongScreenshot")},

    {"chrome-sharing-hub-launch-adjacent",
     flag_descriptions::kChromeSharingHubLaunchAdjacentName,
     flag_descriptions::kChromeSharingHubLaunchAdjacentDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeSharingHubLaunchAdjacent)},
    {"persist-share-hub-on-app-switch",
     flag_descriptions::kPersistShareHubOnAppSwitchName,
     flag_descriptions::kPersistShareHubOnAppSwitchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(share::kPersistShareHubOnAppSwitch)},
    {"webnotes-publish", flag_descriptions::kWebNotesPublishName,
     flag_descriptions::kWebNotesPublishDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(content_creation::kWebNotesPublish)},
    {"lightweight-reactions-android",
     flag_descriptions::kLightweightReactionsAndroidName,
     flag_descriptions::kLightweightReactionsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(content_creation::kLightweightReactions)},
#endif  // OS_ANDROID
    {"enable-automatic-snooze", flag_descriptions::kEnableAutomaticSnoozeName,
     flag_descriptions::kEnableAutomaticSnoozeDescription, kOsAll,
     FEATURE_VALUE_TYPE(feature_engagement::kEnableAutomaticSnooze)},
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeChoiceName,
     flag_descriptions::kInProductHelpDemoModeChoiceDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"in-product-help-snooze", flag_descriptions::kInProductHelpSnoozeName,
     flag_descriptions::kInProductHelpSnoozeDescription, kOsAll,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHSnooze)},
    {"in-product-help-use-client-config",
     flag_descriptions::kInProductHelpUseClientConfigName,
     flag_descriptions::kInProductHelpUseClientConfigDescription, kOsAll,
     FEATURE_VALUE_TYPE(feature_engagement::kUseClientConfigIPH)},
    {"disable-threaded-scrolling", flag_descriptions::kThreadedScrollingName,
     flag_descriptions::kThreadedScrollingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(blink::switches::kDisableThreadedScrolling)},
    {"extension-content-verification",
     flag_descriptions::kExtensionContentVerificationName,
     flag_descriptions::kExtensionContentVerificationDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
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
    {"system-extensions", flag_descriptions::kSystemExtensionsName,
     flag_descriptions::kSystemExtensionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSystemExtensions)},
    {"crostini-bullseye-upgrade",
     flag_descriptions::kCrostiniBullseyeUpgradeName,
     flag_descriptions::kCrostiniBullseyeUpgradeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniBullseyeUpgrade)},
    {"crostini-use-dlc", flag_descriptions::kCrostiniUseDlcName,
     flag_descriptions::kCrostiniUseDlcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseDlc)},
    {"crostini-reset-lxd-db", flag_descriptions::kCrostiniResetLxdDbName,
     flag_descriptions::kCrostiniResetLxdDbDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniResetLxdDb)},
    {"terminal-ssh", flag_descriptions::kTerminalSSHName,
     flag_descriptions::kTerminalSSHDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kTerminalSSH)},
    {"terminal-tmux-integration",
     flag_descriptions::kTerminalTmuxIntegrationName,
     flag_descriptions::kTerminalTmuxIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kTerminalTmuxIntegration)},
    {"crostini-use-lxd-4", flag_descriptions::kCrostiniUseLxd4Name,
     flag_descriptions::kCrostiniUseLxd4Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseLxd4)},
    {"crostini-multi-container", flag_descriptions::kCrostiniMultiContainerName,
     flag_descriptions::kCrostiniMultiContainerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniMultiContainer)},
    {"crostini-ime-support", flag_descriptions::kCrostiniImeSupportName,
     flag_descriptions::kCrostiniImeSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniImeSupport)},
    {"crostini-virtual-keyboard-support",
     flag_descriptions::kCrostiniVirtualKeyboardSupportName,
     flag_descriptions::kCrostiniVirtualKeyboardSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniVirtualKeyboardSupport)},
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
#endif
    {"isolate-origins", flag_descriptions::kIsolateOriginsName,
     flag_descriptions::kIsolateOriginsDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kIsolateOrigins, "")},
    {"restricted-api-origins", flag_descriptions::kRestrictedApiOriginsName,
     flag_descriptions::kRestrictedApiOriginsDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kRestrictedApiOrigins, "")},
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
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    {"enable-desktop-pwas-prefix-app-name-in-window-title",
     flag_descriptions::kDesktopPWAsPrefixAppNameInWindowTitleName,
     flag_descriptions::kDesktopPWAsPrefixAppNameInWindowTitleDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(features::kPrefixWebAppWindowsWithAppName)},
    {"enable-desktop-pwas-remove-status-bar",
     flag_descriptions::kDesktopPWAsRemoveStatusBarName,
     flag_descriptions::kDesktopPWAsRemoveStatusBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRemoveStatusBarInWebApps)},
    {"enable-desktop-pwas-default-offline-page",
     flag_descriptions::kDesktopPWAsDefaultOfflinePageName,
     flag_descriptions::kDesktopPWAsDefaultOfflinePageDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsDefaultOfflinePage)},
    {"enable-desktop-pwas-elided-extensions-menu",
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuName,
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsElidedExtensionsMenu)},
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
    {"enable-desktop-pwas-launch-handler",
     flag_descriptions::kDesktopPWAsLaunchHandlerName,
     flag_descriptions::kDesktopPWAsLaunchHandlerDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableLaunchHandler)},
    {"enable-desktop-pwas-link-capturing",
     flag_descriptions::kDesktopPWAsLinkCapturingName,
     flag_descriptions::kDesktopPWAsLinkCapturingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableLinkCapturing)},
    {"enable-desktop-pwas-manifest-id",
     flag_descriptions::kDesktopPWAsManifestIdName,
     flag_descriptions::kDesktopPWAsManifestIdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableManifestId)},
    {"enable-desktop-pwas-sub-apps", flag_descriptions::kDesktopPWAsSubAppsName,
     flag_descriptions::kDesktopPWAsSubAppsDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(blink::features::kDesktopPWAsSubApps)},
    {"enable-desktop-pwas-protocol-handling",
     flag_descriptions::kDesktopPWAsProtocolHandlingName,
     flag_descriptions::kDesktopPWAsProtocolHandlingDescription,
     kOsWin | kOsLinux | kOsMac | kOsFuchsia,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableProtocolHandlers)},
    {"enable-desktop-pwas-url-handling",
     flag_descriptions::kDesktopPWAsUrlHandlingName,
     flag_descriptions::kDesktopPWAsUrlHandlingDescription,
     kOsWin | kOsLinux | kOsMac | kOsFuchsia,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableUrlHandlers)},
    {"enable-desktop-pwas-window-controls-overlay",
     flag_descriptions::kDesktopPWAsWindowControlsOverlayName,
     flag_descriptions::kDesktopPWAsWindowControlsOverlayDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kWebAppWindowControlsOverlay)},
    {"enable-desktop-pwas-additional-windowing-controls",
     flag_descriptions::kDesktopPWAsAdditionalWindowingControlsName,
     flag_descriptions::kDesktopPWAsAdditionalWindowingControlsDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsAdditionalWindowingControls)},
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
    {"global-media-controls-cast-start-stop",
     flag_descriptions::kGlobalMediaControlsCastStartStopName,
     flag_descriptions::kGlobalMediaControlsCastStartStopDescription,
     kOsWin | kOsMac | kOsLinux | kOsFuchsia,
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

    {"enable-openscreen-cast-streaming-session",
     flag_descriptions::kOpenscreenCastStreamingSessionName,
     flag_descriptions::kOpenscreenCastStreamingSessionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kOpenscreenCastStreamingSession)},

    {"enable-cast-streaming-av1", flag_descriptions::kCastStreamingAv1Name,
     flag_descriptions::kCastStreamingAv1Description, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kCastStreamingAv1)},

    {"enable-cast-streaming-vp9", flag_descriptions::kCastStreamingVp9Name,
     flag_descriptions::kCastStreamingVp9Description, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kCastStreamingVp9)},

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
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MAC)
    {"web-share", flag_descriptions::kWebShareName,
     flag_descriptions::kWebShareDescription, kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebShare)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || OS_WIN || OS_MAC

#if defined(OS_LINUX)
    {"ozone-platform-hint", flag_descriptions::kOzonePlatformHintName,
     flag_descriptions::kOzonePlatformHintDescription, kOsLinux,
     MULTI_VALUE_TYPE(kOzonePlatformHintRuntimeChoices)},

    {"clean-undecryptable-passwords",
     flag_descriptions::kCleanUndecryptablePasswordsLinuxName,
     flag_descriptions::kCleanUndecryptablePasswordsLinuxDescription, kOsLinux,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSyncUndecryptablePasswordsLinux)},
#endif

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
     FEATURE_WITH_PARAMS_VALUE_TYPE(query_tiles::features::kQueryTiles,
                                    kQueryTilesVariations,
                                    "QueryTilesVariations")},
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
    {"query-tiles-segmentation", flag_descriptions::kQueryTilesSegmentationName,
     flag_descriptions::kQueryTilesSegmentationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesSegmentation)},
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
    {"document-transition-slowdown-factor",
     flag_descriptions::kDocumentTransitionSlowdownFactorName,
     flag_descriptions::kDocumentTransitionSlowdownFactorDescription, kOsAll,
     MULTI_VALUE_TYPE(kDocumentTransitionSlowdownFactorChoices)},
#if defined(OS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // OS_WIN
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
    {"feed-back-to-top", flag_descriptions::kFeedBackToTopName,
     flag_descriptions::kFeedBackToTopDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedBackToTop)},
    {"feed-interactive-refresh", flag_descriptions::kFeedInteractiveRefreshName,
     flag_descriptions::kFeedInteractiveRefreshDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedInteractiveRefresh)},
    {"feed-loading-placeholder", flag_descriptions::kFeedLoadingPlaceholderName,
     flag_descriptions::kFeedLoadingPlaceholderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedLoadingPlaceholder)},
    {"feed-stamp", flag_descriptions::kFeedStampName,
     flag_descriptions::kFeedStampDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedStamp)},
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
    {"web-feed-sort", flag_descriptions::kWebFeedSortName,
     flag_descriptions::kWebFeedSortDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kWebFeedSort)},
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
    {"password-import", flag_descriptions::kPasswordImportName,
     flag_descriptions::kPasswordImportDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordImport)},
    {"enable-force-dark", flag_descriptions::kAutoWebContentsDarkModeName,
     flag_descriptions::kAutoWebContentsDarkModeDescription, kOsAll,
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
    {"enable-accessibility-page-zoom",
     flag_descriptions::kAccessibilityPageZoomName,
     flag_descriptions::kAccessibilityPageZoomDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAccessibilityPageZoom)},
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
    {"enable-cros-ime-assist-multi-word-lacros",
     flag_descriptions::kImeAssistMultiWordLacrosSupportName,
     flag_descriptions::kImeAssistMultiWordLacrosSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistMultiWordLacrosSupport)},
    {"enable-cros-ime-assist-personal-info",
     flag_descriptions::kImeAssistPersonalInfoName,
     flag_descriptions::kImeAssistPersonalInfoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAssistPersonalInfo)},
    {"enable-cros-virtual-keyboard-dark-mode",
     flag_descriptions::kVirtualKeyboardDarkModeName,
     flag_descriptions::kVirtualKeyboardDarkModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardDarkMode)},
    {"enable-cros-ime-system-emoji-picker",
     flag_descriptions::kImeSystemEmojiPickerName,
     flag_descriptions::kImeSystemEmojiPickerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPicker)},
    {"enable-cros-ime-system-emoji-picker-clipboard",
     flag_descriptions::kImeSystemEmojiPickerClipboardName,
     flag_descriptions::kImeSystemEmojiPickerClipboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPickerClipboard)},
    {"enable-cros-ime-system-emoji-picker-extension",
     flag_descriptions::kImeSystemEmojiPickerExtensionName,
     flag_descriptions::kImeSystemEmojiPickerExtensionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPickerExtension)},
    {"enable-cros-ime-stylus-handwriting",
     flag_descriptions::kImeStylusHandwritingName,
     flag_descriptions::kImeStylusHandwritingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeStylusHandwriting)},
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
    {"enable-cros-system-chinese-physical-typing",
     flag_descriptions::kSystemChinesePhysicalTypingName,
     flag_descriptions::kSystemChinesePhysicalTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSystemChinesePhysicalTyping)},
    {"enable-cros-system-japanese-physical-typing",
     flag_descriptions::kSystemJapanesePhysicalTypingName,
     flag_descriptions::kSystemJapanesePhysicalTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSystemJapanesePhysicalTyping)},
    {"enable-cros-system-korean-physical-typing",
     flag_descriptions::kSystemKoreanPhysicalTypingName,
     flag_descriptions::kSystemKoreanPhysicalTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSystemKoreanPhysicalTyping)},
    {"enable-cros-virtual-keyboard-bordered-key",
     flag_descriptions::kVirtualKeyboardBorderedKeyName,
     flag_descriptions::kVirtualKeyboardBorderedKeyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardBorderedKey)},
    {"enable-experimental-accessibility-dictation-extension",
     flag_descriptions::kExperimentalAccessibilityDictationExtensionName,
     flag_descriptions::kExperimentalAccessibilityDictationExtensionDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kExperimentalAccessibilityDictationExtension)},
    {"enable-experimental-accessibility-dictation-offline",
     flag_descriptions::kExperimentalAccessibilityDictationOfflineName,
     flag_descriptions::kExperimentalAccessibilityDictationOfflineDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kExperimentalAccessibilityDictationOffline)},
    {"enable-experimental-accessibility-dictation-commands",
     flag_descriptions::kExperimentalAccessibilityDictationCommandsName,
     flag_descriptions::kExperimentalAccessibilityDictationCommandsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kExperimentalAccessibilityDictationCommands)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
    {"enable-experimental-accessibility-switch-access-multistep-automation",
     flag_descriptions::
         kExperimentalAccessibilitySwitchAccessMultistepAutomationName,
     flag_descriptions::
         kExperimentalAccessibilitySwitchAccessMultistepAutomationDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::
             kEnableExperimentalAccessibilitySwitchAccessMultistepAutomation)},
    {"enable-experimental-kernel-vm-support",
     flag_descriptions::kKernelnextVMsName,
     flag_descriptions::kKernelnextVMsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kKernelnextVMs)},
    {"enable-magnifier-continuous-mouse-following-mode-setting",
     flag_descriptions::kMagnifierContinuousMouseFollowingModeSettingName,
     flag_descriptions::
         kMagnifierContinuousMouseFollowingModeSettingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kMagnifierContinuousMouseFollowingModeSetting)},
    {"enable-docked-magnifier-resizing",
     flag_descriptions::kDockedMagnifierResizingName,
     flag_descriptions::kDockedMagnifierResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDockedMagnifierResizing)},
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
    {"enable-payment-request-basic-card",
     flag_descriptions::kPaymentRequestBasicCardName,
     flag_descriptions::kPaymentRequestBasicCardDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPaymentRequestBasicCard)},
    {"enable-debug-for-store-billing",
     flag_descriptions::kAppStoreBillingDebugName,
     flag_descriptions::kAppStoreBillingDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kAppStoreBillingDebug)},
    {"enable-debug-for-secure-payment-confirmation",
     flag_descriptions::kSecurePaymentConfirmationDebugName,
     flag_descriptions::kSecurePaymentConfirmationDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSecurePaymentConfirmationDebug)},
#if defined(OS_ANDROID)
    {"enable-secure-payment-confirmation-android",
     flag_descriptions::kSecurePaymentConfirmationAndroidName,
     flag_descriptions::kSecurePaymentConfirmationAndroidDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(features::kSecurePaymentConfirmation)},
#endif
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-account-restrictions", flag_descriptions::kArcAccountRestrictionsName,
     flag_descriptions::kArcAccountRestrictionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kArcAccountRestrictions)},
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
    {"arc-keyboard-shortcut-helper-integration",
     flag_descriptions::kArcKeyboardShortcutHelperIntegrationName,
     flag_descriptions::kArcKeyboardShortcutHelperIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kKeyboardShortcutHelperIntegrationFeature)},
    {"arc-mouse-wheel-smooth-scroll",
     flag_descriptions::kArcMouseWheelSmoothScrollName,
     flag_descriptions::kArcMouseWheelSmoothScrollDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kMouseWheelSmoothScroll)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
    {"arc-native-bridge-64bit-support-experiment",
     flag_descriptions::kArcNativeBridge64BitSupportExperimentName,
     flag_descriptions::kArcNativeBridge64BitSupportExperimentDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridge64BitSupportExperimentFeature)},
    {"arc-right-click-long-press",
     flag_descriptions::kArcRightClickLongPressName,
     flag_descriptions::kArcRightClickLongPressDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRightClickLongPress)},
    {"arc-rt-vcpu-dual-core", flag_descriptions::kArcRtVcpuDualCoreName,
     flag_descriptions::kArcRtVcpuDualCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuDualCore)},
    {"arc-rt-vcpu-quad-core", flag_descriptions::kArcRtVcpuQuadCoreName,
     flag_descriptions::kArcRtVcpuQuadCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuQuadCore)},
    {"arc-usb-device-attach-to-vm-experiment",
     flag_descriptions::kArcUsbDeviceDefaultAttachToVmName,
     flag_descriptions::kArcUsbDeviceDefaultAttachToVmDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbDeviceDefaultAttachToArcVm)},
    {kArcVmBalloonPolicyInternalName,
     flag_descriptions::kArcVmBalloonPolicyName,
     flag_descriptions::kArcVmBalloonPolicyDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kVmBalloonPolicy)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-generic-sensor-extra-classes",
     flag_descriptions::kEnableGenericSensorExtraClassesName,
     flag_descriptions::kEnableGenericSensorExtraClassesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensorExtraClasses)},

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
    {"audio-url", flag_descriptions::kAudioUrlName,
     flag_descriptions::kAudioUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAudioUrl)},
    {"prefer-constant-frame-rate",
     flag_descriptions::kPreferConstantFrameRateName,
     flag_descriptions::kPreferConstantFrameRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPreferConstantFrameRate)},
    {"force-control-face-ae", flag_descriptions::kForceControlFaceAeName,
     flag_descriptions::kForceControlFaceAeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kForceControlFaceAeChoices)},
    {"hdrnet-override", flag_descriptions::kHdrNetOverrideName,
     flag_descriptions::kHdrNetOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kHdrNetOverrideChoices)},
    {"auto-framing-override", flag_descriptions::kAutoFramingOverrideName,
     flag_descriptions::kAutoFramingOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAutoFramingOverrideChoices)},
    {"camera-app-document-manual-crop",
     flag_descriptions::kCameraAppDocumentManualCropName,
     flag_descriptions::kCameraAppDocumentManualCropDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCameraAppDocumentManualCrop)},
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
    {"files-archivemount2", flag_descriptions::kFilesArchivemount2Name,
     flag_descriptions::kFilesArchivemount2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesArchivemount2)},
    {"files-banner-framework", flag_descriptions::kFilesBannerFrameworkName,
     flag_descriptions::kFilesBannerFrameworkDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesBannerFramework)},
    {"files-extract-archive", flag_descriptions::kFilesExtractArchiveName,
     flag_descriptions::kFilesExtractArchiveDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesExtractArchive)},
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
    {"force-spectre-v2-mitigation",
     flag_descriptions::kForceSpectreVariant2MitigationName,
     flag_descriptions::kForceSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         sandbox::policy::features::kForceSpectreVariant2Mitigation)},
    {"fuse-box", flag_descriptions::kFuseBoxName,
     flag_descriptions::kFuseBoxDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFuseBox)},
    {"spectre-v2-mitigation", flag_descriptions::kSpectreVariant2MitigationName,
     flag_descriptions::kSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(sandbox::policy::features::kSpectreVariant2Mitigation)},
    {"eche-phone-hub-permissions-onboarding",
     flag_descriptions::kEchePhoneHubPermissionsOnboardingName,
     flag_descriptions::kEchePhoneHubPermissionsOnboardingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kEchePhoneHubPermissionsOnboarding)},
    {"eche-swa", flag_descriptions::kEcheSWAName,
     flag_descriptions::kEcheSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWA)},
    {"eche-swa-resizing", flag_descriptions::kEcheSWAResizingName,
     flag_descriptions::kEcheSWAResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWAResizing)},
    {"eche-swa-debug-mode", flag_descriptions::kEcheSWADebugModeName,
     flag_descriptions::kEcheSWADebugModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWADebugMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
    {"enable-universal-links", flag_descriptions::kEnableUniversalLinksName,
     flag_descriptions::kEnableUniversalLinksDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kEnableUniveralLinks)},
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
    {"omnibox-most-visited-tiles",
     flag_descriptions::kOmniboxMostVisitedTilesName,
     flag_descriptions::kOmniboxMostVisitedTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTiles)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-pedals-android-batch1",
     flag_descriptions::kOmniboxPedalsAndroidBatch1Name,
     flag_descriptions::kOmniboxPedalsAndroidBatch1Description, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsAndroidBatch1)},
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

    {"omnibox-zero-suggest-prefetching",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kZeroSuggestPrefetching,
                                    kOmniboxZeroSuggestPrefetchingVariations,
                                    "OmniboxBundledExperimentV1")},

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN) || defined(OS_FUCHSIA)
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
    {"omnibox-pedals-batch2-nonenglish",
     flag_descriptions::kOmniboxPedalsBatch2NonEnglishName,
     flag_descriptions::kOmniboxPedalsBatch2NonEnglishDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsBatch2NonEnglish)},
    {"omnibox-pedals-batch3", flag_descriptions::kOmniboxPedalsBatch3Name,
     flag_descriptions::kOmniboxPedalsBatch3Description, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsBatch3)},
    {"omnibox-pedals-batch3-nonenglish",
     flag_descriptions::kOmniboxPedalsBatch3NonEnglishName,
     flag_descriptions::kOmniboxPedalsBatch3NonEnglishDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsBatch3NonEnglish)},
    {"omnibox-pedals-translation-console",
     flag_descriptions::kOmniboxPedalsTranslationConsoleName,
     flag_descriptions::kOmniboxPedalsTranslationConsoleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalsTranslationConsole)},
    {"omnibox-keyword-search-button",
     flag_descriptions::kOmniboxKeywordSearchButtonName,
     flag_descriptions::kOmniboxKeywordSearchButtonDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxKeywordSearchButton)},
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
    {"omnibox-document-provider-aso",
     flag_descriptions::kOmniboxDocumentProviderAsoName,
     flag_descriptions::kOmniboxDocumentProviderAsoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDocumentProviderAso)},
    {"omnibox-preserve-longer-shortcuts-text",
     flag_descriptions::kOmniboxPreserveLongerShortcutsTextName,
     flag_descriptions::kOmniboxPreserveLongerShortcutsTextDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(omnibox::kPreserveLongerShortcutsText)},
    {"omnibox-aggregate-shortcuts",
     flag_descriptions::kOmniboxAggregateShortcutsName,
     flag_descriptions::kOmniboxAggregateShortcutsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kAggregateShortcuts)},
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN) || defined(OS_FUCHSIA)

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

    {"history-journeys", flag_descriptions::kJourneysName,
     flag_descriptions::kJourneysDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(history_clusters::internal::kJourneys)},

    {"history-journeys-omnibox-action",
     flag_descriptions::kJourneysOmniboxActionName,
     flag_descriptions::kJourneysOmniboxActionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(history_clusters::kOmniboxAction)},

    {"page-content-annotations", flag_descriptions::kPageContentAnnotationsName,
     flag_descriptions::kPageContentAnnotationsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         optimization_guide::features::kPageContentAnnotations,
         kPageContentAnnotationsVariations,
         "PageContentAnnotations")},

    {"search-prefetch", flag_descriptions::kEnableSearchPrefetchName,
     flag_descriptions::kEnableSearchPrefetchDescription, kOsAll,
     SINGLE_VALUE_TYPE(kSearchPrefetchServiceCommandLineFlag)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"handwriting-gesture-editing",
     flag_descriptions::kHandwritingGestureEditingName,
     flag_descriptions::kHandwritingGestureEditingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHandwritingGestureEditing)},
    {"handwriting-legacy-recognition",
     flag_descriptions::kHandwritingLegacyRecognitionName,
     flag_descriptions::kHandwritingLegacyRecognitionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kHandwritingLegacyRecognition)},
    {"handwriting-legacy-recognition-all-lang",
     flag_descriptions::kHandwritingLegacyRecognitionAllLangName,
     flag_descriptions::kHandwritingLegacyRecognitionAllLangDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kHandwritingLegacyRecognitionAllLang)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"block-insecure-private-network-requests",
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsName,
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBlockInsecurePrivateNetworkRequests)},

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

#if defined(OS_ANDROID)
    {flag_descriptions::kReadLaterFlagId, flag_descriptions::kReadLaterName,
     flag_descriptions::kReadLaterDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(reading_list::switches::kReadLater,
                                    kReadLaterVariations,
                                    "ReadLater")},
#else
    {flag_descriptions::kReadLaterFlagId, flag_descriptions::kReadLaterName,
     flag_descriptions::kReadLaterDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(reading_list::switches::kReadLater)},
#endif

    {"read-later-new-badge-promo",
     flag_descriptions::kReadLaterNewBadgePromoName,
     flag_descriptions::kReadLaterNewBadgePromoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadLaterNewBadgePromo)},

#if defined(OS_ANDROID)
    {"read-later-reminder-notification",
     flag_descriptions::kReadLaterReminderNotificationName,
     flag_descriptions::kReadLaterReminderNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         reading_list::switches::kReadLaterReminderNotification)},

    {"bookmark-bottom-sheet", flag_descriptions::kBookmarkBottomSheetName,
     flag_descriptions::kBookmarkBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBookmarkBottomSheet)},
#endif

    {"apps-shortcut-default-off",
     flag_descriptions::kAppsShortcutDefaultOffName,
     flag_descriptions::kAppsShortcutDefaultOffDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(bookmarks::features::kAppsShortcutDefaultOff)},

    {"tab-groups-auto-create", flag_descriptions::kTabGroupsAutoCreateName,
     flag_descriptions::kTabGroupsAutoCreateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsAutoCreate)},

    {"tab-groups-collapse-freezing",
     flag_descriptions::kTabGroupsCollapseFreezingName,
     flag_descriptions::kTabGroupsCollapseFreezingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsCollapseFreezing)},

    {"tab-groups-new-badge-promo",
     flag_descriptions::kTabGroupsNewBadgePromoName,
     flag_descriptions::kTabGroupsNewBadgePromoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsNewBadgePromo)},

    {"tab-groups-save", flag_descriptions::kTabGroupsSaveName,
     flag_descriptions::kTabGroupsSaveDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroupsSave)},

    {flag_descriptions::kScrollableTabStripFlagId,
     flag_descriptions::kScrollableTabStripName,
     flag_descriptions::kScrollableTabStripDescription, kOsDesktop,
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

    {flag_descriptions::kSidePanelDragAndDropFlagId,
     flag_descriptions::kSidePanelDragAndDropName,
     flag_descriptions::kSidePanelDragAndDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanelDragAndDrop)},

    {flag_descriptions::kUnifiedSidePanelFlagId,
     flag_descriptions::kUnifiedSidePanelName,
     flag_descriptions::kUnifiedSidePanelDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kUnifiedSidePanel)},

    {"tab-outlines-in-low-contrast-themes",
     flag_descriptions::kTabOutlinesInLowContrastThemesName,
     flag_descriptions::kTabOutlinesInLowContrastThemesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabOutlinesInLowContrastThemes)},

    {"prominent-dark-mode-active-tab-title",
     flag_descriptions::kProminentDarkModeActiveTabTitleName,
     flag_descriptions::kProminentDarkModeActiveTabTitleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kProminentDarkModeActiveTabTitle)},

#if defined(OS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
    {"webui-feedback", flag_descriptions::kWebuiFeedbackName,
     flag_descriptions::kWebuiFeedbackDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIFeedback)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS) || defined(OS_FUCHSIA)

#if !defined(OS_ANDROID)
    {"ntp-cache-one-google-bar", flag_descriptions::kNtpCacheOneGoogleBarName,
     flag_descriptions::kNtpCacheOneGoogleBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCacheOneGoogleBar)},

    {"ntp-modules", flag_descriptions::kNtpModulesName,
     flag_descriptions::kNtpModulesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kModules)},

    {"ntp-drive-module", flag_descriptions::kNtpDriveModuleName,
     flag_descriptions::kNtpDriveModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpDriveModule,
                                    kNtpDriveModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-photos-module", flag_descriptions::kNtpPhotosModuleName,
     flag_descriptions::kNtpPhotosModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpPhotosModule,
                                    kNtpPhotosModuleVariations,
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

    {"enable-retail-coupons", flag_descriptions::kRetailCouponsName,
     flag_descriptions::kRetailCouponsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kRetailCoupons)},

    {"ntp-safe-browsing-module", flag_descriptions::kNtpSafeBrowsingModuleName,
     flag_descriptions::kNtpSafeBrowsingModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpSafeBrowsingModule,
                                    kNtpSafeBrowsingModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-modules-drag-and-drop", flag_descriptions::kNtpModulesDragAndDropName,
     flag_descriptions::kNtpModulesDragAndDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesDragAndDrop)},

    {"ntp-modules-redesigned", flag_descriptions::kNtpModulesRedesignedName,
     flag_descriptions::kNtpModulesRedesignedDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesRedesigned)},

    {"ntp-modules-redesigned-layout",
     flag_descriptions::kNtpModulesRedesignedLayoutName,
     flag_descriptions::kNtpModulesRedesignedLayoutDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesRedesignedLayout)},

    {"ntp-realbox-match-omnibox-theme",
     flag_descriptions::kNtpRealboxMatchOmniboxThemeName,
     flag_descriptions::kNtpRealboxMatchOmniboxThemeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealboxMatchOmniboxTheme)},

    {"ntp-realbox-pedals", flag_descriptions::kNtpRealboxPedalsName,
     flag_descriptions::kNtpRealboxPedalsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kNtpRealboxPedals)},

    {"ntp-realbox-suggestion-answers",
     flag_descriptions::kNtpRealboxSuggestionAnswersName,
     flag_descriptions::kNtpRealboxSuggestionAnswersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kNtpRealboxSuggestionAnswers)},

    {"ntp-realbox-tail-suggest", flag_descriptions::kNtpRealboxTailSuggestName,
     flag_descriptions::kNtpRealboxTailSuggestDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kNtpRealboxTailSuggest)},

    {"ntp-realbox-use-google-g-icon",
     flag_descriptions::kNtpRealboxUseGoogleGIconName,
     flag_descriptions::kNtpRealboxUseGoogleGIconDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealboxUseGoogleGIcon)},
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

    {"download-progress-message",
     flag_descriptions::kDownloadProgressMessageName,
     flag_descriptions::kDownloadProgressMessageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDownloadProgressMessage)},

    {"download-later", flag_descriptions::kDownloadLaterName,
     flag_descriptions::kDownloadLaterDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadLater)},

    {"download-later-debug-on-wifi",
     flag_descriptions::kDownloadLaterDebugOnWifiName,
     flag_descriptions::kDownloadLaterDebugOnWifiNameDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(download::switches::kDownloadLaterDebugOnWifi)},

    {"enable-dangerous-download-dialog",
     flag_descriptions::kEnableDangerousDownloadDialogName,
     flag_descriptions::kEnableDangerousDownloadDialogDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kEnableDangerousDownloadDialog,
         kDangerousDownloadDialogVariations,
         "DangerousDownloadDialogVariations")},

    {"enable-duplicate-download-dialog",
     flag_descriptions::kEnableDuplicateDownloadDialogName,
     flag_descriptions::kEnableDuplicateDownloadDialogDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEnableDuplicateDownloadDialog)},

    {"enable-mixed-content-download-dialog",
     flag_descriptions::kEnableMixedContentDownloadDialogName,
     flag_descriptions::kEnableMixedContentDownloadDialogDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEnableMixedContentDownloadDialog)},
#endif

    {"enable-new-download-backend",
     flag_descriptions::kEnableNewDownloadBackendName,
     flag_descriptions::kEnableNewDownloadBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kUseDownloadOfflineContentProvider)},

    {"download-range", flag_descriptions::kDownloadRangeName,
     flag_descriptions::kDownloadRangeDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kDownloadRange)},

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
     kOsDesktop, FEATURE_VALUE_TYPE(device::kWebAuthPhoneSupport)},

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
    {"print-with-postscript-type42-fonts",
     flag_descriptions::kPrintWithPostScriptType42FontsName,
     flag_descriptions::kPrintWithPostScriptType42FontsDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kPrintWithPostScriptType42Fonts)},

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
                                    "CommercePriceTracking")},

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

    {"enable-quick-action-search-widget-android-dino-variant",
     flag_descriptions::kQuickActionSearchWidgetAndroidDinoVariantName,
     flag_descriptions::kQuickActionSearchWidgetAndroidDinoVariantDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kQuickActionSearchWidgetAndroidDinoVariant)},

    {"shopping-list", flag_descriptions::kShoppingListName,
     flag_descriptions::kShoppingListDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
#endif  // OS_ANDROID

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
     kOsMac | kOsWin | kOsLinux | kOsFuchsia,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableToolbarStatusChip)},

    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         network::switches::kUnsafelyTreatInsecureOriginAsSecure,
         "")},

    {"detect-target-embedding-lookalikes",
     flag_descriptions::kDetectTargetEmbeddingLookalikesName,
     flag_descriptions::kDetectTargetEmbeddingLookalikesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         lookalikes::features::kDetectTargetEmbeddingLookalikes)},

    {"disable-process-reuse", flag_descriptions::kDisableProcessReuse,
     flag_descriptions::kDisableProcessReuseDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDisableProcessReuse)},

#if !defined(OS_ANDROID)
    {"enable-accessibility-live-caption",
     flag_descriptions::kEnableAccessibilityLiveCaptionName,
     flag_descriptions::kEnableAccessibilityLiveCaptionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kLiveCaption)},

    {"enable-auto-disable-accessibility",
     flag_descriptions::kEnableAutoDisableAccessibilityName,
     flag_descriptions::kEnableAutoDisableAccessibilityDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAutoDisableAccessibility)},
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
    {"cct-resizable-90-maximum-height",
     flag_descriptions::kCCTResizable90MaximumHeightName,
     flag_descriptions::kCCTResizable90MaximumHeightDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTResizable90MaximumHeight)},
    {"cct-resizable-allow-resize-by-user-gesture",
     flag_descriptions::kCCTResizableAllowResizeByUserGestureName,
     flag_descriptions::kCCTResizableAllowResizeByUserGestureDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kCCTResizableAllowResizeByUserGesture)},
    {"cct-resizable-for-first-parties",
     flag_descriptions::kCCTResizableForFirstPartiesName,
     flag_descriptions::kCCTResizableForFirstPartiesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTResizableForFirstParties)},
    {"cct-resizable-for-third-parties",
     flag_descriptions::kCCTResizableForThirdPartiesName,
     flag_descriptions::kCCTResizableForThirdPartiesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTResizableForThirdParties)},
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

    {"enable-css-selector-fragment-anchor",
     flag_descriptions::kEnableCssSelectorFragmentAnchorName,
     flag_descriptions::kEnableCssSelectorFragmentAnchorDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCssSelectorFragmentAnchor)},

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
#endif  // !defined(OS_ANDROID)

    {"destroy-profile-on-browser-close",
     flag_descriptions::kDestroyProfileOnBrowserCloseName,
     flag_descriptions::kDestroyProfileOnBrowserCloseDescription,
     kOsMac | kOsWin | kOsLinux | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kDestroyProfileOnBrowserClose)},

#if defined(OS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescriptionWindows, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoicesWindows)},
#elif defined(OS_MAC)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescriptionMac, kOsMac,
     MULTI_VALUE_TYPE(kUseAngleChoicesMac)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-dsp", flag_descriptions::kEnableGoogleAssistantDspName,
     flag_descriptions::kEnableGoogleAssistantDspDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kEnableDspHotword)},

    {"disable-quick-answers-v2-translation",
     flag_descriptions::kDisableQuickAnswersV2TranslationName,
     flag_descriptions::kDisableQuickAnswersV2TranslationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableQuickAnswersV2Translation)},
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

#if !defined(OS_ANDROID)
    {"sharing-desktop-screenshots",
     flag_descriptions::kSharingDesktopScreenshotsName,
     flag_descriptions::kSharingDesktopScreenshotsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(sharing_hub::kDesktopScreenshots)},
    {"sharing-desktop-screenshots-edit",
     flag_descriptions::kSharingDesktopScreenshotsEditName,
     flag_descriptions::kSharingDesktopScreenshotsEditDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(share::kSharingDesktopScreenshotsEdit)},
#endif

    {"sharing-prefer-vapid", flag_descriptions::kSharingPreferVapidName,
     flag_descriptions::kSharingPreferVapidDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingPreferVapid)},

    {"sharing-send-via-sync", flag_descriptions::kSharingSendViaSyncName,
     flag_descriptions::kSharingSendViaSyncDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingSendViaSync)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ash-enable-pip-rounded-corners",
     flag_descriptions::kAshEnablePipRoundedCornersName,
     flag_descriptions::kAshEnablePipRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipRoundedCorners)},
    {"ash-enable-floating-window", flag_descriptions::kWindowControlMenu,
     flag_descriptions::kWindowControlMenuDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWindowControlMenu)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-stereo-input",
     flag_descriptions::kEnableGoogleAssistantStereoInputName,
     flag_descriptions::kEnableGoogleAssistantStereoInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableStereoAudioInput)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    {"app-service-external-protocol",
     flag_descriptions::kAppServiceExternalProtocolName,
     flag_descriptions::kAppServiceExternalProtocolDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceExternalProtocol)},

    {"arc-ghost-window", flag_descriptions::kArcGhostWindowName,
     flag_descriptions::kArcGhostWindowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kArcGhostWindow)},

    {"arc-window-predictor", flag_descriptions::kArcWindowPredictorName,
     flag_descriptions::kArcWindowPredictorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kArcWindowPredictor)},

    {"arc-input-overlay", flag_descriptions::kArcInputOverlayName,
     flag_descriptions::kArcInputOverlayDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kArcInputOverlay)},

    {"full-restore", flag_descriptions::kFullRestoreName,
     flag_descriptions::kFullRestoreDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kFullRestore)},

    {"full-restore-for-lacros", flag_descriptions::kFullRestoreForLacrosName,
     flag_descriptions::kFullRestoreForLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kFullRestoreForLacros)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},

    {"intent-picker-pwa-persistence",
     flag_descriptions::kIntentPickerPWAPersistenceName,
     flag_descriptions::kIntentPickerPWAPersistenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kIntentPickerPWAPersistence)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if defined(OS_CHROMEOS)
    {"bluetooth-advertisement-monitoring",
     flag_descriptions::kBluetoothAdvertisementMonitoringName,
     flag_descriptions::kBluetoothAdvertisementMonitoringDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothAdvertisementMonitoring)},
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"allow-disable-mouse-acceleration",
     flag_descriptions::kAllowDisableMouseAccelerationName,
     flag_descriptions::kAllowDisableMouseAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowDisableMouseAcceleration)},

    {"allow-disable-touchpad-haptic-feedback",
     flag_descriptions::kAllowDisableTouchpadHapticFeedbackName,
     flag_descriptions::kAllowDisableTouchpadHapticFeedbackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowDisableTouchpadHapticFeedback)},

    {"allow-repeated-updates", flag_descriptions::kAllowRepeatedUpdatesName,
     flag_descriptions::kAllowRepeatedUpdatesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowRepeatedUpdates)},

    {"allow-scroll-settings", flag_descriptions::kAllowScrollSettingsName,
     flag_descriptions::kAllowScrollSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAllowScrollSettings)},

    {"allow-touchpad-haptic-click-settings",
     flag_descriptions::kAllowTouchpadHapticClickSettingsName,
     flag_descriptions::kAllowTouchpadHapticClickSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowTouchpadHapticClickSettings)},

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
     flag_descriptions::kEnablePalmOnToolTypePalmDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnablePalmOnToolTypePalm)},

    {"enable-reven-log-source", flag_descriptions::kEnableRevenLogSourceName,
     flag_descriptions::kEnableRevenLogSourceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kRevenLogSource)},

    {"enable-pci-guard-ui", flag_descriptions::kEnablePciguardUiName,
     flag_descriptions::kEnablePciguardUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnablePciguardUi)},

    {"enable-heuristic-stylus-palm-rejection",
     flag_descriptions::kEnableHeuristicStylusPalmRejectionName,
     flag_descriptions::kEnableHeuristicStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeuristicPalmDetectionFilter)},

    {"fast-pair", flag_descriptions::kFastPairName,
     flag_descriptions::kFastPairDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPair)},

    {"pcie-billboard-notification",
     flag_descriptions::kPcieBillboardNotificationName,
     flag_descriptions::kPcieBillboardNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPcieBillboardNotification)},

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

    {"enable-firmware-updater-app",
     flag_descriptions::kEnableFirmwareUpdaterAppName,
     flag_descriptions::kEnableFirmwareUpdaterAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFirmwareUpdaterApp)},

    {"enhanced-network-voices", flag_descriptions::kEnhancedNetworkVoicesName,
     flag_descriptions::kEnhancedNetworkVoicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnhancedNetworkVoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

    {"enterprise-reporting-extension-manifest-version",
     flag_descriptions::kEnterpriseReportingExtensionManifestVersionName,
     flag_descriptions::kEnterpriseReportingExtensionManifestVersionDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::kEnterpriseReportingExtensionManifestVersion)},

    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription,
     kOsMac | kOsLinux | kOsWin | kOsFuchsia,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},

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
    {"safe-browsing-password-protection-for-signed-in-users",
     flag_descriptions::kPasswordProtectionForSignedInUsersName,
     flag_descriptions::kPasswordProtectionForSignedInUsersDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(safe_browsing::kPasswordProtectionForSignedInUsers)},
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
     kOsWin | kOsMac | kOsLinux | kOsFuchsia,
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
    defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
    {"global-media-controls-modern-ui",
     flag_descriptions::kGlobalMediaControlsModernUIName,
     flag_descriptions::kGlobalMediaControlsModernUIDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsModernUI)},
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS) || defined(OS_FUCHSIA)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-local-web-approvals", flag_descriptions::kLocalWebApprovalsName,
     flag_descriptions::kLocalWebApprovalsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(supervised_users::kLocalWebApprovals)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    {"enable-web-filter-interstitial-refresh",
     flag_descriptions::kWebFilterInterstitialRefreshName,
     flag_descriptions::kWebFilterInterstitialRefreshDescription,
     kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(supervised_users::kWebFilterInterstitialRefresh)},
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

    {"notification-scheduler", flag_descriptions::kNotificationSchedulerName,
     flag_descriptions::kNotificationSchedulerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kNotificationScheduleService)},

    {"notification-scheduler-debug-options",
     flag_descriptions::kNotificationSchedulerDebugOptionName,
     flag_descriptions::kNotificationSchedulerDebugOptionDescription,
     kOsAndroid, MULTI_VALUE_TYPE(kNotificationSchedulerChoices)},

#if defined(OS_ANDROID)

    {"debug-chime-notification",
     flag_descriptions::kChimeAlwaysShowNotificationName,
     flag_descriptions::kChimeAlwaysShowNotificationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(notifications::switches::kDebugChimeNotification)},

    {"use-chime-android-sdk", flag_descriptions::kChimeAndroidSdkName,
     flag_descriptions::kChimeAndroidSdkDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kUseChimeAndroidSdk)},

#endif  // defined(OS_ANDROID)

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
         content_settings::kDarkenWebsitesCheckboxInThemesSetting)},
#endif  // defined(OS_ANDROID)

    {"enable-autofill-upi-vpa", flag_descriptions::kAutofillSaveAndFillVPAName,
     flag_descriptions::kAutofillSaveAndFillVPADescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveAndFillVPA)},

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

    {"enable-suggested-local-files",
     flag_descriptions::kEnableSuggestedLocalFilesName,
     flag_descriptions::kEnableSuggestedLocalFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableSuggestedLocalFiles)},
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

#if !defined(OS_ANDROID)
    {"closed-tab-cache", flag_descriptions::kClosedTabCacheName,
     flag_descriptions::kClosedTabCacheDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kClosedTabCache)},
#endif  // !defined(OS_ANDROID)

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

    {"device-posture", flag_descriptions::kDevicePostureName,
     flag_descriptions::kDevicePostureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDevicePosture)},

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crosh-swa", flag_descriptions::kCroshSWAName,
     flag_descriptions::kCroshSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCroshSWA)},
    {"crostini-container-install",
     flag_descriptions::kCrostiniContainerInstallName,
     flag_descriptions::kCrostiniContainerInstallDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCrostiniContainerChoices)},
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
    {"sync-settings-categorization",
     flag_descriptions::kSyncSettingsCategorizationName,
     flag_descriptions::kSyncSettingsCategorizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSyncSettingsCategorization)},
    {"os-settings-app-notifications-page",
     flag_descriptions::kOsSettingsAppNotificationsPageName,
     flag_descriptions::kOsSettingsAppNotificationsPageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOsSettingsAppNotificationsPage)},
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
    {"media-app-handles-audio", flag_descriptions::kMediaAppHandlesAudioName,
     flag_descriptions::kMediaAppHandlesAudioDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppHandlesAudio)},
    {"media-app-handles-pdf", flag_descriptions::kMediaAppHandlesPdfName,
     flag_descriptions::kMediaAppHandlesPdfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppHandlesPdf)},
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

#if defined(OS_WIN)
    {"run-video-capture-service-in-browser",
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessName,
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(features::kRunVideoCaptureServiceInBrowserProcess)},
#endif  // defined(OS_WIN)

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
         "PasswordChangeInSettingsFeatureVariations")},
    {"password-scripts-fetching",
     flag_descriptions::kPasswordScriptsFetchingName,
     flag_descriptions::kPasswordScriptsFetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordScriptsFetching)},
    {"password-change-support", flag_descriptions::kPasswordChangeName,
     flag_descriptions::kPasswordChangeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(password_manager::features::kPasswordChange,
                                    kPasswordChangeFeatureVariations,
                                    "PasswordChangeFeatureVariations")},

#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"page-info-history", flag_descriptions::kPageInfoHistoryName,
     flag_descriptions::kPageInfoHistoryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistory)},
    {"page-info-store-info", flag_descriptions::kPageInfoStoreInfoName,
     flag_descriptions::kPageInfoStoreInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoStoreInfo)},
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"page-info-history-desktop",
     flag_descriptions::kPageInfoHistoryDesktopName,
     flag_descriptions::kPageInfoHistoryDesktopDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistoryDesktop)},
#endif  // !defined(OS_ANDROID)

    {"page-info-about-this-site", flag_descriptions::kPageInfoAboutThisSiteName,
     flag_descriptions::kPageInfoAboutThisSiteDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoAboutThisSite)},

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
    {"scan-app-multi-page-scan", flag_descriptions::kScanAppMultiPageScanName,
     flag_descriptions::kScanAppMultiPageScanDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanAppMultiPageScan)},
    {"scan-app-searchable-pdf", flag_descriptions::kScanAppSearchablePdfName,
     flag_descriptions::kScanAppSearchablePdfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kScanAppSearchablePdf)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"color-provider-redirection-for-theme-provider",
     flag_descriptions::kColorProviderRedirectionForThemeProviderName,
     flag_descriptions::kColorProviderRedirectionForThemeProviderDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kColorProviderRedirectionForThemeProvider)},

    {"trust-tokens", flag_descriptions::kTrustTokensName,
     flag_descriptions::kTrustTokensDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(network::features::kTrustTokens,
                                    kPlatformProvidedTrustTokensVariations,
                                    "TrustTokenOriginTrial")},

#if !defined(OS_ANDROID)
    {"copy-link-to-text", flag_descriptions::kCopyLinkToTextName,
     flag_descriptions::kCopyLinkToTextDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCopyLinkToText)},
#endif  // !defined(OS_ANDROID)

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
    {"nearby-sharing-arc", flag_descriptions::kNearbySharingArcName,
     flag_descriptions::kNearbySharingArcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableArcNearbyShare)},
    {"nearby-sharing-background-scanning",
     flag_descriptions::kNearbySharingBackgroundScanningName,
     flag_descriptions::kNearbySharingBackgroundScanningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingBackgroundScanning)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

    {"enable-canvas-context-lost-in-background",
     flag_descriptions::kEnableCanvasContextLostInBackgroundName,
     flag_descriptions::kEnableCanvasContextLostInBackgroundDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableCanvasContextLostInBackground)},

    {"new-canvas-2d-api", flag_descriptions::kNewCanvas2DAPIName,
     flag_descriptions::kNewCanvas2DAPIDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNewCanvas2DAPI)},

    {"enable-translate-sub-frames",
     flag_descriptions::kEnableTranslateSubFramesName,
     flag_descriptions::kEnableTranslateSubFramesDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTranslateSubFrames)},

    {"conversion-measurement-debug-mode",
     flag_descriptions::kConversionMeasurementDebugModeName,
     flag_descriptions::kConversionMeasurementDebugModeDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kConversionsDebugMode)},

    {"client-storage-access-context-auditing",
     flag_descriptions::kClientStorageAccessContextAuditingName,
     flag_descriptions::kClientStorageAccessContextAuditingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClientStorageAccessContextAuditing)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"productivity-launcher", flag_descriptions::kProductivityLauncherName,
     flag_descriptions::kProductivityLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProductivityLauncher)},
    {"productivity-launcher-animation",
     flag_descriptions::kProductivityLauncherAnimationName,
     flag_descriptions::kProductivityLauncherAnimationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProductivityLauncherAnimation)},
    {"shelf-drag-to-pin", flag_descriptions::kShelfDragToPinName,
     flag_descriptions::kShelfDragToPinDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragUnpinnedAppToPin)},
    {"force-show-continue-section",
     flag_descriptions::kForceShowContinueSectionName,
     flag_descriptions::kForceShowContinueSectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kForceShowContinueSection)},
    {"launcher-nudge", flag_descriptions::kLauncherNudgeName,
     flag_descriptions::kLauncherNudgeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShelfLauncherNudge)},
    {"launcher-nudge-short-interval",
     flag_descriptions::kLauncherNudgeShortIntervalName,
     flag_descriptions::kLauncherNudgeShortIntervalDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLauncherNudgeShortInterval)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"categorical-search", flag_descriptions::kCategoricalSearchName,
     flag_descriptions::kCategoricalSearchDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(app_list_features::kCategoricalSearch,
                                    kCategoricalSearchVariations,
                                    "LauncherCategoricalSearch")},

    {"query-search-burn-in-period",
     flag_descriptions::kQuerySearchBurnInPeriodName,
     flag_descriptions::kQuerySearchBurnInPeriodDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(search_features::kQuerySearchBurnInPeriod,
                                    kQuerySearchBurnInPeriodVariations,
                                    "LauncherQuerySearchBurnInPeriod")},

    {"app-discovery-remote-url-search",
     flag_descriptions::kAppDiscoveryRemoteUrlSearchName,
     flag_descriptions::kAppDiscoveryRemoteUrlSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(apps::kAppDiscoveryRemoteUrlSearch)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-offers-in-downstream",
     flag_descriptions::kAutofillEnableOffersInDownstreamName,
     flag_descriptions::kAutofillEnableOffersInDownstreamDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOffersInDownstream)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"chromeos-sharing-hub", flag_descriptions::kChromeOSSharingHubName,
     flag_descriptions::kChromeOSSharingHubDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kChromeOSSharingHub)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-bluetooth-spp-in-serial-api",
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiName,
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiDescription,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableBluetoothSerialPortProfileInSerialApi)},

    {"add-passwords-in-settings",
     flag_descriptions::kAddPasswordsInSettingsName,
     flag_descriptions::kAddPasswordsInSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSupportForAddPasswordsInSettings)},

    {"edit-passwords-in-settings",
     flag_descriptions::kEditPasswordsInSettingsName,
     flag_descriptions::kEditPasswordsInSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kEditPasswordsInSettings)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"frame-throttle-fps", flag_descriptions::kFrameThrottleFpsName,
     flag_descriptions::kFrameThrottleFpsDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kFrameThrottleFpsChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

    {"incognito-reauthentication-for-android",
     flag_descriptions::kIncognitoReauthenticationForAndroidName,
     flag_descriptions::kIncognitoReauthenticationForAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kIncognitoReauthenticationForAndroid)},
#endif

#if defined(OS_MAC) || defined(OS_WIN) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
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
#endif  // defined(OS_MAC) || defined(OS_WIN) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS) || defined(OS_FUCHSIA)

    {"consolidated-site-storage-controls",
     flag_descriptions::kConsolidatedSiteStorageControlsName,
     flag_descriptions::kConsolidatedSiteStorageControlsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kConsolidatedSiteStorageControls)},

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

#if defined(OS_ANDROID)
    {"messages-for-android-ads-blocked",
     flag_descriptions::kMessagesForAndroidAdsBlockedName,
     flag_descriptions::kMessagesForAndroidAdsBlockedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidAdsBlocked)},
    {"messages-for-android-chrome-survey",
     flag_descriptions::kMessagesForAndroidChromeSurveyName,
     flag_descriptions::kMessagesForAndroidChromeSurveyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidChromeSurvey)},
    {"messages-for-android-infrastructure",
     flag_descriptions::kMessagesForAndroidInfrastructureName,
     flag_descriptions::kMessagesForAndroidInfrastructureDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidInfrastructure)},
    {"messages-for-android-near-oom-reduction",
     flag_descriptions::kMessagesForAndroidNearOomReductionName,
     flag_descriptions::kMessagesForAndroidNearOomReductionDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidNearOomReduction)},
    {"messages-for-android-notification-blocked",
     flag_descriptions::kMessagesForAndroidNotificationBlockedName,
     flag_descriptions::kMessagesForAndroidNotificationBlockedDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidNotificationBlocked)},
    {"messages-for-android-passwords",
     flag_descriptions::kMessagesForAndroidPasswordsName,
     flag_descriptions::kMessagesForAndroidPasswordsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPasswords)},
    {"messages-for-android-permission-update",
     flag_descriptions::kMessagesForAndroidPermissionUpdateName,
     flag_descriptions::kMessagesForAndroidPermissionUpdateDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPermissionUpdate)},
    {"messages-for-android-popup-blocked",
     flag_descriptions::kMessagesForAndroidPopupBlockedName,
     flag_descriptions::kMessagesForAndroidPopupBlockedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidPopupBlocked)},
    {"messages-for-android-pwa-install",
     flag_descriptions::kMessagesForAndroidPWAInstallName,
     flag_descriptions::kMessagesForAndroidPWAInstallDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(webapps::features::kInstallableAmbientBadgeMessage)},
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
    {"messages-for-android-sync-error",
     flag_descriptions::kMessagesForAndroidSyncErrorName,
     flag_descriptions::kMessagesForAndroidSyncErrorDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidSyncError)},
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

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_FUCHSIA)
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

#if defined(OS_WIN)
    {"pwa-uninstall-in-windows-os",
     flag_descriptions::kPwaUninstallInWindowsOsName,
     flag_descriptions::kPwaUninstallInWindowsOsDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableWebAppUninstallFromOsSettings)},
#endif

    {"pwa-update-dialog-for-name-and-icon",
     flag_descriptions::kPwaUpdateDialogForNameAndIconName,
     flag_descriptions::kPwaUpdateDialogForNameAndIconDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPwaUpdateDialogForNameAndIcon)},

    {"sync-autofill-wallet-offer-data",
     flag_descriptions::kSyncAutofillWalletOfferDataName,
     flag_descriptions::kSyncAutofillWalletOfferDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncAutofillWalletOfferData)},

#if BUILDFLAG(ENABLE_OOP_PRINTING)
    {"enable-oop-print-drivers", flag_descriptions::kEnableOopPrintDriversName,
     flag_descriptions::kEnableOopPrintDriversDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(printing::features::kEnableOopPrintDrivers)},
#endif

    {"enable-browsing-data-lifetime-manager",
     flag_descriptions::kEnableBrowsingDataLifetimeManagerName,
     flag_descriptions::kEnableBrowsingDataLifetimeManagerDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableBrowsingDataLifetimeManager)},

    {"privacy-advisor", flag_descriptions::kPrivacyAdvisorName,
     flag_descriptions::kPrivacyAdvisorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrivacyAdvisor)},

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
    {"incognito-screenshot", flag_descriptions::kIncognitoScreenshotName,
     flag_descriptions::kIncognitoScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kIncognitoScreenshot)},
#endif
    {"incognito-downloads-warning",
     flag_descriptions::kIncognitoDownloadsWarningName,
     flag_descriptions::kIncognitoDownloadsWarningDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kIncognitoDownloadsWarning)},

    {"incognito-ntp-revamp", flag_descriptions::kIncognitoNtpRevampName,
     flag_descriptions::kIncognitoNtpRevampDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kIncognitoNtpRevamp)},

    {"use-first-party-set", flag_descriptions::kUseFirstPartySetName,
     flag_descriptions::kUseFirstPartySetDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(network::switches::kUseFirstPartySet, "")},

    {"check-offline-capability", flag_descriptions::kCheckOfflineCapabilityName,
     flag_descriptions::kCheckOfflineCapabilityDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kCheckOfflineCapability,
                                    kCheckOfflineCapabilityVariations,
                                    "CheckOfflineCapability")},
    {"detect-form-submission-on-form-clear",
     flag_descriptions::kDetectFormSubmissionOnFormClearName,
     flag_descriptions::kDetectFormSubmissionOnFormClearDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kDetectFormSubmissionOnFormClear)},

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
    {"omnibox-trigger-for-prerender2",
     flag_descriptions::kOmniboxTriggerForPrerender2Name,
     flag_descriptions::kOmniboxTriggerForPrerender2Description, kOsAll,
     FEATURE_VALUE_TYPE(features::kOmniboxTriggerForPrerender2)},

#if !defined(OS_ANDROID)
    {"webui-branding-update", flag_descriptions::kWebUIBrandingUpdateName,
     flag_descriptions::kWebUIBrandingUpdateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIBrandingUpdate)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"continuous-search", flag_descriptions::kContinuousSearchName,
     flag_descriptions::kContinuousSearchDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kContinuousSearch,
                                    kContinuousSearchFeatureVariations,
                                    "ContinuousSearchNavigation")},

    {"scroll-capture", flag_descriptions::kScrollCaptureName,
     flag_descriptions::kScrollCaptureDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kScrollCapture,
                                    kScrollCaptureVariations,
                                    "AndroidScrollCapture")},
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
     flag_descriptions::kPdfXfaFormsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfXfaSupport)},
#endif  // BUILDFLAG(ENABLE_PDF)

    {"send-tab-to-self-when-signed-in",
     flag_descriptions::kSendTabToSelfWhenSignedInName,
     flag_descriptions::kSendTabToSelfWhenSignedInDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfWhenSignedIn)},

    {"send-tab-to-self-manage-devices-link",
     flag_descriptions::kSendTabToSelfManageDevicesLinkName,
     flag_descriptions::kSendTabToSelfManageDevicesLinkDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfManageDevicesLink)},

#if defined(OS_ANDROID)
    {"send-tab-to-self-v2", flag_descriptions::kSendTabToSelfV2Name,
     flag_descriptions::kSendTabToSelfV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfV2)},
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
    {"raw-audio-capture", flag_descriptions::kRawAudioCaptureName,
     flag_descriptions::kRawAudioCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kWasapiRawAudioCapture)},
#endif  // defined(OS_WIN)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kPersonalizationHubInternalName,
     flag_descriptions::kPersonalizationHubName,
     flag_descriptions::kPersonalizationHubDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPersonalizationHub)},
    {kWallpaperFullScreenPreviewInternalName,
     flag_descriptions::kWallpaperFullScreenPreviewName,
     flag_descriptions::kWallpaperFullScreenPreviewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperFullScreenPreview)},
    {kWallpaperPerDeskName, flag_descriptions::kWallpaperPerDeskName,
     flag_descriptions::kWallpaperPerDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperPerDesk)},
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

    {"enable-vp9-kSVC-decode-acceleration",
     flag_descriptions::kVp9kSVCHWDecodingName,
     flag_descriptions::kVp9kSVCHWDecodingDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kVp9kSVCHWDecoding)},

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC) || defined(OS_FUCHSIA)
    {
        "ui-debug-tools",
        flag_descriptions::kUIDebugToolsName,
        flag_descriptions::kUIDebugToolsDescription,
        kOsWin | kOsLinux | kOsMac | kOsFuchsia,
        FEATURE_VALUE_TYPE(features::kUIDebugTools),
    },
#endif
    {"http-cache-partitioning",
     flag_descriptions::kSplitCacheByNetworkIsolationKeyName,
     flag_descriptions::kSplitCacheByNetworkIsolationKeyDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS | kOsAndroid | kOsFuchsia,
     FEATURE_VALUE_TYPE(net::features::kSplitCacheByNetworkIsolationKey)},

    {"autofill-address-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptName,
     flag_descriptions::kEnableAutofillAddressSavePromptDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS | kOsAndroid | kOsFuchsia,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAddressProfileSavePrompt)},

    {"detected-source-language-option",
     flag_descriptions::kDetectedSourceLanguageOptionName,
     flag_descriptions::kDetectedSourceLanguageOptionDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(language::kDetectedSourceLanguageOption)},

#if defined(OS_ANDROID)
    {"content-languages-in-language-picker",
     flag_descriptions::kContentLanguagesInLanguagePickerName,
     flag_descriptions::kContentLanguagesInLanguagePickerDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(language::kContentLanguagesInLanguagePicker,
                                    kContentLanguagesInLanguaePickerVariations,
                                    "ContentLanguagesInLanguagePicker")},
#endif

    {"filling-across-affiliated-websites",
     flag_descriptions::kFillingAcrossAffiliatedWebsitesName,
     flag_descriptions::kFillingAcrossAffiliatedWebsitesDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingAcrossAffiliatedWebsites)},

    {"draw-predicted-ink-point", flag_descriptions::kDrawPredictedPointsName,
     flag_descriptions::kDrawPredictedPointsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kDrawPredictedInkPoint,
                                    kDrawPredictedPointVariations,
                                    "DrawPredictedInkPoint")},

    {flag_descriptions::kTabSearchMediaTabsId,
     flag_descriptions::kTabSearchMediaTabsName,
     flag_descriptions::kTabSearchMediaTabsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabSearchMediaTabs,
                                    kTabSearchMediaTabsVariations,
                                    "TabSearchMediaTabs")},

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

    {"fedcm", flag_descriptions::kFedCmName,
     flag_descriptions::kFedCmDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kFedCm,
                                    kFedCmFeatureVariations,
                                    "FedCmFeatureVariations")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"bluetooth-sessionized-metrics",
     flag_descriptions::kBluetoothSessionizedMetricsName,
     flag_descriptions::kBluetoothSessionizedMetricsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(metrics::structured::kBluetoothSessionizedMetrics)},
#endif

    {"subframe-shutdown-delay", flag_descriptions::kSubframeShutdownDelayName,
     flag_descriptions::kSubframeShutdownDelayDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSubframeShutdownDelay)},

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

    {"autofill-enable-virtual-cards-risk-based-authentication",
     flag_descriptions::kAutofillEnableVirtualCardsRiskBasedAuthenticationName,
     flag_descriptions::
         kAutofillEnableVirtualCardsRiskBasedAuthenticationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableVirtualCardsRiskBasedAuthentication)},

    {"autofill-fix-offer-in-incognito",
     flag_descriptions::kAutofillFixOfferInIncognitoName,
     flag_descriptions::kAutofillFixOfferInIncognitoDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillFixOfferInIncognito)},

    {"autofill-highlight-only-changed-value-in-preview-mode",
     flag_descriptions::kAutofillHighlightOnlyChangedValuesInPreviewModeName,
     flag_descriptions::
         kAutofillHighlightOnlyChangedValuesInPreviewModeDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillHighlightOnlyChangedValuesInPreviewMode)},

    {"sanitizer-api", flag_descriptions::kSanitizerApiName,
     flag_descriptions::kSanitizerApiDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kSanitizerAPI)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"productivity-reorder-apps", flag_descriptions::kLauncherAppSortName,
     flag_descriptions::kLauncherAppSortDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLauncherAppSort)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_FUCHSIA)
    {"enable-desktop-pwas-app-icon-shortcuts-menu-ui",
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuUIName,
     flag_descriptions::kDesktopPWAsAppIconShortcutsMenuUIDescription,
     kOsCrOS | kOsMac | kOsLinux | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsAppIconShortcutsMenuUI)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-input-event-logging",
     flag_descriptions::kEnableInputEventLoggingName,
     flag_descriptions::kEnableInputEventLoggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableInputEventLogging)},
#endif

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
    {"enable-input-noise-cancellation-ui",
     flag_descriptions::kEnableInputNoiseCancellationUiName,
     flag_descriptions::kEnableInputNoiseCancellationUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableInputNoiseCancellationUi)},

    {"enable-keyboard-backlight-toggle",
     flag_descriptions::kEnableKeyboardBacklightToggleName,
     flag_descriptions::kEnableKeyboardBacklightToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableKeyboardBacklightToggle)},
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
     kOsMac | kOsLinux | kOsCrOS | kOsAndroid | kOsFuchsia,
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
     flag_descriptions::kPrivacyReviewDescription, kOsDesktop | kOsAndroid,
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

    {"tab-search-fuzzy-search", flag_descriptions::kTabSearchFuzzySearchName,
     flag_descriptions::kTabSearchFuzzySearchDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabSearchFuzzySearch,
                                    kTabSearchSearchThresholdVariations,
                                    "TabSearchFuzzySearch")},

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
    {"enable-phone-hub-call-notification",
     flag_descriptions::kPhoneHubCallNotificationName,
     flag_descriptions::kPhoneHubCallNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPhoneHubCallNotification)},

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

    {"partitioned-cookies", flag_descriptions::kPartitionedCookiesName,
     flag_descriptions::kPartitionedCookiesDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kPartitionedCookies)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kBorealisBigGlInternalName, flag_descriptions::kBorealisBigGlName,
     flag_descriptions::kBorealisBigGlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisBigGl)},
    {kBorealisDiskManagementInternalName,
     flag_descriptions::kBorealisDiskManagementName,
     flag_descriptions::kBorealisDiskManagementDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisDiskManagement)},
    {kBorealisForceBetaClientInternalName,
     flag_descriptions::kBorealisForceBetaClientName,
     flag_descriptions::kBorealisForceBetaClientDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisForceBetaClient)},
    {kBorealisLinuxModeInternalName, flag_descriptions::kBorealisLinuxModeName,
     flag_descriptions::kBorealisLinuxModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisLinuxMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"https-only-mode-setting", flag_descriptions::kHttpsOnlyModeName,
     flag_descriptions::kHttpsOnlyModeDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsOnlyMode)},

#if defined(OS_ANDROID)
    {"dynamic-color-android", flag_descriptions::kDynamicColorAndroidName,
     flag_descriptions::kDynamicColorAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kDynamicColorAndroid,
                                    kDynamicColorAndroidVariations,
                                    "AndroidDynamicColor")},
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

    {"omnibox-updated-connection-security-indicators",
     flag_descriptions::kOmniboxUpdatedConnectionSecurityIndicatorsName,
     flag_descriptions::kOmniboxUpdatedConnectionSecurityIndicatorsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kUpdatedConnectionSecurityIndicators)},

#if defined(OS_ANDROID)
    {"share-usage-ranking", flag_descriptions::kShareUsageRankingName,
     flag_descriptions::kShareUsageRankingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kShareUsageRanking)},
    {"share-usage-ranking-fixed-more",
     flag_descriptions::kShareUsageRankingFixedMoreName,
     flag_descriptions::kShareUsageRankingFixedMoreDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kShareUsageRankingFixedMore)},
    {"swap-android-share-hub-rows",
     flag_descriptions::kSwapAndroidShareHubRowsName,
     flag_descriptions::kSwapAndroidShareHubRowsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(share::kSwapAndroidShareHubRows)},
#endif

#if !defined(OS_ANDROID)
    {"share-context-menu", flag_descriptions::kShareContextMenuName,
     flag_descriptions::kShareContextMenuDescription, kOsAll,
     FEATURE_VALUE_TYPE(share::kShareMenu)},
#endif

    {"enable-drdc", flag_descriptions::kEnableDrDcName,
     flag_descriptions::kEnableDrDcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableDrDc)},
    {"autofill-fill-merchant-promo-code-fields",
     flag_descriptions::kAutofillFillMerchantPromoCodeFieldsName,
     flag_descriptions::kAutofillFillMerchantPromoCodeFieldsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillFillMerchantPromoCodeFields)},

    {"passwords-account-storage-revised-opt-in-flow",
     flag_descriptions::kPasswordsAccountStorageRevisedOptInFlowName,
     flag_descriptions::kPasswordsAccountStorageRevisedOptInFlowDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordsAccountStorageRevisedOptInFlow)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"traffic-counters-settings-ui",
     flag_descriptions::kTrafficCountersSettingsUiName,
     flag_descriptions::kTrafficCountersSettingsUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTrafficCountersSettingsUi)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"extensions-menu-access-control",
     flag_descriptions::kExtensionsMenuAccessControlName,
     flag_descriptions::kExtensionsMenuAccessControlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExtensionsMenuAccessControl)},

    {"persistent-quota-is-temporary-quota",
     flag_descriptions::kPersistentQuotaIsTemporaryQuotaName,
     flag_descriptions::kPersistentQuotaIsTemporaryQuotaDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPersistentQuotaIsTemporaryQuota)},

    {"canvas-oop-rasterization", flag_descriptions::kCanvasOopRasterizationName,
     flag_descriptions::kCanvasOopRasterizationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCanvasOopRasterization)},

#if defined(OS_ANDROID)
    {"bookmarks-improved-save-flow",
     flag_descriptions::kBookmarksImprovedSaveFlowName,
     flag_descriptions::kBookmarksImprovedSaveFlowDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBookmarksImprovedSaveFlow)},

    {"bookmarks-refresh", flag_descriptions::kBookmarksRefreshName,
     flag_descriptions::kBookmarksRefreshDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kBookmarksRefresh,
                                    kBookmarksRefreshVariations,
                                    "BookmarksRefresh")},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-app-discovery-for-oobe",
     flag_descriptions::kAppDiscoveryForOobeName,
     flag_descriptions::kAppDiscoveryForOobeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppDiscoveryForOobe)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ambient-mode-new-url", flag_descriptions::kAmbientModeNewUrlName,
     flag_descriptions::kAmbientModeNewUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAmbientModeNewUrl)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"request-desktop-site-global",
     flag_descriptions::kRequestDesktopSiteGlobalName,
     flag_descriptions::kRequestDesktopSiteGlobalDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kRequestDesktopSiteGlobal)},
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"u2f-security-key-api", flag_descriptions::kU2FSecurityKeyAPIName,
     flag_descriptions::kU2FSecurityKeyAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(extensions_features::kU2FSecurityKeyAPI)},
#endif  // ENABLE_EXTENSIONS
    {"force-major-version-to-minor",
     flag_descriptions::kForceMajorVersionInMinorPositionInUserAgentName,
     flag_descriptions::kForceMajorVersionInMinorPositionInUserAgentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kForceMajorVersionInMinorPositionInUserAgent)},
    {"force-major-version-to-100",
     flag_descriptions::kForceMajorVersion100InUserAgentName,
     flag_descriptions::kForceMajorVersion100InUserAgentDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kForceMajorVersion100InUserAgent)},
    {"force-minor-version-to-100",
     flag_descriptions::kForceMinorVersion100InUserAgentName,
     flag_descriptions::kForceMinorVersion100InUserAgentDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kForceMinorVersion100InUserAgent)},
    {"autofill-enable-offer-notification-for-promo-codes",
     flag_descriptions::kAutofillEnableOfferNotificationForPromoCodesName,
     flag_descriptions::
         kAutofillEnableOfferNotificationForPromoCodesDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableOfferNotificationForPromoCodes)},

    {"u2f-permission-prompt", flag_descriptions::kU2FPermissionPromptName,
     flag_descriptions::kU2FPermissionPromptDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(device::kU2fPermissionPrompt)},
    {"upcoming-sharing-features",
     flag_descriptions::kUpcomingSharingFeaturesName,
     flag_descriptions::kUpcomingSharingFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(share::kUpcomingSharingFeatures)},

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
    {"side-search", flag_descriptions::kSideSearchName,
     flag_descriptions::kSideSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSideSearch)},

    {"side-search-clear-cache-when-closed",
     flag_descriptions::kSideSearchClearCacheWhenClosedName,
     flag_descriptions::kSideSearchClearCacheWhenClosedDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSideSearchClearCacheWhenClosed)},

    {"side-search-state-per-tab", flag_descriptions::kSideSearchStatePerTabName,
     flag_descriptions::kSideSearchStatePerTabDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSideSearchStatePerTab)},
#endif  // BUILDFLAG(ENABLE_SIDE_SEARCH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-component-updater-test-request",
     flag_descriptions::kComponentUpdaterTestRequestName,
     flag_descriptions::kComponentUpdaterTestRequestDescription, kOsCrOS,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kComponentUpdater,
                                 component_updater::kSwitchTestRequestParam)},
#endif

    {"enable-raw-draw", flag_descriptions::kEnableRawDrawName,
     flag_descriptions::kEnableRawDrawDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRawDraw)},

    {"web-midi", flag_descriptions::kWebMidiName,
     flag_descriptions::kWebMidiDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebMidi)},
#if defined(OS_ANDROID)
    {"use-real-color-space-for-android-video",
     flag_descriptions::kUseRealColorSpaceForAndroidVideoName,
     flag_descriptions::kUseRealColorSpaceForAndroidVideoDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(media::kUseRealColorSpaceForAndroidVideo)},
#endif

#if defined(OS_WIN)
    {"win11-style-menus", flag_descriptions::kWin11StyleMenusName,
     flag_descriptions::kWin11StyleMenusDescription, kOsWin,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kWin11StyleMenus,
                                    kWin11StyleMenusVariations,
                                    "Win11StyleMenus")},
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desks-trackpad-swipe-improvements",
     flag_descriptions::kDesksTrackpadSwipeImprovementsName,
     flag_descriptions::kDesksTrackpadSwipeImprovementsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableDesksTrackpadSwipeImprovements)},
#endif

    {"enable-cascade-layers", flag_descriptions::kCSSCascadeLayersName,
     flag_descriptions::kCSSCascadeLayersDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCSSCascadeLayers)},

    {"enable-commerce-developer", flag_descriptions::kCommerceDeveloperName,
     flag_descriptions::kCommerceDeveloperDescription, kOsAll,
     FEATURE_VALUE_TYPE(commerce::kCommerceDeveloper)},

    {"bluetooth-bond-on-demand",
     flag_descriptions::kWebBluetoothBondOnDemandName,
     flag_descriptions::kWebBluetoothBondOnDemandDescription, kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebBluetoothBondOnDemand)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-libinput-to-handle-touchpad",
     flag_descriptions::kEnableLibinputToHandleTouchpadName,
     flag_descriptions::kEnableLibinputToHandleTouchpadDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kLibinputHandleTouchpad)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-sharesheet-copy-to-clipboard",
     flag_descriptions::kSharesheetCopyToClipboardName,
     flag_descriptions::kSharesheetCopyToClipboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSharesheetCopyToClipboard)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-idle-inhibit", flag_descriptions::kEnableIdleInhibitName,
     flag_descriptions::kEnableIdleInhibitDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableIdleInhibit)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
    {"enable-chrome-management-page-android",
     flag_descriptions::kChromeManagementPageAndroidName,
     flag_descriptions::kChromeManagementPageAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(policy::features::kChromeManagementPageAndroid)},

    {"context-menu-popup-style", flag_descriptions::kContextMenuPopupStyleName,
     flag_descriptions::kContextMenuPopupStyleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuPopupStyle)},

    {"grid-tab-switcher-for-tablets",
     flag_descriptions::kGridTabSwitcherForTabletsName,
     flag_descriptions::kGridTabSwitcherForTabletsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kGridTabSwitcherForTablets)},

    {"enable-tab-groups-for-tablets",
     flag_descriptions::kTabGroupsForTabletsName,
     flag_descriptions::kTabGroupsForTabletsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabGroupsForTablets)},

    {"activate-metrics-reporting-enabled-policy",
     flag_descriptions::kActivateMetricsReportingEnabledPolicyAndroidName,
     flag_descriptions::
         kActivateMetricsReportingEnabledPolicyAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         policy::features::kActivateMetricsReportingEnabledPolicyAndroid)},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desks-templates", flag_descriptions::kDesksTemplatesName,
     flag_descriptions::kDesksTemplatesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDesksTemplates)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"default-link-capturing-in-browser",
     flag_descriptions::kDefaultLinkCapturingInBrowserName,
     flag_descriptions::kDefaultLinkCapturingInBrowserDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDefaultLinkCapturingInBrowser)},
#endif

    {"large-favicon-from-google",
     flag_descriptions::kLargeFaviconFromGoogleName,
     flag_descriptions::kLargeFaviconFromGoogleDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kLargeFaviconFromGoogle,
                                    kLargeFaviconFromGoogleVariations,
                                    "LargeFaviconFromGoogle")},
#if defined(OS_ANDROID)
    {"web-bluetooth-request-larger-mtu",
     flag_descriptions::kWebBluetoothRequestLargerMtuName,
     flag_descriptions::kWebBluetoothRequestLargerMtuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kWebBluetoothRequestLargerMtu)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"request-desktop-site-exceptions",
     flag_descriptions::kRequestDesktopSiteExceptionsName,
     flag_descriptions::kRequestDesktopSiteExceptionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kRequestDesktopSiteExceptions)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"snooping-protection", flag_descriptions::kSnoopingProtectionName,
     flag_descriptions::kSnoopingProtectionDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kSnoopingProtection,
                                    kSnoopingProtectionVariations,
                                    "SnoopingProtection")},

    {"quick-dim", flag_descriptions::kQuickDimName,
     flag_descriptions::kQuickDimDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kQuickDim,
                                    kQuickDimVariations,
                                    "QuickDim")},
#endif

#if defined(OS_WIN)
    {"pervasive-system-accent-color",
     flag_descriptions::kPervasiveSystemAccentColorName,
     flag_descriptions::kPervasiveSystemAccentColorDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kPervasiveSystemAccentColor)},
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"improve-accessibility-tree-using-local-ml",
     flag_descriptions::kImproveAccessibilityTreeUsingLocalMLName,
     flag_descriptions::kImproveAccessibilityTreeUsingLocalMLDescription,
     kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kImproveAccessibilityTreeUsingLocalML)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"use-multiple-overlays", flag_descriptions::kUseMultipleOverlaysName,
     flag_descriptions::kUseMultipleOverlaysDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kUseMultipleOverlays,
                                    kUseMultipleOverlaysVariations,
                                    "UseMultipleOverlays")},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"usb-notification-controller",
     flag_descriptions::kUsbNotificationControllerName,
     flag_descriptions::kUsbNotificationControllerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUsbNotificationController)},
#endif

#if defined(OS_CHROMEOS)
    {"link-capturing-ui-update", flag_descriptions::kLinkCapturingUiUpdateName,
     flag_descriptions::kLinkCapturingUiUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kLinkCapturingUiUpdate)},
#endif

#if defined(OS_ANDROID)
    {"drag-and-drop-android", flag_descriptions::kDragAndDropAndroidName,
     flag_descriptions::kDragAndDropAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDragAndDropAndroid)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"use-ulp-languages-in-chrome",
     flag_descriptions::kUseULPLanguagesInChromeName,
     flag_descriptions::kUseULPLanguagesInChromeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kUseULPLanguagesInChrome)},
#endif

    {"autofill-enable-update-virtual-card-enrollment",
     flag_descriptions::kAutofillEnableUpdateVirtualCardEnrollmentName,
     flag_descriptions::kAutofillEnableUpdateVirtualCardEnrollmentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableUpdateVirtualCardEnrollment)},

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

  if (!strcmp(kArcVmBalloonPolicyInternalName, entry.internal_name)) {
    return !arc::IsArcVmEnabled();
  }

  // Only show Borealis flags on enabled devices.
  if (!strcmp(kBorealisBigGlInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  if (!strcmp(kBorealisDiskManagementInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  if (!strcmp(kBorealisForceBetaClientInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  if (!strcmp(kBorealisLinuxModeInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  // Only show full screen preview flag if wallpaper flag is enabled.
  if (!strcmp(kWallpaperFullScreenPreviewInternalName, entry.internal_name))
    return !ash::features::IsWallpaperWebUIEnabled();

  // personalization-hub is only available for Unknown/Canary/Dev channels.
  if (!strcmp(kPersonalizationHubInternalName, entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // In order to be considered for Lacros, flags additionally need to be usable
  // on Chrome OS.
  if (!(entry.supported_platforms & (kOsCrOS | kOsCrOSOwnerOnly)))
    return true;
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)

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

#if defined(OS_CHROMEOS)
void CrosUrlFlagsRedirect() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_url_handling::NavigateInAsh(GURL(chrome::kChromeUIFlagsURL));
#else
  // Note: This will only be called by the UI when Lacros is available.
  DCHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->OpenUrl(GURL(chrome::kChromeUIFlagsURL));
#endif
}
#endif

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

base::span<const FeatureEntry> GetFeatureEntries() {
  if (!GetEntriesForTesting()->empty())
    return base::span<FeatureEntry>(*GetEntriesForTesting());
  return base::make_span(kFeatureEntries, base::size(kFeatureEntries));
}

}  // namespace testing

}  // namespace about_flags
