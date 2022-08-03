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
#include "base/feature_list.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_features.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "chrome/browser/ash/android_sms/android_sms_switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
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
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/site_isolation/about_flags.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/browser/ui/app_list/search/search_features.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
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
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_ui/site_settings/android/features.h"
#include "components/browsing_data/core/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/flag_descriptions.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/public.h"
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
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
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
#include "components/optimization_guide/core/optimization_guide_switches.h"
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
#include "components/security_state/core/security_state.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_isolation/features.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
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
#include "device/bluetooth/bluez/bluez_features.h"
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
#include "services/tracing/public/cpp/tracing_features.h"
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/allocator/buildflags.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/process/process.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/reactions/core/reactions_features.h"
#include "components/translate/content/android/translate_message.h"
#include "components/webapps/browser/features.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/memory/memory_ablation_study.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/components/memory/swap_configuration.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/app_restore/features.h"
#include "components/metrics/structured/structured_metrics_features.h"  // nogncheck
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "remoting/host/chromeos/features.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/i18n/icu_mergeable_data_file.h"
#include "chrome/browser/lacros/lacros_url_handling.h"
#include "chrome/common/webui_url_constants.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_features.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_features/supervised_user_features.h"  // nogncheck
#endif  // ENABLE_SUPERVISED_USERS

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_switches.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/titlebar_config.h"
#include "ui/color/color_switches.h"  // nogncheck
#endif

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
using flags_ui::kOsLacros;
using flags_ui::kOsLinux;
using flags_ui::kOsMac;
using flags_ui::kOsWin;

namespace about_flags {

namespace {

const unsigned kOsAll =
    kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid | kOsFuchsia | kOsLacros;
const unsigned kOsDesktop =
    kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsFuchsia | kOsLacros;

#if defined(USE_AURA)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS | kOsFuchsia | kOsLacros;
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

#if BUILDFLAG(IS_WIN)
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
#elif BUILDFLAG(IS_MAC)
const FeatureEntry::Choice kUseAngleChoicesMac[] = {
    {flag_descriptions::kUseAngleDefault, "", ""},
    {flag_descriptions::kUseAngleGL, switches::kUseANGLE,
     gl::kANGLEImplementationOpenGLName},
    {flag_descriptions::kUseAngleMetal, switches::kUseANGLE,
     gl::kANGLEImplementationMetalName}};
#endif

#if BUILDFLAG(IS_WIN)
const FeatureEntry::FeatureParam kDXGIWaitableSwapChain1Frame = {
    "DXGIWaitableSwapChainMaxQueuedFrames", "1"};

const FeatureEntry::FeatureParam kDXGIWaitableSwapChain2Frames = {
    "DXGIWaitableSwapChainMaxQueuedFrames", "2"};

const FeatureEntry::FeatureParam kDXGIWaitableSwapChain3Frames = {
    "DXGIWaitableSwapChainMaxQueuedFrames", "3"};

const FeatureEntry::FeatureVariation kDXGIWaitableSwapChainVariations[] = {
    {"Max 1 Frame", &kDXGIWaitableSwapChain1Frame, 1, nullptr},
    {"Max 2 Frames", &kDXGIWaitableSwapChain2Frames, 1, nullptr},
    {"Max 3 Frames", &kDXGIWaitableSwapChain3Frames, 1, nullptr}};
#endif

#if BUILDFLAG(IS_LINUX)
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

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kElasticOverscrollFilterType[] = {
    {features::kElasticOverscrollType, features::kElasticOverscrollTypeFilter}};
const FeatureEntry::FeatureParam kElasticOverscrollTransformType[] = {
    {features::kElasticOverscrollType,
     features::kElasticOverscrollTypeTransform}};

const FeatureEntry::FeatureVariation kElasticOverscrollVariations[] = {
    {"Pixel shader stretch", kElasticOverscrollFilterType,
     std::size(kElasticOverscrollFilterType), nullptr},
    {"Transform stretch", kElasticOverscrollTransformType,
     std::size(kElasticOverscrollTransformType), nullptr}};

const FeatureEntry::FeatureParam kCCTResizablePolicyParamUseAllowlist[] = {
    {"default_policy", "use-allowlist"}};
const FeatureEntry::FeatureParam kCCTResizablePolicyParamUseDenylist[] = {
    {"default_policy", "use-denylist"}};

const FeatureEntry::FeatureVariation
    kCCTResizableThirdPartiesDefaultPolicyVariations[] = {
        {"Use Allowlist", kCCTResizablePolicyParamUseAllowlist,
         std::size(kCCTResizablePolicyParamUseAllowlist), nullptr},
        {"Use Denylist", kCCTResizablePolicyParamUseDenylist,
         std::size(kCCTResizablePolicyParamUseDenylist), nullptr}};

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
#else   // BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kReaderModeOfferInSettings[] = {
    {switches::kReaderModeDiscoverabilityParamName,
     switches::kReaderModeOfferInSettings}};

const FeatureEntry::FeatureVariation kReaderModeDiscoverabilityVariations[] = {
    {"available in settings", kReaderModeOfferInSettings,
     std::size(kReaderModeOfferInSettings), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
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
     std::size(kAdaptiveButton_AlwaysNone), nullptr},
    {"Always New Tab", kAdaptiveButton_AlwaysNewTab,
     std::size(kAdaptiveButton_AlwaysNewTab), nullptr},
    {"Always Share", kAdaptiveButton_AlwaysShare,
     std::size(kAdaptiveButton_AlwaysShare), nullptr},
    {"Always Voice", kAdaptiveButton_AlwaysVoice,
     std::size(kAdaptiveButton_AlwaysVoice), nullptr},
};

const FeatureEntry::FeatureParam kAdaptiveButtonCustomization_NewTab[] = {
    {"default_segment", "new-tab"},
    {"show_ui_only_after_ready", "false"},
    {"ignore_segmentation_results", "true"}};
const FeatureEntry::FeatureParam kAdaptiveButtonCustomization_Share[] = {
    {"default_segment", "share"},
    {"show_ui_only_after_ready", "false"},
    {"ignore_segmentation_results", "true"}};
const FeatureEntry::FeatureParam kAdaptiveButtonCustomization_Voice[] = {
    {"default_segment", "voice"},
    {"show_ui_only_after_ready", "false"},
    {"ignore_segmentation_results", "true"}};
const FeatureEntry::FeatureVariation
    kAdaptiveButtonInTopToolbarCustomizationVariations[] = {
        {"New Tab", kAdaptiveButtonCustomization_NewTab,
         std::size(kAdaptiveButtonCustomization_NewTab), nullptr},
        {"Share", kAdaptiveButtonCustomization_Share,
         std::size(kAdaptiveButtonCustomization_Share), nullptr},
        {"Voice", kAdaptiveButtonCustomization_Voice,
         std::size(kAdaptiveButtonCustomization_Voice), nullptr},
};

const FeatureEntry::FeatureParam kContextualPageActionPriceTracking_Quiet[] = {
    {"action_chip", "false"},
};
const FeatureEntry::FeatureParam
    kContextualPageActionPriceTracking_ActionChip[] = {
        {"action_chip", "true"},
        {"action_chip_time_ms", "3000"},
};
const FeatureEntry::FeatureParam
    kContextualPageActionPriceTracking_ActionChip_6s[] = {
        {"action_chip", "true"},
        {"action_chip_time_ms", "6000"},
};
const FeatureEntry::FeatureParam
    kContextualPageActionPriceTracking_ActionChip_AltColor[] = {
        {"action_chip", "true"},
        {"action_chip_time_ms", "3000"},
        {"action_chip_with_different_color", "true"},
};
const FeatureEntry::FeatureParam
    kContextualPageActionPriceTracking_ActionChip_AltColor_6s[] = {
        {"action_chip", "true"},
        {"action_chip_time_ms", "6000"},
        {"action_chip_with_different_color", "true"},
};
const FeatureEntry::FeatureVariation
    kContextualPageActionPriceTrackingVariations[] = {
        {"Quiet", kContextualPageActionPriceTracking_Quiet,
         std::size(kContextualPageActionPriceTracking_Quiet), nullptr},
        {"Action Chip", kContextualPageActionPriceTracking_ActionChip,
         std::size(kContextualPageActionPriceTracking_ActionChip), nullptr},
        {"Action Chip - 6s", kContextualPageActionPriceTracking_ActionChip_6s,
         std::size(kContextualPageActionPriceTracking_ActionChip_6s), nullptr},
        {"Action Chip - Alternative Color",
         kContextualPageActionPriceTracking_ActionChip_AltColor,
         std::size(kContextualPageActionPriceTracking_ActionChip_AltColor),
         nullptr},
        {"Action Chip - Alternative Color - 6s",
         kContextualPageActionPriceTracking_ActionChip_AltColor_6s,
         std::size(kContextualPageActionPriceTracking_ActionChip_AltColor_6s),
         nullptr},
};

const FeatureEntry::FeatureParam
    kOmniboxRemoveSuggestionHeaderChevron_AllowCollapse[] = {
        {"allow_group_collapsed_state", "true"}};
const FeatureEntry::FeatureVariation
    kOmniboxRemoveSuggestionHeaderChevronVariations[] = {
        {"AllowCollapse", kOmniboxRemoveSuggestionHeaderChevron_AllowCollapse,
         std::size(kOmniboxRemoveSuggestionHeaderChevron_AllowCollapse),
         nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam
    kAutofillSaveCardUiExperimentEnableCurrentWithUserAvatarAndEmail[] = {
        {"autofill_save_card_ui_experiment_selector_in_number", "3"},
};

const FeatureEntry::FeatureParam
    kAutofillSaveCardUiExperimentEnableEncryptedAndSecure[] = {
        {"autofill_save_card_ui_experiment_selector_in_number", "2"},
};

const FeatureEntry::FeatureParam
    kAutofillSaveCardUiExperimentEnableFasterAndProtected[] = {
        {"autofill_save_card_ui_experiment_selector_in_number", "1"},
};

const FeatureEntry::FeatureVariation kAutofillSaveCardUiExperimentOptions[] = {
    {flag_descriptions::kAutofillSaveCardUiExperimentFasterAndProtected,
     kAutofillSaveCardUiExperimentEnableFasterAndProtected,
     std::size(kAutofillSaveCardUiExperimentEnableFasterAndProtected), nullptr},
    {flag_descriptions::kAutofillSaveCardUiExperimentEncryptedAndSecure,
     kAutofillSaveCardUiExperimentEnableEncryptedAndSecure,
     std::size(kAutofillSaveCardUiExperimentEnableEncryptedAndSecure), nullptr},
    {flag_descriptions::
         kAutofillSaveCardUiExperimentCurrentWithUserAvatarAndEmail,
     kAutofillSaveCardUiExperimentEnableCurrentWithUserAvatarAndEmail,
     std::size(
         kAutofillSaveCardUiExperimentEnableCurrentWithUserAvatarAndEmail),
     nullptr},
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kForceDark_SimpleHsl[] = {
    {"inversion_method", "hsl_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureParam kForceDark_SimpleCielab[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

const FeatureEntry::FeatureParam kForceDark_SimpleRgb[] = {
    {"inversion_method", "rgb_based"},
    {"image_behavior", "none"},
    {"foreground_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

// Keep in sync with the kForceDark_SelectiveImageInversion
// in aw_feature_entries.cc if you tweak these parameters.
const FeatureEntry::FeatureParam kForceDark_SelectiveImageInversion[] = {
    {"inversion_method", "cielab_based"},
    {"image_behavior", "selective"},
    {"foreground_lightness_threshold", "150"},
    {"background_lightness_threshold", "205"}};

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
     std::size(kForceDark_SimpleHsl), nullptr},
    {"with simple CIELAB-based inversion", kForceDark_SimpleCielab,
     std::size(kForceDark_SimpleCielab), nullptr},
    {"with simple RGB-based inversion", kForceDark_SimpleRgb,
     std::size(kForceDark_SimpleRgb), nullptr},
    {"with selective image inversion", kForceDark_SelectiveImageInversion,
     std::size(kForceDark_SelectiveImageInversion), nullptr},
    {"with selective inversion of non-image elements",
     kForceDark_SelectiveElementInversion,
     std::size(kForceDark_SelectiveElementInversion), nullptr},
    {"with selective inversion of everything",
     kForceDark_SelectiveGeneralInversion,
     std::size(kForceDark_SelectiveGeneralInversion), nullptr},
    {"with increased text contrast", kForceDark_IncreaseTextContrast,
     std::size(kForceDark_IncreaseTextContrast), nullptr}};
#endif  // !BUILDFLAG(IS_CHROMEOS)

const FeatureEntry::FeatureParam kMBIModeLegacy[] = {{"mode", "legacy"}};
const FeatureEntry::FeatureParam kMBIModeEnabledPerRenderProcessHost[] = {
    {"mode", "per_render_process_host"}};
const FeatureEntry::FeatureParam kMBIModeEnabledPerSiteInstance[] = {
    {"mode", "per_site_instance"}};

const FeatureEntry::FeatureVariation kMBIModeVariations[] = {
    {"legacy mode", kMBIModeLegacy, std::size(kMBIModeLegacy), nullptr},
    {"per render process host", kMBIModeEnabledPerRenderProcessHost,
     std::size(kMBIModeEnabledPerRenderProcessHost), nullptr},
    {"per site instance", kMBIModeEnabledPerSiteInstance,
     std::size(kMBIModeEnabledPerSiteInstance), nullptr}};

const FeatureEntry::FeatureParam kIntensiveWakeUpThrottlingAfter10Seconds[] = {
    {blink::features::kIntensiveWakeUpThrottling_GracePeriodSeconds_Name,
     "10"}};

const FeatureEntry::FeatureVariation kIntensiveWakeUpThrottlingVariations[] = {
    {"10 seconds after a tab is hidden (facilitates testing)",
     kIntensiveWakeUpThrottlingAfter10Seconds,
     std::size(kIntensiveWakeUpThrottlingAfter10Seconds), nullptr},
};

const FeatureEntry::FeatureParam kFencedFramesImplementationTypeShadowDOM[] = {
    {"implementation_type", "shadow_dom"}};
const FeatureEntry::FeatureParam kFencedFramesImplementationTypeMPArch[] = {
    {"implementation_type", "mparch"}};

const FeatureEntry::FeatureVariation
    kFencedFramesImplementationTypeVariations[] = {
        {"with ShadowDOM", kFencedFramesImplementationTypeShadowDOM,
         std::size(kFencedFramesImplementationTypeShadowDOM), nullptr},
        {"with multiple page architecture",
         kFencedFramesImplementationTypeMPArch,
         std::size(kFencedFramesImplementationTypeMPArch), nullptr}};

const FeatureEntry::FeatureParam kSearchSuggestionPrerenderUsingPrefetch[] = {
    {"implementation_type", "use_prefetch"}};
const FeatureEntry::FeatureParam kSearchSuggestionPrerenderIgnoringPrefetch[] =
    {{"implementation_type", "ignore_prefetch"}};

const FeatureEntry::FeatureVariation
    kSearchSuggsetionPrerenderTypeVariations[] = {
        {"use prefetched request", kSearchSuggestionPrerenderUsingPrefetch,
         std::size(kSearchSuggestionPrerenderUsingPrefetch), nullptr},
        {"ignore prefetched request",
         kSearchSuggestionPrerenderIgnoringPrefetch,
         std::size(kSearchSuggestionPrerenderIgnoringPrefetch), nullptr}};

#if BUILDFLAG(IS_ANDROID)
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
     std::size(kCloseTabSuggestionsStale_Immediate), nullptr},
    {"Group+Close Immediate", kGroupAndCloseTabSuggestions_Immediate,
     std::size(kGroupAndCloseTabSuggestions_Immediate), nullptr},
    {"4 hours", kCloseTabSuggestionsStale_4Hours,
     std::size(kCloseTabSuggestionsStale_4Hours), nullptr},
    {"8 hours", kCloseTabSuggestionsStale_8Hours,
     std::size(kCloseTabSuggestionsStale_8Hours), nullptr},
    {"7 days", kCloseTabSuggestionsStale_7Days,
     std::size(kCloseTabSuggestionsStale_7Days), nullptr},
    {"Time & Site Engagement", kCloseTabSuggestionsTimeSiteEngagement,
     std::size(kCloseTabSuggestionsTimeSiteEngagement), nullptr},
};

const FeatureEntry::FeatureParam kLongScreenshot_AutoscrollDragSlow[] = {
    {"autoscroll", "1"}};
const FeatureEntry::FeatureParam kLongScreenshot_AutoscrollDragQuick[] = {
    {"autoscroll", "2"}};
const FeatureEntry::FeatureVariation kLongScreenshotVariations[] = {
    {"Autoscroll Experiment 1", kLongScreenshot_AutoscrollDragSlow,
     std::size(kLongScreenshot_AutoscrollDragSlow), nullptr},
    {"Autoscroll Experiment 2", kLongScreenshot_AutoscrollDragQuick,
     std::size(kLongScreenshot_AutoscrollDragQuick), nullptr}};

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
     std::size(kShowSingleRowMVTiles), nullptr},
    {"(show two rows of MV tiles)", kShowTwoRowsMVTiles,
     std::size(kShowTwoRowsMVTiles), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

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
const FeatureEntry::Choice kUXStudy1Choices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {memory::kUXStudy1A, memory::kUXStudy1Switch, memory::kUXStudy1A},
    {memory::kUXStudy1B, memory::kUXStudy1Switch, memory::kUXStudy1B},
    {memory::kUXStudy1C, memory::kUXStudy1Switch, memory::kUXStudy1C},
    {memory::kUXStudy1D, memory::kUXStudy1Switch, memory::kUXStudy1D},
};

const char kLacrosAvailabilityIgnoreInternalName[] =
    "lacros-availability-ignore";
const char kLacrosOnlyInternalName[] = "lacros-only";
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

const FeatureEntry::Choice kLacrosAvailabilityPolicyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {crosapi::browser_util::kLacrosAvailabilityPolicyUserChoice,
     crosapi::browser_util::kLacrosAvailabilityPolicySwitch,
     crosapi::browser_util::kLacrosAvailabilityPolicyUserChoice},
    {crosapi::browser_util::kLacrosAvailabilityPolicyLacrosDisabled,
     crosapi::browser_util::kLacrosAvailabilityPolicySwitch,
     crosapi::browser_util::kLacrosAvailabilityPolicyLacrosDisabled},
    {crosapi::browser_util::kLacrosAvailabilityPolicySideBySide,
     crosapi::browser_util::kLacrosAvailabilityPolicySwitch,
     crosapi::browser_util::kLacrosAvailabilityPolicySideBySide},
    {crosapi::browser_util::kLacrosAvailabilityPolicyLacrosPrimary,
     crosapi::browser_util::kLacrosAvailabilityPolicySwitch,
     crosapi::browser_util::kLacrosAvailabilityPolicyLacrosPrimary},
    {crosapi::browser_util::kLacrosAvailabilityPolicyLacrosOnly,
     crosapi::browser_util::kLacrosAvailabilityPolicySwitch,
     crosapi::browser_util::kLacrosAvailabilityPolicyLacrosOnly},
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kDynamicSearchUpdateAnimationDuration_50[] = {
    {"search_result_translation_duration", "50"}};
const FeatureEntry::FeatureParam kDynamicSearchUpdateAnimationDuration_100[] = {
    {"search_result_translation_duration", "100"}};
const FeatureEntry::FeatureParam kDynamicSearchUpdateAnimationDuration_150[] = {
    {"search_result_translation_duration", "150"}};

const FeatureEntry::FeatureVariation kDynamicSearchUpdateAnimationVariations[] =
    {{"50ms", kDynamicSearchUpdateAnimationDuration_50,
      std::size(kDynamicSearchUpdateAnimationDuration_50), nullptr},
     {"100ms", kDynamicSearchUpdateAnimationDuration_100,
      std::size(kDynamicSearchUpdateAnimationDuration_100), nullptr},
     {"150ms", kDynamicSearchUpdateAnimationDuration_150,
      std::size(kDynamicSearchUpdateAnimationDuration_150), nullptr}};
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
    {"write_to_history_service", "true"},
};
const FeatureEntry::FeatureParam kPageContentAnnotationsTitleParams[] = {
    {"annotate_title_instead_of_page_content", "true"},
    {"extract_related_searches", "true"},
    {"write_to_history_service", "true"},
};
const FeatureEntry::FeatureVariation kPageContentAnnotationsVariations[] = {
    {"All Annotations and Persistence on Content",
     kPageContentAnnotationsContentParams,
     std::size(kPageContentAnnotationsContentParams), nullptr},
    {"All Annotations and Persistence on Title",
     kPageContentAnnotationsTitleParams,
     std::size(kPageContentAnnotationsTitleParams), nullptr},
};
const FeatureEntry::FeatureParam
    kPageEntitiesPageContentAnnotationsAllLocalesParams[] = {
        {"supported_locales", "*"},
};
const FeatureEntry::FeatureVariation
    kPageEntitiesPageContentAnnotationsVariations[] = {
        {"All Supported Locales",
         kPageEntitiesPageContentAnnotationsAllLocalesParams,
         std::size(kPageEntitiesPageContentAnnotationsAllLocalesParams),
         nullptr},
};
const FeatureEntry::FeatureParam
    kJourneysSortClustersWithinBatchForQueryParams[] = {
        {"JourneysLocaleOrLanguageAllowlist", "*"},
        {"JourneysSortClustersWithinBatchForQuery", "true"},
};
const FeatureEntry::FeatureParam kJourneysDropHiddenVisitsParams[] = {
    {"JourneysLocaleOrLanguageAllowlist", "*"},
    {"drop_hidden_visits", "true"},
};
const FeatureEntry::FeatureParam kJourneysShowAllVisitsParams[] = {
    {"JourneysLocaleOrLanguageAllowlist", "*"},
    // To show all visits, set the number of visits above the fold to a very
    // high number. We drop the rest above this very high number because we
    // definitely don't want to surface a Show More UI after that number.
    {"JourneysNumVisitsToAlwaysShowAboveTheFold", "200"},
    {"drop_hidden_visits", "true"},
};
const FeatureEntry::FeatureParam kJourneysAllLocalesParams[] = {
    {"JourneysLocaleOrLanguageAllowlist", "*"},
};
const FeatureEntry::FeatureVariation kJourneysVariations[] = {
    {"Sort Clusters Within Batch for Query",
     kJourneysSortClustersWithinBatchForQueryParams,
     std::size(kJourneysSortClustersWithinBatchForQueryParams), nullptr},
    {"No 'Show More' - Drop hidden visits", kJourneysDropHiddenVisitsParams,
     std::size(kJourneysDropHiddenVisitsParams), nullptr},
    {"No 'Show More' - Show all visits", kJourneysShowAllVisitsParams,
     std::size(kJourneysShowAllVisitsParams), nullptr},
    {"All Supported Locales", kJourneysAllLocalesParams,
     std::size(kJourneysAllLocalesParams), nullptr},
};
const FeatureEntry::FeatureParam kJourneysOmniboxActionOnAllURLsParams[] = {
    {"omnibox_action_on_urls", "true"},
    {"omnibox_action_on_noisy_urls", "true"},
    {"omnibox_action_on_navigation_intents", "true"},
    {"omnibox_action_with_pedals", "true"},
};
const FeatureEntry::FeatureParam kJourneysOmniboxActionOnNonNoisyURLsParams[] =
    {
        {"omnibox_action_on_urls", "true"},
        {"omnibox_action_on_noisy_urls", "false"},
        {"omnibox_action_on_navigation_intents", "true"},
        {"omnibox_action_with_pedals", "true"},
};
const FeatureEntry::FeatureParam
    kJourneysOmniboxActionOnNavigationIntentsParams[] = {
        {"omnibox_action_on_urls", "false"},
        {"omnibox_action_on_noisy_urls", "false"},
        {"omnibox_action_on_navigation_intents", "true"},
        {"omnibox_action_with_pedals", "false"},
};
const FeatureEntry::FeatureParam kJourneysOmniboxActionWithPedalsParams[] = {
    {"omnibox_action_on_urls", "false"},
    {"omnibox_action_on_noisy_urls", "false"},
    {"omnibox_action_on_navigation_intents", "false"},
    {"omnibox_action_with_pedals", "true"},
};
const FeatureEntry::FeatureVariation kJourneysOmniboxActionVariations[] = {
    {"Action Chips on All URLs", kJourneysOmniboxActionOnAllURLsParams,
     std::size(kJourneysOmniboxActionOnAllURLsParams), nullptr},
    {"Action Chips on Non-Noisy URLs",
     kJourneysOmniboxActionOnNonNoisyURLsParams,
     std::size(kJourneysOmniboxActionOnNonNoisyURLsParams), nullptr},
    {"Action Chips Enabled on Navigation Intents",
     kJourneysOmniboxActionOnNavigationIntentsParams,
     std::size(kJourneysOmniboxActionOnNavigationIntentsParams), nullptr},
    {"Action Chips Enabled with Pedals", kJourneysOmniboxActionWithPedalsParams,
     std::size(kJourneysOmniboxActionWithPedalsParams), nullptr},
};
const FeatureEntry::FeatureParam kJourneysLabelsWithEntitiesParams[] = {
    {"labels_from_entities", "true"},
};
const FeatureEntry::FeatureParam
    kJourneysLabelsWithEntitiesNoHostnamesParams[] = {
        {"labels_from_hostnames", "false"},
        {"labels_from_entities", "true"},
};
const FeatureEntry::FeatureVariation kJourneysLabelsVariations[] = {
    {"With Entities", kJourneysLabelsWithEntitiesParams,
     std::size(kJourneysLabelsWithEntitiesParams), nullptr},
    {"With Entities, No Hostnames",
     kJourneysLabelsWithEntitiesNoHostnamesParams,
     std::size(kJourneysLabelsWithEntitiesNoHostnamesParams), nullptr},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringNoContentClusteringParams[] = {
        {"content_clustering_enabled", "false"},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringContentClusteringParams[] = {
        {"content_clustering_enabled", "true"},
};
const FeatureEntry::FeatureParam kJourneysShowSingleDomainClustersParams[] = {
    {"hide_single_domain_clusters_on_prominent_ui_surfaces", "false"},
};
const FeatureEntry::FeatureParam kJourneysHideSingleDomainClustersParams[] = {
    {"hide_single_domain_clusters_on_prominent_ui_surfaces", "true"},
};
const FeatureEntry::FeatureVariation kJourneysOnDeviceClusteringVariations[] = {
    {"No Content Clustering",
     kJourneysOnDeviceClusteringNoContentClusteringParams,
     std::size(kJourneysOnDeviceClusteringNoContentClusteringParams), nullptr},
    {"Content Clustering", kJourneysOnDeviceClusteringContentClusteringParams,
     std::size(kJourneysOnDeviceClusteringContentClusteringParams), nullptr},
    {"Show Single Domain Journeys", kJourneysShowSingleDomainClustersParams,
     std::size(kJourneysShowSingleDomainClustersParams), nullptr},
    {"Hide Single Domain Journeys", kJourneysHideSingleDomainClustersParams,
     std::size(kJourneysHideSingleDomainClustersParams), nullptr},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringKeywordFilteringAllVariationsParams[] = {
        {"keyword_filter_on_categories", "false"},
        {"keyword_filter_on_noisy_visits", "false"},
        {"keyword_filter_on_visit_hosts", "false"},
        {"keyword_filter_on_search_terms", "true"},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringKeywordFilteringNoCategoriesParams[] = {
        {"keyword_filter_on_categories", "false"},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringKeywordFilteringNoNoisyVisitsParams[] = {
        {"keyword_filter_on_noisy_visits", "false"},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringKeywordFilteringNoVisitHostsParams[] = {
        {"keyword_filter_on_visit_hosts", "false"},
};
const FeatureEntry::FeatureParam
    kJourneysOnDeviceClusteringKeywordFilteringWithSearchTermsParams[] = {
        {"keyword_filter_on_search_terms", "true"},
};
const FeatureEntry::FeatureVariation
    kJourneysOnDeviceClusteringKeywordFilteringVariations[] = {
        {"All Variations",
         kJourneysOnDeviceClusteringKeywordFilteringAllVariationsParams,
         std::size(
             kJourneysOnDeviceClusteringKeywordFilteringAllVariationsParams),
         nullptr},
        {"No Categories",
         kJourneysOnDeviceClusteringKeywordFilteringNoCategoriesParams,
         std::size(
             kJourneysOnDeviceClusteringKeywordFilteringNoCategoriesParams),
         nullptr},
        {"No Noisy Visits",
         kJourneysOnDeviceClusteringKeywordFilteringNoNoisyVisitsParams,
         std::size(
             kJourneysOnDeviceClusteringKeywordFilteringNoNoisyVisitsParams),
         nullptr},
        {"No Visit Hosts",
         kJourneysOnDeviceClusteringKeywordFilteringNoVisitHostsParams,
         std::size(
             kJourneysOnDeviceClusteringKeywordFilteringNoVisitHostsParams),
         nullptr},
        {"With Search Terms",
         kJourneysOnDeviceClusteringKeywordFilteringWithSearchTermsParams,
         std::size(
             kJourneysOnDeviceClusteringKeywordFilteringWithSearchTermsParams),
         nullptr},
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
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
     std::size(kOmniboxDocumentProviderServerScoring), nullptr},
    {"server scores capped by rank",
     kOmniboxDocumentProviderServerScoringCappedByRank,
     std::size(kOmniboxDocumentProviderServerScoringCappedByRank), nullptr},
    {"client scores", kOmniboxDocumentProviderClientScoring,
     std::size(kOmniboxDocumentProviderClientScoring), nullptr},
    {"server and client scores", kOmniboxDocumentProviderServerAndClientScoring,
     std::size(kOmniboxDocumentProviderServerAndClientScoring), nullptr}};

// A limited number of combinations of the rich autocompletion params.
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
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionAggressive2[] = {
    {"RichAutocompletionAutocompleteTitlesShortcutProvider", "true"},
    {"RichAutocompletionAutocompleteTitlesMinChar", "2"},
    {"RichAutocompletionAutocompleteShortcutText", "true"},
    {"RichAutocompletionAutocompleteShortcutTextMinChar", "2"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionAggressive3[] = {
    {"RichAutocompletionAutocompleteTitlesShortcutProvider", "true"},
    {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
    {"RichAutocompletionAutocompleteShortcutText", "true"},
    {"RichAutocompletionAutocompleteShortcutTextMinChar", "3"}};
const FeatureEntry::FeatureParam kOmniboxRichAutocompletionAggressive4[] = {
    {"RichAutocompletionAutocompleteTitlesShortcutProvider", "true"},
    {"RichAutocompletionAutocompleteTitlesMinChar", "4"},
    {"RichAutocompletionAutocompleteShortcutText", "true"},
    {"RichAutocompletionAutocompleteShortcutTextMinChar", "4"}};

const FeatureEntry::FeatureVariation
    kOmniboxRichAutocompletionPromisingVariations[] = {
        {"Conservative Moderate - Title, Shortcut Non-Prefix, min 3/5",
         kOmniboxRichAutocompletionConservativeModerate,
         std::size(kOmniboxRichAutocompletionConservativeModerate), nullptr},
        {"Conservative Moderate 2 - Shortcut Title, Shortcut Non-Prefix, min "
         "3/5",
         kOmniboxRichAutocompletionConservativeModerate2,
         std::size(kOmniboxRichAutocompletionConservativeModerate2), nullptr},
        {"Aggressive 2 - Title Shortcut Title 2, Shortcut Text 2",
         kOmniboxRichAutocompletionAggressive2,
         std::size(kOmniboxRichAutocompletionAggressive2), nullptr},
        {"Aggressive 3 - Title Shortcut Title 3, Shortcut Text 3",
         kOmniboxRichAutocompletionAggressive3,
         std::size(kOmniboxRichAutocompletionAggressive3), nullptr},
        {"Aggressive 4 - Title Shortcut Title 4, Shortcut Text 4",
         kOmniboxRichAutocompletionAggressive4,
         std::size(kOmniboxRichAutocompletionAggressive4), nullptr},
};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)

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
     std::size(kOmniboxBookmarkPathsReplaceTitle), nullptr},
    {"Replace URL (Title - Path)", kOmniboxBookmarkPathsReplaceUrl,
     std::size(kOmniboxBookmarkPathsReplaceUrl), nullptr},
    {"Append after title (Title : Path - URL)",
     kOmniboxBookmarkPathsAppendAfterTitle,
     std::size(kOmniboxBookmarkPathsAppendAfterTitle), nullptr},
    {"Dynamic Replace URL (Title - Path|URL)",
     kOmniboxBookmarkPathsDynamicReplaceUrl,
     std::size(kOmniboxBookmarkPathsDynamicReplaceUrl), nullptr}};

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
    {"5", kMaxZeroSuggestMatches5, std::size(kMaxZeroSuggestMatches5), nullptr},
    {"6", kMaxZeroSuggestMatches6, std::size(kMaxZeroSuggestMatches6), nullptr},
    {"7", kMaxZeroSuggestMatches7, std::size(kMaxZeroSuggestMatches7), nullptr},
    {"8", kMaxZeroSuggestMatches8, std::size(kMaxZeroSuggestMatches8), nullptr},
    {"9", kMaxZeroSuggestMatches9, std::size(kMaxZeroSuggestMatches9), nullptr},
    {"10", kMaxZeroSuggestMatches10, std::size(kMaxZeroSuggestMatches10),
     nullptr},
    {"11", kMaxZeroSuggestMatches11, std::size(kMaxZeroSuggestMatches11),
     nullptr},
    {"12", kMaxZeroSuggestMatches12, std::size(kMaxZeroSuggestMatches12),
     nullptr},
    {"13", kMaxZeroSuggestMatches13, std::size(kMaxZeroSuggestMatches13),
     nullptr},
    {"14", kMaxZeroSuggestMatches14, std::size(kMaxZeroSuggestMatches14),
     nullptr},
    {"15", kMaxZeroSuggestMatches15, std::size(kMaxZeroSuggestMatches15),
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
         std::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         std::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         std::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         std::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"7 matches", kOmniboxUIMaxAutocompleteMatches7,
         std::size(kOmniboxUIMaxAutocompleteMatches7), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         std::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"9 matches", kOmniboxUIMaxAutocompleteMatches9,
         std::size(kOmniboxUIMaxAutocompleteMatches9), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         std::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         std::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

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
    {"2 matches", kOmniboxMaxURLMatches2, std::size(kOmniboxMaxURLMatches2),
     nullptr},
    {"3 matches", kOmniboxMaxURLMatches3, std::size(kOmniboxMaxURLMatches3),
     nullptr},
    {"4 matches", kOmniboxMaxURLMatches4, std::size(kOmniboxMaxURLMatches4),
     nullptr},
    {"5 matches", kOmniboxMaxURLMatches5, std::size(kOmniboxMaxURLMatches5),
     nullptr},
    {"6 matches", kOmniboxMaxURLMatches6, std::size(kOmniboxMaxURLMatches6),
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
         std::size(kOmniboxDynamicMaxAutocomplete90), nullptr},
        {"9 suggestions if 1 or fewer URLs", kOmniboxDynamicMaxAutocomplete91,
         std::size(kOmniboxDynamicMaxAutocomplete91), nullptr},
        {"9 suggestions if 2 or fewer URLs", kOmniboxDynamicMaxAutocomplete92,
         std::size(kOmniboxDynamicMaxAutocomplete92), nullptr},
        {"10 suggestions if 0 or fewer URLs", kOmniboxDynamicMaxAutocomplete100,
         std::size(kOmniboxDynamicMaxAutocomplete100), nullptr},
        {"10 suggestions if 1 or fewer URLs", kOmniboxDynamicMaxAutocomplete101,
         std::size(kOmniboxDynamicMaxAutocomplete101), nullptr},
        {"10 suggestions if 2 or fewer URLs", kOmniboxDynamicMaxAutocomplete102,
         std::size(kOmniboxDynamicMaxAutocomplete102), nullptr}};

const FeatureEntry::FeatureParam
    kOrganicRepeatableQueriesCappedWithHighPrivilege[] = {
        {"MaxNumRepeatableQueries", "4"},
        {"ScaleRepeatableQueriesScores", "true"},
        {"PrivilegeRepeatableQueries", "true"}};
const FeatureEntry::FeatureParam
    kOrganicRepeatableQueriesCappedWithLowPrivilege[] = {
        {"MaxNumRepeatableQueries", "4"},
        {"ScaleRepeatableQueriesScores", "true"},
        {"PrivilegeRepeatableQueries", "false"}};
const FeatureEntry::FeatureParam
    kOrganicRepeatableQueriesUncappedWithHighPrivilege[] = {
        {"ScaleRepeatableQueriesScores", "true"},
        {"PrivilegeRepeatableQueries", "true"}};
const FeatureEntry::FeatureParam
    kOrganicRepeatableQueriesUncappedWithLowPrivilege[] = {
        {"ScaleRepeatableQueriesScores", "true"},
        {"PrivilegeRepeatableQueries", "false"}};

const FeatureEntry::FeatureVariation kOrganicRepeatableQueriesVariations[] = {
    {"- No max, High privilege",
     kOrganicRepeatableQueriesUncappedWithHighPrivilege,
     std::size(kOrganicRepeatableQueriesUncappedWithHighPrivilege), nullptr},
    {"- No max, Low privilege",
     kOrganicRepeatableQueriesUncappedWithLowPrivilege,
     std::size(kOrganicRepeatableQueriesUncappedWithLowPrivilege), nullptr},
    {"- Max 4, High privilege",
     kOrganicRepeatableQueriesCappedWithHighPrivilege,
     std::size(kOrganicRepeatableQueriesCappedWithHighPrivilege), nullptr},
    {"- Max 4, Low privilege", kOrganicRepeatableQueriesCappedWithLowPrivilege,
     std::size(kOrganicRepeatableQueriesCappedWithLowPrivilege), nullptr},
};

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
     std::size(kMinimumTabWidthSettingPinned), nullptr},
    {" - tabs shrink to a medium width", kMinimumTabWidthSettingMedium,
     std::size(kMinimumTabWidthSettingMedium), nullptr},
    {" - tabs shrink to a large width", kMinimumTabWidthSettingLarge,
     std::size(kMinimumTabWidthSettingLarge), nullptr},
    {" - tabs don't shrink", kMinimumTabWidthSettingFull,
     std::size(kMinimumTabWidthSettingFull), nullptr}};

const FeatureEntry::FeatureParam kAlsoShowMediaTabsinOpenTabsSection[] = {
    {features::kTabSearchAlsoShowMediaTabsinOpenTabsSectionParameterName,
     "true"}};

const FeatureEntry::FeatureVariation kTabSearchMediaTabsVariations[] = {
    {" - media tabs also shown in open tabs section",
     kAlsoShowMediaTabsinOpenTabsSection,
     std::size(kAlsoShowMediaTabsinOpenTabsSection), nullptr}};

const FeatureEntry::FeatureParam kTabSearchSearchThresholdSmall[] = {
    {features::kTabSearchSearchThresholdName, "0.3"}};
const FeatureEntry::FeatureParam kTabSearchSearchThresholdMedium[] = {
    {features::kTabSearchSearchThresholdName, "0.6"}};
const FeatureEntry::FeatureParam kTabSearchSearchThresholdLarge[] = {
    {features::kTabSearchSearchThresholdName, "0.8"}};

const FeatureEntry::FeatureVariation kTabSearchSearchThresholdVariations[] = {
    {" - fuzzy level: small", kTabSearchSearchThresholdSmall,
     std::size(kTabSearchSearchThresholdSmall), nullptr},
    {" - fuzzy level: medium", kTabSearchSearchThresholdMedium,
     std::size(kTabSearchSearchThresholdMedium), nullptr},
    {" - fuzzy level: large", kTabSearchSearchThresholdLarge,
     std::size(kTabSearchSearchThresholdLarge), nullptr}};

const FeatureEntry::FeatureParam kTabHoverCardImagesAlternateFormat[] = {
    {features::kTabHoverCardAlternateFormat, "1"}};

const FeatureEntry::FeatureVariation kTabHoverCardImagesVariations[] = {
    {" alternate hover card format", kTabHoverCardImagesAlternateFormat,
     std::size(kTabHoverCardImagesAlternateFormat), nullptr}};

const FeatureEntry::FeatureParam kSharedHighlightingMaxContextWords5[] = {
    {shared_highlighting::kSharedHighlightingRefinedMaxContextWordsName, "5"}};
const FeatureEntry::FeatureParam kSharedHighlightingMaxContextWords10[] = {
    {shared_highlighting::kSharedHighlightingRefinedMaxContextWordsName, "10"}};
const FeatureEntry::FeatureParam kSharedHighlightingMaxContextWords15[] = {
    {shared_highlighting::kSharedHighlightingRefinedMaxContextWordsName, "15"}};
const FeatureEntry::FeatureParam kSharedHighlightingMaxContextWords20[] = {
    {shared_highlighting::kSharedHighlightingRefinedMaxContextWordsName, "20"}};

const FeatureEntry::FeatureVariation
    kSharedHighlightingMaxContextWordsVariations[] = {
        {" - maxContextWords: 5", kSharedHighlightingMaxContextWords5,
         std::size(kSharedHighlightingMaxContextWords5), nullptr},
        {" - maxContextWords: 10", kSharedHighlightingMaxContextWords10,
         std::size(kSharedHighlightingMaxContextWords10), nullptr},
        {" - maxContextWords: 15", kSharedHighlightingMaxContextWords15,
         std::size(kSharedHighlightingMaxContextWords15), nullptr},
        {" - maxContextWords: 20", kSharedHighlightingMaxContextWords20,
         std::size(kSharedHighlightingMaxContextWords20), nullptr}};

#if !BUILDFLAG(IS_ANDROID)

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
     std::size(kNtpChromeCartModuleFakeData), nullptr},
    {"- Abandoned Cart Discount", kNtpChromeCartModuleAbandonedCartDiscount,
     std::size(kNtpChromeCartModuleAbandonedCartDiscount), nullptr},
    {"- Heuristics Improvement", kNtpChromeCartModuleHeuristicsImprovement,
     std::size(kNtpChromeCartModuleHeuristicsImprovement), nullptr},
    {"- RBD and Coupons", kNtpChromeCartModuleRBDAndCouponDiscount,
     std::size(kNtpChromeCartModuleRBDAndCouponDiscount), nullptr},
};

// The following are consent v2 variations in the Chrome Cart module.
const flags_ui::FeatureEntry::FeatureParam kDiscountConsentNtpDialog[] = {
    {commerce::kNtpChromeCartModuleDiscountConsentNtpVariationParam, "3"}};
const flags_ui::FeatureEntry::FeatureParam kDiscountConsentNtpNativeDialog[] = {
    {commerce::kNtpChromeCartModuleDiscountConsentNtpVariationParam, "4"}};

const FeatureEntry::FeatureVariation kDiscountConsentV2Variations[] = {
    {"WebUi Dialog Consent", kDiscountConsentNtpDialog,
     std::size(kDiscountConsentNtpDialog), nullptr},
    {"Native Dialog Consent", kDiscountConsentNtpNativeDialog,
     std::size(kDiscountConsentNtpNativeDialog), nullptr},
};

const FeatureEntry::FeatureParam kNtpRecipeTasksModuleFakeData[] = {
    {ntp_features::kNtpRecipeTasksModuleDataParam, "fake"}};
const FeatureEntry::FeatureParam kNtpRecipeTasksModuleHistorical7Days[] = {
    {ntp_features::kNtpRecipeTasksModuleExperimentGroupParam, "historical-7"}};
const FeatureEntry::FeatureParam kNtpRecipeTasksModuleHistorical14Days[] = {
    {ntp_features::kNtpRecipeTasksModuleExperimentGroupParam, "historical-14"}};
const FeatureEntry::FeatureParam kNtpRecipeTasksModuleMix7Days[] = {
    {ntp_features::kNtpRecipeTasksModuleExperimentGroupParam, "mix-7"}};
const FeatureEntry::FeatureParam kNtpRecipeTasksModuleMix14Days[] = {
    {ntp_features::kNtpRecipeTasksModuleExperimentGroupParam, "mix-14"}};
const FeatureEntry::FeatureVariation kNtpRecipeTasksModuleVariations[] = {
    {"- Fake Data", kNtpRecipeTasksModuleFakeData,
     std::size(kNtpRecipeTasksModuleFakeData), nullptr},
    {"- Historical Arm (7 days)", kNtpRecipeTasksModuleHistorical7Days,
     std::size(kNtpRecipeTasksModuleHistorical7Days), "t3349934"},
    {"- Historical Arm (14 days)", kNtpRecipeTasksModuleHistorical14Days,
     std::size(kNtpRecipeTasksModuleHistorical14Days), "t3349935"},
    {"- Recommended Mix Arm (7 days)", kNtpRecipeTasksModuleMix7Days,
     std::size(kNtpRecipeTasksModuleMix7Days), "t3349936"},
    {"- Recommended Mix Arm (14 days)", kNtpRecipeTasksModuleMix14Days,
     std::size(kNtpRecipeTasksModuleMix14Days), "t3349937"},
};

const FeatureEntry::FeatureParam kNtpDriveModuleFakeData[] = {
    {ntp_features::kNtpDriveModuleDataParam, "fake"}};
const FeatureEntry::FeatureParam kNtpDriveModuleManagedUsersOnly[] = {
    {ntp_features::kNtpDriveModuleManagedUsersOnlyParam, "true"}};
const FeatureEntry::FeatureVariation kNtpDriveModuleVariations[] = {
    {"- Fake Data", kNtpDriveModuleFakeData, std::size(kNtpDriveModuleFakeData),
     nullptr},
    {"- Managed Users Only", kNtpDriveModuleManagedUsersOnly,
     std::size(kNtpDriveModuleManagedUsersOnly), nullptr},
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
     std::size(kNtpPhotosModuleFakeData0), nullptr},
    {" - Fake memories: 1", kNtpPhotosModuleFakeData1,
     std::size(kNtpPhotosModuleFakeData1), nullptr},
    {" - Fake memories: 2", kNtpPhotosModuleFakeData2,
     std::size(kNtpPhotosModuleFakeData2), nullptr},
    {" - Fake memories: 3", kNtpPhotosModuleFakeData3,
     std::size(kNtpPhotosModuleFakeData3), nullptr},
    {" - Fake memories: 4", kNtpPhotosModuleFakeData4,
     std::size(kNtpPhotosModuleFakeData4), nullptr}};

const FeatureEntry::FeatureParam kNtpPhotosModuleOptInRHTitle[] = {
    {ntp_features::kNtpPhotosModuleOptInTitleParam, "0"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleOptInFavoriteTitle[] = {
    {ntp_features::kNtpPhotosModuleOptInTitleParam, "1"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleOptInPersonalizedTitle[] = {
    {ntp_features::kNtpPhotosModuleOptInTitleParam, "2"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleOptInTripsTitle[] = {
    {ntp_features::kNtpPhotosModuleOptInTitleParam, "3"}};

const FeatureEntry::FeatureVariation kNtpPhotosModuleOptInTitleVariations[] = {
    {" - Recent Highlights", kNtpPhotosModuleOptInRHTitle,
     std::size(kNtpPhotosModuleOptInRHTitle), nullptr},
    {" - Favorite people", kNtpPhotosModuleOptInFavoriteTitle,
     std::size(kNtpPhotosModuleOptInFavoriteTitle), nullptr},
    {" - Personalized title", kNtpPhotosModuleOptInPersonalizedTitle,
     std::size(kNtpPhotosModuleOptInPersonalizedTitle), nullptr},
    {" - Trips title", kNtpPhotosModuleOptInTripsTitle,
     std::size(kNtpPhotosModuleOptInTripsTitle), nullptr}};

const FeatureEntry::FeatureParam kNtpPhotosModuleLogo1ArtWork[] = {
    {ntp_features::kNtpPhotosModuleOptInArtWorkParam, "1"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleLogo2ArtWork[] = {
    {ntp_features::kNtpPhotosModuleOptInArtWorkParam, "2"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleIllustrationsArtWork[] = {
    {ntp_features::kNtpPhotosModuleOptInArtWorkParam, "3"}};
const FeatureEntry::FeatureParam kNtpPhotosModuleStockpileArtWork[] = {
    {ntp_features::kNtpPhotosModuleOptInArtWorkParam, "4"}};

const FeatureEntry::FeatureVariation kNtpPhotosModuleOptInArtWorkVariations[] =
    {{" - Artwork with Logo - 1", kNtpPhotosModuleLogo1ArtWork,
      std::size(kNtpPhotosModuleLogo1ArtWork), nullptr},
     {" - Artwork with Logo - 2", kNtpPhotosModuleLogo2ArtWork,
      std::size(kNtpPhotosModuleLogo2ArtWork), nullptr},
     {" - Artwork with Illustrations", kNtpPhotosModuleIllustrationsArtWork,
      std::size(kNtpPhotosModuleIllustrationsArtWork), nullptr},
     {" - Artwork with Stockpile", kNtpPhotosModuleStockpileArtWork,
      std::size(kNtpPhotosModuleStockpileArtWork), nullptr}};

const FeatureEntry::FeatureParam kRealboxMatchOmniboxThemeVar1[] = {
    {ntp_features::kRealboxMatchOmniboxThemeVariantParam, "1"}};
const FeatureEntry::FeatureParam kRealboxMatchOmniboxThemeVar2[] = {
    {ntp_features::kRealboxMatchOmniboxThemeVariantParam, "2"}};

const FeatureEntry::FeatureVariation kRealboxMatchOmniboxThemeVariations[] = {
    {"(NTP background on steady state and Omnibox steady state background on "
     "hover)",
     kRealboxMatchOmniboxThemeVar1, std::size(kRealboxMatchOmniboxThemeVar1),
     nullptr},
    {"(NTP background on steady state and Omnibox active state background on "
     "hover)",
     kRealboxMatchOmniboxThemeVar2, std::size(kRealboxMatchOmniboxThemeVar2),
     nullptr}};

const FeatureEntry::FeatureParam kRealboxMatchSearchboxThemeRoundedCorners[] = {
    {ntp_features::kRealboxMatchSearchboxThemeParam, "1"}};

const FeatureEntry::FeatureVariation kRealboxMatchSearchboxThemeVariations[] = {
    {"(Rounded Corners)", kRealboxMatchSearchboxThemeRoundedCorners,
     std::size(kRealboxMatchSearchboxThemeRoundedCorners), nullptr}};

const FeatureEntry::FeatureParam kNtpSafeBrowsingModuleFastCooldown[] = {
    {ntp_features::kNtpSafeBrowsingModuleCooldownPeriodDaysParam, "0.001"},
    {ntp_features::kNtpSafeBrowsingModuleCountMaxParam, "1"}};
const FeatureEntry::FeatureVariation kNtpSafeBrowsingModuleVariations[] = {
    {"(Fast Cooldown)", kNtpSafeBrowsingModuleFastCooldown,
     std::size(kNtpSafeBrowsingModuleFastCooldown), nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
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
         std::size(kTranslateForceTriggerOnEnglishGeo), nullptr},
        {"(Zero threshold)", kTranslateForceTriggerOnEnglishBackoff,
         std::size(kTranslateForceTriggerOnEnglishBackoff), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kOverridePrefsForHrefTranslateForceAuto[] = {
    {translate::kForceAutoTranslateKey, "true"}};

const FeatureEntry::FeatureVariation
    kOverrideLanguagePrefsForHrefTranslateVariations[] = {
        {"(Force automatic translation of blocked languages for hrefTranslate)",
         kOverridePrefsForHrefTranslateForceAuto,
         std::size(kOverridePrefsForHrefTranslateForceAuto), nullptr}};

const FeatureEntry::FeatureVariation
    kOverrideSitePrefsForHrefTranslateVariations[] = {
        {"(Force automatic translation of blocked sites for hrefTranslate)",
         kOverridePrefsForHrefTranslateForceAuto,
         std::size(kOverridePrefsForHrefTranslateForceAuto), nullptr}};

const FeatureEntry::FeatureParam
    kOverrideUnsupportedPageLanguageForHrefTranslateForceAuto[] = {
        {"force-auto-translate-for-unsupported-page-language", "true"}};

const FeatureEntry::FeatureVariation
    kOverrideUnsupportedPageLanguageForHrefTranslateVariations[] = {
        {"(Force automatic translation of pages with unknown language for "
         "hrefTranslate)",
         kOverrideUnsupportedPageLanguageForHrefTranslateForceAuto,
         std::size(kOverrideUnsupportedPageLanguageForHrefTranslateForceAuto),
         nullptr}};

const FeatureEntry::FeatureParam
    kOverrideSimilarLanguagesForHrefTranslateForceAuto[] = {
        {"force-auto-translate-for-similar-languages", "true"}};

const FeatureEntry::FeatureVariation
    kOverrideSimilarLanguagesForHrefTranslateVariations[] = {
        {"(Force automatic translation of pages with the same language as the "
         "target language for hrefTranslate)",
         kOverrideSimilarLanguagesForHrefTranslateForceAuto,
         std::size(kOverrideSimilarLanguagesForHrefTranslateForceAuto),
         nullptr}};

#if BUILDFLAG(IS_ANDROID)
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
     std::size(kExploreSitesDenseTitleBottom), nullptr},
    {"Dense Title Right", kExploreSitesDenseTitleRight,
     std::size(kExploreSitesDenseTitleRight), nullptr}};

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
     std::size(kRelatedSearchesInBarShowDefaultChipWith110SpEllipsis), nullptr},
    {"with 115sp ellipsized default query chip",
     kRelatedSearchesInBarShowDefaultChipWith115SpEllipsis,
     std::size(kRelatedSearchesInBarShowDefaultChipWith115SpEllipsis), nullptr},
    {"with 120sp ellipsized default query chip",
     kRelatedSearchesInBarShowDefaultChipWith120SpEllipsis,
     std::size(kRelatedSearchesInBarShowDefaultChipWith120SpEllipsis), nullptr},
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
     std::size(kRelatedSearchesAlternateUxShowDefaultChipWith110SpEllipsis),
     nullptr},
    {"with 115sp ellipsized default query chip",
     kRelatedSearchesAlternateUxShowDefaultChipWith115SpEllipsis,
     std::size(kRelatedSearchesAlternateUxShowDefaultChipWith115SpEllipsis),
     nullptr},
    {"with 120sp ellipsized default query chip",
     kRelatedSearchesAlternateUxShowDefaultChipWith120SpEllipsis,
     std::size(kRelatedSearchesAlternateUxShowDefaultChipWith120SpEllipsis),
     nullptr},
};

#endif  // BUILDFLAG(IS_ANDROID)

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
      std::size(kResamplingInputEventsLSQEnabled), nullptr},
     {features::kPredictorNameKalman, kResamplingInputEventsKalmanEnabled,
      std::size(kResamplingInputEventsKalmanEnabled), nullptr},
     {features::kPredictorNameLinearFirst,
      kResamplingInputEventsLinearFirstEnabled,
      std::size(kResamplingInputEventsLinearFirstEnabled), nullptr},
     {features::kPredictorNameLinearSecond,
      kResamplingInputEventsLinearSecondEnabled,
      std::size(kResamplingInputEventsLinearSecondEnabled), nullptr},
     {features::kPredictorNameLinearResampling,
      kResamplingInputEventsLinearResamplingEnabled,
      std::size(kResamplingInputEventsLinearResamplingEnabled), nullptr}};

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
         std::size(kResamplingScrollEventsPredictionTimeBasedEnabled), nullptr},
        {features::kPredictionTypeFramesBased,
         kResamplingScrollEventsPredictionFramesBasedEnabled,
         std::size(kResamplingScrollEventsPredictionFramesBasedEnabled),
         nullptr}};

const FeatureEntry::FeatureParam kFilteringPredictionEmptyFilterEnabled[] = {
    {"filter", features::kFilterNameEmpty}};
const FeatureEntry::FeatureParam kFilteringPredictionOneEuroFilterEnabled[] = {
    {"filter", features::kFilterNameOneEuro}};

const FeatureEntry::FeatureVariation kFilteringPredictionFeatureVariations[] = {
    {features::kFilterNameEmpty, kFilteringPredictionEmptyFilterEnabled,
     std::size(kFilteringPredictionEmptyFilterEnabled), nullptr},
    {features::kFilterNameOneEuro, kFilteringPredictionOneEuroFilterEnabled,
     std::size(kFilteringPredictionOneEuroFilterEnabled), nullptr}};

#if BUILDFLAG(IS_ANDROID)
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
     std::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"1 minute", kTabSwitcherOnReturn_1Minute,
     std::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"30 minutes", kTabSwitcherOnReturn_30Minutes,
     std::size(kTabSwitcherOnReturn_30Minutes), nullptr},
    {"60 minutes", kTabSwitcherOnReturn_60Minutes,
     std::size(kTabSwitcherOnReturn_60Minutes), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kTabGridLayoutAndroid_NewTabVariation[] = {
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"allow_to_refetch", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_TallNTV[] = {
    {"thumbnail_aspect_ratio", "0.85"},
    {"allow_to_refetch", "true"},
    {"tab_grid_layout_android_new_tab", "NewTabVariation"},
    {"enable_launch_polish", "true"},
    {"enable_launch_bug_fix", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_SearchChip[] = {
    {"enable_search_term_chip", "true"}};

const FeatureEntry::FeatureParam
    kTabGridLayoutAndroid_TabGroupAutoCreation_TabGroupFirst[] = {
        {"enable_tab_group_auto_creation", "false"},
        {"show_open_in_tab_group_menu_item_first", "true"}};

const FeatureEntry::FeatureParam kTabGridLayoutAndroid_TabGroupAutoCreation[] =
    {{"enable_tab_group_auto_creation", "false"},
     {"show_open_in_tab_group_menu_item_first", "false"}};

const FeatureEntry::FeatureVariation kTabGridLayoutAndroidVariations[] = {
    {"New Tab Variation", kTabGridLayoutAndroid_NewTabVariation,
     std::size(kTabGridLayoutAndroid_NewTabVariation), nullptr},
    {"Tall NTV", kTabGridLayoutAndroid_TallNTV,
     std::size(kTabGridLayoutAndroid_TallNTV), nullptr},
    {"Search term chip", kTabGridLayoutAndroid_SearchChip,
     std::size(kTabGridLayoutAndroid_SearchChip), nullptr},
    {"Without auto group", kTabGridLayoutAndroid_TabGroupAutoCreation,
     std::size(kTabGridLayoutAndroid_TabGroupAutoCreation), nullptr},
    {"Without auto group-group first",
     kTabGridLayoutAndroid_TabGroupAutoCreation_TabGroupFirst,
     std::size(kTabGridLayoutAndroid_TabGroupAutoCreation_TabGroupFirst),
     nullptr},
};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface[] = {
    {"start_surface_variation", "single"},
    {"show_tabs_in_mru_order", "true"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface_V2[] = {
    {"start_surface_variation", "single"},
    {"show_last_active_tab_only", "true"},
    {"open_ntp_instead_of_start", "true"}};

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

const FeatureEntry::FeatureParam
    kStartSurfaceAndroid_CandidateB_AlwaysShowIncognito[] = {
        {"start_surface_variation", "single"},
        {"show_last_active_tab_only", "true"},
        {"hide_switch_when_no_incognito_tabs", "false"},
        {"tab_count_button_on_start_surface", "true"},
        {"open_ntp_instead_of_start", "true"}};

const FeatureEntry::FeatureVariation kStartSurfaceAndroidVariations[] = {
    {"Canidate A", kStartSurfaceAndroid_CandidateA,
     std::size(kStartSurfaceAndroid_CandidateA), nullptr},
    {"Canidate A + Sync check", kStartSurfaceAndroid_CandidateA_SyncCheck,
     std::size(kStartSurfaceAndroid_CandidateA_SyncCheck), nullptr},
    {"Canidate A + Sign in promo backgrounded time limit",
     kStartSurfaceAndroid_CandidateA_SigninPromoTimeLimit,
     std::size(kStartSurfaceAndroid_CandidateA_SigninPromoTimeLimit), nullptr},
    {"Canidate B", kStartSurfaceAndroid_CandidateB,
     std::size(kStartSurfaceAndroid_CandidateB), nullptr},
    {"Canidate B + Always show Incognito icon",
     kStartSurfaceAndroid_CandidateB_AlwaysShowIncognito,
     std::size(kStartSurfaceAndroid_CandidateB_AlwaysShowIncognito), nullptr},
    {"Single Surface", kStartSurfaceAndroid_SingleSurface,
     std::size(kStartSurfaceAndroid_SingleSurface), nullptr},
    {"Single Surface V2", kStartSurfaceAndroid_SingleSurface_V2,
     std::size(kStartSurfaceAndroid_SingleSurface_V2), nullptr},
    {"Single Surface + Single Tab", kStartSurfaceAndroid_SingleSurfaceSingleTab,
     std::size(kStartSurfaceAndroid_SingleSurfaceSingleTab), nullptr},
};

const FeatureEntry::FeatureParam kFeedPositionAndroid_push_down_feed_small[] = {
    {"push_down_feed_small", "true"}};

const FeatureEntry::FeatureParam kFeedPositionAndroid_push_down_feed_large[] = {
    {"push_down_feed_large", "true"}};

const FeatureEntry::FeatureParam kFeedPositionAndroid_pull_up_feed[] = {
    {"pull_up_feed", "true"}};

const FeatureEntry::FeatureParam
    kFeedPositionAndroid_push_down_feed_large_target_feed_active[] = {
        {"push_down_feed_large", "true"},
        {"feed_active_targeting", "active"}};

const FeatureEntry::FeatureParam
    kFeedPositionAndroid_push_down_feed_large_target_non_feed_active[] = {
        {"push_down_feed_large", "true"},
        {"feed_active_targeting", "non-active"}};

const FeatureEntry::FeatureParam
    kFeedPositionAndroid_pull_up_feed_target_feed_active[] = {
        {"pull_up_feed", "true"},
        {"feed_active_targeting", "active"}};

const FeatureEntry::FeatureParam
    kFeedPositionAndroid_pull_up_feed_target_non_feed_active[] = {
        {"pull_up_feed", "true"},
        {"feed_active_targeting", "non-active"}};

const FeatureEntry::FeatureVariation kFeedPositionAndroidVariations[] = {
    {"Push down Feed (small)", kFeedPositionAndroid_push_down_feed_small,
     std::size(kFeedPositionAndroid_push_down_feed_small), nullptr},
    {"Push down Feed (large)", kFeedPositionAndroid_push_down_feed_large,
     std::size(kFeedPositionAndroid_push_down_feed_large), nullptr},
    {"Pull up Feed", kFeedPositionAndroid_pull_up_feed,
     std::size(kFeedPositionAndroid_pull_up_feed), nullptr},
    {"Push down Feed (large) with targeting Feed active users",
     kFeedPositionAndroid_push_down_feed_large_target_feed_active,
     std::size(kFeedPositionAndroid_push_down_feed_large_target_feed_active),
     nullptr},
    {"Push down Feed (large) with targeting non-Feed active users",
     kFeedPositionAndroid_push_down_feed_large_target_non_feed_active,
     std::size(
         kFeedPositionAndroid_push_down_feed_large_target_non_feed_active),
     nullptr},
    {"Pull up Feed with targeting Feed active users",
     kFeedPositionAndroid_pull_up_feed_target_feed_active,
     std::size(kFeedPositionAndroid_pull_up_feed_target_feed_active), nullptr},
    {"Pull up Feed with targeting non-Feed active users",
     kFeedPositionAndroid_pull_up_feed_target_non_feed_active,
     std::size(kFeedPositionAndroid_pull_up_feed_target_non_feed_active),
     nullptr},

};

const FeatureEntry::FeatureParam kSearchResumption_use_new_service[] = {
    {"use_new_service", "true"}};
const FeatureEntry::FeatureVariation
    kSearchResumptionModuleAndroidVariations[] = {
        {"Use New Service", kSearchResumption_use_new_service,
         std::size(kSearchResumption_use_new_service), nullptr},
};

const FeatureEntry::FeatureParam kFeatureNotificationGuide_low_engaged[] = {
    {"enable_feature_incognito_tab", "true"},
    {"enable_feature_ntp_suggestion_card", "true"},
    {"enable_feature_voice_search", "true"}};

const FeatureEntry::FeatureParam kFeatureNotificationGuide_default_browser[] = {
    {"enable_feature_default_browser", "true"}};

const FeatureEntry::FeatureVariation kFeatureNotificationGuideVariations[] = {
    {"Low engaged users", kFeatureNotificationGuide_low_engaged,
     std::size(kFeatureNotificationGuide_low_engaged), nullptr},
    {"Default browser", kFeatureNotificationGuide_default_browser,
     std::size(kFeatureNotificationGuide_default_browser), nullptr},
};

const FeatureEntry::FeatureParam
    kNotificationPermissionRationale_show_dialog_next_start_text_variant[] = {
        {"always_show_rationale_before_requesting_permission", "true"},
        {"notification_permission_dialog_text_variant_2", "true"},
        {"permission_request_interval_days", "0"},
};

const FeatureEntry::FeatureParam
    kNotificationPermissionRationale_show_dialog_next_start[] = {
        {"always_show_rationale_before_requesting_permission", "true"},
        {"notification_permission_dialog_text_variant_2", "false"},
        {"permission_request_interval_days", "0"},
};

const FeatureEntry::FeatureVariation
    kNotificationPermissionRationaleVariations[] = {
        {"- Show rationale dialog on next startup",
         kNotificationPermissionRationale_show_dialog_next_start,
         std::size(kNotificationPermissionRationale_show_dialog_next_start),
         nullptr},
        {"- Show rationale dialog on next startup - alternative copy",
         kNotificationPermissionRationale_show_dialog_next_start_text_variant,
         std::size(
             kNotificationPermissionRationale_show_dialog_next_start_text_variant),
         nullptr},
};

const FeatureEntry::FeatureParam kWebFeed_accelerator[] = {
    {"intro_style", "accelerator"}};

const FeatureEntry::FeatureParam kWebFeed_IPH[] = {{"intro_style", "IPH"}};

const FeatureEntry::FeatureVariation kWebFeedVariations[] = {
    {"accelerator recommendations", kWebFeed_accelerator,
     std::size(kWebFeed_accelerator), nullptr},
    {"IPH recommendations", kWebFeed_IPH, std::size(kWebFeed_IPH), nullptr},
};

const FeatureEntry::FeatureParam kWebFeedAwareness_new_animation[] = {
    {"awareness_style", "new animation"}};

const FeatureEntry::FeatureParam kWebFeedAwareness_IPH[] = {
    {"awareness_style", "IPH"}};

const FeatureEntry::FeatureVariation kWebFeedAwarenessVariations[] = {
    {"new animation", kWebFeedAwareness_new_animation,
     std::size(kWebFeedAwareness_new_animation), nullptr},
    {"IPH and dot", kWebFeedAwareness_IPH, std::size(kWebFeedAwareness_IPH),
     nullptr},
};

const FeatureEntry::FeatureParam kFeedCloseRefresh_Open[] = {
    {"require_interaction", "false"}};

const FeatureEntry::FeatureParam kFeedCloseRefresh_Interact[] = {
    {"require_interaction", "true"}};

const FeatureEntry::FeatureVariation kFeedCloseRefreshVariations[] = {
    {"Open", kFeedCloseRefresh_Open, std::size(kFeedCloseRefresh_Open),
     nullptr},
    {"Interact", kFeedCloseRefresh_Interact,
     std::size(kFeedCloseRefresh_Interact), nullptr},
};

const FeatureEntry::FeatureParam kConditionalTabStripAndroid_Immediate[] = {
    {"conditional_tab_strip_session_time_ms", "0"}};
const FeatureEntry::FeatureParam kConditionalTabStripAndroid_60Minutes[] = {
    {"conditional_tab_strip_session_time_ms", "3600000"}};
const FeatureEntry::FeatureVariation kConditionalTabStripAndroidVariations[] = {
    {"Immediate", kConditionalTabStripAndroid_Immediate,
     std::size(kConditionalTabStripAndroid_Immediate), nullptr},
    {"60 minutes", kConditionalTabStripAndroid_60Minutes,
     std::size(kConditionalTabStripAndroid_60Minutes), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kAddToHomescreen_UseTextBubble[] = {
    {"use_text_bubble", "true"}};
const FeatureEntry::FeatureParam kAddToHomescreen_UseMessage[] = {
    {"use_message", "true"}};

const FeatureEntry::FeatureVariation kAddToHomescreenIPHVariations[] = {
    {"Use Text Bubble", kAddToHomescreen_UseTextBubble,
     std::size(kAddToHomescreen_UseTextBubble), nullptr},
    {"Use Message", kAddToHomescreen_UseMessage,
     std::size(kAddToHomescreen_UseMessage), nullptr}};
#endif

#if BUILDFLAG(IS_ANDROID)
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
         std::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         std::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
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

const FeatureEntry::FeatureParam kLensCameraAssistedSearchOnTablet[] = {
    {"enableCameraAssistedSearchOnTablet", "true"}};

const FeatureEntry::FeatureVariation kLensCameraAssistedSearchVariations[] = {
    {"(Lens then Mic)", kLensCameraAssistedSearchLensButtonStart,
     std::size(kLensCameraAssistedSearchLensButtonStart), nullptr},
    {"(Mic then Lens)", kLensCameraAssistedSearchLensButtonEnd,
     std::size(kLensCameraAssistedSearchLensButtonEnd), nullptr},
    {"(without AGSA version check)",
     kLensCameraAssistedSkipAgsaVersionCheckEnabled,
     std::size(kLensCameraAssistedSkipAgsaVersionCheckEnabled), nullptr},
    {"(with AGSA version check )",
     kLensCameraAssistedSkipAgsaVersionCheckDisabled,
     std::size(kLensCameraAssistedSkipAgsaVersionCheckDisabled), nullptr},
    {"(on Tablet)", kLensCameraAssistedSearchOnTablet,
     std::size(kLensCameraAssistedSearchOnTablet), nullptr}};

const FeatureEntry::FeatureParam kLensContextMenuSearchOnTablet[] = {
    {"enableContextMenuSearchOnTablet", "true"}};

const FeatureEntry::FeatureVariation kLensContextMenuSearchVariations[] = {
    {"(on Tablet)", kLensContextMenuSearchOnTablet,
     std::size(kLensContextMenuSearchOnTablet), nullptr},
};

const FeatureEntry::FeatureParam kDynamicColorFull[] = {
    {"dynamic_color_full", "true"}};

const FeatureEntry::FeatureVariation kDynamicColorAndroidVariations[] = {
    {"(Full)", kDynamicColorFull, std::size(kDynamicColorFull), nullptr},
};

#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::Choice kNotificationSchedulerChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::
         kNotificationSchedulerImmediateBackgroundTaskDescription,
     notifications::switches::kNotificationSchedulerImmediateBackgroundTask,
     ""},
};

#if BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kAssistantConsentV2_reprompts_counter[] = {
    {"count", "3"}};

const FeatureEntry::FeatureVariation kAssistantConsentV2_Variations[] = {
    {"Limited Re-prompts", kAssistantConsentV2_reprompts_counter,
     std::size(kAssistantConsentV2_reprompts_counter), nullptr},
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
         std::size(kOmniboxAssistantVoiceSearchGreyMic), nullptr},
        {"(colorful mic)", kOmniboxAssistantVoiceSearchColorfulMic,
         std::size(kOmniboxAssistantVoiceSearchColorfulMic), nullptr},
        {"(no account check)", kOmniboxAssistantVoiceSearchNoMultiAccountCheck,
         std::size(kOmniboxAssistantVoiceSearchNoMultiAccountCheck), nullptr},
};

const FeatureEntry::FeatureParam
    kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails[] = {
        {"animate_thumbnails", "true"}};
const FeatureEntry::FeatureVariation
    kPhotoPickerVideoSupportFeatureVariations[] = {
        {"(with animated thumbnails)",
         kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails,
         std::size(kPhotoPickerVideoSupportEnabledWithAnimatedThumbnails),
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
      std::size(kRequestDesktopSiteForTablets100), nullptr},
     {"for 600dp+ screens", kRequestDesktopSiteForTablets600,
      std::size(kRequestDesktopSiteForTablets600), nullptr},
     {"for 768dp+ screens", kRequestDesktopSiteForTablets768,
      std::size(kRequestDesktopSiteForTablets768), nullptr},
     {"for 960dp+ screens", kRequestDesktopSiteForTablets960,
      std::size(kRequestDesktopSiteForTablets960), nullptr},
     {"for 1024dp+ screens", kRequestDesktopSiteForTablets1024,
      std::size(kRequestDesktopSiteForTablets1024), nullptr},
     {"for 1280dp+ screens", kRequestDesktopSiteForTablets1280,
      std::size(kRequestDesktopSiteForTablets1280), nullptr},
     {"for 1920dp+ screens", kRequestDesktopSiteForTablets1920,
      std::size(kRequestDesktopSiteForTablets1920), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

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
     std::size(kBackForwardCache_SameSite), nullptr},
    {"force caching all pages (experimental)", kBackForwardCache_ForceCaching,
     std::size(kBackForwardCache_ForceCaching), nullptr},
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

#if BUILDFLAG(IS_ANDROID)
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

#endif  // BUILDFLAG(IS_ANDROID)

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
     std::size(kDrawPredictedPointExperiment1Point12Ms), nullptr},
    {flag_descriptions::kDraw2PredictedPoints6Ms,
     kDrawPredictedPointExperiment2Points6Ms,
     std::size(kDrawPredictedPointExperiment2Points6Ms), nullptr},
    {flag_descriptions::kDraw1PredictedPoint6Ms,
     kDrawPredictedPointExperiment1Point6Ms,
     std::size(kDrawPredictedPointExperiment1Point6Ms), nullptr},
    {flag_descriptions::kDraw2PredictedPoints3Ms,
     kDrawPredictedPointExperiment2Points3Ms,
     std::size(kDrawPredictedPointExperiment2Points3Ms), nullptr}};

const FeatureEntry::FeatureParam kFedCmVariationAutoSignin[] = {
    {features::kFedCmAutoSigninFieldTrialParamName, "true"}};
const FeatureEntry::FeatureParam kFedCmVariationIdpSignout[] = {
    {features::kFedCmIdpSignoutFieldTrialParamName, "true"}};
const FeatureEntry::FeatureParam kFedCmVariationIframe[] = {
    {features::kFedCmIframeSupportFieldTrialParamName, "true"}};
const FeatureEntry::FeatureVariation kFedCmFeatureVariations[] = {
    {"- with FedCM auto sign-in", kFedCmVariationAutoSignin,
     std::size(kFedCmVariationAutoSignin), nullptr},
    {"- with FedCM IDP sign-out", kFedCmVariationIdpSignout,
     std::size(kFedCmVariationIdpSignout), nullptr},
    {"- with iframe support", kFedCmVariationIframe,
     std::size(kFedCmVariationIframe), nullptr},
};

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

// The variations of --password-domain-capabilities-fetching.
const FeatureEntry::FeatureParam
    kPasswordDomainCapabilitiesFetchingVariationLiveExperiment[] = {
        {password_manager::features::kPasswordChangeLiveExperimentParam.name,
         "true"}};

const FeatureEntry::FeatureVariation
    kPasswordDomainCapabilitiesFetchingFeatureVariations[] = {
        {"Live experiment",
         kPasswordDomainCapabilitiesFetchingVariationLiveExperiment,
         std::size(kPasswordDomainCapabilitiesFetchingVariationLiveExperiment),
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
     std::size(
         kPasswordChangeVariationWithForcedDialogAfterEverySuccessfulSubmission),
     nullptr}};

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
         std::size(
             kPasswordChangeInSettingsVariationWithForcedWarningForEverySite),
         nullptr}};

#if BUILDFLAG(IS_ANDROID)
// The variations of --touch-to-fill-password-submission.
const FeatureEntry::FeatureParam
    kTouchToFillPasswordSubmissionWithConservativeHeuristics[] = {
        {password_manager::features::
             kTouchToFillPasswordSubmissionWithConservativeHeuristics,
         "true"}};

const FeatureEntry::FeatureVariation
    kTouchToFillPasswordSubmissionVariations[] = {
        {"Use conservative heuristics",
         kTouchToFillPasswordSubmissionWithConservativeHeuristics,
         std::size(kTouchToFillPasswordSubmissionWithConservativeHeuristics),
         nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kPermissionIconTimeout6000[] = {
    {"PermissionIconTimeoutMs", "6000"}};
const FeatureEntry::FeatureParam kPermissionIconTimeout4000[] = {
    {"PermissionIconTimeoutMs", "4000"}};
const FeatureEntry::FeatureParam kPermissionIconTimeout3000[] = {
    {"PermissionIconTimeoutMs", "3000"}};
const FeatureEntry::FeatureParam kPermissionIconTimeout2000[] = {
    {"PermissionIconTimeoutMs", "2000"}};

const FeatureEntry::FeatureVariation
    kPageInfoDiscoverabilityTimeoutVariations[] = {
        {"Long (6s)", kPermissionIconTimeout6000,
         std::size(kPermissionIconTimeout6000), nullptr},
        {"Medium (4s)", kPermissionIconTimeout4000,
         std::size(kPermissionIconTimeout4000), nullptr},
        {"Short (3s)", kPermissionIconTimeout3000,
         std::size(kPermissionIconTimeout3000), nullptr},
        {"Extra-Short (2s)", kPermissionIconTimeout2000,
         std::size(kPermissionIconTimeout2000), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// The variations of --metrics-settings-android.
const FeatureEntry::FeatureParam kMetricsSettingsAndroidAlternativeOne[] = {
    {"fre", "1"}};

const FeatureEntry::FeatureParam kMetricsSettingsAndroidAlternativeTwo[] = {
    {"fre", "2"}};

const FeatureEntry::FeatureVariation kMetricsSettingsAndroidVariations[] = {
    {"Alternative FRE 1", kMetricsSettingsAndroidAlternativeOne,
     std::size(kMetricsSettingsAndroidAlternativeOne), nullptr},
    {"Alternative FRE 2", kMetricsSettingsAndroidAlternativeTwo,
     std::size(kMetricsSettingsAndroidAlternativeTwo), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// SCT Auditing feature variations.
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateNone[] = {
    {"sampling_rate", "0.0"}};
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateAlternativeOne[] = {
    {"sampling_rate", "0.0001"}};
const FeatureEntry::FeatureParam kSCTAuditingSamplingRateAlternativeTwo[] = {
    {"sampling_rate", "0.001"}};

const FeatureEntry::FeatureVariation kSCTAuditingVariations[] = {
    {"Sampling rate 0%", kSCTAuditingSamplingRateNone,
     std::size(kSCTAuditingSamplingRateNone), nullptr},
    {"Sampling rate 0.01%", kSCTAuditingSamplingRateAlternativeOne,
     std::size(kSCTAuditingSamplingRateAlternativeOne), nullptr},
    {"Sampling rate 0.1%", kSCTAuditingSamplingRateAlternativeTwo,
     std::size(kSCTAuditingSamplingRateAlternativeTwo), nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kUseDnsHttpsSvcbBoth[] = {
    {"UseDnsHttpsSvcbEnableInsecure", "true"}};
const FeatureEntry::FeatureParam kUseDnsHttpsSvcbDohOnly[] = {
    {"UseDnsHttpsSvcbEnableInsecure", "false"}};

const FeatureEntry::FeatureVariation kUseDnsHttpsSvcbVariations[] = {
    {"for DNS-over-HTTPS and insecure DNS", kUseDnsHttpsSvcbBoth,
     std::size(kUseDnsHttpsSvcbBoth), nullptr},
    {"for DNS-over-HTTPS only", kUseDnsHttpsSvcbDohOnly,
     std::size(kUseDnsHttpsSvcbDohOnly), nullptr},
};

#if BUILDFLAG(IS_ANDROID)
// The variations of ContentLanguagesInLanguagePicker.
const FeatureEntry::FeatureParam
    kContentLanguagesInLanguagePickerDisableObservers[] = {
        {language::kContentLanguagesDisableObserversParam, "true"}};

const FeatureEntry::FeatureVariation
    kContentLanguagesInLanguaePickerVariations[] = {
        {"Without observers", kContentLanguagesInLanguagePickerDisableObservers,
         std::size(kContentLanguagesInLanguagePickerDisableObservers), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kCheckOfflineCapabilityWarnOnly[] = {
    {"check_mode", "warn_only"}};
const FeatureEntry::FeatureParam kCheckOfflineCapabilityEnforce[] = {
    {"check_mode", "enforce"}};

const FeatureEntry::FeatureVariation kCheckOfflineCapabilityVariations[] = {
    {"Warn-only", kCheckOfflineCapabilityWarnOnly,
     std::size(kCheckOfflineCapabilityWarnOnly), nullptr},
    {"Enforce", kCheckOfflineCapabilityEnforce,
     std::size(kCheckOfflineCapabilityEnforce), nullptr},
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kCategoricalSearch_Unranked[] = {
    {"ranking", "none"}};

const FeatureEntry::FeatureParam kCategoricalSearch_ByItem[] = {
    {"ranking", "item"}};

const FeatureEntry::FeatureParam kCategoricalSearch_ByUsage[] = {
    {"ranking", "usage"}};

const FeatureEntry::FeatureVariation kCategoricalSearchVariations[] = {
    {"Unranked", kCategoricalSearch_Unranked,
     std::size(kCategoricalSearch_Unranked), nullptr},
    {"By item", kCategoricalSearch_ByItem, std::size(kCategoricalSearch_ByItem),
     nullptr},
    {"By usage", kCategoricalSearch_ByUsage,
     std::size(kCategoricalSearch_ByUsage), nullptr}};

const FeatureEntry::FeatureParam kProductivityLauncher_WithoutContinue[] = {
    {"enable_continue", "false"}};

const FeatureEntry::FeatureVariation kProductivityLauncherVariations[] = {
    {"without Continue", kProductivityLauncher_WithoutContinue,
     std::size(kProductivityLauncher_WithoutContinue), nullptr}};

const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay10Mins[] = {
    {"long_delay_minutes", "10"}};
const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay12Hours[] = {
    {"long_delay_minutes", "720"}};
const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay24Hours[] = {
    {"long_delay_minutes", "1440"}};
const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay36Hours[] = {
    {"long_delay_minutes", "2160"}};
const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay48Hours[] = {
    {"long_delay_minutes", "2880"}};
const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay60Hours[] = {
    {"long_delay_minutes", "3600"}};
const FeatureEntry::FeatureParam kLauncherItemSuggest_LongDelay72Hours[] = {
    {"long_delay_minutes", "4320"}};

const FeatureEntry::FeatureVariation kLauncherItemSuggestVariations[] = {
    {"with 10 minute long delay", kLauncherItemSuggest_LongDelay10Mins,
     std::size(kLauncherItemSuggest_LongDelay10Mins), nullptr},
    {"with 12 hour long delay", kLauncherItemSuggest_LongDelay12Hours,
     std::size(kLauncherItemSuggest_LongDelay12Hours), nullptr},
    {"with 24 hour long delay", kLauncherItemSuggest_LongDelay24Hours,
     std::size(kLauncherItemSuggest_LongDelay24Hours), nullptr},
    {"with 36 hour long delay", kLauncherItemSuggest_LongDelay36Hours,
     std::size(kLauncherItemSuggest_LongDelay36Hours), nullptr},
    {"with 48 hour long delay", kLauncherItemSuggest_LongDelay48Hours,
     std::size(kLauncherItemSuggest_LongDelay48Hours), nullptr},
    {"with 60 hour long delay", kLauncherItemSuggest_LongDelay60Hours,
     std::size(kLauncherItemSuggest_LongDelay60Hours), nullptr},
    {"with 72 hour long delay", kLauncherItemSuggest_LongDelay72Hours,
     std::size(kLauncherItemSuggest_LongDelay72Hours), nullptr}};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

constexpr FeatureEntry::FeatureParam kPlatformProvidedTrustTokenIssuance[] = {
    {"PlatformProvidedTrustTokenIssuance", "true"}};

constexpr FeatureEntry::FeatureVariation
    kPlatformProvidedTrustTokensVariations[] = {
        {"with platform-provided trust token issuance",
         kPlatformProvidedTrustTokenIssuance,
         std::size(kPlatformProvidedTrustTokenIssuance), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kPersonalizationHubInternalName[] = "personalization-hub";
constexpr char kWallpaperFastRefreshInternalName[] = "wallpaper-fast-refresh";
constexpr char kWallpaperFullScreenPreviewInternalName[] =
    "wallpaper-fullscreen-preview";
constexpr char kWallpaperGooglePhotosIntegrationInternalName[] =
    "wallpaper-google-photos-integration";
constexpr char kWallpaperPerDeskName[] = "per-desk-wallpaper";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kPaintPreviewStartupWithAccessibility[] = {
    {"has_accessibility_support", "true"}};

const FeatureEntry::FeatureVariation kPaintPreviewStartupVariations[] = {
    {"with accessibility support", kPaintPreviewStartupWithAccessibility,
     std::size(kPaintPreviewStartupWithAccessibility), nullptr}};
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kBorealisBigGlInternalName[] = "borealis-big-gl";
constexpr char kBorealisDiskManagementInternalName[] =
    "borealis-disk-management";
constexpr char kBorealisForceBetaClientInternalName[] =
    "borealis-force-beta-client";
constexpr char kBorealisLinuxModeInternalName[] = "borealis-linux-mode";
// This differs slightly from its symbol's name since "enabled" is used
// internally to refer to whether borealis is installed or not.
constexpr char kBorealisPermittedInternalName[] = "borealis-enabled";
constexpr char kBorealisStorageBallooningInternalName[] =
    "borealis-storage-ballooning";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kClipboardHistoryReorderInternalName[] =
    "clipboard-history-reorder";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
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
     std::size(kReadLaterUseRootBookmarkAsDefault), nullptr},
    {"(with app menu item)", kReadLaterInAppMenu,
     std::size(kReadLaterInAppMenu), nullptr},
    {"(bookmarks semi-integration)", kReadLaterSemiIntegrated,
     std::size(kReadLaterSemiIntegrated), nullptr},
    {"(no custom tab)", kReadLaterNoCustomTab, std::size(kReadLaterNoCustomTab),
     nullptr}};

const FeatureEntry::FeatureParam kBookmarksRefreshVisuals[] = {
    {"bookmark_visuals_enabled", "true"}};
const FeatureEntry::FeatureParam kBookmarksRefreshCompactVisuals[] = {
    {"bookmark_visuals_enabled", "true"},
    {"bookmark_compact_visuals_enabled", "true"}};
const FeatureEntry::FeatureParam kBookmarksRefreshAppMenu[] = {
    {"bookmark_in_app_menu", "true"}};
const FeatureEntry::FeatureParam kBookmarksRefreshNormal[] = {
    {"bookmark_visuals_enabled", "true"},
    {"bookmark_in_app_menu", "true"}};
const FeatureEntry::FeatureParam kBookmarksRefreshCompact[] = {
    {"bookmark_visuals_enabled", "true"},
    {"bookmark_compact_visuals_enabled", "true"},
    {"bookmark_in_app_menu", "true"}};

const FeatureEntry::FeatureVariation kBookmarksRefreshVariations[] = {
    {"(enabled w/ visuals)", kBookmarksRefreshNormal,
     std::size(kBookmarksRefreshNormal), nullptr},
    {"(enabled w/ compact visuals)", kBookmarksRefreshCompact,
     std::size(kBookmarksRefreshCompact), nullptr},
    {"(visuals only)", kBookmarksRefreshVisuals,
     std::size(kBookmarksRefreshVisuals), nullptr},
    {"(compact visuals only)", kBookmarksRefreshCompactVisuals,
     std::size(kBookmarksRefreshCompactVisuals), nullptr},
    {"(app menu item only)", kBookmarksRefreshAppMenu,
     std::size(kBookmarksRefreshAppMenu), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kLargeFaviconFromGoogle96[] = {
    {"favicon_size_in_dip", "96"}};
const FeatureEntry::FeatureParam kLargeFaviconFromGoogle128[] = {
    {"favicon_size_in_dip", "128"}};

const FeatureEntry::FeatureVariation kLargeFaviconFromGoogleVariations[] = {
    {"(96dip)", kLargeFaviconFromGoogle96, std::size(kLargeFaviconFromGoogle96),
     nullptr},
    {"(128dip)", kLargeFaviconFromGoogle128,
     std::size(kLargeFaviconFromGoogle128), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Possible configurations for the snooping protection feature.
// Empty params configures the feature to apply a simple threshold to one
// sample.

const FeatureEntry::FeatureParam kSnoopingProtectionPrecision[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "0"},
    {"SnoopingProtection_negative_score_threshold", "0"}};

const FeatureEntry::FeatureParam kSnoopingProtectionConfidence[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "2"},
    {"SnoopingProtection_negative_count_threshold", "2"},
    {"SnoopingProtection_uncertain_count_threshold", "2"},
    {"SnoopingProtection_positive_score_threshold", "0"},
    {"SnoopingProtection_negative_score_threshold", "0"}};

const FeatureEntry::FeatureParam kSnoopingProtectionThreshold20[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "20"},
    {"SnoopingProtection_negative_score_threshold", "20"}};

const FeatureEntry::FeatureParam kSnoopingProtectionThresholdMinus20[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "-20"},
    {"SnoopingProtection_negative_score_threshold", "-20"}};

const FeatureEntry::FeatureParam kSnoopingProtectionThreshold40[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "40"},
    {"SnoopingProtection_negative_score_threshold", "40"}};

const FeatureEntry::FeatureParam kSnoopingProtectionThresholdMinus40[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "-40"},
    {"SnoopingProtection_negative_score_threshold", "-40"}};

const FeatureEntry::FeatureParam kSnoopingProtectionThreshold60[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "60"},
    {"SnoopingProtection_negative_score_threshold", "60"}};

const FeatureEntry::FeatureParam kSnoopingProtectionThresholdMinus60[] = {
    {"SnoopingProtection_filter_config_case", "2"},
    {"SnoopingProtection_positive_count_threshold", "1"},
    {"SnoopingProtection_negative_count_threshold", "1"},
    {"SnoopingProtection_uncertain_count_threshold", "1"},
    {"SnoopingProtection_positive_score_threshold", "-60"},
    {"SnoopingProtection_negative_score_threshold", "-60"}};

const FeatureEntry::FeatureVariation kSnoopingProtectionVariations[] = {
    {"Precise", kSnoopingProtectionPrecision,
     std::size(kSnoopingProtectionPrecision), nullptr},
    {"Slow Precise", kSnoopingProtectionConfidence,
     std::size(kSnoopingProtectionConfidence), nullptr},
    {"Threshold20", kSnoopingProtectionThreshold20,
     std::size(kSnoopingProtectionThreshold20), nullptr},
    {"Threshold-20", kSnoopingProtectionThresholdMinus20,
     std::size(kSnoopingProtectionThresholdMinus20), nullptr},
    {"Threshold40", kSnoopingProtectionThreshold40,
     std::size(kSnoopingProtectionThreshold40), nullptr},
    {"Threshold-40", kSnoopingProtectionThresholdMinus40,
     std::size(kSnoopingProtectionThresholdMinus40), nullptr},
    {"Threshold60", kSnoopingProtectionThreshold60,
     std::size(kSnoopingProtectionThreshold60), nullptr},
    {"Threshold-60", kSnoopingProtectionThresholdMinus60,
     std::size(kSnoopingProtectionThresholdMinus60), nullptr},
};

const FeatureEntry::FeatureParam kQuickDim10s[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock70s[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "70000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock130s[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "130000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock130sFeedback[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "130000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
    {"QuickDim_send_feedback_if_undimmed", "true"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock130sThreshold20[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "130000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "20"},
    {"QuickDim_negative_score_threshold", "20"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock130sThresholdMinus20[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "130000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "-20"},
    {"QuickDim_negative_score_threshold", "-20"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock130sThreshold40[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "130000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "40"},
    {"QuickDim_negative_score_threshold", "40"},
};

const FeatureEntry::FeatureParam kQuickDim10sQuickLock130sThresholdMinus40[] = {
    {"QuickDim_quick_dim_ms", "10000"},
    {"QuickDim_quick_lock_ms", "130000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "-40"},
    {"QuickDim_negative_score_threshold", "-40"},
};

const FeatureEntry::FeatureParam kQuickDim45sQuickLock105s[] = {
    {"QuickDim_quick_dim_ms", "45000"},
    {"QuickDim_quick_lock_ms", "105000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
};

const FeatureEntry::FeatureParam kQuickDim45sQuickLock165s[] = {
    {"QuickDim_quick_dim_ms", "45000"},
    {"QuickDim_quick_lock_ms", "165000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
};

const FeatureEntry::FeatureParam kQuickDim120sQuickLock240s[] = {
    {"QuickDim_quick_dim_ms", "120000"},
    {"QuickDim_quick_lock_ms", "240000"},
    {"QuickDim_filter_config_case", "2"},
    {"QuickDim_positive_count_threshold", "1"},
    {"QuickDim_negative_count_threshold", "2"},
    {"QuickDim_uncertain_count_threshold", "2"},
    {"QuickDim_positive_score_threshold", "0"},
    {"QuickDim_negative_score_threshold", "0"},
};

const FeatureEntry::FeatureVariation kQuickDimVariations[] = {
    {"Dim10sLock70s", kQuickDim10sQuickLock70s,
     std::size(kQuickDim10sQuickLock70s), nullptr},
    {"Dim10sLock130s", kQuickDim10sQuickLock130s,
     std::size(kQuickDim10sQuickLock130s), nullptr},
    {"Dim45sLock105s", kQuickDim45sQuickLock105s,
     std::size(kQuickDim45sQuickLock105s), nullptr},
    {"Dim45sLock165s", kQuickDim45sQuickLock165s,
     std::size(kQuickDim45sQuickLock165s), nullptr},
    {"Dim120sLock240s", kQuickDim120sQuickLock240s,
     std::size(kQuickDim120sQuickLock240s), nullptr},
    {"Dim10sNoLock", kQuickDim10s, std::size(kQuickDim10s), nullptr},
    {"Dim10sLock130sThreshold20", kQuickDim10sQuickLock130sThreshold20,
     std::size(kQuickDim10sQuickLock130sThreshold20), nullptr},
    {"Dim10sLock130sThreshold-20", kQuickDim10sQuickLock130sThresholdMinus20,
     std::size(kQuickDim10sQuickLock130sThresholdMinus20), nullptr},
    {"Dim10sLock130sThreshold40", kQuickDim10sQuickLock130sThreshold40,
     std::size(kQuickDim10sQuickLock130sThreshold40), nullptr},
    {"Dim10sLock130sThreshold-40", kQuickDim10sQuickLock130sThresholdMinus40,
     std::size(kQuickDim10sQuickLock130sThresholdMinus40), nullptr},
    {"Dim10sLock130sWithFeedback", kQuickDim10sQuickLock130sFeedback,
     std::size(kQuickDim10sQuickLock130sFeedback), nullptr},
};

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

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kGridTabSwitcherForTabletsPolished[] = {
    {"enable_launch_polish", "true"}};

const FeatureEntry::FeatureParam kGridTabSwitcherForTabletsDelayCreation[] = {
    {"delay_creation", "true"},
    {"enable_launch_polish", "true"}};

const FeatureEntry::FeatureVariation kGridTabSwitcherForTabletsVariations[] = {
    {"(Polished)", kGridTabSwitcherForTabletsPolished,
     std::size(kGridTabSwitcherForTabletsPolished), nullptr},
    {"(DelayCreatePolish)", kGridTabSwitcherForTabletsDelayCreation,
     std::size(kGridTabSwitcherForTabletsDelayCreation), nullptr},
};

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kTabStripImprovementsTabWidthShort[] = {
    {"min_tab_width", "108"}};
const FeatureEntry::FeatureParam kTabStripImprovementsTabWidthMedium[] = {
    {"min_tab_width", "156"}};

const FeatureEntry::FeatureVariation kTabStripImprovementsTabWidthVariations[] =
    {
        {"Short Tab Width", kTabStripImprovementsTabWidthShort,
         std::size(kTabStripImprovementsTabWidthShort), nullptr},
        {"Medium Tab Width", kTabStripImprovementsTabWidthMedium,
         std::size(kTabStripImprovementsTabWidthMedium), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kUpmAndroidShadowSyncingUsers[] = {
    {password_manager::features::kUpmExperimentVariationParam.name,
     password_manager::features::kUpmExperimentVariationOption[1].name}};
const FeatureEntry::FeatureParam kUpmAndroidEnableWithLegacyUi[] = {
    {password_manager::features::kUpmExperimentVariationParam.name,
     password_manager::features::kUpmExperimentVariationOption[2].name}};
const FeatureEntry::FeatureParam kUpmAndroidEnableForAllUsers[] = {
    {password_manager::features::kUpmExperimentVariationParam.name,
     password_manager::features::kUpmExperimentVariationOption[3].name}};

const FeatureEntry::FeatureVariation
    kUnifiedPasswordManagerAndroidVariations[] = {
        // Skip kEnableForSyncingUsers which is the default Enabled param.
        {"Shadow Traffic only", kUpmAndroidShadowSyncingUsers,
         std::size(kUpmAndroidShadowSyncingUsers), nullptr},
        {"With Legacy UI", kUpmAndroidEnableWithLegacyUi,
         std::size(kUpmAndroidEnableWithLegacyUi), nullptr},
        {"For All Users", kUpmAndroidEnableForAllUsers,
         std::size(kUpmAndroidEnableForAllUsers), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kUnthrottledNestedTimeout_NestingLevel = {
    "nesting", "100"};

const FeatureEntry::FeatureVariation kUnthrottledNestedTimeout_Variations[] = {
    {"100", &kUnthrottledNestedTimeout_NestingLevel, 1, nullptr},
};

constexpr FeatureEntry::FeatureParam kLensStandaloneWithSidePanel[] = {
    {"enable-side-panel", "true"}};
constexpr FeatureEntry::FeatureVariation kLensStandaloneVariations[] = {
    {"With Side Panel", kLensStandaloneWithSidePanel,
     std::size(kLensStandaloneWithSidePanel), nullptr},
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kAlwaysEnableHdcpChoices[] = {
    {flag_descriptions::kAlwaysEnableHdcpDefault, "", ""},
    {flag_descriptions::kAlwaysEnableHdcpType0,
     ash::switches::kAlwaysEnableHdcp, "type0"},
    {flag_descriptions::kAlwaysEnableHdcpType1,
     ash::switches::kAlwaysEnableHdcp, "type1"},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kDesktopSharePreviewVariant16{
    "variant", share::kDesktopSharePreviewVariant16};
const FeatureEntry::FeatureParam kDesktopSharePreviewVariant40{
    "variant", share::kDesktopSharePreviewVariant40};
const FeatureEntry::FeatureParam kDesktopSharePreviewVariant72{
    "variant", share::kDesktopSharePreviewVariant72};

const FeatureEntry::FeatureVariation kDesktopSharePreviewVariations[] = {
    {"16pt preview", &kDesktopSharePreviewVariant16, 1, nullptr},
    {"40pt preview", &kDesktopSharePreviewVariant40, 1, nullptr},
    {"72pt preview", &kDesktopSharePreviewVariant72, 1, nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS)
const FeatureEntry::FeatureParam kDmTokenDeletionParam[] = {{"forced", "true"}};
const FeatureEntry::FeatureVariation kDmTokenDeletionVariation[] = {
    {"(Forced)", kDmTokenDeletionParam, std::size(kDmTokenDeletionParam),
     nullptr}};
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Feature variations for kIsolateSandboxedIframes.
#if !BUILDFLAG(IS_ANDROID)
// TODO(wjmaclean): Add FeatureParams for a per-frame grouping when support
// for it is added.
const FeatureEntry::FeatureParam kIsolateSandboxedIframesGroupingPerSite{
    "grouping", "per-site"};
const FeatureEntry::FeatureParam kIsolateSandboxedIframesGroupingPerOrigin{
    "grouping", "per-origin"};
const FeatureEntry::FeatureVariation
    kIsolateSandboxedIframesGroupingVariations[] = {
        {"with grouping by URL's site",
         &kIsolateSandboxedIframesGroupingPerSite, 1, nullptr},
        {"with grouping by URL's origin",
         &kIsolateSandboxedIframesGroupingPerOrigin, 1, nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

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
    {"disable-webrtc-hw-decoding", flag_descriptions::kWebrtcHwDecodingName,
     flag_descriptions::kWebrtcHwDecodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", flag_descriptions::kWebrtcHwEncodingName,
     flag_descriptions::kWebrtcHwEncodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
#if !BUILDFLAG(IS_ANDROID)
    {"enable-reader-mode", flag_descriptions::kEnableReaderModeName,
     flag_descriptions::kEnableReaderModeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(dom_distiller::kReaderMode,
                                    kReaderModeDiscoverabilityVariations,
                                    "ReaderMode")},
#endif  // !BUILDFLAG(IS_ANDROID)
#if defined(WEBRTC_USE_PIPEWIRE)
    {"enable-webrtc-pipewire-capturer",
     flag_descriptions::kWebrtcPipeWireCapturerName,
     flag_descriptions::kWebrtcPipeWireCapturerDescription,
     kOsLinux | kOsLacros,
     FEATURE_VALUE_TYPE(features::kWebRtcPipeWireCapturer)},
#endif  // defined(WEBRTC_USE_PIPEWIRE)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-lacros-in-chrome-kiosk",
     flag_descriptions::kChromeKioskEnableLacrosName,
     flag_descriptions::kChromeKioskEnableLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kChromeKioskEnableLacros)},
    {"enable-lacros-in-web-kiosk", flag_descriptions::kWebKioskEnableLacrosName,
     flag_descriptions::kWebKioskEnableLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebKioskEnableLacros)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
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
    {"enable-container-queries", flag_descriptions::kCSSContainerQueriesName,
     flag_descriptions::kCSSContainerQueriesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCSSContainerQueries)},
#if BUILDFLAG(IS_ANDROID)
    {"contextual-search-debug", flag_descriptions::kContextualSearchDebugName,
     flag_descriptions::kContextualSearchDebugDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDebug)},
    {"contextual-search-force-caption",
     flag_descriptions::kContextualSearchForceCaptionName,
     flag_descriptions::kContextualSearchForceCaptionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchForceCaption)},
    {"contextual-search-suppress-short-view",
     flag_descriptions::kContextualSearchSuppressShortViewName,
     flag_descriptions::kContextualSearchSuppressShortViewDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchSuppressShortView)},
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
#endif  // BUILDFLAG(IS_ANDROID)
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
     kOsLinux | kOsLacros | kOsCrOS | kOsWin | kOsAndroid | kOsFuchsia,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableSmoothScrolling,
                               switches::kDisableSmoothScrolling)},
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
#if defined(ARCH_CPU_X86_64)
    {"enable-experimental-webassembly-stack-switching",
     flag_descriptions::kExperimentalWebAssemblyStackSwitchingName,
     flag_descriptions::kExperimentalWebAssemblyStackSwitchingDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebAssemblyStackSwitching)},
#endif  // defined(ARCH_CPU_X86_64)
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
    {"enable-experimental-web-platform-features",
     flag_descriptions::kExperimentalWebPlatformFeaturesName,
     flag_descriptions::kExperimentalWebPlatformFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
    {"top-chrome-touch-ui", flag_descriptions::kTopChromeTouchUiName,
     flag_descriptions::kTopChromeTouchUiDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTopChromeTouchUiChoices)},
    {"variable-colrv1", flag_descriptions::kVariableCOLRV1Name,
     flag_descriptions::kVariableCOLRV1Description, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kVariableCOLRV1)},
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
    {
        "audio-settings-page",
        flag_descriptions::kAudioSettingsPageName,
        flag_descriptions::kAudioSettingsPageDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(ash::features::kAudioSettingsPage),
    },
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
    {"adaptive-charging", flag_descriptions::kAdaptiveChargingName,
     flag_descriptions::kAdaptiveChargingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAdaptiveCharging)},
    {"adaptive-charging-for-testing",
     flag_descriptions::kAdaptiveChargingForTestingName,
     flag_descriptions::kAdaptiveChargingForTestingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAdaptiveChargingForTesting)},
    {"allow-poly-device-pairing",
     flag_descriptions::kAllowPolyDevicePairingName,
     flag_descriptions::kAllowPolyDevicePairingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAllowPolyDevicePairing)},
    {"ash-bento-bar", flag_descriptions::kBentoBarName,
     flag_descriptions::kBentoBarDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBentoBar)},
    {"ash-capture-mode-selfie-cam", flag_descriptions::kCaptureSelfieCamName,
     flag_descriptions::kCaptureSelfieCamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCaptureModeSelfieCamera)},
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
    {"rounded-display", flag_descriptions::kRoundedDisplay,
     flag_descriptions::kRoundedDisplayDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kRoundedDisplay)},
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
    {"bluetooth-use-llprivacy", flag_descriptions::kBluetoothUseLLPrivacyName,
     flag_descriptions::kBluetoothUseLLPrivacyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(bluez::features::kLinkLayerPrivacy)},
    {"calendar-view", flag_descriptions::kCalendarViewName,
     flag_descriptions::kCalendarViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCalendarView)},
    {"calendar-view-debug-mode", flag_descriptions::kCalendarModelDebugModeName,
     flag_descriptions::kCalendarModelDebugModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCalendarModelDebugMode)},
    {"cellular-bypass-esim-installation-connectivity-check",
     flag_descriptions::kCellularBypassESimInstallationConnectivityCheckName,
     flag_descriptions::
         kCellularBypassESimInstallationConnectivityCheckDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kCellularBypassESimInstallationConnectivityCheck)},
    {"cellular-custom-apn-profiles",
     flag_descriptions::kCellularCustomAPNProfilesName,
     flag_descriptions::kCellularCustomAPNProfilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularCustomAPNProfiles)},
    {"cellular-forbid-attach-apn",
     flag_descriptions::kCellularForbidAttachApnName,
     flag_descriptions::kCellularForbidAttachApnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularForbidAttachApn)},
    {"cellular-use-attach-apn", flag_descriptions::kCellularUseAttachApnName,
     flag_descriptions::kCellularUseAttachApnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularUseAttachApn)},
    {"cellular-use-second-euicc",
     flag_descriptions::kCellularUseSecondEuiccName,
     flag_descriptions::kCellularUseSecondEuiccDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCellularUseSecondEuicc)},
    {"cryptauth-v2-dedup-device-last-activity-time",
     flag_descriptions::kCryptAuthV2DedupDeviceLastActivityTimeName,
     flag_descriptions::kCryptAuthV2DedupDeviceLastActivityTimeDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kCryptAuthV2DedupDeviceLastActivityTime)},
    {"oobe-hid-detection-revamp",
     flag_descriptions::kOobeHidDetectionRevampName,
     flag_descriptions::kOobeHidDetectionRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kOobeHidDetectionRevamp)},
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
    {"enable-zram-writeback", flag_descriptions::kEnableZramWriteback,
     flag_descriptions::kEnableZramWritebackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::kCrOSEnableZramWriteback)},
    // Used to carry the policy value crossing the Chrome process lifetime.
    {crosapi::browser_util::kLacrosAvailabilityPolicyInternalName, "", "",
     kOsCrOS, MULTI_VALUE_TYPE(kLacrosAvailabilityPolicyChoices)},
    {kLacrosSupportInternalName, flag_descriptions::kLacrosSupportName,
     flag_descriptions::kLacrosSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosSupport)},
    {kLacrosStabilityInternalName, flag_descriptions::kLacrosStabilityName,
     flag_descriptions::kLacrosStabilityDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosStabilityChoices)},
    {"uxstudy1", flag_descriptions::kUXStudy1Name,
     flag_descriptions::kUXStudy1Description, kOsCrOS,
     MULTI_VALUE_TYPE(kUXStudy1Choices)},
    {"lacros-profile-migration-for-any-user",
     flag_descriptions::kLacrosProfileMigrationForAnyUserName,
     flag_descriptions::kLacrosProfileMigrationForAnyUserDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosProfileMigrationForAnyUser)},
    {"lacros-move-profile-migration",
     flag_descriptions::kLacrosMoveProfileMigrationName,
     flag_descriptions::kLacrosMoveProfileMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosMoveProfileMigration)},
    {"lacros-profile-migration-force-off",
     flag_descriptions::kLacrosProfileMigrationForceOffName,
     flag_descriptions::kLacrosProfileMigrationForceOffDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosProfileMigrationForceOff)},
    {kLacrosSelectionInternalName, flag_descriptions::kLacrosSelectionName,
     flag_descriptions::kLacrosSelectionDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosSelectionChoices)},
    {kWebAppsCrosapiInternalName, flag_descriptions::kWebAppsCrosapiName,
     flag_descriptions::kWebAppsCrosapiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebAppsCrosapi)},
    {kLacrosPrimaryInternalName, flag_descriptions::kLacrosPrimaryName,
     flag_descriptions::kLacrosPrimaryDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosPrimary)},
    {kLacrosOnlyInternalName, flag_descriptions::kLacrosOnlyName,
     flag_descriptions::kLacrosOnlyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLacrosOnly)},
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
    {"enable-ikev2-vpn", flag_descriptions::kEnableIkev2VpnName,
     flag_descriptions::kEnableIkev2VpnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableIkev2Vpn)},
    {"enforce-ash-extension-keeplist",
     flag_descriptions::kEnforceAshExtensionKeeplistName,
     flag_descriptions::kEnforceAshExtensionKeeplistDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnforceAshExtensionKeeplist)},
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
    {"hidden-network-migration", flag_descriptions::kHiddenNetworkMigrationName,
     flag_descriptions::kHiddenNetworkMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHiddenNetworkMigration)},
    {"shelf-hide-buttons-in-tablet",
     flag_descriptions::kHideShelfControlsInTabletModeName,
     flag_descriptions::kHideShelfControlsInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideShelfControlsInTabletMode)},
    {"shelf-hover-previews", flag_descriptions::kShelfHoverPreviewsName,
     flag_descriptions::kShelfHoverPreviewsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShelfHoverPreviews)},
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
    {"sim-lock-policy", flag_descriptions::kSimLockPolicyName,
     flag_descriptions::kSimLockPolicyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSimLockPolicy)},
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
    {"wifi-connect-mac-address-randomization",
     flag_descriptions::kWifiConnectMacAddressRandomizationName,
     flag_descriptions::kWifiConnectMacAddressRandomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kWifiConnectMacAddressRandomization)},
    {"consumer-auto-update-toggle-allowed",
     flag_descriptions::kConsumerAutoUpdateToggleAllowedName,
     flag_descriptions::kConsumerAutoUpdateToggleAllowedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kConsumerAutoUpdateToggleAllowed)},
    {"disable-lacros-tts-support",
     flag_descriptions::kDisableLacrosTtsSupportName,
     flag_descriptions::kDisableLacrosTtsSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisableLacrosTtsSupport)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"dark-light-mode", flag_descriptions::kDarkLightTestName,
     flag_descriptions::kDarkLightTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDarkLightMode)},
    {"deprecate-low-usage-codecs",
     flag_descriptions::kDeprecateLowUsageCodecsName,
     flag_descriptions::kDeprecateLowUsageCodecsDescription,
     kOsCrOS | kOsLacros, FEATURE_VALUE_TYPE(media::kDeprecateLowUsageCodecs)},
    {"disable-idle-sockets-close-on-memory-pressure",
     flag_descriptions::kDisableIdleSocketsCloseOnMemoryPressureName,
     flag_descriptions::kDisableIdleSocketsCloseOnMemoryPressureDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(
         chromeos::features::kDisableIdleSocketsCloseOnMemoryPressure)},
    {"disable-office-editing-component-app",
     flag_descriptions::kDisableOfficeEditingComponentAppName,
     flag_descriptions::kDisableOfficeEditingComponentAppDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableOfficeEditingComponentApp)},
    {"one-group-per-renderer", flag_descriptions::kOneGroupPerRendererName,
     flag_descriptions::kOneGroupPerRendererDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(base::kOneGroupPerRenderer)},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX)
    {
        "enable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsLinux,
        FEATURE_VALUE_TYPE(media::kVaapiVideoDecodeLinux),
    },
#else
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid | kOsLacros | kOsFuchsia,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
#endif  // BUILDFLAG(IS_LINUX)
    {
        "disable-accelerated-video-encode",
        flag_descriptions::kAcceleratedVideoEncodeName,
        flag_descriptions::kAcceleratedVideoEncodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoEncode),
    },
#if BUILDFLAG(IS_WIN)
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
        "enable-hardware-secure-decryption-fallback",
        flag_descriptions::kHardwareSecureDecryptionFallbackName,
        flag_descriptions::kHardwareSecureDecryptionFallbackDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kHardwareSecureDecryptionFallback),
    },
    {
        "enable-media-foundation-clear",
        flag_descriptions::kMediaFoundationClearName,
        flag_descriptions::kMediaFoundationClearDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kMediaFoundationClearPlayback),
    },
    {
        "enable-waitable-swap-chain",
        flag_descriptions::kUseWaitableSwapChainName,
        flag_descriptions::kUseWaitableSwapChainDescription,
        kOsWin,
        FEATURE_WITH_PARAMS_VALUE_TYPE(features::kDXGIWaitableSwapChain,
                                       kDXGIWaitableSwapChainVariations,
                                       "DXGIWaitableSwapChain"),
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
#if BUILDFLAG(IS_WIN)
    {
        "zero-copy-video-capture",
        flag_descriptions::kZeroCopyVideoCaptureName,
        flag_descriptions::kZeroCopyVideoCaptureDescription,
        kOsWin,
        FEATURE_VALUE_TYPE(media::kMediaFoundationD3D11VideoCapture),
    },
#endif  // BUILDFLAG(IS_WIN)
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
    {"enable-web-bluetooth", flag_descriptions::kWebBluetoothName,
     flag_descriptions::kWebBluetoothDescription, kOsLinux | kOsLacros,
     FEATURE_VALUE_TYPE(features::kWebBluetooth)},
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
     kOsWin | kOsLinux | kOsLacros | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kVulkan)},
#if BUILDFLAG(IS_ANDROID)
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
    {"translate-message-ui", flag_descriptions::kTranslateMessageUIName,
     flag_descriptions::kTranslateMessageUIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(translate::kTranslateMessageUI)},
#endif  // BUILDFLAG(IS_ANDROID)

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
     kOsMac | kOsLinux | kOsLacros | kOsWin,
     FEATURE_VALUE_TYPE(features::kSystemNotifications)},
#endif  // BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_ANDROID)
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
    {"android-media-picker", flag_descriptions::kAndroidMediaPickerSupportName,
     flag_descriptions::kAndroidMediaPickerSupportDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(photo_picker::features::kAndroidMediaPickerSupport)},
    {"contextual-page-actions", flag_descriptions::kContextualPageActionsName,
     flag_descriptions::kContextualPageActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kContextualPageActions)},
    {"contextual-page-actions-with-price-tracking",
     flag_descriptions::kContextualPageActionsWithPriceTrackingName,
     flag_descriptions::kContextualPageActionsWithPriceTrackingDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kContextualPageActionPriceTracking,
         kContextualPageActionPriceTrackingVariations,
         "ContextualPageActionPriceTracking")},
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
    {"screenshots-for-android-v2",
     flag_descriptions::kScreenshotsForAndroidV2Name,
     flag_descriptions::kScreenshotsForAndroidV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(share::kScreenshotsForAndroidV2)},
    {"voice-button-in-top-toolbar",
     flag_descriptions::kVoiceButtonInTopToolbarName,
     flag_descriptions::kVoiceButtonInTopToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kVoiceButtonInTopToolbar)},
    {"assistant-consent-modal", flag_descriptions::kAssistantConsentModalName,
     flag_descriptions::kAssistantConsentModalDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantConsentModal)},
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
    {"webnotes-dynamic-templates",
     flag_descriptions::kWebNotesDynamicTemplatesName,
     flag_descriptions::kWebNotesDynamicTemplatesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(content_creation::kWebNotesDynamicTemplates)},
    {"lightweight-reactions-android",
     flag_descriptions::kLightweightReactionsAndroidName,
     flag_descriptions::kLightweightReactionsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(content_creation::kLightweightReactions)},
#endif  // BUILDFLAG(IS_ANDROID)
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
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"system-extensions", flag_descriptions::kSystemExtensionsName,
     flag_descriptions::kSystemExtensionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSystemExtensions)},
    {"enable-service-workers-for-chrome-untrusted",
     flag_descriptions::kEnableServiceWorkersForChromeUntrustedName,
     flag_descriptions::kEnableServiceWorkersForChromeUntrustedDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableServiceWorkersForChromeUntrusted)},
    {"crostini-bullseye-upgrade",
     flag_descriptions::kCrostiniBullseyeUpgradeName,
     flag_descriptions::kCrostiniBullseyeUpgradeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniBullseyeUpgrade)},
    {"crostini-reset-lxd-db", flag_descriptions::kCrostiniResetLxdDbName,
     flag_descriptions::kCrostiniResetLxdDbDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniResetLxdDb)},
    {"terminal-alternative-renderer",
     flag_descriptions::kTerminalAlternativeRendererName,
     flag_descriptions::kTerminalAlternativeRendererDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kTerminalAlternativeRenderer)},
    {"terminal-dev", flag_descriptions::kTerminalDevName,
     flag_descriptions::kTerminalDevDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kTerminalDev)},
    {"terminal-multi-profile", flag_descriptions::kTerminalMultiProfileName,
     flag_descriptions::kTerminalMultiProfileDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kTerminalMultiProfile)},
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
    {"guest-os-generic-installer",
     flag_descriptions::kGuestOSGenericInstallerName,
     flag_descriptions::kGuestOSGenericInstallerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGuestOSGenericInstaller)},
    {"bruschetta", flag_descriptions::kBruschettaName,
     flag_descriptions::kBruschettaDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBruschetta)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
     BUILDFLAG(IS_ANDROID)) &&                        \
    !BUILDFLAG(IS_NACL)
    {"mojo-linux-sharedmem", flag_descriptions::kMojoLinuxChannelSharedMemName,
     flag_descriptions::kMojoLinuxChannelSharedMemDescription,
     kOsCrOS | kOsLinux | kOsLacros | kOsAndroid,
     FEATURE_VALUE_TYPE(mojo::core::kMojoLinuxChannelSharedMem)},
#endif
#if BUILDFLAG(IS_ANDROID)
    {"enable-site-isolation-for-password-sites",
     flag_descriptions::kSiteIsolationForPasswordSitesName,
     flag_descriptions::kSiteIsolationForPasswordSitesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         site_isolation::features::kSiteIsolationForPasswordSites)},
    {"enable-site-per-process", flag_descriptions::kStrictSiteIsolationName,
     flag_descriptions::kStrictSiteIsolationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
#endif
    {"enable-isolated-web-apps", flag_descriptions::kEnableIsolatedWebAppsName,
     flag_descriptions::kEnableIsolatedWebAppsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kIsolatedWebApps)},
    {"install-isolated-apps-at-startup",
     flag_descriptions::kInstallIssolatedAppsAtStartup,
     flag_descriptions::kInstallIssolatedAppsAtStartupDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kInstallIsolatedAppsAtStartup, "")},
    {"isolate-origins", flag_descriptions::kIsolateOriginsName,
     flag_descriptions::kIsolateOriginsDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kIsolateOrigins, "")},
    {"isolated-app-origins", flag_descriptions::kIsolatedAppOriginsName,
     flag_descriptions::kIsolatedAppOriginsDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kIsolatedAppOrigins, "")},
    {about_flags::kSiteIsolationTrialOptOutInternalName,
     flag_descriptions::kSiteIsolationOptOutName,
     flag_descriptions::kSiteIsolationOptOutDescription, kOsAll,
     MULTI_VALUE_TYPE(kSiteIsolationOptOutChoices)},
    {"isolation-by-default", flag_descriptions::kIsolationByDefaultName,
     flag_descriptions::kIsolationByDefaultDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIsolationByDefault)},
#if !BUILDFLAG(IS_ANDROID)
    {"enable-webview-tag-site-isolation",
     flag_descriptions::kWebViewTagSiteIsolationName,
     flag_descriptions::kWebViewTagSiteIsolationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSiteIsolationForGuests)},
#endif
    {"enable-prefetch-proxy",
     flag_descriptions::kEnablePrivatePrefetchProxyName,
     flag_descriptions::kEnablePrivatePrefetchProxyDescription, kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kEnableFeatures,
         "IsolatePrerenders:allow_all_domains/true/max_srp_prefetches/-1/"
         "use_speculation_rules/true,SpeculationRulesPrefetchProxy")},
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     SINGLE_VALUE_TYPE(webapps::switches::kBypassAppBannerEngagementChecks)},
#if BUILDFLAG(IS_CHROMEOS)
    {"allow-default-web-app-migration-for-chrome-os-managed-users",
     flag_descriptions::kAllowDefaultWebAppMigrationForChromeOsManagedUsersName,
     flag_descriptions::
         kAllowDefaultWebAppMigrationForChromeOsManagedUsersDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         web_app::kAllowDefaultWebAppMigrationForChromeOsManagedUsers)},
#endif  // BUILDFLAG(IS_CHROMEOS)
    {"enable-desktop-pwas-prefix-app-name-in-window-title",
     flag_descriptions::kDesktopPWAsPrefixAppNameInWindowTitleName,
     flag_descriptions::kDesktopPWAsPrefixAppNameInWindowTitleDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(features::kPrefixWebAppWindowsWithAppName)},
    {"enable-desktop-pwas-remove-status-bar",
     flag_descriptions::kDesktopPWAsRemoveStatusBarName,
     flag_descriptions::kDesktopPWAsRemoveStatusBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRemoveStatusBarInWebApps)},
#if BUILDFLAG(IS_ANDROID)
    {"enable-android-pwas-default-offline-page",
     flag_descriptions::kAndroidPWAsDefaultOfflinePageName,
     flag_descriptions::kAndroidPWAsDefaultOfflinePageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidPWAsDefaultOfflinePage)},
#else
    {"enable-desktop-pwas-default-offline-page",
     flag_descriptions::kDesktopPWAsDefaultOfflinePageName,
     flag_descriptions::kDesktopPWAsDefaultOfflinePageDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsDefaultOfflinePage)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"enable-desktop-pwas-elided-extensions-menu",
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuName,
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsElidedExtensionsMenu)},
    {"enable-desktop-pwas-tab-strip",
     flag_descriptions::kDesktopPWAsTabStripName,
     flag_descriptions::kDesktopPWAsTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStrip)},
    {"enable-desktop-pwas-tab-strip-settings",
     flag_descriptions::kDesktopPWAsTabStripSettingsName,
     flag_descriptions::kDesktopPWAsTabStripSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStripSettings)},
    {"enable-desktop-pwas-launch-handler",
     flag_descriptions::kDesktopPWAsLaunchHandlerName,
     flag_descriptions::kDesktopPWAsLaunchHandlerDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableLaunchHandler)},
    {"enable-desktop-pwas-manifest-id",
     flag_descriptions::kDesktopPWAsManifestIdName,
     flag_descriptions::kDesktopPWAsManifestIdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableManifestId)},
    {"enable-desktop-pwas-sub-apps", flag_descriptions::kDesktopPWAsSubAppsName,
     flag_descriptions::kDesktopPWAsSubAppsDescription,
     kOsWin | kOsLinux | kOsLacros | kOsMac | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(blink::features::kDesktopPWAsSubApps)},
    {"enable-desktop-pwas-url-handling",
     flag_descriptions::kDesktopPWAsUrlHandlingName,
     flag_descriptions::kDesktopPWAsUrlHandlingDescription,
     kOsWin | kOsLinux | kOsLacros | kOsMac | kOsFuchsia,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableUrlHandlers)},
    {"enable-desktop-pwas-window-controls-overlay",
     flag_descriptions::kDesktopPWAsWindowControlsOverlayName,
     flag_descriptions::kDesktopPWAsWindowControlsOverlayDescription,
     kOsWin | kOsLinux | kOsLacros | kOsMac | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kWebAppWindowControlsOverlay)},
    {"enable-desktop-pwas-borderless",
     flag_descriptions::kDesktopPWAsBorderlessName,
     flag_descriptions::kDesktopPWAsBorderlessDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppBorderless)},
    {"enable-desktop-pwas-additional-windowing-controls",
     flag_descriptions::kDesktopPWAsAdditionalWindowingControlsName,
     flag_descriptions::kDesktopPWAsAdditionalWindowingControlsDescription,
     kOsWin | kOsLinux | kOsLacros | kOsMac | kOsCrOS | kOsFuchsia,
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
         syncer::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
#if !BUILDFLAG(IS_ANDROID)
    {"block-migrated-default-chrome-app-sync",
     flag_descriptions::kBlockMigratedDefaultChromeAppSyncName,
     flag_descriptions::kBlockMigratedDefaultChromeAppSyncDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kBlockMigratedDefaultChromeAppSync)},
    {"media-router-cast-allow-all-ips",
     flag_descriptions::kMediaRouterCastAllowAllIPsName,
     flag_descriptions::kMediaRouterCastAllowAllIPsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastAllowAllIPsFeature)},
    {"global-media-controls-cast-start-stop",
     flag_descriptions::kGlobalMediaControlsCastStartStopName,
     flag_descriptions::kGlobalMediaControlsCastStartStopDescription,
     kOsDesktop,
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
    {"enable-preinstalled-web-app-duplication-fixer",
     flag_descriptions::kEnablePreinstalledWebAppDuplicationFixerName,
     flag_descriptions::kEnablePreinstalledWebAppDuplicationFixerDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPreinstalledWebAppDuplicationFixer)},

    {"enable-openscreen-cast-streaming-session",
     flag_descriptions::kOpenscreenCastStreamingSessionName,
     flag_descriptions::kOpenscreenCastStreamingSessionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kOpenscreenCastStreamingSession)},

    {"enable-cast-streaming-av1", flag_descriptions::kCastStreamingAv1Name,
     flag_descriptions::kCastStreamingAv1Description, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kCastStreamingAv1)},

    {"enable-cast-streaming-vp9", flag_descriptions::kCastStreamingVp9Name,
     flag_descriptions::kCastStreamingVp9Description, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kCastStreamingVp9)},

    {"enable-cast-remoting-query-blocklist",
     flag_descriptions::kCastUseBlocklistForRemotingQueryName,
     flag_descriptions::kCastUseBlocklistForRemotingQueryDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         mirroring::features::kCastUseBlocklistForRemotingQuery)},

    {"force-enable-cast-remoting-query",
     flag_descriptions::kCastForceEnableRemotingQueryName,
     flag_descriptions::kCastForceEnableRemotingQueryDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kCastForceEnableRemotingQuery)},

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"autofill-keyboard-accessory-view",
     flag_descriptions::kAutofillAccessoryViewName,
     flag_descriptions::kAutofillAccessoryViewDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillKeyboardAccessory)},
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_MAC)
    {"mac-syscall-sandbox", flag_descriptions::kMacSyscallSandboxName,
     flag_descriptions::kMacSyscallSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSyscallSandbox)},
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    {"web-share", flag_descriptions::kWebShareName,
     flag_descriptions::kWebShareDescription, kOsWin | kOsCrOS | kOsMac,
     FEATURE_VALUE_TYPE(features::kWebShare)},
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
    {"ozone-platform-hint", flag_descriptions::kOzonePlatformHintName,
     flag_descriptions::kOzonePlatformHintDescription, kOsLinux,
     MULTI_VALUE_TYPE(kOzonePlatformHintRuntimeChoices)},

    {"clean-undecryptable-passwords",
     flag_descriptions::kCleanUndecryptablePasswordsLinuxName,
     flag_descriptions::kCleanUndecryptablePasswordsLinuxDescription, kOsLinux,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSyncUndecryptablePasswordsLinux)},

    {"force-password-initial-sync-when-decryption-fails",
     flag_descriptions::kForcePasswordInitialSyncWhenDecryptionFailsName,
     flag_descriptions::kForcePasswordInitialSyncWhenDecryptionFailsDescription,
     kOsLinux,
     FEATURE_VALUE_TYPE(
         password_manager::features::kForceInitialSyncWhenDecryptionFails)},
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    {"skip-undecryptable-passwords",
     flag_descriptions::kSkipUndecryptablePasswordsName,
     flag_descriptions::kSkipUndecryptablePasswordsDescription,
     kOsLinux | kOsMac,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSkipUndecryptablePasswords)},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

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
#if BUILDFLAG(IS_ANDROID)
    {"add-to-homescreen-iph", flag_descriptions::kAddToHomescreenIPHName,
     flag_descriptions::kAddToHomescreenIPHDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAddToHomescreenIPH,
                                    kAddToHomescreenIPHVariations,
                                    "AddToHomescreen")},
    {"feature-notification-guide",
     flag_descriptions::kFeatureNotificationGuideName,
     flag_descriptions::kFeatureNotificationGuideDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_guide::features::kFeatureNotificationGuide,
         kFeatureNotificationGuideVariations,
         "FeatureNotificationGuide")},
    {"notification-permission-rationale-dialog",
     flag_descriptions::kNotificationPermissionRationaleName,
     flag_descriptions::kNotificationPermissionRationaleDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kNotificationPermissionVariant,
         kNotificationPermissionRationaleVariations,
         "NotificationPermissionVariant")},
    {"feature-notification-guide-skip-check-for-low-engaged-users",
     flag_descriptions::
         kFeatureNotificationGuideSkipCheckForLowEngagedUsersName,
     flag_descriptions::
         kFeatureNotificationGuideSkipCheckForLowEngagedUsersDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(feature_guide::features::kSkipCheckForLowEngagedUsers)},
    {"offline-pages-live-page-sharing",
     flag_descriptions::kOfflinePagesLivePageSharingName,
     flag_descriptions::kOfflinePagesLivePageSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesLivePageSharingFeature)},
    {"query-tiles", flag_descriptions::kQueryTilesName,
     flag_descriptions::kQueryTilesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(query_tiles::features::kQueryTiles,
                                    kQueryTilesVariations,
                                    "QueryTilesVariations")},
    {"query-tiles-ntp", flag_descriptions::kQueryTilesNTPName,
     flag_descriptions::kQueryTilesNTPDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(query_tiles::features::kQueryTilesInNTP)},
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
    {"back-gesture-refactor-android",
     flag_descriptions::kBackGestureRefactorAndroidName,
     flag_descriptions::kBackGestureRefactorAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackGestureRefactorAndroid)},
#endif  // BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // BUILDFLAG(IS_WIN)
#if defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
#endif  // defined(TOOLKIT_VIEWS) || BUILDFLAG(IS_ANDROID)
    {"force-ui-direction", flag_descriptions::kForceUiDirectionName,
     flag_descriptions::kForceUiDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceUIDirectionChoices)},
    {"force-text-direction", flag_descriptions::kForceTextDirectionName,
     flag_descriptions::kForceTextDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceTextDirectionChoices)},
#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)
    {"enable-tls13-early-data", flag_descriptions::kEnableTLS13EarlyDataName,
     flag_descriptions::kEnableTLS13EarlyDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableTLS13EarlyData)},
    {"post-quantum-cecpq2", flag_descriptions::kPostQuantumCECPQ2Name,
     flag_descriptions::kPostQuantumCECPQ2Description, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPostQuantumCECPQ2)},
#if BUILDFLAG(IS_ANDROID)
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
    {"feed-ablation", flag_descriptions::kFeedIsAblatedName,
     flag_descriptions::kFeedIsAblatedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kIsAblated)},
    {"feed-v2-hearts", flag_descriptions::kInterestFeedV2HeartsName,
     flag_descriptions::kInterestFeedV2HeartsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2Hearts)},
    {"info-card-acknowledgement-tracking",
     flag_descriptions::kInfoCardAcknowledgementTrackingName,
     flag_descriptions::kInfoCardAcknowledgementTrackingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInfoCardAcknowledgementTracking)},
    {"feed-v2-autoplay", flag_descriptions::kInterestFeedV2AutoplayName,
     flag_descriptions::kInterestFeedV2AutoplayDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2Autoplay)},
    {"web-feed", flag_descriptions::kWebFeedName,
     flag_descriptions::kWebFeedDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(feed::kWebFeed,
                                    kWebFeedVariations,
                                    "WebFeed")},
    {"web-feed-awareness", flag_descriptions::kWebFeedAwarenessName,
     flag_descriptions::kWebFeedAwarenessDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(feed::kWebFeedAwareness,
                                    kWebFeedAwarenessVariations,
                                    "WebFeedAwareness")},
    {"web-feed-onboarding", flag_descriptions::kWebFeedOnboardingName,
     flag_descriptions::kWebFeedOnboardingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kWebFeedOnboarding)},
    {"web-feed-sort", flag_descriptions::kWebFeedSortName,
     flag_descriptions::kWebFeedSortDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kWebFeedSort)},
    {"xsurface-metrics-reporting",
     flag_descriptions::kXsurfaceMetricsReportingName,
     flag_descriptions::kXsurfaceMetricsReportingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kXsurfaceMetricsReporting)},
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
    {"feed-close-refresh", flag_descriptions::kFeedCloseRefreshName,
     flag_descriptions::kFeedCloseRefreshDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(feed::kFeedCloseRefresh,
                                    kFeedCloseRefreshVariations,
                                    "FeedCloseRefresh")},
#endif  // BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
    {"enable-accessibility-page-zoom",
     flag_descriptions::kAccessibilityPageZoomName,
     flag_descriptions::kAccessibilityPageZoomDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAccessibilityPageZoom)},
#endif  // BUILDFLAG(IS_ANDROID)
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
    {"enable-cros-autocorrect-params-tuning",
     flag_descriptions::kAutocorrectParamsTuningName,
     flag_descriptions::kAutocorrectParamsTuningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAutocorrectParamsTuning)},
    {"enable-cros-diacritics-on-physical-keyboard-longpress",
     flag_descriptions::kDiacriticsOnPhysicalKeyboardLongpressName,
     flag_descriptions::kDiacriticsOnPhysicalKeyboardLongpressDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kDiacriticsOnPhysicalKeyboardLongpress)},
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
    {"enable-cros-virtual-keyboard-new-header",
     flag_descriptions::kVirtualKeyboardNewHeaderName,
     flag_descriptions::kVirtualKeyboardNewHeaderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardNewHeader)},
    {"enable-cros-ime-system-emoji-picker-clipboard",
     flag_descriptions::kImeSystemEmojiPickerClipboardName,
     flag_descriptions::kImeSystemEmojiPickerClipboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPickerClipboard)},
    {"enable-cros-ime-system-emoji-picker-extension",
     flag_descriptions::kImeSystemEmojiPickerExtensionName,
     flag_descriptions::kImeSystemEmojiPickerExtensionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeSystemEmojiPickerExtension)},
    {"enable-cros-ime-system-emoji-picker-search-extension",
     flag_descriptions::kImeSystemEmojiPickerSearchExtensionName,
     flag_descriptions::kImeSystemEmojiPickerSearchExtensionDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kImeSystemEmojiPickerSearchExtension)},
    {"enable-cros-ime-stylus-handwriting",
     flag_descriptions::kImeStylusHandwritingName,
     flag_descriptions::kImeStylusHandwritingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeStylusHandwriting)},
    {"enable-cros-language-settings-update-japanese",
     flag_descriptions::kCrosLanguageSettingsUpdateJapaneseName,
     flag_descriptions::kCrosLanguageSettingsUpdateJapaneseDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kCrosLanguageSettingsUpdateJapanese)},
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
    {"enable-cros-system-transliteration-physical-typing",
     flag_descriptions::kSystemTransliterationPhysicalTypingName,
     flag_descriptions::kSystemTransliterationPhysicalTypingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kSystemTransliterationPhysicalTyping)},
    {"enable-cros-virtual-keyboard-bordered-key",
     flag_descriptions::kVirtualKeyboardBorderedKeyName,
     flag_descriptions::kVirtualKeyboardBorderedKeyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardBorderedKey)},
    {"enable-cros-virtual-keyboard-multitouch",
     flag_descriptions::kVirtualKeyboardMultitouchName,
     flag_descriptions::kVirtualKeyboardMultitouchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVirtualKeyboardMultitouch)},
    {"enable-cros-virtual-keyboard-round-corners",
     flag_descriptions::kVirtualKeyboardRoundCornersName,
     flag_descriptions::kVirtualKeyboardRoundCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardRoundCorners)},
    {"enable-experimental-accessibility-dictation-with-pumpkin",
     flag_descriptions::kExperimentalAccessibilityDictationWithPumpkinName,
     flag_descriptions::
         kExperimentalAccessibilityDictationWithPumpkinDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kExperimentalAccessibilityDictationWithPumpkin)},
    {"enable-experimental-accessibility-google-tts-language-packs",
     flag_descriptions::kExperimentalAccessibilityGoogleTtsLanguagePacksName,
     flag_descriptions::
         kExperimentalAccessibilityGoogleTtsLanguagePacksDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kExperimentalAccessibilityGoogleTtsLanguagePacks)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
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
#if BUILDFLAG(IS_MAC)
    {"enable-immersive-fullscreen-toolbar",
     flag_descriptions::kImmersiveFullscreenName,
     flag_descriptions::kImmersiveFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kImmersiveFullscreen)},
#endif  // BUILDFLAG(IS_MAC)
    {"enable-web-payments-experimental-features",
     flag_descriptions::kWebPaymentsExperimentalFeaturesName,
     flag_descriptions::kWebPaymentsExperimentalFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsExperimentalFeatures)},
    {"enable-payment-request-basic-card",
     flag_descriptions::kPaymentRequestBasicCardName,
     flag_descriptions::kPaymentRequestBasicCardDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPaymentRequestBasicCard)},
    {"identity-in-can-make-payment",
     flag_descriptions::kIdentityInCanMakePaymentEventFeatureName,
     flag_descriptions::kIdentityInCanMakePaymentEventFeatureDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kIdentityInCanMakePaymentEventFeature)},
    {"enable-debug-for-store-billing",
     flag_descriptions::kAppStoreBillingDebugName,
     flag_descriptions::kAppStoreBillingDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kAppStoreBillingDebug)},
    {"enable-debug-for-secure-payment-confirmation",
     flag_descriptions::kSecurePaymentConfirmationDebugName,
     flag_descriptions::kSecurePaymentConfirmationDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSecurePaymentConfirmationDebug)},
#if BUILDFLAG(IS_ANDROID)
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
    {"arc-enable-virtio-blk-for-data",
     flag_descriptions::kArcEnableVirtioBlkForDataName,
     flag_descriptions::kArcEnableVirtioBlkForDataDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableVirtioBlkForData)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-game-mode", flag_descriptions::kArcGameModeName,
     flag_descriptions::kArcGameModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kGameModeFeature)},
    {"arc-keyboard-shortcut-helper-integration",
     flag_descriptions::kArcKeyboardShortcutHelperIntegrationName,
     flag_descriptions::kArcKeyboardShortcutHelperIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kKeyboardShortcutHelperIntegrationFeature)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
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
    {"secondary-google-account-usage",
     flag_descriptions::kSecondaryGoogleAccountUsageName,
     flag_descriptions::kSecondaryGoogleAccountUsageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSecondaryGoogleAccountUsage)},
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

#if BUILDFLAG(IS_ANDROID)
    {"enable-autofill-manual-fallback",
     flag_descriptions::kAutofillManualFallbackAndroidName,
     flag_descriptions::kAutofillManualFallbackAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillManualFallbackAndroid)},

    {"enable-autofill-refresh-style",
     flag_descriptions::kEnableAutofillRefreshStyleName,
     flag_descriptions::kEnableAutofillRefreshStyleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRefreshStyleAndroid)},

    {"enable-family-info-feedback",
     flag_descriptions::kEnableFamilyInfoFeedbackName,
     flag_descriptions::kEnableFamilyInfoFeedbackDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(kEnableFamilyInfoFeedback)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kEnableTouchCalibrationSetting)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"audio-url", flag_descriptions::kAudioUrlName,
     flag_descriptions::kAudioUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAudioUrl)},
    {"prefer-constant-frame-rate",
     flag_descriptions::kPreferConstantFrameRateName,
     flag_descriptions::kPreferConstantFrameRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kPreferConstantFrameRate)},
    {"more-video-capture-buffers",
     flag_descriptions::kMoreVideoCaptureBuffersName,
     flag_descriptions::kMoreVideoCaptureBuffersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMoreVideoCaptureBuffers)},
    {"force-control-face-ae", flag_descriptions::kForceControlFaceAeName,
     flag_descriptions::kForceControlFaceAeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kForceControlFaceAeChoices)},
    {"hdrnet-override", flag_descriptions::kHdrNetOverrideName,
     flag_descriptions::kHdrNetOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kHdrNetOverrideChoices)},
    {"auto-framing-override", flag_descriptions::kAutoFramingOverrideName,
     flag_descriptions::kAutoFramingOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAutoFramingOverrideChoices)},
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
    {"drive-fs-chrome-networking",
     flag_descriptions::kDriveFsChromeNetworkingName,
     flag_descriptions::kDriveFsChromeNetworkingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDriveFsChromeNetworking)},
    {"files-app-experimental", flag_descriptions::kFilesAppExperimentalName,
     flag_descriptions::kFilesAppExperimentalDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesAppExperimental)},
    {"files-extract-archive", flag_descriptions::kFilesExtractArchiveName,
     flag_descriptions::kFilesExtractArchiveDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesExtractArchive)},
    {"files-filters-in-recents", flag_descriptions::kFiltersInRecentsName,
     flag_descriptions::kFiltersInRecentsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFiltersInRecents)},
    {"files-filters-in-recents-v2", flag_descriptions::kFiltersInRecentsV2Name,
     flag_descriptions::kFiltersInRecentsV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFiltersInRecentsV2)},

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
    {"files-web-drive-office", flag_descriptions::kFilesWebDriveOfficeName,
     flag_descriptions::kFilesWebDriveOfficeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesWebDriveOffice)},
    {"force-spectre-v2-mitigation",
     flag_descriptions::kForceSpectreVariant2MitigationName,
     flag_descriptions::kForceSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         sandbox::policy::features::kForceSpectreVariant2Mitigation)},
    {"fuse-box", flag_descriptions::kFuseBoxName,
     flag_descriptions::kFuseBoxDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFuseBox)},
    {"fuse-box-debug", flag_descriptions::kFuseBoxDebugName,
     flag_descriptions::kFuseBoxDebugDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFuseBoxDebug)},
    {"guest-os-files", flag_descriptions::kGuestOsFilesName,
     flag_descriptions::kGuestOsFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGuestOsFiles)},
    {"spectre-v2-mitigation", flag_descriptions::kSpectreVariant2MitigationName,
     flag_descriptions::kSpectreVariant2MitigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(sandbox::policy::features::kSpectreVariant2Mitigation)},
    {"upload-office-to-cloud", flag_descriptions::kUploadOfficeToCloudName,
     flag_descriptions::kUploadOfficeToCloudName, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUploadOfficeToCloud)},
    {"eap-gtc-wifi-authentication",
     flag_descriptions::kEapGtcWifiAuthenticationName,
     flag_descriptions::kEapGtcWifiAuthenticationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEapGtcWifiAuthentication)},
    {"eche-swa", flag_descriptions::kEcheSWAName,
     flag_descriptions::kEcheSWADescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWA)},
    {"eche-swa-debug-mode", flag_descriptions::kEcheSWADebugModeName,
     flag_descriptions::kEcheSWADebugModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEcheSWADebugMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
    {"enable-universal-links", flag_descriptions::kEnableUniversalLinksName,
     flag_descriptions::kEnableUniversalLinksDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kEnableUniveralLinks)},
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
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
    {"omnibox-modernize-visual-update",
     flag_descriptions::kOmniboxModernizeVisualUpdateName,
     flag_descriptions::kOmniboxModernizeVisualUpdateDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOmniboxModernizeVisualUpdate)},

    {"omnibox-most-visited-tiles",
     flag_descriptions::kOmniboxMostVisitedTilesName,
     flag_descriptions::kOmniboxMostVisitedTilesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTiles)},

    {"omnibox-remove-suggestion-header-chevron",
     flag_descriptions::kOmniboxRemoveSuggestionHeaderChevronName,
     flag_descriptions::kOmniboxRemoveSuggestionHeaderChevronDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOmniboxRemoveSuggestionHeaderChevron,
         kOmniboxRemoveSuggestionHeaderChevronVariations,
         "OmniboxRemoveSuggestionHeaderChevron")},

    {"omnibox-most-visited-tiles-title-wrap-around",
     flag_descriptions::kOmniboxMostVisitedTilesTitleWrapAroundName,
     flag_descriptions::kOmniboxMostVisitedTilesTitleWrapAroundDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(omnibox::kMostVisitedTilesTitleWrapAround)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"omnibox-on-focus-suggestions-contextual-web",
     flag_descriptions::kOmniboxFocusTriggersContextualWebZeroSuggestName,
     flag_descriptions::
         kOmniboxFocusTriggersContextualWebZeroSuggestDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kFocusTriggersContextualWebZeroSuggest)},

    {"omnibox-on-focus-suggestions-srp",
     flag_descriptions::kOmniboxFocusTriggersSRPZeroSuggestName,
     flag_descriptions::kOmniboxFocusTriggersSRPZeroSuggestDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kFocusTriggersSRPZeroSuggest)},

    {"omnibox-experimental-suggest-scoring",
     flag_descriptions::kOmniboxExperimentalSuggestScoringName,
     flag_descriptions::kOmniboxExperimentalSuggestScoringDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxExperimentalSuggestScoring)},

    {"omnibox-fuzzy-url-suggestions",
     flag_descriptions::kOmniboxFuzzyUrlSuggestionsName,
     flag_descriptions::kOmniboxFuzzyUrlSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxFuzzyUrlSuggestions)},

    {"omnibox-zero-suggest-prefetching",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetching)},

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
    {"omnibox-experimental-keyword-mode",
     flag_descriptions::kOmniboxExperimentalKeywordModeName,
     flag_descriptions::kOmniboxExperimentalKeywordModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kExperimentalKeywordMode)},
    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescriptions, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kDocumentProvider,
         kOmniboxDocumentProviderVariations,
         "OmniboxDocumentProviderNonDogfoodExperiments")},
    {"omnibox-rich-autocompletion-promising",
     flag_descriptions::kOmniboxRichAutocompletionPromisingName,
     flag_descriptions::kOmniboxRichAutocompletionPromisingDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionPromisingVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-disable-cgi-param-matching",
     flag_descriptions::kOmniboxDisableCGIParamMatchingName,
     flag_descriptions::kOmniboxDisableCGIParamMatchingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDisableCGIParamMatching)},
    {"omnibox-document-provider-aso",
     flag_descriptions::kOmniboxDocumentProviderAsoName,
     flag_descriptions::kOmniboxDocumentProviderAsoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDocumentProviderAso)},
    {"omnibox-site-search-starter-pack",
     flag_descriptions::kOmniboxSiteSearchStarterPackName,
     flag_descriptions::kOmniboxSiteSearchStarterPackDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kSiteSearchStarterPack)},
    {"omnibox-aggregate-shortcuts",
     flag_descriptions::kOmniboxAggregateShortcutsName,
     flag_descriptions::kOmniboxAggregateShortcutsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kAggregateShortcuts)},
    {"omnibox-shortcut-expanding",
     flag_descriptions::kOmniboxShortcutExpandingName,
     flag_descriptions::kOmniboxShortcutExpandingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kShortcutExpanding)},
    {"omnibox-close-popup-with-escape",
     flag_descriptions::kOmniboxClosePopupWithEscapeName,
     flag_descriptions::kOmniboxClosePopupWithEscapeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kClosePopupWithEscape)},
    {"omnibox-blur-with-escape", flag_descriptions::kOmniboxBlurWithEscapeName,
     flag_descriptions::kOmniboxBlurWithEscapeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kBlurWithEscape)},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_WIN)
    {"omnibox-on-device-head-suggestions",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsDescription, kOsWin,
     FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProviderNonIncognito)},
    {"omnibox-on-device-head-suggestions-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoDescription,
     kOsWin, FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProviderIncognito)},
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"scheduler-configuration", flag_descriptions::kSchedulerConfigurationName,
     flag_descriptions::kSchedulerConfigurationDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kSchedulerConfigurationChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"enable-command-line-on-non-rooted-devices",
     flag_descriptions::kEnableCommandLineOnNonRootedName,
     flag_descriptions::kEnableCommandLineOnNoRootedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCommandLineOnNonRooted)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

    {"force-color-profile", flag_descriptions::kForceColorProfileName,
     flag_descriptions::kForceColorProfileDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceColorProfileChoices)},

    {"forced-colors", flag_descriptions::kForcedColorsName,
     flag_descriptions::kForcedColorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kForcedColors)},

#if BUILDFLAG(IS_ANDROID)
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

    {"omnibox-retain-suggestions-with-headers",
     flag_descriptions::kOmniboxRetainSuggestionsWithHeadersName,
     flag_descriptions::kOmniboxRetainSuggestionsWithHeadersDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kRetainSuggestionsWithHeaders)},

    {"omnibox-bookmark-paths", flag_descriptions::kOmniboxBookmarkPathsName,
     flag_descriptions::kOmniboxBookmarkPathsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kBookmarkPaths,
                                    kOmniboxBookmarkPathsVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-short-bookmark-suggestions",
     flag_descriptions::kOmniboxShortBookmarkSuggestionsName,
     flag_descriptions::kOmniboxShortBookmarkSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kShortBookmarkSuggestions)},

    {"optimization-guide-debug-logs",
     flag_descriptions::kOptimizationGuideDebugLogsName,
     flag_descriptions::kOptimizationGuideDebugLogsDescription, kOsAll,
     SINGLE_VALUE_TYPE(optimization_guide::switches::kDebugLoggingEnabled)},

    {"organic-repeatable-queries",
     flag_descriptions::kOrganicRepeatableQueriesName,
     flag_descriptions::kOrganicRepeatableQueriesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history::kOrganicRepeatableQueries,
                                    kOrganicRepeatableQueriesVariations,
                                    "OrganicRepeatableQueries")},

    {"history-journeys", flag_descriptions::kJourneysName,
     flag_descriptions::kJourneysDescription, kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history_clusters::internal::kJourneys,
                                    kJourneysVariations,
                                    "HistoryJourneys")},

    {"history-journeys-labels", flag_descriptions::kJourneysLabelsName,
     flag_descriptions::kJourneysLabelsDescription, kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history_clusters::internal::kJourneysLabels,
                                    kJourneysLabelsVariations,
                                    "HistoryJourneysLabels")},

    {"history-journeys-omnibox-action",
     flag_descriptions::kJourneysOmniboxActionName,
     flag_descriptions::kJourneysOmniboxActionDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history_clusters::internal::kOmniboxAction,
                                    kJourneysOmniboxActionVariations,
                                    "HistoryJourneysOmniboxAction")},

    {"history-journeys-omnibox-history-cluster-provider",
     flag_descriptions::kJourneysOmniboxHistoryClusterProviderName,
     flag_descriptions::kJourneysOmniboxHistoryClusterProviderDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         history_clusters::internal::kOmniboxHistoryClusterProvider)},

    {"history-journeys-on-device-clustering",
     flag_descriptions::kJourneysOnDeviceClusteringBackendName,
     flag_descriptions::kJourneysOnDeviceClusteringBackendDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         history_clusters::features::kOnDeviceClustering,
         kJourneysOnDeviceClusteringVariations,
         "HistoryJourneysOnDeviceClusteringBackend")},

    {"history-journeys-on-device-clustering-keyword-filtering",
     flag_descriptions::kJourneysOnDeviceClusteringKeywordFilteringName,
     flag_descriptions::kJourneysOnDeviceClusteringKeywordFilteringDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         history_clusters::features::kOnDeviceClusteringKeywordFiltering,
         kJourneysOnDeviceClusteringKeywordFilteringVariations,
         "HistoryJourneysKeywordFiltering")},

    {"page-content-annotations", flag_descriptions::kPageContentAnnotationsName,
     flag_descriptions::kPageContentAnnotationsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         optimization_guide::features::kPageContentAnnotations,
         kPageContentAnnotationsVariations,
         "PageContentAnnotations")},

    {"page-entities-page-content-annotations",
     flag_descriptions::kPageEntitiesPageContentAnnotationsName,
     flag_descriptions::kPageEntitiesPageContentAnnotationsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         optimization_guide::features::kPageEntitiesPageContentAnnotations,
         kPageEntitiesPageContentAnnotationsVariations,
         "PageEntitiesPageContentAnnotations")},

    {"page-visibility-page-content-annotations",
     flag_descriptions::kPageVisibilityPageContentAnnotationsName,
     flag_descriptions::kPageVisibilityPageContentAnnotationsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         optimization_guide::features::kPageVisibilityPageContentAnnotations)},

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

    {"private-network-access-send-preflights",
     flag_descriptions::kPrivateNetworkAccessSendPreflightsName,
     flag_descriptions::kPrivateNetworkAccessSendPreflightsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPrivateNetworkAccessSendPreflights)},

    {"private-network-access-respect-preflight-results",
     flag_descriptions::kPrivateNetworkAccessRespectPreflightResultsName,
     flag_descriptions::kPrivateNetworkAccessRespectPreflightResultsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kPrivateNetworkAccessRespectPreflightResults)},

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

#if BUILDFLAG(IS_ANDROID)
    {flag_descriptions::kReadLaterFlagId, flag_descriptions::kReadLaterName,
     flag_descriptions::kReadLaterDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(reading_list::switches::kReadLater,
                                    kReadLaterVariations,
                                    "Collections")},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"read-later-reminder-notification",
     flag_descriptions::kReadLaterReminderNotificationName,
     flag_descriptions::kReadLaterReminderNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         reading_list::switches::kReadLaterReminderNotification)},
#endif

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

    {"side-panel-improved-clobbering",
     flag_descriptions::kSidePanelImprovedClobberingName,
     flag_descriptions::kSidePanelImprovedClobberingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanelImprovedClobbering)},

    {flag_descriptions::kSidePanelJourneysFlagId,
     flag_descriptions::kSidePanelJourneysName,
     flag_descriptions::kSidePanelJourneysDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanelJourneys)},

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

#if BUILDFLAG(IS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"enable-retail-coupons", flag_descriptions::kRetailCouponsName,
     flag_descriptions::kRetailCouponsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kRetailCoupons)},

    {"ntp-cache-one-google-bar", flag_descriptions::kNtpCacheOneGoogleBarName,
     flag_descriptions::kNtpCacheOneGoogleBarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCacheOneGoogleBar)},

    {"ntp-chrome-cart-module", flag_descriptions::kNtpChromeCartModuleName,
     flag_descriptions::kNtpChromeCartModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpChromeCartModule,
                                    kNtpChromeCartModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-drive-module", flag_descriptions::kNtpDriveModuleName,
     flag_descriptions::kNtpDriveModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpDriveModule,
                                    kNtpDriveModuleVariations,
                                    "DesktopNtpModules")},

#if !defined(OFFICIAL_BUILD)
    {"ntp-dummy-modules", flag_descriptions::kNtpDummyModulesName,
     flag_descriptions::kNtpDummyModulesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpDummyModules)},
#endif

    {"ntp-middle-slot-promo-dismissal",
     flag_descriptions::kNtpMiddleSlotPromoDismissalName,
     flag_descriptions::kNtpMiddleSlotPromoDismissalDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpMiddleSlotPromoDismissal)},

    {"ntp-modules-drag-and-drop", flag_descriptions::kNtpModulesDragAndDropName,
     flag_descriptions::kNtpModulesDragAndDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesDragAndDrop)},

    {"ntp-modules-first-run-experience",
     flag_descriptions::kNtpModulesFirstRunExperienceName,
     flag_descriptions::kNtpModulesFirstRunExperienceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesFirstRunExperience)},

    {"ntp-modules-redesigned", flag_descriptions::kNtpModulesRedesignedName,
     flag_descriptions::kNtpModulesRedesignedDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesRedesigned)},

    {"ntp-modules-redesigned-layout",
     flag_descriptions::kNtpModulesRedesignedLayoutName,
     flag_descriptions::kNtpModulesRedesignedLayoutDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesRedesignedLayout)},

    {"ntp-photos-module", flag_descriptions::kNtpPhotosModuleName,
     flag_descriptions::kNtpPhotosModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpPhotosModule,
                                    kNtpPhotosModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-photos-opt-in-art-work",
     flag_descriptions::kNtpPhotosModuleOptInArtWorkName,
     flag_descriptions::kNtpPhotosModuleOptInArtWorkDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_features::kNtpPhotosModuleCustomizedOptInArtWork,
         kNtpPhotosModuleOptInArtWorkVariations,
         "DesktopNtpModules")},

    {"ntp-photos-opt-in-title",
     flag_descriptions::kNtpPhotosModuleOptInTitleName,
     flag_descriptions::kNtpPhotosModuleOptInTitleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_features::kNtpPhotosModuleCustomizedOptInTitle,
         kNtpPhotosModuleOptInTitleVariations,
         "DesktopNtpModules")},

    {"ntp-photos-soft-opt-out",
     flag_descriptions::kNtpPhotosModuleSoftOptOutName,
     flag_descriptions::kNtpPhotosModuleSoftOptOutDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpPhotosModuleSoftOptOut)},

    {"ntp-recipe-tasks-module", flag_descriptions::kNtpRecipeTasksModuleName,
     flag_descriptions::kNtpRecipeTasksModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpRecipeTasksModule,
                                    kNtpRecipeTasksModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-realbox-match-omnibox-theme",
     flag_descriptions::kNtpRealboxMatchOmniboxThemeName,
     flag_descriptions::kNtpRealboxMatchOmniboxThemeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kRealboxMatchOmniboxTheme,
                                    kRealboxMatchOmniboxThemeVariations,
                                    "OmniboxBundledExperimentV1")},

    {"ntp-realbox-match-searchbox-theme",
     flag_descriptions::kNtpRealboxMatchSearchboxThemeName,
     flag_descriptions::kNtpRealboxMatchSearchboxThemeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kRealboxMatchSearchboxTheme,
                                    kRealboxMatchSearchboxThemeVariations,
                                    "OmniboxBundledExperimentV1")},

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

    {"ntp-safe-browsing-module", flag_descriptions::kNtpSafeBrowsingModuleName,
     flag_descriptions::kNtpSafeBrowsingModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpSafeBrowsingModule,
                                    kNtpSafeBrowsingModuleVariations,
                                    "DesktopNtpModules")},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    {"chrome-wide-echo-cancellation",
     flag_descriptions::kChromeWideEchoCancellationName,
     flag_descriptions::kChromeWideEchoCancellationDescription,
     kOsMac | kOsWin | kOsLinux | kOsLacros,
     FEATURE_VALUE_TYPE(media::kChromeWideEchoCancellation)},
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    {"enable-async-dns", flag_descriptions::kAsyncDnsName,
     flag_descriptions::kAsyncDnsDescription, kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kAsyncDns)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
    {"download-auto-resumption-native",
     flag_descriptions::kDownloadAutoResumptionNativeName,
     flag_descriptions::kDownloadAutoResumptionNativeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadAutoResumptionNative)},
#endif

    {"enable-new-download-backend",
     flag_descriptions::kEnableNewDownloadBackendName,
     flag_descriptions::kEnableNewDownloadBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kUseDownloadOfflineContentProvider)},

    {"download-range", flag_descriptions::kDownloadRangeName,
     flag_descriptions::kDownloadRangeDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kDownloadRange)},

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

    {"enable-web-authentication-cable-disco-creds",
     flag_descriptions::kEnableWebAuthenticationCableDiscoCredsName,
     flag_descriptions::kEnableWebAuthenticationCableDiscoCredsDescription,
     kOsAll, FEATURE_VALUE_TYPE(device::kWebAuthCableDisco)},

#if !BUILDFLAG(IS_ANDROID)
    {"web-authentication-permit-enterprise-attestation",
     flag_descriptions::kWebAuthenticationPermitEnterpriseAttestationName,
     flag_descriptions::
         kWebAuthenticationPermitEnterpriseAttestationDescription,
     kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         webauthn::switches::kPermitEnterpriseAttestationOriginList,
         "")},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-web-authentication-chromeos-authenticator",
     flag_descriptions::kEnableWebAuthenticationChromeOSAuthenticatorName,
     flag_descriptions::
         kEnableWebAuthenticationChromeOSAuthenticatorDescription,
     kOsCrOS, FEATURE_VALUE_TYPE(device::kWebAuthCrosPlatformAuthenticator)},
#endif

    {
        "zero-copy-tab-capture",
        flag_descriptions::kEnableZeroCopyTabCaptureName,
        flag_descriptions::kEnableZeroCopyTabCaptureDescription,
        kOsMac,
        FEATURE_VALUE_TYPE(blink::features::kZeroCopyTabCapture),
    },

    {
        "region-capture-experimental-subtypes",
        flag_descriptions::kEnableRegionCaptureExperimentalSubtypesName,
        flag_descriptions::kEnableRegionCaptureExperimentalSubtypesDescription,
        kOsDesktop,
        FEATURE_VALUE_TYPE(blink::features::kRegionCaptureExperimentalSubtypes),
    },

#if BUILDFLAG(ENABLE_PDF)
    {"accessible-pdf-form", flag_descriptions::kAccessiblePDFFormName,
     flag_descriptions::kAccessiblePDFFormDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kAccessiblePDFForm)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(IS_MAC)
    {"cups-ipp-printing-backend",
     flag_descriptions::kCupsIppPrintingBackendName,
     flag_descriptions::kCupsIppPrintingBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(printing::features::kCupsIppPrintingBackend)},
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
    {"print-with-postscript-type42-fonts",
     flag_descriptions::kPrintWithPostScriptType42FontsName,
     flag_descriptions::kPrintWithPostScriptType42FontsDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kPrintWithPostScriptType42Fonts)},

    {"print-with-reduced-rasterization",
     flag_descriptions::kPrintWithReducedRasterizationName,
     flag_descriptions::kPrintWithReducedRasterizationDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kPrintWithReducedRasterization)},

    {"read-printer-capabilities-with-xps",
     flag_descriptions::kReadPrinterCapabilitiesWithXpsName,
     flag_descriptions::kReadPrinterCapabilitiesWithXpsDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kReadPrinterCapabilitiesWithXps)},

    {"use-xps-for-printing", flag_descriptions::kUseXpsForPrintingName,
     flag_descriptions::kUseXpsForPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kUseXpsForPrinting)},

    {"use-xps-for-printing-from-pdf",
     flag_descriptions::kUseXpsForPrintingFromPdfName,
     flag_descriptions::kUseXpsForPrintingFromPdfDescription, kOsWin,
     FEATURE_VALUE_TYPE(printing::features::kUseXpsForPrintingFromPdf)},
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(IS_WIN)
    {"enable-windows-gaming-input-data-fetcher",
     flag_descriptions::kEnableWindowsGamingInputDataFetcherName,
     flag_descriptions::kEnableWindowsGamingInputDataFetcherDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableWindowsGamingInputDataFetcher)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"enable-start-surface", flag_descriptions::kStartSurfaceAndroidName,
     flag_descriptions::kStartSurfaceAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kStartSurfaceAndroid,
                                    kStartSurfaceAndroidVariations,
                                    "ChromeStart")},

    {"enable-feed-position-on-ntp", flag_descriptions::kFeedPositionAndroidName,
     flag_descriptions::kFeedPositionAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kFeedPositionAndroid,
                                    kFeedPositionAndroidVariations,
                                    "FeedPositionAndroid")},

    {"enable-instant-start", flag_descriptions::kInstantStartName,
     flag_descriptions::kInstantStartDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInstantStart)},

    {"enable-start-surface-refactor",
     flag_descriptions::kStartSurfaceRefactorName,
     flag_descriptions::kStartSurfaceRefactorDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kStartSurfaceRefactor)},

    {"enable-show-scrollable-mvt-on-ntp",
     flag_descriptions::kShowScrollableMVTOnNTPAndroidName,
     flag_descriptions::kShowScrollableMVTOnNTPAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShowScrollableMVTOnNTPAndroid)},

    {"enable-search-resumption-module",
     flag_descriptions::kSearchResumptionModuleAndroidName,
     flag_descriptions::kSearchResumptionModuleAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kSearchResumptionModuleAndroid,
         kSearchResumptionModuleAndroidVariations,
         "kSearchResumptionModuleAndroid")},

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

    {"suppress-toolbar-captures",
     flag_descriptions::kSuppressToolbarCapturesName,
     flag_descriptions::kSuppressToolbarCapturesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSuppressToolbarCaptures)},

    {"enable-tab-grid-layout", flag_descriptions::kTabGridLayoutAndroidName,
     flag_descriptions::kTabGridLayoutAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabGridLayoutAndroid,
                                    kTabGridLayoutAndroidVariations,
                                    "TabGridLayoutAndroid")},

    {"enable-commerce-coupons", flag_descriptions::kCommerceCouponsName,
     flag_descriptions::kCommerceCouponsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kCommerceCoupons)},

    {"enable-commerce-merchant-viewer",
     flag_descriptions::kCommerceMerchantViewerAndroidName,
     flag_descriptions::kCommerceMerchantViewerAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kCommerceMerchantViewer)},

    {"enable-commerce-price-tracking",
     commerce::flag_descriptions::kCommercePriceTrackingName,
     commerce::flag_descriptions::kCommercePriceTrackingDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         commerce::kCommercePriceTracking,
         commerce::kCommercePriceTrackingAndroidVariations,
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

    {"enable-tab-strip-improvements",
     flag_descriptions::kTabStripImprovementsAndroidName,
     flag_descriptions::kTabStripImprovementsAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabStripImprovements,
                                    kTabStripImprovementsTabWidthVariations,
                                    "TabStripImprovementsAndroid")},

    {"enable-conditional-tabstrip",
     flag_descriptions::kConditionalTabStripAndroidName,
     flag_descriptions::kConditionalTabStripAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kConditionalTabStripAndroid,
         kConditionalTabStripAndroidVariations,
         "ConditioanlTabStrip")},

    {"shopping-list", flag_descriptions::kShoppingListName,
     flag_descriptions::kShoppingListDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-toolbar-status-chip",
     flag_descriptions::kAutofillEnableToolbarStatusChipName,
     flag_descriptions::kAutofillEnableToolbarStatusChipDescription,
     kOsMac | kOsWin | kOsLinux | kOsLacros | kOsFuchsia,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableToolbarStatusChip)},

    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         network::switches::kUnsafelyTreatInsecureOriginAsSecure,
         "")},

    {"disable-process-reuse", flag_descriptions::kDisableProcessReuse,
     flag_descriptions::kDisableProcessReuseDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDisableProcessReuse)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-accessibility-live-caption",
     flag_descriptions::kEnableAccessibilityLiveCaptionName,
     flag_descriptions::kEnableAccessibilityLiveCaptionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kLiveCaption)},

    {"support-tool", flag_descriptions::kSupportTool,
     flag_descriptions::kSupportToolDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSupportTool)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"enable-auto-disable-accessibility",
     flag_descriptions::kEnableAutoDisableAccessibilityName,
     flag_descriptions::kEnableAutoDisableAccessibilityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kAutoDisableAccessibility)},

#if BUILDFLAG(IS_ANDROID)
    {"cct-incognito", flag_descriptions::kCCTIncognitoName,
     flag_descriptions::kCCTIncognitoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognito)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-incognito-available-to-third-party",
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyName,
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognitoAvailableToThirdParty)},
#endif

#if BUILDFLAG(IS_ANDROID)
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
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kCCTResizableForThirdParties,
         kCCTResizableThirdPartiesDefaultPolicyVariations,
         "CCTResizableThirdPartiesDefaultPolicy")},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-real-time-engagement-signals",
     flag_descriptions::kCCTRealTimeEngagementSignalsName,
     flag_descriptions::kCCTRealTimeEngagementSignalsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTRealTimeEngagementSignals)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-brand-transparency", flag_descriptions::kCCTBrandTransparencyName,
     flag_descriptions::kCCTBrandTransparencyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTBrandTransparency)},
#endif

#if BUILDFLAG(IS_CHROMEOS)
    {"allow-dsp-based-aec", flag_descriptions::kCrOSDspBasedAecAllowedName,
     flag_descriptions::kCrOSDspBasedAecAllowedDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrOSDspBasedAecAllowed)},
    {"allow-dsp-based-ns", flag_descriptions::kCrOSDspBasedNsAllowedName,
     flag_descriptions::kCrOSDspBasedNsAllowedDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrOSDspBasedNsAllowed)},
    {"allow-dsp-based-agc", flag_descriptions::kCrOSDspBasedAgcAllowedName,
     flag_descriptions::kCrOSDspBasedAgcAllowedDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrOSDspBasedAgcAllowed)},
    {"enable-cros-privacy-hub", flag_descriptions::kCrosPrivacyHubName,
     flag_descriptions::kCrosPrivacyHubDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrosPrivacyHub)},
    {"enforce-system-aec", flag_descriptions::kCrOSEnforceSystemAecName,
     flag_descriptions::kCrOSEnforceSystemAecDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAec)},
    {"enforce-system-aec-agc", flag_descriptions::kCrOSEnforceSystemAecAgcName,
     flag_descriptions::kCrOSEnforceSystemAecAgcDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAecAgc)},
    {"enforce-system-aec-ns-agc",
     flag_descriptions::kCrOSEnforceSystemAecNsAgcName,
     flag_descriptions::kCrOSEnforceSystemAecNsAgcDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kCrOSEnforceSystemAecNsAgc)},
    {"enforce-system-aec-ns", flag_descriptions::kCrOSEnforceSystemAecNsName,
     flag_descriptions::kCrOSEnforceSystemAecNsDescription, kOsCrOS | kOsLacros,
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

    {"drop-input-events-before-first-paint",
     flag_descriptions::kDropInputEventsBeforeFirstPaintName,
     flag_descriptions::kDropInputEventsBeforeFirstPaintDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDropInputEventsBeforeFirstPaint)},

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

#if BUILDFLAG(IS_WIN)
    {"calculate-native-win-occlusion",
     flag_descriptions::kCalculateNativeWinOcclusionName,
     flag_descriptions::kCalculateNativeWinOcclusionDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kCalculateNativeWinOcclusion)},
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
    {"happiness-tracking-surveys-for-desktop-demo",
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktopDemo)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"destroy-profile-on-browser-close",
     flag_descriptions::kDestroyProfileOnBrowserCloseName,
     flag_descriptions::kDestroyProfileOnBrowserCloseDescription,
     kOsMac | kOsWin | kOsLinux | kOsLacros | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kDestroyProfileOnBrowserClose)},

    {"destroy-system-profiles", flag_descriptions::kDestroySystemProfilesName,
     flag_descriptions::kDestroySystemProfilesDescription,
     kOsCrOS | kOsMac | kOsWin | kOsLinux | kOsLacros | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kDestroySystemProfiles)},

#if BUILDFLAG(IS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescriptionWindows, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoicesWindows)},
#elif BUILDFLAG(IS_MAC)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescriptionMac, kOsMac,
     MULTI_VALUE_TYPE(kUseAngleChoicesMac)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-dsp", flag_descriptions::kEnableGoogleAssistantDspName,
     flag_descriptions::kEnableGoogleAssistantDspDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kEnableDspHotword)},

    {"deprecate-assistant-stylus-features",
     flag_descriptions::kDeprecateAssistantStylusFeaturesName,
     flag_descriptions::kDeprecateAssistantStylusFeaturesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDeprecateAssistantStylusFeatures)},

    {"disable-quick-answers-v2-translation",
     flag_descriptions::kDisableQuickAnswersV2TranslationName,
     flag_descriptions::kDisableQuickAnswersV2TranslationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableQuickAnswersV2Translation)},

    {"quick-answers-for-more-locales",
     flag_descriptions::kQuickAnswersForMoreLocalesName,
     flag_descriptions::kQuickAnswersForMoreLocalesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersForMoreLocales)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"ntp-tiles-title-wrap-around",
     flag_descriptions::kNewTabPageTilesTitleWrapAroundName,
     flag_descriptions::kNewTabPageTilesTitleWrapAroundDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNewTabPageTilesTitleWrapAround)},

    {"new-window-app-menu", flag_descriptions::kNewWindowAppMenuName,
     flag_descriptions::kNewWindowAppMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNewWindowAppMenu)},

    {"instance-switcher", flag_descriptions::kInstanceSwitcherName,
     flag_descriptions::kInstanceSwitcherDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kInstanceSwitcher)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"enable-gamepad-button-axis-events",
     flag_descriptions::kEnableGamepadButtonAxisEventsName,
     flag_descriptions::kEnableGamepadButtonAxisEventsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableGamepadButtonAxisEvents)},

    {"restrict-gamepad-access", flag_descriptions::kRestrictGamepadAccessName,
     flag_descriptions::kRestrictGamepadAccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRestrictGamepadAccess)},

#if !BUILDFLAG(IS_ANDROID)
    {"sharing-desktop-screenshots",
     flag_descriptions::kSharingDesktopScreenshotsName,
     flag_descriptions::kSharingDesktopScreenshotsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(sharing_hub::kDesktopScreenshots)},
    {"sharing-desktop-screenshots-edit",
     flag_descriptions::kSharingDesktopScreenshotsEditName,
     flag_descriptions::kSharingDesktopScreenshotsEditDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(share::kSharingDesktopScreenshotsEdit)},
    {"sharing-desktop-share-preview",
     flag_descriptions::kSharingDesktopSharePreviewName,
     flag_descriptions::kSharingDesktopSharePreviewDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(share::kDesktopSharePreview,
                                    kDesktopSharePreviewVariations,
                                    "DesktopSharePreview")},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ash-enable-pip-rounded-corners",
     flag_descriptions::kAshEnablePipRoundedCornersName,
     flag_descriptions::kAshEnablePipRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipRoundedCorners)},
    {"cros-labs-float-window", flag_descriptions::kFloatWindow,
     flag_descriptions::kFloatWindowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::wm::features::kFloatWindow)},
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

#if !BUILDFLAG(IS_ANDROID)
    {"hardware-media-key-handling",
     flag_descriptions::kHardwareMediaKeyHandling,
     flag_descriptions::kHardwareMediaKeyHandlingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kHardwareMediaKeyHandling)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"app-management-app-details",
     flag_descriptions::kAppManagementAppDetailsName,
     flag_descriptions::kAppManagementAppDetailsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppManagementAppDetails)},

    {"arc-window-predictor", flag_descriptions::kArcWindowPredictorName,
     flag_descriptions::kArcWindowPredictorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kArcWindowPredictor)},

    {"arc-input-overlay", flag_descriptions::kArcInputOverlayName,
     flag_descriptions::kArcInputOverlayDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kArcInputOverlay)},

    {"full-restore-for-lacros", flag_descriptions::kFullRestoreForLacrosName,
     flag_descriptions::kFullRestoreForLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kFullRestoreForLacros)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    {"chromeos-direct-video-decoder",
     flag_descriptions::kChromeOSDirectVideoDecoderName,
     flag_descriptions::kChromeOSDirectVideoDecoderDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kUseChromeOSDirectVideoDecoder)},

    {"enable-vbr-encode-acceleration",
     flag_descriptions::kChromeOSHWVBREncodingName,
     flag_descriptions::kChromeOSHWVBREncodingDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kChromeOSHWVBREncoding)},
#if defined(ARCH_CPU_ARM_FAMILY)
    {"prefer-libyuv-image-processor",
     flag_descriptions::kPreferLibYuvImageProcessorName,
     flag_descriptions::kPreferLibYuvImageProcessorDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kPreferLibYuvImageProcessor)},
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_ANDROID)
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},

    {"tangible-sync", flag_descriptions::kTangibleSyncName,
     flag_descriptions::kTangibleSyncDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kTangibleSync)},

    {"enable-cbd-sign-out", flag_descriptions::kEnableCbdSignOutName,
     flag_descriptions::kEnableCbdSignOutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kEnableCbdSignOut)},
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_ANDROID)
    {"enable-logging-js-console-messages",
     flag_descriptions::kLogJsConsoleMessagesName,
     flag_descriptions::kLogJsConsoleMessagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLogJsConsoleMessages)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

    {"enable-neural-palm-adaptive-hold",
     flag_descriptions::kEnableNeuralPalmAdaptiveHoldName,
     flag_descriptions::kEnableNeuralPalmAdaptiveHoldDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableNeuralPalmAdaptiveHold)},

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

    {"enable-heuristic-stylus-palm-rejection",
     flag_descriptions::kEnableHeuristicStylusPalmRejectionName,
     flag_descriptions::kEnableHeuristicStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeuristicPalmDetectionFilter)},

    {"fast-pair", flag_descriptions::kFastPairName,
     flag_descriptions::kFastPairDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPair)},

    {"fast-pair-low-power", flag_descriptions::kFastPairLowPowerName,
     flag_descriptions::kFastPairLowPowerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairLowPower)},

    {"fast-pair-saved-devices", flag_descriptions::kFastPairSavedDevicesName,
     flag_descriptions::kFastPairSavedDevicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairSavedDevices)},

    {"frame-sink-desktop-capturer-in-crd",
     flag_descriptions::kFrameSinkDesktopCapturerInCrdName,
     flag_descriptions::kFrameSinkDesktopCapturerInCrdDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         remoting::features::kEnableFrameSinkDesktopCapturerInCrd)},

    {"enable-get-display-media-set", flag_descriptions::kGetDisplayMediaSetName,
     flag_descriptions::kGetDisplayMediaSetDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kGetDisplayMediaSet)},

    {"enable-get-display-media-set-auto-select-all-screens",
     flag_descriptions::kGetDisplayMediaSetAutoSelectAllScreensName,
     flag_descriptions::kGetDisplayMediaSetAutoSelectAllScreensDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kGetDisplayMediaSetAutoSelectAllScreens)},

    {"multi-monitors-in-crd", flag_descriptions::kMultiMonitorsInCrdName,
     flag_descriptions::kMultiMonitorsInCrdDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(remoting::features::kEnableMultiMonitorsInCrd)},

    {"fast-pair-software-scanning",
     flag_descriptions::kFastPairSoftwareScanningName,
     flag_descriptions::kFastPairSoftwareScanningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairSoftwareScanning)},

    {"fast-pair-subsequent-pairing-ux",
     flag_descriptions::kFastPairSubsequentPairingUXName,
     flag_descriptions::kFastPairSubsequentPairingUXDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairSubsequentPairingUX)},

    {"pcie-billboard-notification",
     flag_descriptions::kPcieBillboardNotificationName,
     flag_descriptions::kPcieBillboardNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPcieBillboardNotification)},

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

    {"enable-experimental-rgb-keyboard-patterns",
     flag_descriptions::kExperimentalRgbKeyboardPatternsName,
     flag_descriptions::kExperimentalRgbKeyboardPatternsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kExperimentalRgbKeyboardPatterns)},

    {"enable-hostname-setting", flag_descriptions::kEnableHostnameSettingName,
     flag_descriptions::kEnableHostnameSettingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableHostnameSetting)},

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

    {"enable-rgb-keyboard", flag_descriptions::kEnableRgbKeyboardName,
     flag_descriptions::kEnableRgbKeyboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kRgbKeyboard)},

    {"enhanced-network-voices", flag_descriptions::kEnhancedNetworkVoicesName,
     flag_descriptions::kEnhancedNetworkVoicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnhancedNetworkVoices)},

    {"enable-accessibility-os-settings-visibility",
     flag_descriptions::kAccessibilityOSSettingsVisibilityName,
     flag_descriptions::kAccessibilityOSSettingsVisibilityDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityOSSettingsVisibility)},
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
     FEATURE_VALUE_TYPE(net::features::kStorageAccessAPI)},

    {"enable-removing-all-third-party-cookies",
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesName,
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableRemovingAllThirdPartyCookies)},

    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription,
     kOsMac | kOsLinux | kOsLacros | kOsWin | kOsFuchsia,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},

    {"enable-webgpu-developer-features",
     flag_descriptions::kWebGpuDeveloperFeaturesName,
     flag_descriptions::kWebGpuDeveloperFeaturesDescription,
     kOsMac | kOsLinux | kOsLacros | kOsWin | kOsFuchsia,
     SINGLE_VALUE_TYPE(switches::kEnableWebGPUDeveloperFeatures)},

#if BUILDFLAG(IS_ANDROID)
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"auto-screen-brightness", flag_descriptions::kAutoScreenBrightnessName,
     flag_descriptions::kAutoScreenBrightnessDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAutoScreenBrightness)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"metrics-settings-android", flag_descriptions::kMetricsSettingsAndroidName,
     flag_descriptions::kMetricsSettingsAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kMetricsSettingsAndroid,
                                    kMetricsSettingsAndroidVariations,
                                    "MetricsSettingsAndroid")},
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
     kOsWin | kOsMac | kOsLinux | kOsLacros | kOsFuchsia,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardUploadFeedback)},

    {"font-access", flag_descriptions::kFontAccessAPIName,
     flag_descriptions::kFontAccessAPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFontAccess)},

    {"mouse-subframe-no-implicit-capture",
     flag_descriptions::kMouseSubframeNoImplicitCaptureName,
     flag_descriptions::kMouseSubframeNoImplicitCaptureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kMouseSubframeNoImplicitCapture)},

#if BUILDFLAG(IS_CHROMEOS)
    {"global-media-controls-for-cast",
     flag_descriptions::kGlobalMediaControlsForCastName,
     flag_descriptions::kGlobalMediaControlsForCastDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsForCast)},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
    {"global-media-controls-modern-ui",
     flag_descriptions::kGlobalMediaControlsModernUIName,
     flag_descriptions::kGlobalMediaControlsModernUIDescription,
     kOsWin | kOsMac | kOsLinux | kOsLacros | kOsCrOS | kOsFuchsia,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsModernUI)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

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

#if BUILDFLAG(IS_ANDROID)

    {"debug-chime-notification",
     flag_descriptions::kChimeAlwaysShowNotificationName,
     flag_descriptions::kChimeAlwaysShowNotificationDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(notifications::switches::kDebugChimeNotification)},

    {"use-chime-android-sdk", flag_descriptions::kChimeAndroidSdkName,
     flag_descriptions::kChimeAndroidSdkDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(notifications::features::kUseChimeAndroidSdk)},

#endif  // BUILDFLAG(IS_ANDROID)

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

    {"dns-https-svcb", flag_descriptions::kDnsHttpsSvcbName,
     flag_descriptions::kDnsHttpsSvcbDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(net::features::kUseDnsHttpsSvcb,
                                    kUseDnsHttpsSvcbVariations,
                                    "UseDnsHttpsSvcb")},

    {"encrypted-client-hello", flag_descriptions::kEncryptedClientHelloName,
     flag_descriptions::kEncryptedClientHelloDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEncryptedClientHello)},

    {"use-dns-https-svcb-alpn", flag_descriptions::kUseDnsHttpsSvcbAlpnName,
     flag_descriptions::kUseDnsHttpsSvcbAlpnDescription,
     kOsLinux | kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kUseDnsHttpsSvcbAlpn)},

    {"web-bundles", flag_descriptions::kWebBundlesName,
     flag_descriptions::kWebBundlesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebBundles)},

#if !BUILDFLAG(IS_ANDROID)
    {"hidpi-capture", flag_descriptions::kWebContentsCaptureHiDpiName,
     flag_descriptions::kWebContentsCaptureHiDpiDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kWebContentsCaptureHiDpi)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"web-otp-backend", flag_descriptions::kWebOtpBackendName,
     flag_descriptions::kWebOtpBackendDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kWebOtpBackendChoices)},

    {"darken-websites-checkbox-in-themes-setting",
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingName,
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         content_settings::kDarkenWebsitesCheckboxInThemesSetting)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"enable-autofill-upi-vpa", flag_descriptions::kAutofillSaveAndFillVPAName,
     flag_descriptions::kAutofillSaveAndFillVPADescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveAndFillVPA)},

#if BUILDFLAG(IS_ANDROID)
    {"context-menu-google-lens-chip",
     flag_descriptions::kContextMenuGoogleLensChipName,
     flag_descriptions::kContextMenuGoogleLensChipDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuGoogleLensChip)},

    {"context-menu-search-with-google-lens",
     flag_descriptions::kContextMenuSearchWithGoogleLensName,
     flag_descriptions::kContextMenuSearchWithGoogleLensDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextMenuSearchWithGoogleLens,
         kLensContextMenuSearchVariations,
         "ContextMenuSearchWithGoogleLens")},

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

    {"lens-camera-assisted-search",
     flag_descriptions::kLensCameraAssistedSearchName,
     flag_descriptions::kLensCameraAssistedSearchDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kLensCameraAssistedSearch,
                                    kLensCameraAssistedSearchVariations,
                                    "LensCameraAssistedSearch")},

    {"location-bar-model-optimizations",
     flag_descriptions::kLocationBarModelOptimizationsName,
     flag_descriptions::kLocationBarModelOptimizationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kLocationBarModelOptimizations)},

    {"enable-iph", flag_descriptions::kEnableIphName,
     flag_descriptions::kEnableIphDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feature_engagement::kEnableIPH)},
#endif  // BUILDFLAG(IS_ANDROID)

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
    {"enable-back-forward-cache-for-screen-reader",
     flag_descriptions::kEnableBackForwardCacheForScreenReaderName,
     flag_descriptions::kEnableBackForwardCacheForScreenReaderDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableBackForwardCacheForScreenReader)},

#if !BUILDFLAG(IS_ANDROID)
    {"closed-tab-cache", flag_descriptions::kClosedTabCacheName,
     flag_descriptions::kClosedTabCacheDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kClosedTabCache)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"windows-scrolling-personality",
     flag_descriptions::kWindowsScrollingPersonalityName,
     flag_descriptions::kWindowsScrollingPersonalityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWindowsScrollingPersonality)},

    {"scroll-unification", flag_descriptions::kScrollUnificationName,
     flag_descriptions::kScrollUnificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kScrollUnification)},

#if BUILDFLAG(IS_WIN)
    {"elastic-overscroll", flag_descriptions::kElasticOverscrollName,
     flag_descriptions::kElasticOverscrollDescription, kOsWin | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kElasticOverscroll)},
#elif BUILDFLAG(IS_ANDROID)
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
    {"device-force-scheduled-reboot",
     flag_descriptions::kDeviceForceScheduledRebootName,
     flag_descriptions::kDeviceForceScheduledRebootDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDeviceForceScheduledReboot)},
    {"enable-assistant-aec", flag_descriptions::kEnableGoogleAssistantAecName,
     flag_descriptions::kEnableGoogleAssistantAecDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantAudioEraser)},
#endif

#if BUILDFLAG(IS_WIN)
    {"enable-winrt-geolocation-implementation",
     flag_descriptions::kWinrtGeolocationImplementationName,
     flag_descriptions::kWinrtGeolocationImplementationDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWinrtGeolocationImplementation)},
#endif

#if BUILDFLAG(IS_MAC)
    {"enable-core-location-backend",
     flag_descriptions::kMacCoreLocationBackendName,
     flag_descriptions::kMacCoreLocationBackendDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacCoreLocationBackend)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"mute-notification-snooze-action",
     flag_descriptions::kMuteNotificationSnoozeActionName,
     flag_descriptions::kMuteNotificationSnoozeActionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMuteNotificationSnoozeAction)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_MAC)
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
    {"media-app-handles-pdf", flag_descriptions::kMediaAppHandlesPdfName,
     flag_descriptions::kMediaAppHandlesPdfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppHandlesPdf)},
    {"media-app-photos-integration-image",
     flag_descriptions::kMediaAppPhotosIntegrationImageName,
     flag_descriptions::kMediaAppPhotosIntegrationImageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppPhotosIntegrationImage)},
    {"media-app-photos-integration-video",
     flag_descriptions::kMediaAppPhotosIntegrationVideoName,
     flag_descriptions::kMediaAppPhotosIntegrationVideoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaAppPhotosIntegrationVideo)},
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

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-manual-fallback-for-virtual-cards",
     flag_descriptions::kAutofillEnableManualFallbackForVirtualCardsName,
     flag_descriptions::kAutofillEnableManualFallbackForVirtualCardsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableManualFallbackForVirtualCards)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"account-id-migration", flag_descriptions::kAccountIdMigrationName,
     flag_descriptions::kAccountIdMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(switches::kAccountIdMigration)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)
    {"paint-preview-demo", flag_descriptions::kPaintPreviewDemoName,
     flag_descriptions::kPaintPreviewDemoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(paint_preview::kPaintPreviewDemo)},
    {"paint-preview-startup", flag_descriptions::kPaintPreviewStartupName,
     flag_descriptions::kPaintPreviewStartupDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(paint_preview::kPaintPreviewShowOnStartup,
                                    kPaintPreviewStartupVariations,
                                    "PaintPreviewShowOnStartup")},
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

    {"full-user-agent", flag_descriptions::kFullUserAgentName,
     flag_descriptions::kFullUserAgentDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kFullUserAgent)},

    {"reduce-user-agent", flag_descriptions::kReduceUserAgentName,
     flag_descriptions::kReduceUserAgentDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kReduceUserAgent)},

#if BUILDFLAG(IS_WIN)
    {"run-video-capture-service-in-browser",
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessName,
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(features::kRunVideoCaptureServiceInBrowserProcess)},
#endif  // BUILDFLAG(IS_WIN)

    {"double-buffer-compositing",
     flag_descriptions::kDoubleBufferCompositingName,
     flag_descriptions::kDoubleBufferCompositingDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDoubleBufferCompositing)},

    {"password-domain-capabilities-fetching",
     flag_descriptions::kPasswordDomainCapabilitiesFetchingName,
     flag_descriptions::kPasswordDomainCapabilitiesFetchingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kPasswordDomainCapabilitiesFetching,
         kPasswordDomainCapabilitiesFetchingFeatureVariations,
         "PasswordDomainCapabilitiesFetchingFeatureVariations")},
    {"force-enable-password-domain-capabilities",
     flag_descriptions::kForceEnablePasswordDomainCapabilitiesName,
     flag_descriptions::kForceEnablePasswordDomainCapabilitiesDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kForceEnablePasswordDomainCapabilities)},
    {"password-change-support", flag_descriptions::kPasswordChangeName,
     flag_descriptions::kPasswordChangeDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(password_manager::features::kPasswordChange,
                                    kPasswordChangeFeatureVariations,
                                    "PasswordChangeFeatureVariations")},
    {"password-change-in-settings",
     flag_descriptions::kPasswordChangeInSettingsName,
     flag_descriptions::kPasswordChangeInSettingsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kPasswordChangeInSettings,
         kPasswordChangeInSettingsFeatureVariations,
         "PasswordChangeInSettingsFeatureVariations")},

#if BUILDFLAG(IS_ANDROID)
    {"password-scripts-fetching",
     flag_descriptions::kPasswordScriptsFetchingName,
     flag_descriptions::kPasswordScriptsFetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordScriptsFetching)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"page-info-discoverability-timeouts",
     flag_descriptions::kPageInfoDiscoverabilityTimeoutsName,
     flag_descriptions::kPageInfoDiscoverabilityTimeoutsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         page_info::kPageInfoDiscoverability,
         kPageInfoDiscoverabilityTimeoutVariations,
         "kPageInfoDiscoverabilityTimeoutVariations")},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"page-info-hide-site-settings",
     flag_descriptions::kPageInfoHideSiteSettingsName,
     flag_descriptions::kPageInfoHideSiteSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHideSiteSettings)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"page-info-history", flag_descriptions::kPageInfoHistoryName,
     flag_descriptions::kPageInfoHistoryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistory)},
    {"page-info-store-info", flag_descriptions::kPageInfoStoreInfoName,
     flag_descriptions::kPageInfoStoreInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoStoreInfo)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"page-info-history-desktop",
     flag_descriptions::kPageInfoHistoryDesktopName,
     flag_descriptions::kPageInfoHistoryDesktopDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistoryDesktop)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"page-info-about-this-site", flag_descriptions::kPageInfoAboutThisSiteName,
     flag_descriptions::kPageInfoAboutThisSiteDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoAboutThisSiteEn)},

    {"page-info-more-about-this-page",
     flag_descriptions::kPageInfoMoreAboutThisPageName,
     flag_descriptions::kPageInfoMoreAboutThisPageDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(page_info::kPageInfoAboutThisSiteMoreInfo)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kClipboardHistoryReorderInternalName,
     flag_descriptions::kClipboardHistoryReorderName,
     flag_descriptions::kClipboardHistoryReorderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kClipboardHistoryReorder)},
    {"enhanced_clipboard_nudge_session_reset",
     flag_descriptions::kEnhancedClipboardNudgeSessionResetName,
     flag_descriptions::kEnhancedClipboardNudgeSessionResetDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kClipboardHistoryNudgeSessionReset)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
    {"enable-media-foundation-video-capture",
     flag_descriptions::kEnableMediaFoundationVideoCaptureName,
     flag_descriptions::kEnableMediaFoundationVideoCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kMediaFoundationVideoCapture)},
#endif  // BUILDFLAG(IS_WIN)

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

#if !BUILDFLAG(IS_ANDROID)
    {"copy-link-to-text", flag_descriptions::kCopyLinkToTextName,
     flag_descriptions::kCopyLinkToTextDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCopyLinkToText)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"shared-highlighting-amp", flag_descriptions::kSharedHighlightingAmpName,
     flag_descriptions::kSharedHighlightingAmpDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingAmp)},
    {"shared-highlighting-manager",
     flag_descriptions::kSharedHighlightingManagerName,
     flag_descriptions::kSharedHighlightingManagerDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingManager)},
    {"shared-highlighting-refined-blocklist",
     flag_descriptions::kSharedHighlightingRefinedBlocklistName,
     flag_descriptions::kSharedHighlightingRefinedBlocklistDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         shared_highlighting::kSharedHighlightingRefinedBlocklist)},
    {"shared-highlighting-refined-maxcontextwords",
     flag_descriptions::kSharedHighlightingRefinedMaxContextWordsName,
     flag_descriptions::kSharedHighlightingRefinedMaxContextWordsDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         shared_highlighting::kSharedHighlightingRefinedMaxContextWords,
         kSharedHighlightingMaxContextWordsVariations,
         "SharedHighlightingRefinedMaxContextWords")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"shimless-rma-flow", flag_descriptions::kShimlessRMAFlowName,
     flag_descriptions::kShimlessRMAFlowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShimlessRMAFlow)},
    {"shimless-rma-enable-standalone",
     flag_descriptions::kShimlessRMAEnableStandaloneName,
     flag_descriptions::kShimlessRMAEnableStandaloneDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShimlessRMAFlow)},
    {"shimless-rma-os-update", flag_descriptions::kShimlessRMAOsUpdateName,
     flag_descriptions::kShimlessRMAOsUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShimlessRMAOsUpdate)},
    {"shimless-rma-disable-dark-mode",
     flag_descriptions::kShimlessRMADisableDarkModeName,
     flag_descriptions::kShimlessRMADisableDarkModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShimlessRMADisableDarkMode)},
    {"nearby-sharing-arc", flag_descriptions::kNearbySharingArcName,
     flag_descriptions::kNearbySharingArcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableArcNearbyShare)},
    {"nearby-sharing-background-scanning",
     flag_descriptions::kNearbySharingBackgroundScanningName,
     flag_descriptions::kNearbySharingBackgroundScanningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingBackgroundScanning)},
    {"nearby-sharing-one-page-onboarding",
     flag_descriptions::kNearbySharingOnePageOnboardingName,
     flag_descriptions::kNearbySharingOnePageOnboardingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingOnePageOnboarding)},
    {"nearby-sharing-self-share-auto-accept",
     flag_descriptions::kNearbySharingSelfShareAutoAcceptName,
     flag_descriptions::kNearbySharingSelfShareAutoAcceptDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingSelfShareAutoAccept)},
    {"nearby-sharing-self-share-ui",
     flag_descriptions::kNearbySharingSelfShareUIName,
     flag_descriptions::kNearbySharingSelfShareUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingSelfShareUI)},
    {"nearby-sharing-visibility-reminder",
     flag_descriptions::kNearbySharingVisibilityReminderName,
     flag_descriptions::kNearbySharingVisibilityReminderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingVisibilityReminder)},
    {"nearby-sharing-wifilan", flag_descriptions::kNearbySharingWifiLanName,
     flag_descriptions::kNearbySharingWifiLanDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingWifiLan)},
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

    {"enable-machine-learning-model-loader-web-platform-api",
     flag_descriptions::kEnableMachineLearningModelLoaderWebPlatformApiName,
     flag_descriptions::
         kEnableMachineLearningModelLoaderWebPlatformApiDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kEnableMachineLearningModelLoaderWebPlatformApi)},

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
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kProductivityLauncher,
                                    kProductivityLauncherVariations,
                                    "ProductivityLauncher")},
    {"launcher-item-suggest", flag_descriptions::kLauncherItemSuggestName,
     flag_descriptions::kLauncherItemSuggestDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(app_list::ItemSuggestCache::kExperiment,
                                    kLauncherItemSuggestVariations,
                                    "LauncherItemSuggest")},
    {"autocomplete-extended-suggestions",
     flag_descriptions::kAutocompleteExtendedSuggestionsName,
     flag_descriptions::kAutocompleteExtendedSuggestionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAutocompleteExtendedSuggestions)},
    {"compact-bubble-launcher", flag_descriptions::kCompactBubbleLauncherName,
     flag_descriptions::kCompactBubbleLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kCompactBubbleLauncher)},
    {"shelf-drag-to-pin", flag_descriptions::kShelfDragToPinName,
     flag_descriptions::kShelfDragToPinDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragUnpinnedAppToPin)},
    {"shelf-gestures-with-vk",
     flag_descriptions::kShelfGesturesWithVirtualKeyboardName,
     flag_descriptions::kShelfGesturesWithVirtualKeyboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShelfGesturesWithVirtualKeyboard)},
    {"shelf-palm-rejection-swipe-offset",
     flag_descriptions::kShelfPalmRejectionSwipeOffsetName,
     flag_descriptions::kShelfPalmRejectionSwipeOffsetDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShelfPalmRejectionSwipeOffset)},
    {"launcher-game-search", flag_descriptions::kLauncherGameSearchName,
     flag_descriptions::kLauncherGameSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherGameSearch)},
    {"launcher-hide-continue-section",
     flag_descriptions::kLauncherHideContinueSectionName,
     flag_descriptions::kLauncherHideContinueSectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLauncherHideContinueSection)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"launcher-pulsing-blocks-refresh",
     flag_descriptions::kLauncherPulsingBlocksRefreshName,
     flag_descriptions::kLauncherPulsingBlocksRefreshDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLauncherPulsingBlocksRefresh)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"categorical-search", flag_descriptions::kCategoricalSearchName,
     flag_descriptions::kCategoricalSearchDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(app_list_features::kCategoricalSearch,
                                    kCategoricalSearchVariations,
                                    "LauncherCategoricalSearch")},

    {"search-result-inline-icon",
     flag_descriptions::kSearchResultInlineIconName,
     flag_descriptions::kSearchResultInlineIconDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kSearchResultInlineIcon)},

    {"dynamic-search-update-animation",
     flag_descriptions::kDynamicSearchUpdateAnimationName,
     flag_descriptions::kDynamicSearchUpdateAnimationDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         app_list_features::kDynamicSearchUpdateAnimation,
         kDynamicSearchUpdateAnimationVariations,
         "LauncherDynamicAnimations")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-bluetooth-spp-in-serial-api",
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiName,
     flag_descriptions::kEnableBluetoothSerialPortProfileInSerialApiDescription,
     kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableBluetoothSerialPortProfileInSerialApi)},

    {"password-view-page-in-settings",
     flag_descriptions::kPasswordViewPageInSettingsName,
     flag_descriptions::kPasswordViewPageInSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordViewPageInSettings)},

    {"password-notes", flag_descriptions::kPasswordNotesName,
     flag_descriptions::kPasswordNotesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordNotes)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"frame-throttle-fps", flag_descriptions::kFrameThrottleFpsName,
     flag_descriptions::kFrameThrottleFpsDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kFrameThrottleFpsChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
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

    {"consolidated-site-storage-controls",
     flag_descriptions::kConsolidatedSiteStorageControlsName,
     flag_descriptions::kConsolidatedSiteStorageControlsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kConsolidatedSiteStorageControls)},

#if BUILDFLAG(IS_ANDROID)
    {"enable-surface-control", flag_descriptions::kAndroidSurfaceControlName,
     flag_descriptions::kAndroidSurfaceControlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidSurfaceControl)},

    {"enable-image-reader", flag_descriptions::kAImageReaderName,
     flag_descriptions::kAImageReaderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAImageReader)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"smart-suggestion-for-large-downloads",
     flag_descriptions::kSmartSuggestionForLargeDownloadsName,
     flag_descriptions::kSmartSuggestionForLargeDownloadsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kSmartSuggestionForLargeDownloads)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_JXL_DECODER)
    {"enable-jxl", flag_descriptions::kEnableJXLName,
     flag_descriptions::kEnableJXLDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kJXL)},
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

#if BUILDFLAG(IS_ANDROID)
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
    {"messages-for-android-instant-apps",
     flag_descriptions::kMessagesForAndroidInstantAppsName,
     flag_descriptions::kMessagesForAndroidInstantAppsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidInstantApps)},
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
    {"messages-for-android-offer-notification",
     flag_descriptions::kMessagesForAndroidOfferNotificationName,
     flag_descriptions::kMessagesForAndroidOfferNotificationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidOfferNotification)},
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
    {"messages-for-android-stacking-animation",
     flag_descriptions::kMessagesForAndroidStackingAnimationName,
     flag_descriptions::kMessagesForAndroidStackingAnimationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidStackingAnimation)},
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

#if BUILDFLAG(IS_ANDROID)
    {"android-force-app-language-prompt",
     flag_descriptions::kAndroidForceAppLanguagePromptName,
     flag_descriptions::kAndroidForceAppLanguagePromptDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kForceAppLanguagePrompt)},
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
    {"quick-commands", flag_descriptions::kQuickCommandsName,
     flag_descriptions::kQuickCommandsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kQuickCommands)},

    {"desktop-detailed-language-settings",
     flag_descriptions::kDesktopDetailedLanguageSettingsName,
     flag_descriptions::kDesktopDetailedLanguageSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(language::kDesktopDetailedLanguageSettings)},
#endif

#if BUILDFLAG(IS_WIN)
    {"pwa-uninstall-in-windows-os",
     flag_descriptions::kPwaUninstallInWindowsOsName,
     flag_descriptions::kPwaUninstallInWindowsOsDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableWebAppUninstallFromOsSettings)},
#endif

    {"pwa-update-dialog-for-icon",
     flag_descriptions::kPwaUpdateDialogForAppIconName,
     flag_descriptions::kPwaUpdateDialogForAppIconDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPwaUpdateDialogForIcon)},

    {"pwa-update-dialog-for-name",
     flag_descriptions::kPwaUpdateDialogForAppTitleName,
     flag_descriptions::kPwaUpdateDialogForAppTitleDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPwaUpdateDialogForName)},

#if BUILDFLAG(IS_ANDROID)
    {"sync-android-promos-with-alternative-title",
     flag_descriptions::kSyncAndroidPromosWithAlternativeTitleName,
     flag_descriptions::kSyncAndroidPromosWithAlternativeTitleDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(syncer::kSyncAndroidPromosWithAlternativeTitle)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"sync-android-promos-with-illustration",
     flag_descriptions::kSyncAndroidPromosWithIllustrationName,
     flag_descriptions::kSyncAndroidPromosWithIllustrationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(syncer::kSyncAndroidPromosWithIllustration)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"sync-android-promos-with-single-button",
     flag_descriptions::kSyncAndroidPromosWithSingleButtonName,
     flag_descriptions::kSyncAndroidPromosWithSingleButtonDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(syncer::kSyncAndroidPromosWithSingleButton)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"sync-android-promos-with-title",
     flag_descriptions::kSyncAndroidPromosWithTitleName,
     flag_descriptions::kSyncAndroidPromosWithTitleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(syncer::kSyncAndroidPromosWithTitle)},
#endif

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

    {"privacy-sandbox-v3-desktop", flag_descriptions::kPrivacySandboxV3Name,
     flag_descriptions::kPrivacySandboxV3Description, kOsDesktop,
     // Use a command-line parameter instead of a FEATURE_VALUE_TYPE to enable
     // multiple related features.
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kEnableFeatures,
         "PrivacySandboxSettings3:"
         "disable-dialog-for-testing/true/show-sample-data/true,"
         "EnableFetchingAccountCapabilities,InterestGroupStorage,"
         "AdInterestGroupAPI,Fledge,FencedFrames")},

    {"privacy-sandbox-v3-android", flag_descriptions::kPrivacySandboxV3Name,
     flag_descriptions::kPrivacySandboxV3Description, kOsAndroid,
     // Use a command-line parameter instead of a FEATURE_VALUE_TYPE to enable
     // multiple related features when they are available.
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kEnableFeatures,
         "PrivacySandboxSettings3:"
         "disable-dialog-for-testing/true/show-sample-data/true,"
         "EnableFetchingAccountCapabilities,InterestGroupStorage,"
         "AdInterestGroupAPI,Fledge,FencedFrames")},

    {"privacy-sandbox-ads-apis",
     flag_descriptions::kPrivacySandboxAdsAPIsOverrideName,
     flag_descriptions::kPrivacySandboxAdsAPIsOverrideDescription, kOsAll,
     // Use a command-line parameter instead of a FEATURE_VALUE_TYPE to enable
     // multiple related features when they are available.
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kEnableFeatures,
                                 "PrivacySandboxAdsAPIsOverride,"
                                 "InterestGroupStorage,Fledge,"
                                 "BiddingAndScoringDebugReportingAPI,"
                                 "AllowURNsInIframes,BrowsingTopics,"
                                 "ConversionMeasurement,FencedFrames,"
                                 "OverridePrivacySandboxSettingsLocalTesting,"
                                 "SharedStorageAPI")},

#if BUILDFLAG(IS_ANDROID)
    {"site-data-improvements", flag_descriptions::kSiteDataImprovementsName,
     flag_descriptions::kSiteDataImprovementsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(browser_ui::kSiteDataImprovements)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"animated-image-resume", flag_descriptions::kAnimatedImageResumeName,
     flag_descriptions::kAnimatedImageResumeDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kAnimatedImageResume)},

#if !BUILDFLAG(IS_ANDROID)
    {"sct-auditing", flag_descriptions::kSCTAuditingName,
     flag_descriptions::kSCTAuditingDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kSCTAuditing,
                                    kSCTAuditingVariations,
                                    "SCTAuditingVariations")},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"incognito-screenshot", flag_descriptions::kIncognitoScreenshotName,
     flag_descriptions::kIncognitoScreenshotDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kIncognitoScreenshot)},

    {"incognito-downloads-warning",
     flag_descriptions::kIncognitoDownloadsWarningName,
     flag_descriptions::kIncognitoDownloadsWarningDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kIncognitoDownloadsWarning)},
#endif

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

    {"deferred-font-shaping", flag_descriptions::kDeferredFontShapingName,
     flag_descriptions::kDeferredFontShapingDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDeferredFontShaping)},

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
    {"search-suggestion-for-prerender2",
     flag_descriptions::kSupportSearchSuggestionForPrerender2Name,
     flag_descriptions::kSupportSearchSuggestionForPrerender2Description,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kSupportSearchSuggestionForPrerender2,
         kSearchSuggsetionPrerenderTypeVariations,
         "SearchSuggestionPrerender")},

    {"chrome-labs", flag_descriptions::kChromeLabsName,
     flag_descriptions::kChromeLabsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kChromeLabs)},

    {"enable-first-party-sets", flag_descriptions::kEnableFirstPartySetsName,
     flag_descriptions::kEnableFirstPartySetsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFirstPartySets)},

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-offers-in-clank-keyboard-accessory",
     flag_descriptions::kAutofillEnableOffersInClankKeyboardAccessoryName,
     flag_descriptions::
         kAutofillEnableOffersInClankKeyboardAccessoryDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableOffersInClankKeyboardAccessory)},
#endif

#if BUILDFLAG(ENABLE_PDF)

#if !BUILDFLAG(IS_ANDROID)
    {"pdf-ocr", flag_descriptions::kPdfOcrName,
     flag_descriptions::kPdfOcrDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPdfOcr)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"pdf-xfa-forms", flag_descriptions::kPdfXfaFormsName,
     flag_descriptions::kPdfXfaFormsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfXfaSupport)},
#endif  // BUILDFLAG(ENABLE_PDF)

    {"send-tab-to-self-signin-promo",
     flag_descriptions::kSendTabToSelfSigninPromoName,
     flag_descriptions::kSendTabToSelfSigninPromoDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfSigninPromo)},

#if BUILDFLAG(IS_ANDROID)
    {"send-tab-to-self-v2", flag_descriptions::kSendTabToSelfV2Name,
     flag_descriptions::kSendTabToSelfV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfV2)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    {"raw-audio-capture", flag_descriptions::kRawAudioCaptureName,
     flag_descriptions::kRawAudioCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kWasapiRawAudioCapture)},
#endif  // BUILDFLAG(IS_WIN)

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
    {kWallpaperFastRefreshInternalName,
     flag_descriptions::kWallpaperFastRefreshName,
     flag_descriptions::kWallpaperFastRefreshDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperFastRefresh)},
    {kWallpaperFullScreenPreviewInternalName,
     flag_descriptions::kWallpaperFullScreenPreviewName,
     flag_descriptions::kWallpaperFullScreenPreviewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperFullScreenPreview)},
    {kWallpaperGooglePhotosIntegrationInternalName,
     flag_descriptions::kWallpaperGooglePhotosIntegrationName,
     flag_descriptions::kWallpaperGooglePhotosIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperGooglePhotosIntegration)},
    {kWallpaperPerDeskName, flag_descriptions::kWallpaperPerDeskName,
     flag_descriptions::kWallpaperPerDeskDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperPerDesk)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"enable-vaapi-av1-decode-acceleration",
     flag_descriptions::kVaapiAV1DecoderName,
     flag_descriptions::kVaapiAV1DecoderDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kVaapiAV1Decoder)},

    {"default-chrome-apps-migration",
     flag_descriptions::kDefaultChromeAppsMigrationName,
     flag_descriptions::kDefaultChromeAppsMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(policy::features::kDefaultChromeAppsMigration)},

    {"messages-preinstall", flag_descriptions::kMessagesPreinstallName,
     flag_descriptions::kMessagesPreinstallDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(web_app::kMessagesPreinstall)},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
    // TODO(b/214589754): revisit the need for this flag when the final design
    // for HW encoding is implemented for lacros-chrome.
    {"enable-vaapi-vp9-kSVC-encode-acceleration",
     flag_descriptions::kVaapiVP9kSVCEncoderName,
     flag_descriptions::kVaapiVP9kSVCEncoderDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kVaapiVp9kSVCHWEncoding)},
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)

    {"enable-global-vaapi-lock", flag_descriptions::kGlobalVaapiLockName,
     flag_descriptions::kGlobalVaapiLockDescription,
     kOsCrOS | kOsLinux | kOsLacros,
     FEATURE_VALUE_TYPE(media::kGlobalVaapiLock)},

    {"enable-vp9-kSVC-decode-acceleration",
     flag_descriptions::kVp9kSVCHWDecodingName,
     flag_descriptions::kVp9kSVCHWDecodingDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kVp9kSVCHWDecoding)},

#if BUILDFLAG(IS_WIN) ||                                      \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
    {
        "ui-debug-tools",
        flag_descriptions::kUIDebugToolsName,
        flag_descriptions::kUIDebugToolsDescription,
        kOsWin | kOsLinux | kOsLacros | kOsMac | kOsFuchsia,
        FEATURE_VALUE_TYPE(features::kUIDebugTools),
    },
#endif
    {"http-cache-partitioning",
     flag_descriptions::kSplitCacheByNetworkIsolationKeyName,
     flag_descriptions::kSplitCacheByNetworkIsolationKeyDescription,
     kOsWin | kOsLinux | kOsLacros | kOsMac | kOsCrOS | kOsAndroid | kOsFuchsia,
     FEATURE_VALUE_TYPE(net::features::kSplitCacheByNetworkIsolationKey)},

    {"autofill-address-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptName,
     flag_descriptions::kEnableAutofillAddressSavePromptDescription,
     kOsWin | kOsMac | kOsLinux | kOsLacros | kOsCrOS | kOsAndroid | kOsFuchsia,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAddressProfileSavePrompt)},

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
    {"optimization-guide-push-notifications",
     flag_descriptions::kOptimizationGuidePushNotificationName,
     flag_descriptions::kOptimizationGuidePushNotificationDescription,
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

    {"sanitizer-api-v0", flag_descriptions::kSanitizerApiv0Name,
     flag_descriptions::kSanitizerApiv0Description, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kSanitizerAPIv0)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"productivity-reorder-apps", flag_descriptions::kLauncherAppSortName,
     flag_descriptions::kLauncherAppSortDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kLauncherAppSort)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-input-event-logging",
     flag_descriptions::kEnableInputEventLoggingName,
     flag_descriptions::kEnableInputEventLoggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableInputEventLogging)},
#endif

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

    {"autofill-prevent-overriding-prefilled-values",
     flag_descriptions::kAutofillPreventOverridingPrefilledValuesName,
     flag_descriptions::kAutofillPreventOverridingPrefilledValuesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillPreventOverridingPrefilledValues)},

    {"autofill-show-manual-fallbacks-in-context-menu",
     flag_descriptions::kAutofillShowManualFallbackInContextMenuName,
     flag_descriptions::kAutofillShowManualFallbackInContextMenuDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillShowManualFallbackInContextMenu)},

    {flag_descriptions::kEnableLensFullscreenSearchFlagId,
     flag_descriptions::kEnableLensFullscreenSearchName,
     flag_descriptions::kEnableLensFullscreenSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(lens::features::kLensSearchOptimizations)},
    {flag_descriptions::kEnableLensStandaloneFlagId,
     flag_descriptions::kEnableLensStandaloneName,
     flag_descriptions::kEnableLensStandaloneDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(lens::features::kLensStandalone,
                                    kLensStandaloneVariations,
                                    "GoogleLensDesktopContextMenuSearch")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-log-controller-for-diagnostics-app",
     flag_descriptions::kEnableLogControllerForDiagnosticsAppName,
     flag_descriptions::kEnableLogControllerForDiagnosticsAppDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kEnableLogControllerForDiagnosticsApp)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-penetrating-image-selection",
     flag_descriptions::kEnablePenetratingImageSelectionName,
     flag_descriptions::kEnablePenetratingImageSelectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEnablePenetratingImageSelection)},

#if BUILDFLAG(IS_ANDROID)
    {"biometric-reauth-password-filling",
     flag_descriptions::kBiometricReauthForPasswordFillingName,
     flag_descriptions::kBiometricReauthForPasswordFillingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kBiometricTouchToFill)},
    {"touch-to-fill-password-submission",
     flag_descriptions::kTouchToFillPasswordSubmissionName,
     flag_descriptions::kTouchToFillPasswordSubmissionDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kTouchToFillPasswordSubmission,
         kTouchToFillPasswordSubmissionVariations,
         "TouchToFillPasswordSubmission")},
    {"fast-checkout", flag_descriptions::kFastCheckoutName,
     flag_descriptions::kFastCheckoutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kFastCheckout)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-keyboard-backlight-toggle",
     flag_descriptions::kEnableKeyboardBacklightToggleName,
     flag_descriptions::kEnableKeyboardBacklightToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableKeyboardBacklightToggle)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"update-history-entry-points-in-incognito",
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoName,
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUpdateHistoryEntryPointsInIncognito)},

    {"throttle-foreground-timers",
     flag_descriptions::kThrottleForegroundTimersName,
     flag_descriptions::kThrottleForegroundTimersDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kThrottleForegroundTimers)},

    {"align-wakeups", flag_descriptions::kAlignWakeUpsName,
     flag_descriptions::kAlignWakeUpsDescription, kOsAll,
     FEATURE_VALUE_TYPE(base::kAlignWakeUps)},

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

    {"use-passthrough-command-decoder",
     flag_descriptions::kUsePassthroughCommandDecoderName,
     flag_descriptions::kUsePassthroughCommandDecoderDescription,
     kOsMac | kOsLinux | kOsLacros | kOsCrOS | kOsAndroid | kOsFuchsia,
     FEATURE_VALUE_TYPE(features::kDefaultPassthroughCommandDecoder)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"focus-follows-cursor", flag_descriptions::kFocusFollowsCursorName,
     flag_descriptions::kFocusFollowsCursorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(::features::kFocusFollowsCursor)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"password-strength-indicator",
     flag_descriptions::kPasswordStrengthIndicatorName,
     flag_descriptions::kPasswordStrengthIndicatorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordStrengthIndicator)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"partial-split", flag_descriptions::kPartialSplit,
     flag_descriptions::kPartialSplitDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPartialSplit)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"performant-split-view-resizing",
     flag_descriptions::kPerformantSplitViewResizing,
     flag_descriptions::kPerformantSplitViewResizingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPerformantSplitViewResizing)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"privacy-guide-2", flag_descriptions::kPrivacyGuide2Name,
     flag_descriptions::kPrivacyGuide2Description, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPrivacyGuide2)},

#if BUILDFLAG(IS_ANDROID)
    {"privacy-guide-android", flag_descriptions::kPrivacyGuideAndroidName,
     flag_descriptions::kPrivacyGuideAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacyGuideAndroid)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"google-mobile-services-passwords",
     flag_descriptions::kUnifiedPasswordManagerAndroidName,
     flag_descriptions::kUnifiedPasswordManagerAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kUnifiedPasswordManagerAndroid,
         kUnifiedPasswordManagerAndroidVariations,
         "UnifiedPasswordManagerAndroid")},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"unified-password-manager-desktop",
     flag_descriptions::kUnifiedPasswordManagerDesktopName,
     flag_descriptions::kUnifiedPasswordManagerDesktopDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kUnifiedPasswordManagerDesktop)},
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
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphrasePromo)},

    {"sync-trusted-vault-passphrase-recovery",
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryName,
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryDescription, kOsAll,
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphraseRecovery)},

    {"sync-standalone-invalidations", flag_descriptions::kSyncInvalidationsName,
     flag_descriptions::kSyncInvalidationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(::syncer::kUseSyncInvalidations)},

    {"sync-standalone-invalidations-wallet-and-offer",
     flag_descriptions::kSyncInvalidationsWalletAndOfferName,
     flag_descriptions::kSyncInvalidationsWalletAndOfferDescription, kOsAll,
     FEATURE_VALUE_TYPE(::syncer::kUseSyncInvalidationsForWalletAndOffer)},

    {"debug-history-intervention-no-user-activation",
     flag_descriptions::kDebugHistoryInterventionNoUserActivationName,
     flag_descriptions::kDebugHistoryInterventionNoUserActivationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kDebugHistoryInterventionNoUserActivation)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"smart-lock-sign-in-removed",
     flag_descriptions::kSmartLockSignInRemovedName,
     flag_descriptions::kSmartLockSignInRemovedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSmartLockSignInRemoved)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"sameparty-cookies-considered-first-party",
     flag_descriptions::kSamePartyCookiesConsideredFirstPartyName,
     flag_descriptions::kSamePartyCookiesConsideredFirstPartyDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(net::features::kSamePartyCookiesConsideredFirstParty)},

    {"partitioned-cookies", flag_descriptions::kPartitionedCookiesName,
     flag_descriptions::kPartitionedCookiesDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPartitionedCookies)},
    // TODO(crbug.com/1296161): Remove this flag when the CHIPS OT ends.
    {"partitioned-cookies-bypass-origin-trial",
     flag_descriptions::kPartitionedCookiesBypassOriginTrialName,
     flag_descriptions::kPartitionedCookiesBypassOriginTrialDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPartitionedCookiesBypassOriginTrial)},

    {"nonced-partitioned-cookies",
     flag_descriptions::kNoncedPartitionedCookiesName,
     flag_descriptions::kNoncedPartitionedCookiesDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kNoncedPartitionedCookies)},

    {"third-party-storage-partitioning",
     flag_descriptions::kThirdPartyStoragePartitioningName,
     flag_descriptions::kThirdPartyStoragePartitioningDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kThirdPartyStoragePartitioning)},

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
    {kBorealisPermittedInternalName, flag_descriptions::kBorealisPermittedName,
     flag_descriptions::kBorealisPermittedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisPermitted)},
    {kBorealisStorageBallooningInternalName,
     flag_descriptions::kBorealisStorageBallooningName,
     flag_descriptions::kBorealisStorageBallooningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisStorageBallooning)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"https-only-mode-setting", flag_descriptions::kHttpsOnlyModeName,
     flag_descriptions::kHttpsOnlyModeDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsOnlyMode)},

#if BUILDFLAG(IS_ANDROID)
    {"dynamic-color-android", flag_descriptions::kDynamicColorAndroidName,
     flag_descriptions::kDynamicColorAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kDynamicColorAndroid,
                                    kDynamicColorAndroidVariations,
                                    "AndroidDynamicColor")},
    {"dynamic-color-buttons-android",
     flag_descriptions::kDynamicColorButtonsAndroidName,
     flag_descriptions::kDynamicColorButtonsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDynamicColorButtonsAndroid)},
#endif  //   BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    {"win-10-tab-search-caption-button",
     flag_descriptions::kWin10TabSearchCaptionButtonName,
     flag_descriptions::kWin10TabSearchCaptionButtonDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWin10TabSearchCaptionButton)},
#endif  // BUILDFLAG(IS_WIN)

    {"omnibox-updated-connection-security-indicators",
     flag_descriptions::kOmniboxUpdatedConnectionSecurityIndicatorsName,
     flag_descriptions::kOmniboxUpdatedConnectionSecurityIndicatorsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kUpdatedConnectionSecurityIndicators)},

    {"enable-drdc", flag_descriptions::kEnableDrDcName,
     flag_descriptions::kEnableDrDcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableDrDc)},

    {"force-gpu-main-thread-to-normal-priority-drdc",
     flag_descriptions::kForceGpuMainThreadToNormalPriorityDrDcName,
     flag_descriptions::kForceGpuMainThreadToNormalPriorityDrDcDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kForceGpuMainThreadToNormalPriorityDrDc)},

#if BUILDFLAG(IS_ANDROID)
    {"enable-drdc-vulkan", flag_descriptions::kEnableDrDcVulkanName,
     flag_descriptions::kEnableDrDcDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kEnableDrDcVulkan)},
#endif  //   BUILDFLAG(IS_ANDROID)

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
    {"traffic-counters", flag_descriptions::kTrafficCountersEnabledName,
     flag_descriptions::kTrafficCountersEnabledDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTrafficCountersEnabled)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extensions-menu-access-control",
     flag_descriptions::kExtensionsMenuAccessControlName,
     flag_descriptions::kExtensionsMenuAccessControlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(extensions_features::kExtensionsMenuAccessControl)},
#endif

    {"persistent-quota-is-temporary-quota",
     flag_descriptions::kPersistentQuotaIsTemporaryQuotaName,
     flag_descriptions::kPersistentQuotaIsTemporaryQuotaDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPersistentQuotaIsTemporaryQuota)},

    {"canvas-oop-rasterization", flag_descriptions::kCanvasOopRasterizationName,
     flag_descriptions::kCanvasOopRasterizationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCanvasOopRasterization)},

#if BUILDFLAG(IS_ANDROID)
    {"bookmarks-improved-save-flow",
     flag_descriptions::kBookmarksImprovedSaveFlowName,
     flag_descriptions::kBookmarksImprovedSaveFlowDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBookmarksImprovedSaveFlow)},

    {"bookmarks-refresh", flag_descriptions::kBookmarksRefreshName,
     flag_descriptions::kBookmarksRefreshDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kBookmarksRefresh,
                                    kBookmarksRefreshVariations,
                                    "Collections")},
#endif

    {"enable-tab-audio-muting", flag_descriptions::kTabAudioMutingName,
     flag_descriptions::kTabAudioMutingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kEnableTabMuting)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-app-discovery-for-oobe",
     flag_descriptions::kAppDiscoveryForOobeName,
     flag_descriptions::kAppDiscoveryForOobeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppDiscoveryForOobe)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if defined(TOOLKIT_VIEWS)
    {"side-search", flag_descriptions::kSideSearchName,
     flag_descriptions::kSideSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSideSearch)},

    {"side-search-dse-support", flag_descriptions::kSideSearchDSESupportName,
     flag_descriptions::kSideSearchDSESupportDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSideSearchDSESupport)},
#endif  // defined(TOOLKIT_VIEWS)

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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"enable-delegated-compositing",
     flag_descriptions::kEnableDelegatedCompositingName,
     flag_descriptions::kEnableDelegatedCompositingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDelegatedCompositing)},
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
    {"document-picture-in-picture-api",
     flag_descriptions::kDocumentPictureInPictureApiName,
     flag_descriptions::kDocumentPictureInPictureApiDescription,
     kOsMac | kOsWin | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kPictureInPictureV2)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

    {"web-midi", flag_descriptions::kWebMidiName,
     flag_descriptions::kWebMidiDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebMidi)},
#if BUILDFLAG(IS_ANDROID)
    {"use-real-color-space-for-android-video",
     flag_descriptions::kUseRealColorSpaceForAndroidVideoName,
     flag_descriptions::kUseRealColorSpaceForAndroidVideoDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(media::kUseRealColorSpaceForAndroidVideo)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desks-trackpad-swipe-improvements",
     flag_descriptions::kDesksTrackpadSwipeImprovementsName,
     flag_descriptions::kDesksTrackpadSwipeImprovementsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableDesksTrackpadSwipeImprovements)},
#endif

    {"enable-commerce-developer", flag_descriptions::kCommerceDeveloperName,
     flag_descriptions::kCommerceDeveloperDescription, kOsAll,
     FEATURE_VALUE_TYPE(commerce::kCommerceDeveloper)},

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

#if BUILDFLAG(IS_ANDROID)
    {"context-menu-popup-style", flag_descriptions::kContextMenuPopupStyleName,
     flag_descriptions::kContextMenuPopupStyleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuPopupStyle)},

    {"grid-tab-switcher-for-tablets",
     flag_descriptions::kGridTabSwitcherForTabletsName,
     flag_descriptions::kGridTabSwitcherForTabletsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kGridTabSwitcherForTablets,
                                    kGridTabSwitcherForTabletsVariations,
                                    "GridTabSwitcherForTablets")},

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
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_WIN)
    {"pervasive-system-accent-color",
     flag_descriptions::kPervasiveSystemAccentColorName,
     flag_descriptions::kPervasiveSystemAccentColorDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kPervasiveSystemAccentColor)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"use-multiple-overlays", flag_descriptions::kUseMultipleOverlaysName,
     flag_descriptions::kUseMultipleOverlaysDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kUseMultipleOverlays,
                                    kUseMultipleOverlaysVariations,
                                    "UseMultipleOverlays")},
#endif

#if BUILDFLAG(IS_CHROMEOS)
    {"link-capturing-ui-update", flag_descriptions::kLinkCapturingUiUpdateName,
     flag_descriptions::kLinkCapturingUiUpdateDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(apps::features::kLinkCapturingUiUpdate)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"touch-drag-and-context-menu",
     flag_descriptions::kTouchDragAndContextMenuName,
     flag_descriptions::kTouchDragAndContextMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kTouchDragAndContextMenu)},

    {"new-instance-from-dragged-link",
     flag_descriptions::kNewInstanceFromDraggedLinkName,
     flag_descriptions::kNewInstanceFromDraggedLinkDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNewInstanceFromDraggedLink)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-update-virtual-card-enrollment",
     flag_descriptions::kAutofillEnableUpdateVirtualCardEnrollmentName,
     flag_descriptions::kAutofillEnableUpdateVirtualCardEnrollmentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableUpdateVirtualCardEnrollment)},

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"enable-desktop-capture-lacros-v2",
     flag_descriptions::kDesktopCaptureLacrosV2Name,
     flag_descriptions::kDesktopCaptureLacrosV2Description, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kDesktopCaptureLacrosV2)},

    {"lacros-merge-icu-data-file",
     flag_descriptions::kLacrosMergeIcuDataFileName,
     flag_descriptions::kLacrosMergeIcuDataFileDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(base::i18n::kLacrosMergeIcuDataFile)},

    {"lacros-screen-coordinates-enabled",
     flag_descriptions::kLacrosScreenCoordinatesEnabledName,
     flag_descriptions::kLacrosScreenCoordinatesEnabledDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kWaylandScreenCoordinatesEnabled)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"enable-tailored-security-desktop-notice",
     flag_descriptions::kTailoredSecurityDesktopNoticeName,
     flag_descriptions::kTailoredSecurityDesktopNoticeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(safe_browsing::kTailoredSecurityDesktopNotice)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"screen-ai", flag_descriptions::kScreenAIName,
     flag_descriptions::kScreenAIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kScreenAI)},
#endif

    {"autofill-enable-virtual-card-management-in-desktop-settings-page",
     flag_descriptions::
         kAutofillEnableVirtualCardManagementInDesktopSettingsPageName,
     flag_descriptions::
         kAutofillEnableVirtualCardManagementInDesktopSettingsPageDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableVirtualCardManagementInDesktopSettingsPage)},

    {"leak-detection-unauthenticated",
     flag_descriptions::kLeakDetectionUnauthenticated,
     flag_descriptions::kLeakDetectionUnauthenticatedDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kLeakDetectionUnauthenticated)},

    {"origin-agent-cluster-default",
     flag_descriptions::kOriginAgentClusterDefaultName,
     flag_descriptions::kOriginAgentClusterDefaultDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kOriginAgentClusterDefaultEnabled)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-user-cloud-signin-restriction-policy",
     flag_descriptions::kEnableUserCloudSigninRestrictionPolicyName,
     flag_descriptions::kEnableUserCloudSigninRestrictionPolicyDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         policy::features::kEnableUserCloudSigninRestrictionPolicyFetcher)},
#endif

    {"autofill-enable-sending-bcn-in-get-upload-details",
     flag_descriptions::kAutofillEnableSendingBcnInGetUploadDetailsName,
     flag_descriptions::kAutofillEnableSendingBcnInGetUploadDetailsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSendingBcnInGetUploadDetails)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-discount-consent-v2", flag_descriptions::kDiscountConsentV2Name,
     flag_descriptions::kDiscountConsentV2Description, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kDiscountConsentV2,
                                    kDiscountConsentV2Variations,
                                    "DiscountConsentV2")},
#endif

    {"autofill-enable-unmask-card-request-set-instrument-id",
     flag_descriptions::kAutofillEnableUnmaskCardRequestSetInstrumentIdName,
     flag_descriptions::
         kAutofillEnableUnmaskCardRequestSetInstrumentIdDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableUnmaskCardRequestSetInstrumentId)},
    {"durable-client-hints-cache",
     flag_descriptions::kDurableClientHintsCacheName,
     flag_descriptions::kDurableClientHintsCacheDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDurableClientHintsCache)},

    {"edit-context", flag_descriptions::kEditContextName,
     flag_descriptions::kEditContextDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEditContext)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-fake-keyboard-heuristic",
     flag_descriptions::kEnableFakeKeyboardHeuristicName,
     flag_descriptions::kEnableFakeKeyboardHeuristicDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableFakeKeyboardHeuristic)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"initial-navigation-entry", flag_descriptions::kInitialNavigationEntryName,
     flag_descriptions::kInitialNavigationEntryDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kInitialNavigationEntry)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-isolated-sandboxed-iframes",
     flag_descriptions::kIsolatedSandboxedIframesName,
     flag_descriptions::kIsolatedSandboxedIframesDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kIsolateSandboxedIframes,
         kIsolateSandboxedIframesGroupingVariations,
         "IsolateSandboxedIframes" /* trial name */)},
#endif

    {"download-bubble", flag_descriptions::kDownloadBubbleName,
     flag_descriptions::kDownloadBubbleDescription,
     kOsLinux | kOsLacros | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(safe_browsing::kDownloadBubble)},

    {"download-bubble-v2", flag_descriptions::kDownloadBubbleV2Name,
     flag_descriptions::kDownloadBubbleV2Description,
     kOsLinux | kOsLacros | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(safe_browsing::kDownloadBubbleV2)},

    {"unthrottled-nested-timeout",
     flag_descriptions::kUnthrottledNestedTimeoutName,
     flag_descriptions::kUnthrottledNestedTimeoutDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kMaxUnthrottledTimeoutNestingLevel,
         kUnthrottledNestedTimeout_Variations,
         "NestingLevel")},

    {"reduce-user-agent-minor-version",
     flag_descriptions::kReduceUserAgentMinorVersionName,
     flag_descriptions::kReduceUserAgentMinorVersionDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kReduceUserAgentMinorVersion)},

    {"reduce-user-agent-platform-oscpu",
     flag_descriptions::kReduceUserAgentPlatformOsCpuName,
     flag_descriptions::kReduceUserAgentPlatformOsCpuDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kReduceUserAgentPlatformOsCpu)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-variable-refresh-rate",
     flag_descriptions::kEnableVariableRefreshRateName,
     flag_descriptions::kEnableVariableRefreshRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableVariableRefreshRate)},

    {"enable-app-provisioning-static-server",
     flag_descriptions::kAppProvisioningStaticName,
     flag_descriptions::kAppProvisioningStaticDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppProvisioningStatic)},

    {"enable-projector", flag_descriptions::kProjectorName,
     flag_descriptions::kProjectorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProjector)},

    {"enable-projector-annotator", flag_descriptions::kProjectorAnnotatorName,
     flag_descriptions::kProjectorAnnotatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProjectorAnnotator)},

    {"enable-projector-exclude-transcript",
     flag_descriptions::kProjectorExcludeTranscriptName,
     flag_descriptions::kProjectorExcludeTranscriptDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProjectorExcludeTranscript)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"web-sql-access", flag_descriptions::kWebSQLAccessName,
     flag_descriptions::kWebSQLAccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kWebSQLAccess)},

    {"omit-cors-client-cert", flag_descriptions::kOmitCorsClientCertName,
     flag_descriptions::kOmitCorsClientCertDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kOmitCorsClientCert)},

#if BUILDFLAG(IS_CHROMEOS)
    {"link-capturing-infobar", flag_descriptions::kLinkCapturingInfoBarName,
     flag_descriptions::kLinkCapturingInfoBarDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(apps::features::kLinkCapturingInfoBar)},

    {"intent-chip-skips-intent-picker",
     flag_descriptions::kIntentChipSkipsPickerName,
     flag_descriptions::kIntentChipSkipsPickerDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(apps::features::kIntentChipSkipsPicker)},

    {"intent-chip-app-icon", flag_descriptions::kIntentChipAppIconName,
     flag_descriptions::kIntentChipAppIconDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(apps::features::kIntentChipAppIcon)},

    {"sync-chromeos-explicit-passphrase-sharing",
     flag_descriptions::kSyncChromeOSExplicitPassphraseSharingName,
     flag_descriptions::kSyncChromeOSExplicitPassphraseSharingDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(syncer::kSyncChromeOSExplicitPassphraseSharing)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"launcher-lacros-integration",
     flag_descriptions::kLauncherLacrosIntegrationName,
     flag_descriptions::kLauncherLacrosIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kLauncherLacrosIntegration)},
    {"always-enable-hdcp", flag_descriptions::kAlwaysEnableHdcpName,
     flag_descriptions::kAlwaysEnableHdcpDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAlwaysEnableHdcpChoices)},
    {"enable-desks-close-all", flag_descriptions::kDesksCloseAllName,
     flag_descriptions::kDesksCloseAllDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDesksCloseAll)},
    {"enable-touchpads-in-diagnostics-app",
     flag_descriptions::kEnableTouchpadsInDiagnosticsAppName,
     flag_descriptions::kEnableTouchpadsInDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableTouchpadsInDiagnosticsApp)},
    {"enable-touchscreens-in-diagnostics-app",
     flag_descriptions::kEnableTouchscreensInDiagnosticsAppName,
     flag_descriptions::kEnableTouchscreensInDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kEnableTouchscreensInDiagnosticsApp)},
    {"enable-external-keyboards-in-diagnostics-app",
     flag_descriptions::kEnableExternalKeyboardsInDiagnosticsAppName,
     flag_descriptions::kEnableExternalKeyboardsInDiagnosticsAppDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kEnableExternalKeyboardsInDiagnostics)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enforce-delays-in-strike-database",
     flag_descriptions::kAutofillEnforceDelaysInStrikeDatabaseName,
     flag_descriptions::kAutofillEnforceDelaysInStrikeDatabaseDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceDelaysInStrikeDatabase)},

    {"autofill-enable-virtual-card-metadata",
     flag_descriptions::kAutofillEnableVirtualCardMetadataName,
     flag_descriptions::kAutofillEnableVirtualCardMetadataDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVirtualCardMetadata)},

#if BUILDFLAG(IS_ANDROID)
    {"password-edit-dialog-with-details",
     flag_descriptions::kPasswordEditDialogWithDetailsName,
     flag_descriptions::kPasswordEditDialogWithDetailsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordEditDialogWithDetails)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"autofill-enable-ranking-formula",
     flag_descriptions::kAutofillEnableRankingFormulaName,
     flag_descriptions::kAutofillEnableRankingFormulaDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableRankingFormula)},

    {"autofill-enable-virtual-card-fido-enrollment",
     flag_descriptions::kAutofillEnableVirtualCardFidoEnrollmentName,
     flag_descriptions::kAutofillEnableVirtualCardFidoEnrollmentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVirtualCardFidoEnrollment)},
    {"autofill-upstream-allow-additional-email-domains",
     flag_descriptions::kAutofillUpstreamAllowAdditionalEmailDomainsName,
     flag_descriptions::kAutofillUpstreamAllowAdditionalEmailDomainsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamAllowAdditionalEmailDomains)},
    {"autofill-upstream-allow-all-email-domains",
     flag_descriptions::kAutofillUpstreamAllowAllEmailDomainsName,
     flag_descriptions::kAutofillUpstreamAllowAllEmailDomainsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamAllowAllEmailDomains)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desks-save-and-recall", flag_descriptions::kDesksSaveAndRecallName,
     flag_descriptions::kDesksSaveAndRecallDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableSavedDesks)},

    {"launcher-play-store-search",
     flag_descriptions::kLauncherPlayStoreSearchName,
     flag_descriptions::kLauncherPlayStoreSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kLauncherPlayStoreSearch)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"safe-mode-for-cached-flags",
     flag_descriptions::kSafeModeForCachedFlagsName,
     flag_descriptions::kSafeModeForCachedFlagsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSafeModeForCachedFlags)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {media_router::switches::kAccessCodeCastDeviceDurationSwitch,
     flag_descriptions::kAccessCodeCastDeviceDurationName,
     flag_descriptions::kAccessCodeCastDeviceDurationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAccessCodeCastRememberDevices)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"bulk-tab-restore-android", flag_descriptions::kBulkTabRestoreAndroidName,
     flag_descriptions::kBulkTabRestoreAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBulkTabRestore)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-upstream-save-card-offer-ui-experiment",
     flag_descriptions::kAutofillSaveCardUiExperimentName,
     flag_descriptions::kAutofillSaveCardUiExperimentDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillSaveCardUiExperiment,
         kAutofillSaveCardUiExperimentOptions,
         "AutofillSaveCardUiExperiment")},

#if BUILDFLAG(IS_ANDROID)
    {"network-service-in-process",
     flag_descriptions::kNetworkServiceInProcessName,
     flag_descriptions::kNetworkServiceInProcessDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kNetworkServiceInProcess)},
#endif

    {"broker-file-operations-on-disk-cache-in-network-service",
     flag_descriptions::kBrokerFileOperationsOnDiskCacheInNetworkServiceName,
     flag_descriptions::
         kBrokerFileOperationsOnDiskCacheInNetworkServiceDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kBrokerFileOperationsOnDiskCacheInNetworkService)},

    {"autofill-remove-card-expiry-from-downstream-suggestion",
     flag_descriptions::kAutofillRemoveCardExpiryFromDownstreamSuggestionName,
     flag_descriptions::
         kAutofillRemoveCardExpiryFromDownstreamSuggestionDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::
                            kAutofillRemoveCardExpiryFromDownstreamSuggestion)},

#if !BUILDFLAG(IS_CHROMEOS)
    {"dm-token-deletion", flag_descriptions::kDmTokenDeletionName,
     flag_descriptions::kDmTokenDeletionDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(policy::features::kDmTokenDeletion,
                                    kDmTokenDeletionVariation,
                                    "DmTokenDeletion")},
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
    {"enable-commerce-hint-android",
     flag_descriptions::kCommerceHintAndroidName,
     flag_descriptions::kCommerceHintAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kCommerceHintAndroid)},
#endif

    {"autofill-enable-get-details-for-enroll-parsing-in-upload-card-response",
     flag_descriptions::
         kAutofillEnableGetDetailsForEnrollParsingInUploadCardResponseName,
     flag_descriptions::
         kAutofillEnableGetDetailsForEnrollParsingInUploadCardResponseDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableGetDetailsForEnrollParsingInUploadCardResponse)},

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    {"enable-web-bluetooth-confirm-pairing-support",
     flag_descriptions::kWebBluetoothConfirmPairingSupportName,
     flag_descriptions::kWebBluetoothConfirmPairingSupportDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(device::features::kWebBluetoothConfirmPairingSupport)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

    {"quick-intensive-throttling-after-loading",
     flag_descriptions::kQuickIntensiveWakeUpThrottlingAfterLoadingName,
     flag_descriptions::kQuickIntensiveWakeUpThrottlingAfterLoadingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kQuickIntensiveWakeUpThrottlingAfterLoading)},

#if BUILDFLAG(IS_MAC)
    {"system-color-chooser", flag_descriptions::kSystemColorChooserName,
     flag_descriptions::kSystemColorChooserDescription, kOsMac,
     FEATURE_VALUE_TYPE(blink::features::kSystemColorChooser)},
#endif  // BUILDFLAG(IS_MAC)

    {"ignore-sync-encryption-keys-long-missing",
     flag_descriptions::kIgnoreSyncEncryptionKeysLongMissingName,
     flag_descriptions::kIgnoreSyncEncryptionKeysLongMissingDescription, kOsAll,
     FEATURE_VALUE_TYPE(syncer::kIgnoreSyncEncryptionKeysLongMissing)},

    {"autofill-parse-iban-fields",
     flag_descriptions::kAutofillParseIbanFieldsName,
     flag_descriptions::kAutofillParseIbanFieldsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillParseIbanFields)},

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-fido-progress-dialog",
     flag_descriptions::kAutofillEnableFIDOProgressDialogName,
     flag_descriptions::kAutofillEnableFIDOProgressDialogDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableFIDOProgressDialog)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"enable-perfetto-system-tracing",
     flag_descriptions::kEnablePerfettoSystemTracingName,
     flag_descriptions::kEnablePerfettoSystemTracingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kEnablePerfettoSystemTracing)},

#if BUILDFLAG(IS_ANDROID)
    {"enable-android-gamepad-vibration",
     flag_descriptions::kEnableAndroidGamepadVibrationName,
     flag_descriptions::kEnableAndroidGamepadVibrationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kEnableAndroidGamepadVibration)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"request-desktop-site-additions",
     flag_descriptions::kRequestDesktopSiteAdditionsName,
     flag_descriptions::kRequestDesktopSiteAdditionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kRequestDesktopSiteAdditions)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"enable-web-hid-on-extension-service-worker",
     flag_descriptions::kEnableWebHidOnExtensionServiceWorkerName,
     flag_descriptions::kEnableWebHidOnExtensionServiceWorkerDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnableWebHidOnExtensionServiceWorker)},
#endif

    {"enable-sync-history-datatype",
     flag_descriptions::kSyncEnableHistoryDataTypeName,
     flag_descriptions::kSyncEnableHistoryDataTypeDescription, kOsAll,
     FEATURE_VALUE_TYPE(syncer::kSyncEnableHistoryDataType)},

#if BUILDFLAG(IS_CHROMEOS)
    {"link-capturing-auto-display-intent-picker",
     flag_descriptions::kLinkCapturingAutoDisplayIntentPickerName,
     flag_descriptions::kLinkCapturingAutoDisplayIntentPickerDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(apps::features::kLinkCapturingAutoDisplayIntentPicker)},
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {"enable-biometric-authentication-in-settings",
     flag_descriptions::kEnableBiometricAuthenticationInSettingsName,
     flag_descriptions::kEnableBiometricAuthenticationInSettingsDescription,
     kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnableBiometricAuthenticationInSettings)},
#endif

    {"autofill-enable-remade-downstream-metrics",
     flag_descriptions::kAutofillEnableRemadeDownstreamMetricsName,
     flag_descriptions::kAutofillEnableRemadeDownstreamMetricsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableRemadeDownstreamMetrics)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-seamless-refresh-rate-switching",
     flag_descriptions::kEnableSeamlessRefreshRateSwitchingName,
     flag_descriptions::kEnableSeamlessRefreshRateSwitchingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSeamlessRefreshRateSwitching)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"clipboard-unsanitized-content",
     flag_descriptions::kClipboardUnsanitizedContentName,
     flag_descriptions::kClipboardUnsanitizedContentDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kClipboardUnsanitizedContent)},

#if BUILDFLAG(IS_ANDROID)
    {"assistant-non-personalized-voice-search",
     flag_descriptions::kAssistantNonPersonalizedVoiceSearchName,
     flag_descriptions::kAssistantNonPersonalizedVoiceSearchDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAssistantNonPersonalizedVoiceSearch)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"stylus-input", flag_descriptions::kStylusWritingToInputName,
     flag_descriptions::kStylusWritingToInputDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kStylusWritingToInput)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-media-dynamic-cgroup", flag_descriptions::kMediaDynamicCgroupName,
     flag_descriptions::kMediaDynamicCgroupDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootMediaDynamicCgroup")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  // Skip lacros-availability-policy always. This is a pseudo entry
  // and used to carry the policy value crossing the Chrome's lifetime.
  if (!strcmp(crosapi::browser_util::kLacrosAvailabilityPolicyInternalName,
              entry.internal_name)) {
    return true;
  }

  if (!strcmp(kLacrosSupportInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosSupportFlagAllowed();
  }

  if (!strcmp(kLacrosStabilityInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosAllowedToBeEnabled();
  }

  if (!strcmp(kLacrosOnlyInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosOnlyFlagAllowed();
  }

  if (!strcmp(kLacrosPrimaryInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosPrimaryFlagAllowed();
  }

  if (!strcmp(kWebAppsCrosapiInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosAllowedToBeEnabled();
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

  if (!strcmp(kBorealisPermittedInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  if (!strcmp(kBorealisStorageBallooningInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  // Only show wallpaper fast refresh flag if channel is one of
  // Dev/Canary/Unknown.
  if (!strcmp(kWallpaperFastRefreshInternalName, entry.internal_name)) {
    return (channel != version_info::Channel::DEV &&
            channel != version_info::Channel::CANARY &&
            channel != version_info::Channel::UNKNOWN);
  }

  // Only show clipboard history reorder flag if channel is one of
  // Dev/Canary/Unknown.
  if (!strcmp(kClipboardHistoryReorderInternalName, entry.internal_name)) {
    return channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // enable-unsafe-webgpu is only available on Dev/Canary channels.
  if (!strcmp("enable-unsafe-webgpu", entry.internal_name) &&
      channel != version_info::Channel::DEV &&
      channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::UNKNOWN) {
    return true;
  }
#if !BUILDFLAG(IS_ANDROID)
  // Only show the AccessCodeCast duration flag if the AccessCodeCast Ui is
  // displayed and it is only available on Dev/Canary channels.
  if (!strcmp(media_router::switches::kAccessCodeCastDeviceDurationSwitch,
              entry.internal_name)) {
    return !media_router::IsAccessCodeCastEnabled();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
  // HDR mode works, but displays everything horribly wrong prior to windows 10.
  if (!strcmp("enable-hdr", entry.internal_name) &&
      base::win::GetVersion() < base::win::Version::WIN10) {
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

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
                           base::Value::List& supported_entries,
                           base::Value::List& unsupported_entries) {
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
    base::Value::List& supported_entries,
    base::Value::List& unsupported_entries) {
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

#if BUILDFLAG(IS_CHROMEOS)
void CrosUrlFlagsRedirect() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_url_handling::NavigateInAsh(GURL(chrome::kChromeUIFlagsURL));
#else
  // Note: This will only be called by the UI when Lacros is available.
  DCHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->SwitchToTab(
      GURL(chrome::kChromeUIFlagsURL),
      /*path_behavior=*/NavigateParams::RESPECT);
#endif
}
#endif

void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage,
                         const std::string& histogram_name) {
  std::set<std::string> switches;
  std::set<std::string> features;
  std::set<std::string> variation_ids;
  FlagsStateSingleton::GetFlagsState()->GetSwitchesAndFeaturesFromFlags(
      flags_storage, &switches, &features, &variation_ids);
  // Don't report variation IDs since we don't have an UMA histogram for them.
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
  return base::make_span(kFeatureEntries, std::size(kFeatureEntries));
}

}  // namespace testing

}  // namespace about_flags
