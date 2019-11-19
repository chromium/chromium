// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/net/dns_util.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/webrtc/webrtc_flags.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/components/proximity_auth/switches.h"
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
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/public.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/public/common/download_features.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/favicon/core/features.h"
#include "components/feature_engagement/buildflags.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/games/core/games_features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/policy/core/common/features.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/printing/browser/features.h"
#include "components/safe_browsing/features.h"
#include "components/security_interstitials/core/features.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/send_tab_to_self/features.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "components/ui_devtools/switches.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/fido/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/midi/midi_switches.h"
#include "media/webrtc/webrtc_switches.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "services/media_session/public/cpp/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/switches.h"
#include "third_party/blink/public/common/experiments/memory_ablation_experiment.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/predictor_factory.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "ui/android/buildflags.h"
#else  // OS_ANDROID
#include "chrome/browser/media/router/media_router_feature.h"
#include "components/mirroring/service/features.h"
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/features.h"
#include "components/arc/arc_features.h"
#include "printing/printing_features_chromeos.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter_factory.h"
#endif  // OS_CHROMEOS

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // OS_MACOSX

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#endif  // ENABLE_EXTENSIONS

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/common/printing_features.h"
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

#if defined(USE_AURA) || defined(OS_ANDROID)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS;
#endif  // USE_AURA || OS_ANDROID

const FeatureEntry::Choice kTouchEventFeatureDetectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kTouchEventFeatureDetection,
     switches::kTouchEventFeatureDetectionEnabled},
    {flags_ui::kGenericExperimentChoiceAutomatic,
     switches::kTouchEventFeatureDetection,
     switches::kTouchEventFeatureDetectionAuto}};

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
     switches::kTouchTextSelectionStrategy, "character"},
    {flag_descriptions::kTouchSelectionStrategyDirection,
     switches::kTouchTextSelectionStrategy, "direction"}};

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

const FeatureEntry::Choice kPassiveListenersChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kPassiveEventListenerTrue,
     switches::kPassiveListenersDefault, "true"},
    {flag_descriptions::kPassiveEventListenerForceAllTrue,
     switches::kPassiveListenersDefault, "forcealltrue"},
};

const FeatureEntry::Choice kDataReductionProxyServerExperiment[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kDataReductionProxyServerAlternative1,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative1},
    {flag_descriptions::kDataReductionProxyServerAlternative2,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative2},
    {flag_descriptions::kDataReductionProxyServerAlternative3,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative3},
    {flag_descriptions::kDataReductionProxyServerAlternative4,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative4},
    {flag_descriptions::kDataReductionProxyServerAlternative5,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative5},
    {flag_descriptions::kDataReductionProxyServerAlternative6,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative6},
    {flag_descriptions::kDataReductionProxyServerAlternative7,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative7},
    {flag_descriptions::kDataReductionProxyServerAlternative8,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative8},
    {flag_descriptions::kDataReductionProxyServerAlternative9,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative9},
    {flag_descriptions::kDataReductionProxyServerAlternative10,
     data_reduction_proxy::switches::kDataReductionProxyExperiment,
     data_reduction_proxy::switches::kDataReductionProxyServerAlternative10}};

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

#if defined(OS_ANDROID)
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

const FeatureEntry::FeatureParam kCCTModuleCache_ZeroMinutes[] = {
    {"cct_module_cache_time_limit_ms", "0"}};
const FeatureEntry::FeatureParam kCCTModuleCache_OneMinute[] = {
    {"cct_module_cache_time_limit_ms", "60000"}};
const FeatureEntry::FeatureParam kCCTModuleCache_FiveMinutes[] = {
    {"cct_module_cache_time_limit_ms", "300000"}};
const FeatureEntry::FeatureParam kCCTModuleCache_ThirtyMinutes[] = {
    {"cct_module_cache_time_limit_ms", "1800000"}};
const FeatureEntry::FeatureVariation kCCTModuleCacheVariations[] = {
    {"0 minutes", kCCTModuleCache_ZeroMinutes,
     base::size(kCCTModuleCache_ZeroMinutes), nullptr},
    {"1 minute", kCCTModuleCache_OneMinute,
     base::size(kCCTModuleCache_OneMinute), nullptr},
    {"5 minutes", kCCTModuleCache_FiveMinutes,
     base::size(kCCTModuleCache_FiveMinutes), nullptr},
    {"30 minutes", kCCTModuleCache_ThirtyMinutes,
     base::size(kCCTModuleCache_ThirtyMinutes), nullptr},
};

#endif  // OS_ANDROID

#if !defined(OS_CHROMEOS)
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
     base::size(kForceDark_SelectiveGeneralInversion), nullptr}};
#endif  // !OS_CHROMEOS

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_4Hours[] = {
    {"close_tab_suggestions_stale_time_ms", "14400000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_8Hours[] = {
    {"close_tab_suggestions_stale_time_ms", "28800000"}};
const FeatureEntry::FeatureParam kCloseTabSuggestionsStale_7Days[] = {
    {"close_tab_suggestions_stale_time_ms", "604800000"}};
const FeatureEntry::FeatureVariation kCloseTabSuggestionsStaleVariations[] = {
    {"4 hours", kCloseTabSuggestionsStale_4Hours,
     base::size(kCloseTabSuggestionsStale_4Hours), nullptr},
    {"8 hours", kCloseTabSuggestionsStale_8Hours,
     base::size(kCloseTabSuggestionsStale_8Hours), nullptr},
    {"7 days", kCloseTabSuggestionsStale_7Days,
     base::size(kCloseTabSuggestionsStale_7Days), nullptr},
};
#endif  // OS_ANDROID

const FeatureEntry::Choice kEnableGpuRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableGpuRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableGpuRasterization, ""},
    {flag_descriptions::kForceGpuRasterization,
     switches::kForceGpuRasterization, ""},
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

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kUiShowCompositedLayerBordersChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kUiShowCompositedLayerBordersRenderPass,
     cc::switches::kUIShowCompositedLayerBorders,
     cc::switches::kCompositedRenderPassBorders},
    {flag_descriptions::kUiShowCompositedLayerBordersSurface,
     cc::switches::kUIShowCompositedLayerBorders,
     cc::switches::kCompositedSurfaceBorders},
    {flag_descriptions::kUiShowCompositedLayerBordersLayer,
     cc::switches::kUIShowCompositedLayerBorders,
     cc::switches::kCompositedLayerBorders},
    {flag_descriptions::kUiShowCompositedLayerBordersAll,
     cc::switches::kUIShowCompositedLayerBorders, ""}};

#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kCrosRegionsModeChoices[] = {
    {flag_descriptions::kCrosRegionsModeDefault, "", ""},
    {flag_descriptions::kCrosRegionsModeOverride,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeOverride},
    {flag_descriptions::kCrosRegionsModeHide,
     chromeos::switches::kCrosRegionsMode,
     chromeos::switches::kCrosRegionsModeHide},
};
#endif  // OS_CHROMEOS

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

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kSchedulerConfigurationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kSchedulerConfigurationConservative,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationConservative},
    {flag_descriptions::kSchedulerConfigurationPerformance,
     switches::kSchedulerConfiguration,
     switches::kSchedulerConfigurationPerformance},
};
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kInterestFeedLargerImagesFeatureVariationConstant[] = {
        {"feed_ui_enabled", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedSnippetsFeatureVariationConstant[] = {
        {"snippets_enabled", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedLargeImagesAndSnippetsFeatureVariationConstant[] = {
        {"feed_ui_enabled", "true"},
        {"snippets_enabled", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedLargerImagesWithUndoableActionsFeatureVariationConstant[] = {
        {"feed_ui_enabled", "true"},
        {"undoable_actions_enabled", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedSnippetsWithUndoableActionsFeatureVariationConstant[] = {
        {"snippets_enabled", "true"},
        {"undoable_actions_enabled", "true"}};
const FeatureEntry::FeatureParam
    kInterestFeedLargeImagesAndSnippetsWithUndoableActionsFeatureVariationConstant
        [] = {{"feed_ui_enabled", "true"},
              {"snippets_enabled", "true"},
              {"undoable_actions_enabled", "true"}};
const FeatureEntry::FeatureVariation kInterestFeedFeatureVariations[] = {
    {"(larger images)", kInterestFeedLargerImagesFeatureVariationConstant,
     base::size(kInterestFeedLargerImagesFeatureVariationConstant), nullptr},
    {"(snippets)", kInterestFeedSnippetsFeatureVariationConstant,
     base::size(kInterestFeedSnippetsFeatureVariationConstant), nullptr},
    {"(larger images and snippets)",
     kInterestFeedLargeImagesAndSnippetsFeatureVariationConstant,
     base::size(kInterestFeedLargeImagesAndSnippetsFeatureVariationConstant),
     nullptr},
    {"(larger images w/ undoable actions)",
     kInterestFeedLargerImagesWithUndoableActionsFeatureVariationConstant,
     base::size(
         kInterestFeedLargerImagesWithUndoableActionsFeatureVariationConstant),
     nullptr},
    {"(snippets w/ undoable actions)",
     kInterestFeedSnippetsWithUndoableActionsFeatureVariationConstant,
     base::size(
         kInterestFeedSnippetsWithUndoableActionsFeatureVariationConstant),
     nullptr},
    {"(larger images and snippets w/ undoable actions)",
     kInterestFeedLargeImagesAndSnippetsWithUndoableActionsFeatureVariationConstant,
     base::size(
         kInterestFeedLargeImagesAndSnippetsWithUndoableActionsFeatureVariationConstant),
     nullptr}};

const FeatureEntry::FeatureVariation kRemoteSuggestionsFeatureVariations[] = {
    {"via content suggestion server (backed by ChromeReader)", nullptr, 0,
     "3313421"},
    {"via content suggestion server (backed by Google Now)", nullptr, 0,
     "3313422"}};
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

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationAnimationDuration[] = {
        {autofill::kAutofillKeyboardAccessoryAnimationDurationKey, "1000"}};

const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth[] = {
        {autofill::kAutofillKeyboardAccessoryLimitLabelWidthKey, "true"}};

const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationShowHint[] = {
        {autofill::kAutofillKeyboardAccessoryHintKey, "true"}};

const FeatureEntry::FeatureParam
    kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint[] = {
        {autofill::kAutofillKeyboardAccessoryAnimationDurationKey, "1000"},
        {autofill::kAutofillKeyboardAccessoryHintKey, "true"}};

const FeatureEntry::FeatureVariation
    kAutofillKeyboardAccessoryFeatureVariations[] = {
        {"Animate", kAutofillKeyboardAccessoryFeatureVariationAnimationDuration,
         base::size(
             kAutofillKeyboardAccessoryFeatureVariationAnimationDuration),
         nullptr},
        {"Limit label width",
         kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth,
         base::size(kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth),
         nullptr},
        {"Show hint", kAutofillKeyboardAccessoryFeatureVariationShowHint,
         base::size(kAutofillKeyboardAccessoryFeatureVariationShowHint),
         nullptr},
        {"Animate with hint",
         kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint,
         base::size(kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint),
         nullptr}};
#endif  // OS_ANDROID

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
    {flag_descriptions::kMemlogStackModePseudo,
     heap_profiling::kMemlogStackMode, heap_profiling::kMemlogStackModePseudo},
    {flag_descriptions::kMemlogStackModeMixed, heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeMixed},
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

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
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
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

const FeatureEntry::FeatureParam kOmniboxOnFocusSuggestionsParamSERP[] = {
    {"ZeroSuggestVariant:6:*", "RemoteSendUrl"}};
#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kOmniboxNTPZPSLocal[] = {
    {"ZeroSuggestVariant:1:*", "Local"},
    {"ZeroSuggestVariant:7:*", "Local"},
    {"ZeroSuggestVariant:8:*", "Local"}};
const FeatureEntry::FeatureParam kOmniboxNTPZPSRemote[] = {
    {"ZeroSuggestVariant:1:*", "RemoteNoUrl"},
    {"ZeroSuggestVariant:7:*", "RemoteNoUrl"},
    {"ZeroSuggestVariant:8:*", "RemoteNoUrl"}};
#else   // !defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kOmniboxOnFocusSuggestionsParamNTPOmniboxRemoteLocal[] = {
        {"ZeroSuggestVariant:7:*", "RemoteNoUrl,Local"}};
const FeatureEntry::FeatureParam
    kOmniboxOnFocusSuggestionsParamNTPRealboxRemoteLocal[] = {
        {"ZeroSuggestVariant:15:*", "RemoteNoUrl,Local"}};
const FeatureEntry::FeatureParam
    kOmniboxOnFocusSuggestionsParamNTPOmniboxRealboxRemoteLocal[] = {
        *kOmniboxOnFocusSuggestionsParamNTPOmniboxRemoteLocal,
        *kOmniboxOnFocusSuggestionsParamNTPRealboxRemoteLocal};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureVariation kOmniboxOnFocusSuggestionsVariations[] = {
    {"SERP - RemoteSendURL", kOmniboxOnFocusSuggestionsParamSERP,
     base::size(kOmniboxOnFocusSuggestionsParamSERP),
     "t3315869" /* variation_id */},
#if defined(OS_ANDROID)
    {"ZPS on NTP: Local History", kOmniboxNTPZPSLocal,
     base::size(kOmniboxNTPZPSLocal), nullptr},
    {"ZPS on NTP: Remote History", kOmniboxNTPZPSRemote,
     base::size(kOmniboxNTPZPSRemote), "t3314248"},
#else   // !defined(OS_ANDROID)
    {"NTP Omnibox - Remote,Local",
     kOmniboxOnFocusSuggestionsParamNTPOmniboxRemoteLocal,
     base::size(kOmniboxOnFocusSuggestionsParamNTPOmniboxRemoteLocal),
     "t3316133" /* variation_id */},
    {"NTP Realbox - Remote,Local",
     kOmniboxOnFocusSuggestionsParamNTPRealboxRemoteLocal,
     base::size(kOmniboxOnFocusSuggestionsParamNTPRealboxRemoteLocal),
     "t3316133" /* variation_id */},
    {"NTP Omnibox,Realbox - Remote,Local",
     kOmniboxOnFocusSuggestionsParamNTPOmniboxRealboxRemoteLocal,
     base::size(kOmniboxOnFocusSuggestionsParamNTPOmniboxRealboxRemoteLocal),
     "t3316133" /* variation_id */},
#endif  // defined(OS_ANDROID)
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

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN) || \
    defined(OS_CHROMEOS)
const FeatureEntry::FeatureParam kTranslateBubbleUIButton[] = {
    {language::kTranslateUIBubbleKey, language::kTranslateUIBubbleButtonValue}};
const FeatureEntry::FeatureParam kTranslateBubbleUITab[] = {
    {language::kTranslateUIBubbleKey, language::kTranslateUIBubbleTabValue}};
const FeatureEntry::FeatureParam kTranslateBubbleUIButtonGM2[] = {
    {language::kTranslateUIBubbleKey,
     language::kTranslateUIBubbleButtonGM2Value}};

const FeatureEntry::FeatureVariation kTranslateBubbleUIVariations[] = {
    {"Button", kTranslateBubbleUIButton, base::size(kTranslateBubbleUIButton),
     nullptr},
    {"Tab", kTranslateBubbleUITab, base::size(kTranslateBubbleUITab), nullptr},
    {"Button_GM2", kTranslateBubbleUIButtonGM2,
     base::size(kTranslateBubbleUIButton), nullptr}};
#endif  // OS_LINUX || OS_MACOSX || OS_WIN || OS_CHROMEOS

const FeatureEntry::FeatureParam kMarkHttpAsDangerous[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerous}};
const FeatureEntry::FeatureParam kMarkHttpAsWarningAndDangerousOnFormEdits[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::
         kMarkHttpAsParameterWarningAndDangerousOnFormEdits}};
const FeatureEntry::FeatureParam kMarkHttpAsDangerWarning[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerWarning}};

// The "Enabled" state for this feature is "0" and representing setting A.
const FeatureEntry::FeatureParam kTabHoverCardsSettingB[] = {
    {features::kTabHoverCardsFeatureParameterName, "1"}};
const FeatureEntry::FeatureParam kTabHoverCardsSettingC[] = {
    {features::kTabHoverCardsFeatureParameterName, "2"}};

const FeatureEntry::FeatureVariation kTabHoverCardsFeatureVariations[] = {
    {"B", kTabHoverCardsSettingB, base::size(kTabHoverCardsSettingB), nullptr},
    {"C", kTabHoverCardsSettingC, base::size(kTabHoverCardsSettingC), nullptr}};

const FeatureEntry::FeatureVariation kMarkHttpAsFeatureVariations[] = {
    {"(mark as actively dangerous)", kMarkHttpAsDangerous,
     base::size(kMarkHttpAsDangerous), nullptr},
    {"(mark with a Not Secure warning and dangerous on form edits)",
     kMarkHttpAsWarningAndDangerousOnFormEdits,
     base::size(kMarkHttpAsWarningAndDangerousOnFormEdits), nullptr},
    {"(mark with a grey triangle icon)", kMarkHttpAsDangerWarning,
     base::size(kMarkHttpAsDangerWarning), nullptr}};

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

const FeatureEntry::FeatureParam kOverscrollHistoryNavigationBottomSheet[] = {
    {"overscroll_history_navigation_bottom_sheet", "true"}};
const FeatureEntry::FeatureVariation kOverscrollHistoryNavigationVariations[] =
    {{"Navigation sheet", kOverscrollHistoryNavigationBottomSheet,
      base::size(kOverscrollHistoryNavigationBottomSheet), nullptr}};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTabFreeze_FreezeNoUnfreeze[] = {
    {resource_coordinator::
         kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam,
     "false"},
    {resource_coordinator::
         kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam,
     "false"}};
const FeatureEntry::FeatureParam kTabFreeze_FreezeWithUnfreeze[] = {
    {resource_coordinator::
         kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam,
     "false"},
    {resource_coordinator::
         kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam,
     "true"}};
const FeatureEntry::FeatureVariation kTabFreezeVariations[] = {
    {"Freeze - No Unfreeze", kTabFreeze_FreezeNoUnfreeze,
     base::size(kTabFreeze_FreezeNoUnfreeze), nullptr},
    {"Freeze - Unfreeze 10 seconds every 15 minutes",
     kTabFreeze_FreezeWithUnfreeze, base::size(kTabFreeze_FreezeWithUnfreeze),
     nullptr}};
#endif

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kExploreSitesExperimental = {
    chrome::android::explore_sites::kExploreSitesVariationParameterName,
    chrome::android::explore_sites::kExploreSitesVariationExperimental};
const FeatureEntry::FeatureParam kExploreSitesPersonalized = {
    chrome::android::explore_sites::kExploreSitesVariationParameterName,
    chrome::android::explore_sites::kExploreSitesVariationPersonalized};
const FeatureEntry::FeatureParam kExploreSitesDenseTitleBottom[] = {
    {chrome::android::explore_sites::kExploreSitesVariationParameterName,
     chrome::android::explore_sites::kExploreSitesVariationMostLikelyTile},
    {chrome::android::explore_sites::kExploreSitesDenseVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesDenseVariationDenseTitleBottom},
    {chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationParameterName,
     chrome::android::explore_sites::kExploreSitesMostLikelyVariationIconDots}};
const FeatureEntry::FeatureParam kExploreSitesDenseTitleRight[] = {
    {chrome::android::explore_sites::kExploreSitesVariationParameterName,
     chrome::android::explore_sites::kExploreSitesVariationMostLikelyTile},
    {chrome::android::explore_sites::kExploreSitesDenseVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesDenseVariationDenseTitleRight},
    {chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationParameterName,
     chrome::android::explore_sites::kExploreSitesMostLikelyVariationIconDots}};
const FeatureEntry::FeatureParam kExploreSitesIconArrow[] = {
    {chrome::android::explore_sites::kExploreSitesVariationParameterName,
     chrome::android::explore_sites::kExploreSitesVariationMostLikelyTile},
    {chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationIconArrow}};
const FeatureEntry::FeatureParam kExploreSitesIconDots[] = {
    {chrome::android::explore_sites::kExploreSitesVariationParameterName,
     chrome::android::explore_sites::kExploreSitesVariationMostLikelyTile},
    {chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationParameterName,
     chrome::android::explore_sites::kExploreSitesMostLikelyVariationIconDots}};
const FeatureEntry::FeatureParam kExploreSitesIconGrouped[] = {
    {chrome::android::explore_sites::kExploreSitesVariationParameterName,
     chrome::android::explore_sites::kExploreSitesVariationMostLikelyTile},
    {chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationParameterName,
     chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationIconGrouped}};
const FeatureEntry::FeatureParam kExploreSitesWithGamesTop[] = {
    {chrome::android::explore_sites::kExploreSitesVariationParameterName,
     chrome::android::explore_sites::kExploreSitesVariationMostLikelyTile},
    {chrome::android::explore_sites::
         kExploreSitesMostLikelyVariationParameterName,
     chrome::android::explore_sites::kExploreSitesMostLikelyVariationIconDots},
    {chrome::android::explore_sites::
         kExploreSitesHeadersExperimentParameterName,
     chrome::android::explore_sites::kExploreSitesGamesTopExperiment}};
const FeatureEntry::FeatureVariation kExploreSitesVariations[] = {
    {"Experimental", &kExploreSitesExperimental, 1, nullptr},
    {"Personalized", &kExploreSitesPersonalized, 1, nullptr},
    {"Arrow Icon", kExploreSitesIconArrow, base::size(kExploreSitesIconArrow),
     nullptr},
    {"Dots Icon", kExploreSitesIconDots, base::size(kExploreSitesIconDots),
     nullptr},
    {"Grouped Icon", kExploreSitesIconGrouped,
     base::size(kExploreSitesIconGrouped), nullptr},
    {"Games Top", kExploreSitesWithGamesTop,
     base::size(kExploreSitesWithGamesTop), nullptr},
    {"Dense Title Bottom", kExploreSitesDenseTitleBottom,
     base::size(kExploreSitesDenseTitleBottom), nullptr},
    {"Dense Title Right", kExploreSitesDenseTitleRight,
     base::size(kExploreSitesDenseTitleRight), nullptr}};

const FeatureEntry::FeatureParam kSimplifiedServerAllCocaCards = {
    contextual_search::kContextualCardsVersionParamName,
    contextual_search::kContextualCardsSimplifiedServerWithDiagnosticChar};
const FeatureEntry::FeatureVariation kSimplifiedServerVariations[] = {
    {"and allow all CoCa cards", &kSimplifiedServerAllCocaCards, 1, nullptr}};

const FeatureEntry::FeatureParam kLongpressResolvePreserveTap = {
    contextual_search::kLongpressResolveParamName,
    contextual_search::kLongpressResolvePreserveTap};
const FeatureEntry::FeatureVariation kLongpressResolveVariations[] = {
    {"and preserve Tap behavior", &kLongpressResolvePreserveTap, 1, nullptr},
};

#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kResamplingInputEventsLSQEnabled[] = {
    {"predictor", ui::input_prediction::kScrollPredictorNameLsq}};
const FeatureEntry::FeatureParam kResamplingInputEventsKalmanEnabled[] = {
    {"predictor", ui::input_prediction::kScrollPredictorNameKalman}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearFirstEnabled[] = {
    {"predictor", ui::input_prediction::kScrollPredictorNameLinearFirst}};
const FeatureEntry::FeatureParam kResamplingInputEventsLinearSecondEnabled[] = {
    {"predictor", ui::input_prediction::kScrollPredictorNameLinearSecond}};
const FeatureEntry::FeatureParam
    kResamplingInputEventsLinearResamplingEnabled[] = {
        {"predictor",
         ui::input_prediction::kScrollPredictorNameLinearResampling}};

const FeatureEntry::FeatureVariation kResamplingInputEventsFeatureVariations[] =
    {{ui::input_prediction::kScrollPredictorNameLsq,
      kResamplingInputEventsLSQEnabled,
      base::size(kResamplingInputEventsLSQEnabled), nullptr},
     {ui::input_prediction::kScrollPredictorNameKalman,
      kResamplingInputEventsKalmanEnabled,
      base::size(kResamplingInputEventsKalmanEnabled), nullptr},
     {ui::input_prediction::kScrollPredictorNameLinearFirst,
      kResamplingInputEventsLinearFirstEnabled,
      base::size(kResamplingInputEventsLinearFirstEnabled), nullptr},
     {ui::input_prediction::kScrollPredictorNameLinearSecond,
      kResamplingInputEventsLinearSecondEnabled,
      base::size(kResamplingInputEventsLinearSecondEnabled), nullptr},
     {ui::input_prediction::kScrollPredictorNameLinearResampling,
      kResamplingInputEventsLinearResamplingEnabled,
      base::size(kResamplingInputEventsLinearResamplingEnabled), nullptr}};

const FeatureEntry::FeatureParam kFilteringPredictionEmptyFilterEnabled[] = {
    {"filter", ui::input_prediction::kFilterNameEmpty}};
const FeatureEntry::FeatureParam kFilteringPredictionOneEuroFilterEnabled[] = {
    {"filter", ui::input_prediction::kFilterNameOneEuro}};

const FeatureEntry::FeatureVariation kFilteringPredictionFeatureVariations[] = {
    {ui::input_prediction::kFilterNameEmpty,
     kFilteringPredictionEmptyFilterEnabled,
     base::size(kFilteringPredictionEmptyFilterEnabled), nullptr},
    {ui::input_prediction::kFilterNameOneEuro,
     kFilteringPredictionOneEuroFilterEnabled,
     base::size(kFilteringPredictionOneEuroFilterEnabled), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kBottomOfflineIndicatorEnabled[] = {
    {"bottom_offline_indicator", "true"}};

const FeatureEntry::FeatureVariation kOfflineIndicatorFeatureVariations[] = {
    {"(bottom)", kBottomOfflineIndicatorEnabled,
     base::size(kBottomOfflineIndicatorEnabled), nullptr}};
#endif  // OS_ANDROID

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
    {"tab_grid_layout_android_new_tab", "NewTabVariation"}};

const FeatureEntry::FeatureVariation kTabGridLayoutAndroidVariations[] = {
    {"New Tab Variation", kTabGridLayoutAndroid_NewTabVariation,
     base::size(kTabGridLayoutAndroid_NewTabVariation), nullptr},
};
const FeatureEntry::FeatureParam kStartSurfaceAndroid_SingleSurface[] = {
    {"start_surface_variation", "single"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_TwoPanesSurface[] = {
    {"start_surface_variation", "twopanes"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_Toolbar[] = {
    {"start_surface_variation", "toolbar"}};

const FeatureEntry::FeatureParam kStartSurfaceAndroid_TasksOnly[] = {
    {"start_surface_variation", "tasksonly"}};

const FeatureEntry::FeatureVariation kStartSurfaceAndroidVariations[] = {
    {"Single Surface", kStartSurfaceAndroid_SingleSurface,
     base::size(kStartSurfaceAndroid_SingleSurface), nullptr},
    {"Two Panes Surface", kStartSurfaceAndroid_TwoPanesSurface,
     base::size(kStartSurfaceAndroid_TwoPanesSurface), nullptr},
    {"Start Surface Toolbar", kStartSurfaceAndroid_Toolbar,
     base::size(kStartSurfaceAndroid_Toolbar), nullptr},
    {"Tasks Only", kStartSurfaceAndroid_TasksOnly,
     base::size(kStartSurfaceAndroid_TasksOnly), nullptr}};
#endif  // OS_ANDROID

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
const FeatureEntry::FeatureParam kAndroidNightModeDefaultToLightConstant[] = {
    {"default_light_theme", "true"}};
const FeatureEntry::FeatureVariation kAndroidNightModeFeatureVariations[] = {
    {"(default to light theme)", kAndroidNightModeDefaultToLightConstant,
     base::size(kAndroidNightModeDefaultToLightConstant), nullptr}};

const FeatureEntry::FeatureParam
    kOmniboxSearchEngineLogoRoundedEdgesVariationConstant[] = {
        {"rounded_edges", "true"}};
const FeatureEntry::FeatureParam
    kOmniboxSearchEngineLogoLoupeEverywhereVariationConstant[] = {
        {"loupe_everywhere", "true"}};
const FeatureEntry::FeatureVariation
    kOmniboxSearchEngineLogoFeatureVariations[] = {
        {"(rounded edges)",
         kOmniboxSearchEngineLogoRoundedEdgesVariationConstant,
         base::size(kOmniboxSearchEngineLogoRoundedEdgesVariationConstant),
         nullptr},
        {"(loupe everywhere)",
         kOmniboxSearchEngineLogoLoupeEverywhereVariationConstant,
         base::size(kOmniboxSearchEngineLogoLoupeEverywhereVariationConstant),
         nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kQuietNotificationPromptsForceQuietNotifications[] = {
        {kQuietNotificationPromptsUIFlavorParameterName,
         kQuietNotificationPromptsQuietNotification},
        {kQuietNotificationPromptsActivationParameterName,
         kQuietNotificationPromptsActivationAlways},
};
const FeatureEntry::FeatureParam
    kQuietNotificationPromptsForceHeadsUpNotifications[] = {
        {kQuietNotificationPromptsUIFlavorParameterName,
         kQuietNotificationPromptsHeadsUpNotification},
        {kQuietNotificationPromptsActivationParameterName,
         kQuietNotificationPromptsActivationAlways},
};
const FeatureEntry::FeatureParam kQuietNotificationPromptsForceMiniInfobars[] =
    {
        {kQuietNotificationPromptsUIFlavorParameterName,
         kQuietNotificationPromptsMiniInfobar},
        {kQuietNotificationPromptsActivationParameterName,
         kQuietNotificationPromptsActivationAlways},
};

// The "default" option that only shows "Enabled" will be "quiet notifications",
// triggered after 3 consecutive denies.
const FeatureEntry::FeatureVariation kQuietNotificationPromptsVariations[] = {
    {"(force quiet notifications)",
     kQuietNotificationPromptsForceQuietNotifications,
     base::size(kQuietNotificationPromptsForceQuietNotifications), nullptr},
    {"(force heads-up notifications)",
     kQuietNotificationPromptsForceHeadsUpNotifications,
     base::size(kQuietNotificationPromptsForceHeadsUpNotifications), nullptr},
    {"(force mini-infobars)", kQuietNotificationPromptsForceMiniInfobars,
     base::size(kQuietNotificationPromptsForceMiniInfobars), nullptr},
};
#else   // OS_ANDROID
const FeatureEntry::FeatureParam
    kQuietNotificationPromptsForceStaticIconNotificationsPrompt[] = {
        {kQuietNotificationPromptsUIFlavorParameterName,
         kQuietNotificationPromptsStaticIcon},
        {kQuietNotificationPromptsActivationParameterName,
         kQuietNotificationPromptsActivationAlways},
};
const FeatureEntry::FeatureParam
    kQuietNotificationPromptsForceAnimatedIconNotificationsPrompt[] = {
        {kQuietNotificationPromptsUIFlavorParameterName,
         kQuietNotificationPromptsAnimatedIcon},
        {kQuietNotificationPromptsActivationParameterName,
         kQuietNotificationPromptsActivationAlways},
};

// The "default" option that only shows "Enabled" will be the static icon,
// triggered after 3 consecutive denies.
const FeatureEntry::FeatureVariation kQuietNotificationPromptsVariations[] = {
    {"(force static-icon)",
     kQuietNotificationPromptsForceStaticIconNotificationsPrompt,
     base::size(kQuietNotificationPromptsForceStaticIconNotificationsPrompt),
     nullptr},
    {"(force animated-icon)",
     kQuietNotificationPromptsForceAnimatedIconNotificationsPrompt,
     base::size(kQuietNotificationPromptsForceAnimatedIconNotificationsPrompt),
     nullptr},
};
#endif  // !OS_ANDROID

// TODO(crbug.com/991082,1015377): Remove after proper support for back-forward
// cache is implemented.
const FeatureEntry::FeatureParam kBackForwardCache_ForceCaching[] = {
    {"TimeToLiveInBackForwardCacheInSeconds", "300"},
    {"should_ignore_blocklists", "true"}};

const FeatureEntry::FeatureVariation kBackForwardCacheVariations[] = {
    {"force caching all pages", kBackForwardCache_ForceCaching,
     base::size(kBackForwardCache_ForceCaching), nullptr},
};

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kEnableCrOSActionRecorderChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {ash::switches::kCrOSActionRecorderWithHash,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderWithHash},
    {ash::switches::kCrOSActionRecorderWithoutHash,
     ash::switches::kEnableCrOSActionRecorder,
     ash::switches::kCrOSActionRecorderWithoutHash},
};
#endif  // defined(OS_CHROMEOS)

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
    {"ignore-gpu-blacklist", flag_descriptions::kIgnoreGpuBlacklistName,
     flag_descriptions::kIgnoreGpuBlacklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)},
    {"disable-accelerated-2d-canvas",
     flag_descriptions::kAccelerated2dCanvasName,
     flag_descriptions::kAccelerated2dCanvasDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAccelerated2dCanvas)},
    {"composited-layer-borders", flag_descriptions::kCompositedLayerBordersName,
     flag_descriptions::kCompositedLayerBordersDescription, kOsAll,
     SINGLE_VALUE_TYPE(cc::switches::kShowCompositedLayerBorders)},
    {"overlay-strategies", flag_descriptions::kOverlayStrategiesName,
     flag_descriptions::kOverlayStrategiesDescription, kOsAll,
     MULTI_VALUE_TYPE(kOverlayStrategiesChoices)},
    {"tint-gl-composited-content",
     flag_descriptions::kTintGlCompositedContentName,
     flag_descriptions::kTintGlCompositedContentDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kTintGlCompositedContent)},
    {"ui-disable-partial-swap", flag_descriptions::kUiPartialSwapName,
     flag_descriptions::kUiPartialSwapDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kUIDisablePartialSwap)},
    {"disable-webrtc-hw-decoding", flag_descriptions::kWebrtcHwDecodingName,
     flag_descriptions::kWebrtcHwDecodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", flag_descriptions::kWebrtcHwEncodingName,
     flag_descriptions::kWebrtcHwEncodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
#if !defined(OS_ANDROID)
    {"enable-reader-mode", flag_descriptions::kEnableReaderModeName,
     flag_descriptions::kEnableReaderModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(dom_distiller::kReaderMode)},
#endif  // !defined(OS_ANDROID)
#if defined(WEBRTC_USE_PIPEWIRE)
    {"enable-webrtc-pipewire-capturer",
     flag_descriptions::kWebrtcPipeWireCapturerName,
     flag_descriptions::kWebrtcPipeWireCapturerDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebRtcPipeWireCapturer)},
#endif  // defined(WEBRTC_USE_PIPEWIRE)
#if !defined(OS_ANDROID)
    {"enable-webrtc-remote-event-log",
     flag_descriptions::kWebRtcRemoteEventLogName,
     flag_descriptions::kWebRtcRemoteEventLogDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebRtcRemoteEventLog)},
#endif
    {"enable-webrtc-srtp-aes-gcm", flag_descriptions::kWebrtcSrtpAesGcmName,
     flag_descriptions::kWebrtcSrtpAesGcmDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcSrtpAesGcm)},
    {"enable-webrtc-stun-origin", flag_descriptions::kWebrtcStunOriginName,
     flag_descriptions::kWebrtcStunOriginDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcStunOrigin)},
    {"enable-webrtc-hybrid-agc", flag_descriptions::kWebrtcHybridAgcName,
     flag_descriptions::kWebrtcHybridAgcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcHybridAgc)},
    {"enable-webrtc-multi-channel-audio-processing",
     flag_descriptions::kWebrtcMultiChannelApmName,
     flag_descriptions::kWebrtcMultiChannelApmDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcEnableMultiChannelApm)},
    {"enable-webrtc-new-encode-cpu-load-estimator",
     flag_descriptions::kWebrtcNewEncodeCpuLoadEstimatorName,
     flag_descriptions::kWebrtcNewEncodeCpuLoadEstimatorDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kNewEncodeCpuLoadEstimator)},
    {"enable-webrtc-hide-local-ips-with-mdns",
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsName,
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsDecription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcHideLocalIpsWithMdns)},
#if defined(OS_ANDROID)
    {"clear-old-browsing-data", flag_descriptions::kClearOldBrowsingDataName,
     flag_descriptions::kClearOldBrowsingDataDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kClearOldBrowsingData)},
    {"enable-surfacecontrol", flag_descriptions::kAndroidSurfaceControl,
     flag_descriptions::kAndroidSurfaceControlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidSurfaceControl)},
#endif  // OS_ANDROID
#if BUILDFLAG(ENABLE_NACL)
    {"enable-nacl", flag_descriptions::kNaclName,
     flag_descriptions::kNaclDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
#endif  // ENABLE_NACL
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extension-apis", flag_descriptions::kExperimentalExtensionApisName,
     flag_descriptions::kExperimentalExtensionApisDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)},
    {"extensions-toolbar-menu", flag_descriptions::kExtensionsToolbarMenuName,
     flag_descriptions::kExtensionsToolbarMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kExtensionsToolbarMenu)},
    {"extensions-on-chrome-urls",
     flag_descriptions::kExtensionsOnChromeUrlsName,
     flag_descriptions::kExtensionsOnChromeUrlsDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif  // ENABLE_EXTENSIONS
    {"enable-history-manipulation-intervention",
     flag_descriptions::kHistoryManipulationIntervention,
     flag_descriptions::kHistoryManipulationInterventionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHistoryManipulationIntervention)},
    {"disable-pushstate-throttle",
     flag_descriptions::kDisablePushStateThrottleName,
     flag_descriptions::kDisablePushStateThrottleDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisablePushStateThrottle)},
    {"disable-ipc-flooding-protection",
     flag_descriptions::kDisableIpcFloodingProtectionName,
     flag_descriptions::kDisableIpcFloodingProtectionDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableIpcFloodingProtection)},
#if defined(OS_ANDROID)
    {"contextual-search-definitions",
     flag_descriptions::kContextualSearchDefinitionsName,
     flag_descriptions::kContextualSearchDefinitionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchDefinitions)},

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

    {"contextual-search-simplified-server",
     flag_descriptions::kContextualSearchSimplifiedServerName,
     flag_descriptions::kContextualSearchSimplifiedServerDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextualSearchSimplifiedServer,
         kSimplifiedServerVariations,
         "ContextualSearchSimplifiedServer")},

    {"contextual-search-translation-model",
     flag_descriptions::kContextualSearchTranslationModelName,
     flag_descriptions::kContextualSearchTranslationModelDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchTranslationModel)},
    {"direct-actions", flag_descriptions::kDirectActionsName,
     flag_descriptions::kDirectActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDirectActions)},
    {"explore-sites", flag_descriptions::kExploreSitesName,
     flag_descriptions::kExploreSitesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kExploreSites,
                                    kExploreSitesVariations,
                                    "ExploreSites InitialCountries")},
    {"shopping-assist", flag_descriptions::kShoppingAssistName,
     flag_descriptions::kShoppingAssistDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShoppingAssist)},
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
    {"overlay-scrollbars-flash-after-scroll-update",
     flag_descriptions::kOverlayScrollbarsFlashAfterAnyScrollUpdateName,
     flag_descriptions::kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription,
     kOsAura,
     FEATURE_VALUE_TYPE(features::kOverlayScrollbarFlashAfterAnyScrollUpdate)},
    {"overlay-scrollbars-flash-when-mouse-enter",
     flag_descriptions::kOverlayScrollbarsFlashWhenMouseEnterName,
     flag_descriptions::kOverlayScrollbarsFlashWhenMouseEnterDescription,
     kOsAura,
     FEATURE_VALUE_TYPE(features::kOverlayScrollbarFlashWhenMouseEnter)},
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
    {"enable-webassembly-code-cache",
     flag_descriptions::kEnableWasmCodeCacheName,
     flag_descriptions::kEnableWasmCodeCacheDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kWasmCodeCache)},
    {"enable-webassembly-code-gc", flag_descriptions::kEnableWasmCodeGCName,
     flag_descriptions::kEnableWasmCodeGCDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyCodeGC)},
    {"enable-webassembly-simd", flag_descriptions::kEnableWasmSimdName,
     flag_descriptions::kEnableWasmSimdDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblySimd)},
    {"enable-webassembly-threads", flag_descriptions::kEnableWasmThreadsName,
     flag_descriptions::kEnableWasmThreadsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyThreads)},
    {"shared-array-buffer", flag_descriptions::kEnableSharedArrayBufferName,
     flag_descriptions::kEnableSharedArrayBufferDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSharedArrayBuffer)},
    {"enable-future-v8-vm-features", flag_descriptions::kV8VmFutureName,
     flag_descriptions::kV8VmFutureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8VmFuture)},
    {"enable-gpu-rasterization", flag_descriptions::kGpuRasterizationName,
     flag_descriptions::kGpuRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"enable-oop-rasterization", flag_descriptions::kOopRasterizationName,
     flag_descriptions::kOopRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableOopRasterizationChoices)},
    {"enable-experimental-web-platform-features",
     flag_descriptions::kExperimentalWebPlatformFeaturesName,
     flag_descriptions::kExperimentalWebPlatformFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
#if defined(OS_ANDROID)
    {"enable-app-notification-status-messaging",
     flag_descriptions::kAppNotificationStatusMessagingName,
     flag_descriptions::kAppNotificationStatusMessagingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAppNotificationStatusMessaging)},
#endif  // OS_ANDROID
    {"enable-devtools-experiments", flag_descriptions::kDevtoolsExperimentsName,
     flag_descriptions::kDevtoolsExperimentsDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableDevToolsExperiments)},
    {"silent-debugger-extension-api",
     flag_descriptions::kSilentDebuggerExtensionApiName,
     flag_descriptions::kSilentDebuggerExtensionApiDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kSilentDebuggerExtensionAPI)},
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
    {"webui-tab-strip-demo-options",
     flag_descriptions::kWebUITabStripDemoOptionsName,
     flag_descriptions::kWebUITabStripDemoOptionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUITabStripDemoOptions)},
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
    {"focus-mode", flag_descriptions::kFocusMode,
     flag_descriptions::kFocusModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kFocusMode)},
    {"touch-events", flag_descriptions::kTouchEventsName,
     flag_descriptions::kTouchEventsDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTouchEventFeatureDetectionChoices)},
    {"disable-touch-adjustment", flag_descriptions::kTouchAdjustmentName,
     flag_descriptions::kTouchAdjustmentDescription,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableTouchAdjustment)},
#if defined(OS_CHROMEOS)
    {"disable-explicit-dma-fences",
     flag_descriptions::kDisableExplicitDmaFencesName,
     flag_descriptions::kDisableExplicitDmaFencesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableExplicitDmaFences)},
    // TODO(crbug.com/1012846): Remove this flag and provision when HDR is fully
    //  supported on ChromeOS.
    {"enable-use-hdr-transfer-function",
     flag_descriptions::kEnableUseHDRTransferFunctionName,
     flag_descriptions::kEnableUseHDRTransferFunctionDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUseHDRTransferFunction)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"ash-enable-unified-desktop",
     flag_descriptions::kAshEnableUnifiedDesktopName,
     flag_descriptions::kAshEnableUnifiedDesktopDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUnifiedDesktop)},
    {"bluetooth-aggressive-appearance-filter",
     flag_descriptions::kBluetoothAggressiveAppearanceFilterName,
     flag_descriptions::kBluetoothAggressiveAppearanceFilterDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kBluetoothAggressiveAppearanceFilter)},
    {"cryptauth-v1-devicesync-deprecate",
     flag_descriptions::kCryptAuthV1DeviceSyncDeprecateName,
     flag_descriptions::kCryptAuthV1DeviceSyncDeprecateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV1DeviceSyncDeprecate)},
    {"cryptauth-v2-device-activity-status",
     flag_descriptions::kCryptAuthV2DeviceActivityStatusName,
     flag_descriptions::kCryptAuthV2DeviceActivityStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceActivityStatus)},
    {"cryptauth-v2-devicesync", flag_descriptions::kCryptAuthV2DeviceSyncName,
     flag_descriptions::kCryptAuthV2DeviceSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2DeviceSync)},
    {"cryptauth-v2-enrollment", flag_descriptions::kCryptAuthV2EnrollmentName,
     flag_descriptions::kCryptAuthV2EnrollmentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCryptAuthV2Enrollment)},
    {"disable-office-editing-component-app",
     flag_descriptions::kDisableOfficeEditingComponentAppName,
     flag_descriptions::kDisableOfficeEditingComponentAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableOfficeEditingComponentApp)},
    {"updated_cellular_activation_ui",
     flag_descriptions::kUpdatedCellularActivationUiName,
     flag_descriptions::kUpdatedCellularActivationUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUpdatedCellularActivationUi)},
    {"use_messages_google_com_domain",
     flag_descriptions::kUseMessagesGoogleComDomainName,
     flag_descriptions::kUseMessagesGoogleComDomainDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseMessagesGoogleComDomain)},
    {"use_messages_staging_url", flag_descriptions::kUseMessagesStagingUrlName,
     flag_descriptions::kUseMessagesStagingUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseMessagesStagingUrl)},
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
    {"enable-app-list-search-autocomplete",
     flag_descriptions::kEnableAppListSearchAutocompleteName,
     flag_descriptions::kEnableAppListSearchAutocompleteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppListSearchAutocomplete)},
    {"list-all-display-modes", flag_descriptions::kListAllDisplayModesName,
     flag_descriptions::kListAllDisplayModesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kListAllDisplayModes)},
    {"instant-tethering", flag_descriptions::kTetherName,
     flag_descriptions::kTetherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kInstantTethering)},
    {"newblue", flag_descriptions::kNewblueName,
     flag_descriptions::kNewblueDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(device::kNewblueDaemon)},
    {"shelf-hotseat", flag_descriptions::kShelfHotseatName,
     flag_descriptions::kShelfHotseatDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kShelfHotseat)},
    {"shelf-hover-previews", flag_descriptions::kShelfHoverPreviewsName,
     flag_descriptions::kShelfHoverPreviewsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kShelfHoverPreviews)},
    {"shelf-scrollable", flag_descriptions::kShelfScrollableName,
     flag_descriptions::kShelfScrollableDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShelfScrollable)},
    {"show-bluetooth-debug-log-toggle",
     flag_descriptions::kShowBluetoothDebugLogToggleName,
     flag_descriptions::kShowBluetoothDebugLogToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShowBluetoothDebugLogToggle)},
    {"show-bluetooth-device-battery",
     flag_descriptions::kShowBluetoothDeviceBatteryName,
     flag_descriptions::kShowBluetoothDeviceBatteryDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kShowBluetoothDeviceBattery)},
    {"show-taps", flag_descriptions::kShowTapsName,
     flag_descriptions::kShowTapsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShowTaps)},
    {"show-touch-hud", flag_descriptions::kShowTouchHudName,
     flag_descriptions::kShowTouchHudDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {"enable-virtual-desks", flag_descriptions::kEnableVirtualDesksName,
     flag_descriptions::kEnableVirtualDesksDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVirtualDesks)},
    {"trim-on-all-frames-frozen", flag_descriptions::kTrimOnFreezeName,
     flag_descriptions::kTrimOnFreezeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kTrimOnFreeze)},
    {"trim-on-memory-pressure", flag_descriptions::kTrimOnMemoryPressureName,
     flag_descriptions::kTrimOnMemoryPressureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(performance_manager::features::kTrimOnMemoryPressure)},
    {"message-center-redesign", flag_descriptions::kMessageCenterRedesignName,
     flag_descriptions::kMessageCenterRedesignName, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUnifiedMessageCenterRefactor)},
#endif  // OS_CHROMEOS
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
    {
        "disable-accelerated-video-encode",
        flag_descriptions::kAcceleratedVideoEncodeName,
        flag_descriptions::kAcceleratedVideoEncodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoEncode),
    },
    {"enable-history-favicons-google-server-query",
     flag_descriptions::kEnableHistoryFaviconsGoogleServerQueryName,
     flag_descriptions::kEnableHistoryFaviconsGoogleServerQueryDescription,
     kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(favicon::kEnableHistoryFaviconsGoogleServerQuery)},
#if defined(OS_CHROMEOS)
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
    {"ui-show-composited-layer-borders",
     flag_descriptions::kUiShowCompositedLayerBordersName,
     flag_descriptions::kUiShowCompositedLayerBordersDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kUiShowCompositedLayerBordersChoices)},
    {"enable-request-tablet-site", flag_descriptions::kRequestTabletSiteName,
     flag_descriptions::kRequestTabletSiteDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableRequestTabletSite)},
#endif  // OS_CHROMEOS
    {"debug-packed-apps", flag_descriptions::kDebugPackedAppName,
     flag_descriptions::kDebugPackedAppDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"username-first-flow", flag_descriptions::kUsernameFirstFlowName,
     flag_descriptions::kUsernameFirstFlowDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kUsernameFirstFlow)},
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
#if defined(USE_AURA) || defined(OS_ANDROID)
    {"overscroll-history-navigation",
     flag_descriptions::kOverscrollHistoryNavigationName,
     flag_descriptions::kOverscrollHistoryNavigationDescription,
     kOsAura | kOsAndroid,
#if defined(OS_ANDROID)
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kOverscrollHistoryNavigation,
                                    kOverscrollHistoryNavigationVariations,
                                    "OverscrollNavigation")},
#else
     FEATURE_VALUE_TYPE(features::kOverscrollHistoryNavigation)},
    {"pull-to-refresh", flag_descriptions::kPullToRefreshName,
     flag_descriptions::kPullToRefreshDescription, kOsAura,
     MULTI_VALUE_TYPE(kPullToRefreshChoices)},
#endif  // OS_ANDROID
#endif  // USE_AURA || OS_ANDROID
    {"enable-touch-drag-drop", flag_descriptions::kTouchDragDropName,
     flag_descriptions::kTouchDragDropDescription, kOsWin | kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableTouchDragDrop,
                               switches::kDisableTouchDragDrop)},
    {"touch-selection-strategy", flag_descriptions::kTouchSelectionStrategyName,
     flag_descriptions::kTouchSelectionStrategyDescription,
     kOsAndroid,  // TODO(mfomitchev): Add CrOS/Win/Linux support soon.
     MULTI_VALUE_TYPE(kTouchTextSelectionStrategyChoices)},
    {"enable-navigation-tracing",
     flag_descriptions::kEnableNavigationTracingName,
     flag_descriptions::kEnableNavigationTracingDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNavigationTracing)},
    {"trace-upload-url", flag_descriptions::kTraceUploadUrlName,
     flag_descriptions::kTraceUploadUrlDescription, kOsAll,
     MULTI_VALUE_TYPE(kTraceUploadURL)},
    {"enable-suggestions-with-substring-match",
     flag_descriptions::kSuggestionsWithSubStringMatchName,
     flag_descriptions::kSuggestionsWithSubStringMatchDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillTokenPrefixMatching)},
    {"enable-offer-store-unmasked-wallet-cards",
     flag_descriptions::kOfferStoreUnmaskedWalletCardsName,
     flag_descriptions::kOfferStoreUnmaskedWalletCardsDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferStoreUnmaskedWalletCards,
         autofill::switches::kDisableOfferStoreUnmaskedWalletCards)},
#if defined(OS_CHROMEOS)
    {"enable-virtual-keyboard", flag_descriptions::kVirtualKeyboardName,
     flag_descriptions::kVirtualKeyboardDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
#endif  // OS_CHROMEOS
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     flag_descriptions::kDeviceDiscoveryNotificationsName,
     flag_descriptions::kDeviceDiscoveryNotificationsDescription, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                               switches::kDisableDeviceDiscoveryNotifications)},
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#if defined(OS_WIN)
    {"enable-cloud-print-xps", flag_descriptions::kCloudPrintXpsName,
     flag_descriptions::kCloudPrintXpsDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)},
#endif  // OS_WIN
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"enable-webgl2-compute-context",
     flag_descriptions::kWebGL2ComputeContextName,
     flag_descriptions::kWebGL2ComputeContextDescription,
     kOsWin | kOsLinux | kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWebGL2ComputeContext)},
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
    {"enable-webgl-draft-extensions",
     flag_descriptions::kWebglDraftExtensionsName,
     flag_descriptions::kWebglDraftExtensionsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"force-dice-migration", flag_descriptions::kForceDiceMigrationName,
     flag_descriptions::kForceDiceMigrationDescription, kOsAll,
     FEATURE_VALUE_TYPE(kForceDiceMigration)},
    {"show-sync-paused-reason-cookies-cleared-on-exit",
     flag_descriptions::kShowSyncPausedReasonCookiesClearedOnExitName,
     flag_descriptions::kShowSyncPausedReasonCookiesClearedOnExitDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kShowSyncPausedReasonCookiesClearedOnExit)},
#endif
#if defined(OS_ANDROID)
    {"enable-android-autofill-accessibility",
     flag_descriptions::kAndroidAutofillAccessibilityName,
     flag_descriptions::kAndroidAutofillAccessibilityDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidAutofillAccessibility)},
#endif  // OS_ANDROID
    {"enable-zero-copy", flag_descriptions::kZeroCopyName,
     flag_descriptions::kZeroCopyDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableZeroCopy,
                               switches::kDisableZeroCopy)},
    {"enable-vulkan", flag_descriptions::kEnableVulkanName,
     flag_descriptions::kEnableVulkanDescription, kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kVulkan)},
#if defined(OS_MACOSX)
    {"disable-hosted-app-shim-creation",
     flag_descriptions::kHostedAppShimCreationName,
     flag_descriptions::kHostedAppShimCreationDescription, kOsMac,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableHostedAppShimCreation)},
    {"enable-hosted-app-quit-notification",
     flag_descriptions::kHostedAppQuitNotificationName,
     flag_descriptions::kHostedAppQuitNotificationDescription, kOsMac,
     SINGLE_VALUE_TYPE(switches::kHostedAppQuitNotification)},
#endif  // OS_MACOSX
#if defined(OS_ANDROID)
    {"translate-force-trigger-on-english",
     flag_descriptions::kTranslateForceTriggerOnEnglishName,
     flag_descriptions::kTranslateForceTriggerOnEnglishDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(language::kOverrideTranslateTriggerInIndia,
                                    kTranslateForceTriggerOnEnglishVariations,
                                    "OverrideTranslateTriggerInIndia")},
#endif  // OS_ANDROID

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN) || \
    defined(OS_CHROMEOS)
    {"translate-ui-bubble-options", flag_descriptions::kTranslateBubbleUIName,
     flag_descriptions::kTranslateBubbleUIDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(language::kUseButtonTranslateBubbleUi,
                                    kTranslateBubbleUIVariations,
                                    "UseButtonTranslateBubbleUI")},
#endif  // OS_LINUX || OS_MACOSX || OS_WIN || OS_CHROMEOS

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS) && !defined(OS_CHROMEOS)
    {"enable-native-notifications",
     flag_descriptions::kNotificationsNativeFlagName,
     flag_descriptions::kNotificationsNativeFlagDescription,
     kOsMac | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kNativeNotifications)},
#endif  // ENABLE_NATIVE_NOTIFICATIONS
#if defined(OS_ANDROID)
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"enable-chrome-duet", flag_descriptions::kChromeDuetName,
     flag_descriptions::kChromeDuetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeDuetFeature)},
    {"enable-chrome-duet-labels", flag_descriptions::kChromeDuetLabelsName,
     flag_descriptions::kChromeDuetLabelsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeDuetLabeled)},
    {"chrome-sharing-hub", flag_descriptions::kChromeSharingHubName,
     flag_descriptions::kChromeSharingHubDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeSharingHub)},
    {"enable-bookmark-reorder", flag_descriptions::kReorderBookmarksName,
     flag_descriptions::kReorderBookmarksDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReorderBookmarks)},
    {"request-unbuffered-dispatch",
     flag_descriptions::kRequestUnbufferedDispatchName,
     flag_descriptions::kRequestUnbufferedDispatchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kRequestUnbufferedDispatch)},
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
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableThreadedScrolling)},
    {"extension-content-verification",
     flag_descriptions::kExtensionContentVerificationName,
     flag_descriptions::kExtensionContentVerificationDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
#if defined(OS_CHROMEOS)
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"wake-on-wifi-packet", flag_descriptions::kWakeOnPacketsName,
     flag_descriptions::kWakeOnPacketsDescription, kOsCrOSOwnerOnly,
     SINGLE_VALUE_TYPE(chromeos::switches::kWakeOnWifiPacket)},
#endif  // OS_CHROMEOS
    {"reduced-referrer-granularity",
     flag_descriptions::kReducedReferrerGranularityName,
     flag_descriptions::kReducedReferrerGranularityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kReducedReferrerGranularity)},
#if defined(OS_CHROMEOS)
    {"crostini-backup", flag_descriptions::kCrostiniBackupName,
     flag_descriptions::kCrostiniBackupDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniBackup)},
    {"terminal-system-app", flag_descriptions::kTerminalSystemAppName,
     flag_descriptions::kTerminalSystemAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTerminalSystemApp)},
    {"terminal-system-app-splits",
     flag_descriptions::kTerminalSystemAppSplitsName,
     flag_descriptions::kTerminalSystemAppSplitsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTerminalSystemAppSplits)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
    {"dynamic-tcmalloc-tuning", flag_descriptions::kDynamicTcmallocName,
     flag_descriptions::kDynamicTcmallocDescription, kOsCrOS | kOsLinux,
     FEATURE_VALUE_TYPE(performance_manager::features::kDynamicTcmallocTuning)},
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // OS_CHROMEOS || OS_LINUX
#if defined(OS_ANDROID)
    {"enable-credit-card-assist", flag_descriptions::kCreditCardAssistName,
     flag_descriptions::kCreditCardAssistDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardAssist)},
#endif  // OS_ANDROID
    {"http-auth-committed-interstitials",
     flag_descriptions::kHTTPAuthCommittedInterstitialsName,
     flag_descriptions::kHTTPAuthCommittedInterstitialsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHTTPAuthCommittedInterstitials)},
#if defined(OS_ANDROID)
    {"enable-site-isolation-for-password-sites",
     flag_descriptions::kSiteIsolationForPasswordSitesName,
     flag_descriptions::kSiteIsolationForPasswordSitesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSiteIsolationForPasswordSites)},
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
    {"kids-management-url-classification",
     flag_descriptions::kKidsManagementUrlClassificationName,
     flag_descriptions::kKidsManagementUrlClassificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kKidsManagementUrlClassification)},
    {"site-isolation-trial-opt-out",
     flag_descriptions::kSiteIsolationOptOutName,
     flag_descriptions::kSiteIsolationOptOutDescription, kOsAll,
     MULTI_VALUE_TYPE(kSiteIsolationOptOutChoices)},
    {"enable-use-zoom-for-dsf", flag_descriptions::kEnableUseZoomForDsfName,
     flag_descriptions::kEnableUseZoomForDsfDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableUseZoomForDSFChoices)},
    {"allow-previews", flag_descriptions::kPreviewsAllowedName,
     flag_descriptions::kPreviewsAllowedDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kPreviews)},
    {"data-saver-server-previews",
     flag_descriptions::kDataSaverServerPreviewsName,
     flag_descriptions::kDataSaverServerPreviewsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         data_reduction_proxy::features::kDataReductionProxyDecidesTransform)},
    {"ignore-previews-blocklist",
     flag_descriptions::kIgnorePreviewsBlacklistName,
     flag_descriptions::kIgnorePreviewsBlacklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(previews::switches::kIgnorePreviewsBlacklist)},
    {"ignore-litepage-redirect-optimization-blacklist",
     flag_descriptions::kIgnoreLitePageRedirectHintsBlacklistName,
     flag_descriptions::kIgnoreLitePageRedirectHintsBlacklistDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         previews::switches::kIgnoreLitePageRedirectOptimizationBlacklist)},
    {"enable-data-reduction-proxy-server-experiment",
     flag_descriptions::kEnableDataReductionProxyServerExperimentName,
     flag_descriptions::kEnableDataReductionProxyServerExperimentDescription,
     kOsAll, MULTI_VALUE_TYPE(kDataReductionProxyServerExperiment)},
    {"enable-subresource-redirect",
     flag_descriptions::kEnableSubresourceRedirectName,
     flag_descriptions::kEnableSubresourceRedirectDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kSubresourceRedirect)},
#if defined(OS_ANDROID)
    {"enable-offline-previews", flag_descriptions::kEnableOfflinePreviewsName,
     flag_descriptions::kEnableOfflinePreviewsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(previews::features::kOfflinePreviews)},
    {"enable-lite-page-server-previews",
     flag_descriptions::kEnableLitePageServerPreviewsName,
     flag_descriptions::kEnableLitePageServerPreviewsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(previews::features::kLitePageServerPreviews)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
    {"enable-save-data", flag_descriptions::kEnableSaveDataName,
     flag_descriptions::kEnableSaveDataDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(
         data_reduction_proxy::switches::kEnableDataReductionProxy)},
#endif  // OS_CHROMEOS
    {"enable-noscript-previews", flag_descriptions::kEnableNoScriptPreviewsName,
     flag_descriptions::kEnableNoScriptPreviewsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kNoScriptPreviews)},
    {"enable-resource-loading-hints",
     flag_descriptions::kEnableResourceLoadingHintsName,
     flag_descriptions::kEnableResourceLoadingHintsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kResourceLoadingHints)},
    {"enable-previews-coin-flip",
     flag_descriptions::kEnablePreviewsCoinFlipName,
     flag_descriptions::kEnablePreviewsCoinFlipDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kCoinFlipHoldback)},
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kBypassAppBannerEngagementChecks)},
    {"enable-desktop-pwas-local-updating",
     flag_descriptions::kDesktopPWAsLocalUpdatingName,
     flag_descriptions::kDesktopPWAsLocalUpdatingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsLocalUpdating)},
    {"enable-system-webapps", flag_descriptions::kEnableSystemWebAppsName,
     flag_descriptions::kEnableSystemWebAppsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemWebApps)},
    {"enable-desktop-pwas-omnibox-install",
     flag_descriptions::kDesktopPWAsOmniboxInstallName,
     flag_descriptions::kDesktopPWAsOmniboxInstallDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsOmniboxInstall)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
#if !defined(OS_ANDROID)
    {"load-media-router-component-extension",
     flag_descriptions::kLoadMediaRouterComponentExtensionName,
     flag_descriptions::kLoadMediaRouterComponentExtensionDescription,
     kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         switches::kLoadMediaRouterComponentExtension,
         "1",
         switches::kLoadMediaRouterComponentExtension,
         "0")},
    {"media-router-cast-allow-all-ips",
     flag_descriptions::kMediaRouterCastAllowAllIPsName,
     flag_descriptions::kMediaRouterCastAllowAllIPsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastAllowAllIPsFeature)},
    {"cast-media-route-provider",
     flag_descriptions::kCastMediaRouteProviderName,
     flag_descriptions::kCastMediaRouteProviderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastMediaRouteProvider)},
    {"dial-media-route-provider",
     flag_descriptions::kDialMediaRouteProviderName,
     flag_descriptions::kDialMediaRouteProviderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kDialMediaRouteProvider)},
    {"mirroring-service", flag_descriptions::kMirroringServiceName,
     flag_descriptions::kMirroringServiceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kMirroringService)},
#endif  // !OS_ANDROID
#if defined(OS_ANDROID)
    {"autofill-keyboard-accessory-view",
     flag_descriptions::kAutofillAccessoryViewName,
     flag_descriptions::kAutofillAccessoryViewDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillKeyboardAccessory,
         kAutofillKeyboardAccessoryFeatureVariations,
         "AutofillKeyboardAccessoryVariations")},
#endif  // OS_ANDROID
#if defined(OS_WIN)
    {"try-supported-channel-layouts",
     flag_descriptions::kTrySupportedChannelLayoutsName,
     flag_descriptions::kTrySupportedChannelLayoutsDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTrySupportedChannelLayouts)},
#endif  // OS_WIN
#if defined(OS_MACOSX)
    {"mac-syscall-sandbox", flag_descriptions::kMacSyscallSandboxName,
     flag_descriptions::kMacSyscallSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSyscallSandbox)},
    {"mac-v2-gpu-sandbox", flag_descriptions::kMacV2GPUSandboxName,
     flag_descriptions::kMacV2GPUSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacV2GPUSandbox)},
    {"mac-views-task-manager", flag_descriptions::kMacViewsTaskManagerName,
     flag_descriptions::kMacViewsTaskManagerDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kViewsTaskManager)},
#endif  // OS_MACOSX
#if BUILDFLAG(ENABLE_VR)
    {"webxr", flag_descriptions::kWebXrName,
     flag_descriptions::kWebXrDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXr)},
    {"webxr-ar-module", flag_descriptions::kWebXrArModuleName,
     flag_descriptions::kWebXrArModuleDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrArModule)},
    {"webxr-ar-dom-overlay", flag_descriptions::kWebXrArDOMOverlayName,
     flag_descriptions::kWebXrArDOMOverlayDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kWebXrArDOMOverlay)},
    {"webxr-hit-test", flag_descriptions::kWebXrHitTestName,
     flag_descriptions::kWebXrHitTestDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrHitTest)},
    {"webxr-anchors", flag_descriptions::kWebXrAnchorsName,
     flag_descriptions::kWebXrAnchorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrAnchors)},
    {"webxr-plane-detection", flag_descriptions::kWebXrPlaneDetectionName,
     flag_descriptions::kWebXrPlaneDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrPlaneDetection)},
    {"webxr-orientation-sensor-device",
     flag_descriptions::kWebXrOrientationSensorDeviceName,
     flag_descriptions::kWebXrOrientationSensorDeviceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(device::kWebXrOrientationSensorDevice)},
#if BUILDFLAG(ENABLE_OCULUS_VR)
    {"oculus-vr", flag_descriptions::kOculusVRName,
     flag_descriptions::kOculusVRDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kOculusVR)},
#endif  // ENABLE_OCULUS_VR
#if BUILDFLAG(ENABLE_OPENVR)
    {"openvr", flag_descriptions::kOpenVRName,
     flag_descriptions::kOpenVRDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kOpenVR)},
#endif  // ENABLE_OPENVR
#if BUILDFLAG(ENABLE_WINDOWS_MR)
    {"windows-mixed-reality", flag_descriptions::kWindowsMixedRealityName,
     flag_descriptions::kWindowsMixedRealityDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWindowsMixedReality)},
#endif  // ENABLE_WINDOWS_MR
#if BUILDFLAG(ENABLE_OPENXR)
    {"openxr", flag_descriptions::kOpenXRName,
     flag_descriptions::kOpenXRDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kOpenXR)},
#endif  // ENABLE_OPENXR
#if !defined(OS_ANDROID)
    {"xr-sandbox", flag_descriptions::kXRSandboxName,
     flag_descriptions::kXRSandboxDescription, kOsWin,
     FEATURE_VALUE_TYPE(service_manager::features::kXRSandbox)},
#endif  // !defined(OS_ANDROID)
#endif  // ENABLE_VR
#if defined(OS_CHROMEOS)
    {"disable-accelerated-mjpeg-decode",
     flag_descriptions::kAcceleratedMjpegDecodeName,
     flag_descriptions::kAcceleratedMjpegDecodeDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // OS_CHROMEOS
    {"system-keyboard-lock", flag_descriptions::kSystemKeyboardLockName,
     flag_descriptions::kSystemKeyboardLockDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemKeyboardLock)},
#if defined(OS_ANDROID)
    {"offline-pages-load-signal-collecting",
     flag_descriptions::kOfflinePagesLoadSignalCollectingName,
     flag_descriptions::kOfflinePagesLoadSignalCollectingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesLoadSignalCollectingFeature)},
    {"offline-pages-live-page-sharing",
     flag_descriptions::kOfflinePagesLivePageSharingName,
     flag_descriptions::kOfflinePagesLivePageSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesLivePageSharingFeature)},
    {"offline-pages-prefetching",
     flag_descriptions::kOfflinePagesPrefetchingName,
     flag_descriptions::kOfflinePagesPrefetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kPrefetchingOfflinePagesFeature)},
    {"offline-pages-failed-download",
     flag_descriptions::kOfflinePagesDescriptiveFailStatusName,
     flag_descriptions::kOfflinePagesDescriptiveFailStatusDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesDescriptiveFailStatusFeature)},
    {"offline-pages-pending-download",
     flag_descriptions::kOfflinePagesDescriptivePendingStatusName,
     flag_descriptions::kOfflinePagesDescriptivePendingStatusDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesDescriptivePendingStatusFeature)},
    {"offline-pages-resource-based-snapshot",
     flag_descriptions::kOfflinePagesResourceBasedSnapshotName,
     flag_descriptions::kOfflinePagesResourceBasedSnapshotDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesResourceBasedSnapshotFeature)},
    {"offline-pages-renovations",
     flag_descriptions::kOfflinePagesRenovationsName,
     flag_descriptions::kOfflinePagesRenovationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesRenovationsFeature)},
    {"offline-pages-in-downloads-home-open-in-cct",
     flag_descriptions::kOfflinePagesInDownloadHomeOpenInCctName,
     flag_descriptions::kOfflinePagesInDownloadHomeOpenInCctDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesInDownloadHomeOpenInCctFeature)},
    {"offline-pages-alternate-dino-page",
     flag_descriptions::kOfflinePagesShowAlternateDinoPageName,
     flag_descriptions::kOfflinePagesShowAlternateDinoPageDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesShowAlternateDinoPageFeature)},
    {"offline-indicator-choice", flag_descriptions::kOfflineIndicatorChoiceName,
     flag_descriptions::kOfflineIndicatorChoiceDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(offline_pages::kOfflineIndicatorFeature,
                                    kOfflineIndicatorFeatureVariations,
                                    "OfflineIndicator")},
    {"offline-indicator-always-http-probe",
     flag_descriptions::kOfflineIndicatorAlwaysHttpProbeName,
     flag_descriptions::kOfflineIndicatorAlwaysHttpProbeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflineIndicatorAlwaysHttpProbeFeature)},
    {"offline-home", flag_descriptions::kOfflineHomeName,
     flag_descriptions::kOfflineHomeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOfflineHome)},
    {"offline-indicator-v2", flag_descriptions::kOfflineIndicatorV2Name,
     flag_descriptions::kOfflineIndicatorV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOfflineIndicatorV2)},
    {"on-the-fly-mhtml-hash-computation",
     flag_descriptions::kOnTheFlyMhtmlHashComputationName,
     flag_descriptions::kOnTheFlyMhtmlHashComputationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOnTheFlyMhtmlHashComputationFeature)},
    {"android-picture-in-picture-api",
     flag_descriptions::kAndroidPictureInPictureAPIName,
     flag_descriptions::kAndroidPictureInPictureAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(media::kPictureInPictureAPI)},
#endif  // OS_ANDROID
    {"disallow-doc-written-script-loads",
     flag_descriptions::kDisallowDocWrittenScriptsUiName,
     flag_descriptions::kDisallowDocWrittenScriptsUiDescription, kOsAll,
     // NOTE: if we want to add additional experiment entries for other
     // features controlled by kBlinkSettings, we'll need to add logic to
     // merge the flag values.
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=true",
         switches::kBlinkSettings,
         "disallowFetchForDocWrittenScriptsInMainFrame=false")},
#if defined(OS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
#endif  // OS_WIN
#if defined(OS_CHROMEOS)
    {"cros-regions-mode", flag_descriptions::kCrosRegionsModeName,
     flag_descriptions::kCrosRegionsModeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCrosRegionsModeChoices)},
#endif  // OS_CHROMEOS
#if defined(OS_WIN)
    {"enable-aura-tooltips-on-windows",
     flag_descriptions::kEnableAuraTooltipsOnWindowsName,
     flag_descriptions::kEnableAuraTooltipsOnWindowsDescription, kOsWin,
     FEATURE_VALUE_TYPE(views::features::kEnableAuraTooltipsOnWindows)},
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
    {"tls13-hardening-for-local-anchors",
     flag_descriptions::kTLS13HardeningForLocalAnchorsName,
     flag_descriptions::kTLS13HardeningForLocalAnchorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kTLS13HardeningForLocalAnchors)},
    {"enable-tls13-early-data", flag_descriptions::kEnableTLS13EarlyDataName,
     flag_descriptions::kEnableTLS13EarlyDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableTLS13EarlyData)},
#if defined(OS_ANDROID)
    {"interest-feed-content-suggestions",
     flag_descriptions::kInterestFeedContentSuggestionsName,
     flag_descriptions::kInterestFeedContentSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(feed::kInterestFeedContentSuggestions,
                                    kInterestFeedFeatureVariations,
                                    "InterestFeedContentSuggestions")},
    {"interest-feed-notifications",
     flag_descriptions::kInterestFeedNotificationsName,
     flag_descriptions::kInterestFeedNotificationsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedNotifications)},
    {"enable-ntp-remote-suggestions",
     flag_descriptions::kEnableNtpRemoteSuggestionsName,
     flag_descriptions::kEnableNtpRemoteSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_snippets::kArticleSuggestionsFeature,
                                    kRemoteSuggestionsFeatureVariations,
                                    "NTPArticleSuggestions")},
    {"offlining-recent-pages", flag_descriptions::kOffliningRecentPagesName,
     flag_descriptions::kOffliningRecentPagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOffliningRecentPagesFeature)},
    {"offline-pages-ct", flag_descriptions::kOfflinePagesCtName,
     flag_descriptions::kOfflinePagesCtDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesCTFeature)},
    {"offline-pages-ct-v2", flag_descriptions::kOfflinePagesCtV2Name,
     flag_descriptions::kOfflinePagesCtV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesCTV2Feature)},
    {"offline-pages-ct-suppress-completed-notification",
     flag_descriptions::kOfflinePagesCTSuppressNotificationsName,
     flag_descriptions::kOfflinePagesCTSuppressNotificationsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesCTSuppressNotificationsFeature)},
#endif  // OS_ANDROID
    {"PasswordImport", flag_descriptions::kPasswordImportName,
     flag_descriptions::kPasswordImportDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordImport)},
#if defined(OS_ANDROID)
    {"password-editing-android", flag_descriptions::kPasswordEditingAndroidName,
     flag_descriptions::kPasswordEditingAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordEditingAndroid)},
#endif  // OS_ANDROID
#if !defined(OS_CHROMEOS)
    // TODO(https://crbug.com/1011696): Investigate crash reports and re-enable
    // for ChromeOS.
    {"enable-force-dark", flag_descriptions::kForceWebContentsDarkModeName,
     flag_descriptions::kForceWebContentsDarkModeDescription,
     kOsWin | kOsLinux | kOsMac | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kForceWebContentsDarkMode,
                                    kForceDarkVariations,
                                    "ForceDarkVariations")},
#endif  // !OS_CHROMEOS
#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)
    {"enable-android-night-mode", flag_descriptions::kAndroidNightModeName,
     flag_descriptions::kAndroidNightModeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAndroidNightMode,
                                    kAndroidNightModeFeatureVariations,
                                    "AndroidNightMode")},
#endif  // BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)
#endif  // OS_ANDROID
    {"enable-experimental-accessibility-features",
     flag_descriptions::kExperimentalAccessibilityFeaturesName,
     flag_descriptions::kExperimentalAccessibilityFeaturesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(::switches::kEnableExperimentalAccessibilityFeatures)},
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
#if defined(OS_CHROMEOS)
    {"enable-encryption-migration",
     flag_descriptions::kEnableEncryptionMigrationName,
     flag_descriptions::kEnableEncryptionMigrationDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableEncryptionMigration,
         chromeos::switches::kDisableEncryptionMigration)},
    {"enable-cros-ime-input-logic-fst",
     flag_descriptions::kImeInputLogicFstName,
     flag_descriptions::kImeInputLogicFstDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeInputLogicFst)},
    {"enable-cros-ime-native-decoder", flag_descriptions::kImeNativeDecoderName,
     flag_descriptions::kImeNativeDecoderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kImeDecoderWithSandbox)},
    {"enable-cros-virtual-keyboard-bordered-key",
     flag_descriptions::kVirtualKeyboardBorderedKeyName,
     flag_descriptions::kVirtualKeyboardBorderedKeyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVirtualKeyboardBorderedKey)},
    {"enable-experimental-accessibility-switch-access",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccess)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
    {"enable-experimental-accessibility-chromevox-language-switching",
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxLanguageSwitchingName,
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxLanguageSwitchingDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::
             kEnableExperimentalAccessibilityChromeVoxLanguageSwitching)},
    {"enable-experimental-accessibility-chromevox-sub-node-language-"
     "switching",
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxSubNodeLanguageSwitchingName,
     flag_descriptions::
         kExperimentalAccessibilityChromeVoxSubNodeLanguageSwitchingDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::
             kEnableExperimentalAccessibilityChromeVoxSubNodeLanguageSwitching)},
    {"enable-experimental-kernel-vm-support",
     flag_descriptions::kKernelnextVMsName,
     flag_descriptions::kKernelnextVMsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kKernelnextVMs)},
#endif  // OS_CHROMEOS
#if !defined(OS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"enable-google-branded-context-menu",
     flag_descriptions::kGoogleBrandedContextMenuName,
     flag_descriptions::kGoogleBrandedContextMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kGoogleBrandedContextMenu)},
#endif  // !OS_ANDROID && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if defined(OS_MACOSX)
    {"enable-immersive-fullscreen-toolbar",
     flag_descriptions::kImmersiveFullscreenName,
     flag_descriptions::kImmersiveFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kImmersiveFullscreen)},
#endif  // OS_MACOSX
    {"rewrite-leveldb-on-deletion",
     flag_descriptions::kRewriteLevelDBOnDeletionName,
     flag_descriptions::kRewriteLevelDBOnDeletionDescription, kOsAll,
     FEATURE_VALUE_TYPE(leveldb::kLevelDBRewriteFeature)},
    {"passive-listener-default",
     flag_descriptions::kPassiveEventListenerDefaultName,
     flag_descriptions::kPassiveEventListenerDefaultDescription, kOsAll,
     MULTI_VALUE_TYPE(kPassiveListenersChoices)},
    {"document-passive-event-listeners",
     flag_descriptions::kPassiveDocumentEventListenersName,
     flag_descriptions::kPassiveDocumentEventListenersDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPassiveDocumentEventListeners)},
    {"document-passive-wheel-event-listeners",
     flag_descriptions::kPassiveDocumentWheelEventListenersName,
     flag_descriptions::kPassiveDocumentWheelEventListenersDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPassiveDocumentWheelEventListeners)},
    {"passive-event-listeners-due-to-fling",
     flag_descriptions::kPassiveEventListenersDueToFlingName,
     flag_descriptions::kPassiveEventListenersDueToFlingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPassiveEventListenersDueToFling)},
#if defined(OS_WIN)
    {"enable-experimental-fling-animation",
     flag_descriptions::kExperimentalFlingAnimationName,
     flag_descriptions::kExperimentalFlingAnimationDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kExperimentalFlingAnimation)},
#endif
    {"per-method-can-make-payment-quota",
     flag_descriptions::kPerMethodCanMakePaymentQuotaName,
     flag_descriptions::kPerMethodCanMakePaymentQuotaDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         payments::features::kWebPaymentsPerMethodCanMakePaymentQuota)},
    {"enable-web-payments-experimental-features",
     flag_descriptions::kWebPaymentsExperimentalFeaturesName,
     flag_descriptions::kWebPaymentsExperimentalFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsExperimentalFeatures)},
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
    {"enable-surfaces-for-videos",
     flag_descriptions::kUseSurfaceLayerForVideoName,
     flag_descriptions::kUseSurfaceLayerForVideoDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kUseSurfaceLayerForVideo)},
#if defined(OS_ANDROID)
    {"no-credit-card-abort", flag_descriptions::kNoCreditCardAbort,
     flag_descriptions::kNoCreditCardAbortDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNoCreditCardAbort)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS)
    {"arc-boot-completed-broadcast", flag_descriptions::kArcBootCompleted,
     flag_descriptions::kArcBootCompletedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kBootCompletedBroadcastFeature)},
    {"arc-custom-tabs-experiment",
     flag_descriptions::kArcCustomTabsExperimentName,
     flag_descriptions::kArcCustomTabsExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kCustomTabsExperimentFeature)},
    {"arc-documents-provider", flag_descriptions::kArcDocumentsProviderName,
     flag_descriptions::kArcDocumentsProviderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableDocumentsProviderInFilesAppFeature)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
    {"arc-print-spooler-experiment",
     flag_descriptions::kArcPrintSpoolerExperimentName,
     flag_descriptions::kArcPrintSpoolerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kPrintSpoolerExperimentFeature)},
    {"arc-usb-host", flag_descriptions::kArcUsbHostName,
     flag_descriptions::kArcUsbHostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbHostFeature)},
    {"arc-usb-storage-ui", flag_descriptions::kArcUsbStorageUIName,
     flag_descriptions::kArcUsbStorageUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbStorageUIFeature)},
    {"arc-vpn", flag_descriptions::kArcVpnName,
     flag_descriptions::kArcVpnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kVpnFeature)},
#endif  // OS_CHROMEOS
#if defined(OS_WIN)
    {"enable-winrt-sensor-implementation",
     flag_descriptions::kWinrtSensorsImplementationName,
     flag_descriptions::kWinrtSensorsImplementationDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWinrtSensorsImplementation)},
#endif
    {"enable-generic-sensor-extra-classes",
     flag_descriptions::kEnableGenericSensorExtraClassesName,
     flag_descriptions::kEnableGenericSensorExtraClassesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensorExtraClasses)},
    {"expensive-background-timer-throttling",
     flag_descriptions::kExpensiveBackgroundTimerThrottlingName,
     flag_descriptions::kExpensiveBackgroundTimerThrottlingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExpensiveBackgroundTimerThrottling)},
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    {"enable-cloud-printer-handler",
     flag_descriptions::kCloudPrinterHandlerName,
     flag_descriptions::kCloudPrinterHandlerDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCloudPrinterHandler)},
#endif

#if defined(OS_CHROMEOS)
    {ui_devtools::switches::kEnableUiDevTools,
     flag_descriptions::kUiDevToolsName,
     flag_descriptions::kUiDevToolsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ui_devtools::switches::kEnableUiDevTools)},
#endif  // defined(OS_CHROMEOS)

    {"enable-autofill-credit-card-ablation-experiment",
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDisplayName,
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillCreditCardAblationExperiment)},
    {"enable-autofill-credit-card-upload-editable-cardholder-name",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableCardholderNameName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableCardholderNameDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableCardholderName)},
    {"enable-autofill-credit-card-upload-editable-expiration-date",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableExpirationDate)},

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

#if defined(OS_CHROMEOS)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchCalibrationSetting)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
    {"android-files-in-files-app",
     flag_descriptions::kShowAndroidFilesInFilesAppName,
     flag_descriptions::kShowAndroidFilesInFilesAppDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kShowAndroidFilesInFilesApp,
         chromeos::switches::kHideAndroidFilesInFilesApp)},
    {"camera-system-web-app", flag_descriptions::kCameraSystemWebAppName,
     flag_descriptions::kCameraSystemWebAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCameraSystemWebApp)},
    {"crostini-gpu-support", flag_descriptions::kCrostiniGpuSupportName,
     flag_descriptions::kCrostiniGpuSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniGpuSupport)},
    {"crostini-usb-allow-unsupported",
     flag_descriptions::kCrostiniUsbAllowUnsupportedName,
     flag_descriptions::kCrostiniUsbAllowUnsupportedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUsbAllowUnsupported)},
    {"file-manager-feedback-panel",
     flag_descriptions::kFileManagerFeedbackPanelName,
     flag_descriptions::kFileManagerFeedbackPanelDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableFileManagerFeedbackPanel)},
    {"file-manager-piex-wasm", flag_descriptions::kFileManagerPiexWasmName,
     flag_descriptions::kFileManagerPiexWasmDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableFileManagerPiexWasm)},
    {"files-ng", flag_descriptions::kFilesNGName,
     flag_descriptions::kFilesNGDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFilesNG)},
#endif  // OS_CHROMEOS

#if defined(OS_WIN)
    {"gdi-text-printing", flag_descriptions::kGdiTextPrinting,
     flag_descriptions::kGdiTextPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kGdiTextPrinting)},
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
    {"new-usb-backend", flag_descriptions::kNewUsbBackendName,
     flag_descriptions::kNewUsbBackendDescription, kOsWin,
     FEATURE_VALUE_TYPE(device::kNewUsbBackend)},
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
    {"omnibox-search-engine-logo",
     flag_descriptions::kOmniboxSearchEngineLogoName,
     flag_descriptions::kOmniboxSearchEngineLogoDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxSearchEngineLogo,
                                    kOmniboxSearchEngineLogoFeatureVariations,
                                    "OmniboxSearchEngineLogo")},
#endif  // defined(OS_ANDROID)

    {"omnibox-on-device-head-suggestions",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProvider)},

    {"omnibox-on-focus-suggestions",
     flag_descriptions::kOmniboxOnFocusSuggestionsName,
     flag_descriptions::kOmniboxOnFocusSuggestionsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOnFocusSuggestions,
                                    kOmniboxOnFocusSuggestionsVariations,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-rich-entity-suggestions",
     flag_descriptions::kOmniboxRichEntitySuggestionsName,
     flag_descriptions::kOmniboxRichEntitySuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxRichEntitySuggestions)},

    {"omnibox-group-suggestions-by-search-vs-url",
     flag_descriptions::kOmniboxGroupSuggestionsBySearchVsUrlName,
     flag_descriptions::kOmniboxGroupSuggestionsBySearchVsUrlDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxGroupSuggestionsBySearchVsUrl)},

    {"omnibox-preserve-default-match-against-async-update",
     flag_descriptions::kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName,
     flag_descriptions::
         kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         omnibox::kOmniboxPreserveDefaultMatchAgainstAsyncUpdate)},

    {"omnibox-local-entity-suggestions",
     flag_descriptions::kOmniboxLocalEntitySuggestionsName,
     flag_descriptions::kOmniboxLocalEntitySuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxLocalEntitySuggestions)},

    {"omnibox-experimental-suggest-scoring",
     flag_descriptions::kOmniboxExperimentalSuggestScoringName,
     flag_descriptions::kOmniboxExperimentalSuggestScoringDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxExperimentalSuggestScoring)},

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    {"omnibox-experimental-keyword-mode",
     flag_descriptions::kOmniboxExperimentalKeywordModeName,
     flag_descriptions::kOmniboxExperimentalKeywordModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kExperimentalKeywordMode)},
    {"omnibox-loose-max-limit-on-dedicated-rows",
     flag_descriptions::kOmniboxLooseMaxLimitOnDedicatedRowsName,
     flag_descriptions::kOmniboxLooseMaxLimitOnDedicatedRowsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxLooseMaxLimitOnDedicatedRows)},
    {"omnibox-reverse-answers", flag_descriptions::kOmniboxReverseAnswersName,
     flag_descriptions::kOmniboxReverseAnswersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxReverseAnswers)},
    {"omnibox-short-bookmark-suggestions",
     flag_descriptions::kOmniboxShortBookmarkSuggestionsName,
     flag_descriptions::kOmniboxShortBookmarkSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxShortBookmarkSuggestions)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-tab-switch-suggestions-dedicated-row",
     flag_descriptions::kOmniboxTabSwitchSuggestionsDedicatedRowName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDedicatedRowDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestionsDedicatedRow)},
    {"omnibox-pedal-suggestions",
     flag_descriptions::kOmniboxPedalSuggestionsName,
     flag_descriptions::kOmniboxPedalSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPedalSuggestions)},
    {"omnibox-suggestion-transparency-options",
     flag_descriptions::kOmniboxSuggestionTransparencyOptionsName,
     flag_descriptions::kOmniboxSuggestionTransparencyOptionsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxSuggestionTransparencyOptions)},
    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescriptions, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kDocumentProvider,
                                    kOmniboxDocumentProviderVariations,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-autocomplete-titles",
     flag_descriptions::kOmniboxAutocompleteTitlesName,
     flag_descriptions::kOmniboxAutocompleteTitlesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kAutocompleteTitles)},
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

    {"enable-speculative-service-worker-start-on-query-input",
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputName,
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kSpeculativeServiceWorkerStartOnQueryInput)},

    // NOTE: This feature is generic and marked kOsAll but is used only in
    // CrOS for AndroidMessagesIntegration feature.
    {"enable-service-worker-long-running-message",
     flag_descriptions::kServiceWorkerLongRunningMessageName,
     flag_descriptions::kServiceWorkerLongRunningMessageDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kServiceWorkerLongRunningMessage)},

    {"enable-service-worker-on-ui", flag_descriptions::kServiceWorkerOnUIName,
     flag_descriptions::kServiceWorkerOnUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kServiceWorkerOnUI)},

#if defined(OS_CHROMEOS)
    {"scheduler-configuration", flag_descriptions::kSchedulerConfigurationName,
     flag_descriptions::kSchedulerConfigurationDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kSchedulerConfigurationChoices)},
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
    {"enable-command-line-on-non-rooted-devices",
     flag_descriptions::kEnableCommandLineOnNonRootedName,
     flag_descriptions::kEnableCommandLineOnNoRootedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCommandLineOnNonRooted)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"enable-revamped-context-menu",
     flag_descriptions::kEnableRevampedContextMenuName,
     flag_descriptions::kEnableRevampedContextMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRevampedContextMenu)},
#endif  // OS_ANDROID

    {"omnibox-display-title-for-current-url",
     flag_descriptions::kOmniboxDisplayTitleForCurrentUrlName,
     flag_descriptions::kOmniboxDisplayTitleForCurrentUrlDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kDisplayTitleForCurrentUrl)},

    {"force-color-profile", flag_descriptions::kForceColorProfileName,
     flag_descriptions::kForceColorProfileDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceColorProfileChoices)},

#if defined(OS_ANDROID)
    {"enable-webnfc", flag_descriptions::kEnableWebNfcName,
     flag_descriptions::kEnableWebNfcDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kWebNfc)},
#endif

    {"force-effective-connection-type",
     flag_descriptions::kForceEffectiveConnectionTypeName,
     flag_descriptions::kForceEffectiveConnectionTypeDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceEffectiveConnectionTypeChoices)},

    {"forced-colors", flag_descriptions::kForcedColorsName,
     flag_descriptions::kForcedColorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kForcedColors)},

    {"memlog", flag_descriptions::kMemlogName,
     flag_descriptions::kMemlogDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogModeChoices)},

    {"memlog-sampling-rate", flag_descriptions::kMemlogSamplingRateName,
     flag_descriptions::kMemlogSamplingRateDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogSamplingRateChoices)},

    {"memlog-stack-mode", flag_descriptions::kMemlogStackModeName,
     flag_descriptions::kMemlogStackModeDescription, kOsAll,
     MULTI_VALUE_TYPE(kMemlogStackModeChoices)},

    {"omnibox-ui-hide-steady-state-url-scheme",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlSchemeName,
     flag_descriptions::kOmniboxUIHideSteadyStateUrlSchemeDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kHideSteadyStateUrlScheme)},

    {"omnibox-ui-hide-steady-state-url-trivial-subdomains",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName,
     flag_descriptions::
         kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kHideSteadyStateUrlTrivialSubdomains)},

    {"omnibox-ui-hide-steady-state-url-path-query-and-ref",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlPathQueryAndRefName,
     flag_descriptions::kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kHideSteadyStateUrlPathQueryAndRef)},

    {"omnibox-ui-one-click-unelide",
     flag_descriptions::kOmniboxUIOneClickUnelideName,
     flag_descriptions::kOmniboxUIOneClickUnelideDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOneClickUnelide)},

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

    {"omnibox-ui-show-suggestion-favicons",
     flag_descriptions::kOmniboxUIShowSuggestionFaviconsName,
     flag_descriptions::kOmniboxUIShowSuggestionFaviconsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentShowSuggestionFavicons)},

    {"omnibox-ui-swap-title-and-url",
     flag_descriptions::kOmniboxUISwapTitleAndUrlName,
     flag_descriptions::kOmniboxUISwapTitleAndUrlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentSwapTitleAndUrl)},

    {"omnibox-zero-suggestions-on-ntp",
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPName,
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestionsOnNTP)},

    {"omnibox-zero-suggestions-on-ntp-realbox",
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPRealboxName,
     flag_descriptions::kOmniboxZeroSuggestionsOnNTPRealboxDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(omnibox::kZeroSuggestionsOnNTPRealbox)},

    {"omnibox-zero-suggestions-on-serp",
     flag_descriptions::kOmniboxZeroSuggestionsOnSERPName,
     flag_descriptions::kOmniboxZeroSuggestionsOnSERPDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestionsOnSERP)},

    {"omnibox-material-design-weather-icons",
     flag_descriptions::kOmniboxMaterialDesignWeatherIconsName,
     flag_descriptions::kOmniboxMaterialDesignWeatherIconsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxMaterialDesignWeatherIcons)},

    {"omnibox-disable-instant-extended-limit",
     flag_descriptions::kOmniboxDisableInstantExtendedLimitName,
     flag_descriptions::kOmniboxDisableInstantExtendedLimitDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxDisableInstantExtendedLimit)},

    {"use-new-accept-language-header",
     flag_descriptions::kUseNewAcceptLanguageHeaderName,
     flag_descriptions::kUseNewAcceptLanguageHeaderDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUseNewAcceptLanguageHeader)},

#if defined(OS_CHROMEOS)
    {"handwriting-gesture", flag_descriptions::kHandwritingGestureName,
     flag_descriptions::kHandwritingGestureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kHandwritingGesture)},
#endif  // OS_CHROMEOS

    {"network-service-in-process",
     flag_descriptions::kEnableNetworkServiceInProcessName,
     flag_descriptions::kEnableNetworkServiceInProcessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kNetworkServiceInProcess)},

    {"out-of-blink-cors", flag_descriptions::kEnableOutOfBlinkCorsName,
     flag_descriptions::kEnableOutOfBlinkCorsDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kOutOfBlinkCors)},

    {"cross-origin-isolation", flag_descriptions::kCrossOriginIsolationName,
     flag_descriptions::kCrossOriginIsolationDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kCrossOriginIsolation)},

    {"disable-keepalive-fetch", flag_descriptions::kDisableKeepaliveFetchName,
     flag_descriptions::kDisableKeepaliveFetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kDisableKeepaliveFetch)},

    {"prefetch-privacy-changes", flag_descriptions::kPrefetchPrivacyChangesName,
     flag_descriptions::kPrefetchPrivacyChangesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPrefetchPrivacyChanges)},

    {"prefetch-main-resource-network-isolation-key",
     flag_descriptions::kPrefetchMainResourceNetworkIsolationKeyName,
     flag_descriptions::kPrefetchMainResourceNetworkIsolationKeyDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         network::features::kPrefetchMainResourceNetworkIsolationKey)},

#if defined(OS_ANDROID)
    {"omnibox-spare-renderer", flag_descriptions::kOmniboxSpareRendererName,
     flag_descriptions::kOmniboxSpareRendererDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOmniboxSpareRenderer)},
#endif

#if defined(OS_CHROMEOS)
    {"double-tap-to-zoom-in-tablet-mode",
     flag_descriptions::kDoubleTapToZoomInTabletModeName,
     flag_descriptions::kDoubleTapToZoomInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDoubleTapToZoomInTabletMode)},
#endif  // defined(OS_CHROMEOS)

    {"tab-groups", flag_descriptions::kTabGroupsName,
     flag_descriptions::kTabGroupsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabGroups)},

    {"new-tabstrip-animation", flag_descriptions::kNewTabstripAnimationName,
     flag_descriptions::kNewTabstripAnimationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNewTabstripAnimation)},

    {"scrollable-tabstrip", flag_descriptions::kScrollableTabStripName,
     flag_descriptions::kScrollableTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kScrollableTabStrip)},

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

    {"click-to-open-pdf", flag_descriptions::kClickToOpenPDFName,
     flag_descriptions::kClickToOpenPDFDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClickToOpenPDFPlaceholder)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"direct-manipulation-stylus",
     flag_descriptions::kDirectManipulationStylusName,
     flag_descriptions::kDirectManipulationStylusDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kDirectManipulationStylus)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if !defined(OS_ANDROID)
    {"chrome-colors", flag_descriptions::kChromeColorsName,
     flag_descriptions::kChromeColorsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kChromeColors)},

    {"chrome-colors-custom-color-picker",
     flag_descriptions::kChromeColorsCustomColorPickerName,
     flag_descriptions::kChromeColorsCustomColorPickerDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kChromeColorsCustomColorPicker)},

    {"ntp-confirm-suggestion-removals",
     flag_descriptions::kNtpConfirmSuggestionRemovalsName,
     flag_descriptions::kNtpConfirmSuggestionRemovalsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kConfirmSuggestionRemovals)},

    {"ntp-customization-menu-v2",
     flag_descriptions::kNtpCustomizationMenuV2Name,
     flag_descriptions::kNtpCustomizationMenuV2Description, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCustomizationMenuV2)},

    {"ntp-dismiss-promos", flag_descriptions::kNtpDismissPromosName,
     flag_descriptions::kNtpDismissPromosDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kDismissPromos)},

    {"ntp-realbox", flag_descriptions::kNtpRealboxName,
     flag_descriptions::kNtpRealboxDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealbox)},

    {"ntp-realbox-match-omnibox-theme",
     flag_descriptions::kNtpRealboxMatchOmniboxThemeName,
     flag_descriptions::kNtpRealboxMatchOmniboxThemeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealboxMatchOmniboxTheme)},

    {"webui-a11y-enhancements", flag_descriptions::kWebUIA11yEnhancementsName,
     flag_descriptions::kWebUIA11yEnhancementsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebUIA11yEnhancements)},
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
#endif

    {"enable-new-download-backend",
     flag_descriptions::kEnableNewDownloadBackendName,
     flag_descriptions::kEnableNewDownloadBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kUseDownloadOfflineContentProvider)},

#if defined(OS_ANDROID)
    {"download-rename", flag_descriptions::kDownloadRenameName,
     flag_descriptions::kDownloadRenameDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDownloadRename)},
#endif

#if defined(OS_ANDROID)
    {"update-notification-scheduling-integration",
     flag_descriptions::kUpdateNotificationSchedulingIntegrationName,
     flag_descriptions::kUpdateNotificationSchedulingIntegrationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kUpdateNotificationSchedulingIntegration)},
#endif

    {"download-resumption-without-strong-validators",
     flag_descriptions::kDownloadResumptionWithoutStrongValidatorsName,
     flag_descriptions::kDownloadResumptionWithoutStrongValidatorsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         download::features::kAllowDownloadResumptionWithoutStrongValidators)},

    {"tab-hover-cards", flag_descriptions::kTabHoverCardsName,
     flag_descriptions::kTabHoverCardsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabHoverCards,
                                    kTabHoverCardsFeatureVariations,
                                    "TabHoverCards")},

    {"tab-hover-card-images", flag_descriptions::kTabHoverCardImagesName,
     flag_descriptions::kTabHoverCardImagesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabHoverCardImages)},

    {"stop-non-timers-in-background",
     flag_descriptions::kStopNonTimersInBackgroundName,
     flag_descriptions::kStopNonTimersInBackgroundDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kStopNonTimersInBackground)},

    {"stop-in-background", flag_descriptions::kStopInBackgroundName,
     flag_descriptions::kStopInBackgroundDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kStopInBackground)},

    {"enable-storage-pressure-ui", flag_descriptions::kStoragePressureUIName,
     flag_descriptions::kStoragePressureUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kStoragePressureUI)},

    {"enable-network-logging-to-file",
     flag_descriptions::kEnableNetworkLoggingToFileName,
     flag_descriptions::kEnableNetworkLoggingToFileDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kLogNetLog)},

    {"enable-mark-http-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         security_state::features::kMarkHttpAsFeature,
         kMarkHttpAsFeatureVariations,
         "HTTPReallyBadFinal")},

#if !defined(OS_ANDROID)
    {"enable-web-authentication-testing-api",
     flag_descriptions::kEnableWebAuthenticationTestingAPIName,
     flag_descriptions::kEnableWebAuthenticationTestingAPIDescription,
     kOsDesktop, SINGLE_VALUE_TYPE(switches::kEnableWebAuthTestingAPI)},
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"enable-web-authentication-ble-support",
     flag_descriptions::kEnableWebAuthenticationBleSupportName,
     flag_descriptions::kEnableWebAuthenticationBleSupportDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(features::kWebAuthBle)},
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"enable-web-authentication-cable-v2-support",
     flag_descriptions::kEnableWebAuthenticationCableV2SupportName,
     flag_descriptions::kEnableWebAuthenticationCableV2SupportDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(device::kWebAuthPhoneSupport)},
#endif  // !defined(OS_ANDROID)

    {"enable-viz-display-compositor",
     flag_descriptions::kVizDisplayCompositorName,
     flag_descriptions::kVizDisplayCompositorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVizDisplayCompositor)},

    {"simplify-https-indicator", flag_descriptions::kSimplifyHttpsIndicatorName,
     flag_descriptions::kSimplifyHttpsIndicatorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kSimplifyHttpsIndicator)},

#if defined(OS_WIN)
    {"enable-gpu-appcontainer", flag_descriptions::kEnableGpuAppcontainerName,
     flag_descriptions::kEnableGpuAppcontainerDescription, kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(
         service_manager::switches::kEnableGpuAppContainer,
         service_manager::switches::kDisableGpuAppContainer)},
#endif  // OS_WIN

    {"BundledConnectionHelp", flag_descriptions::kBundledConnectionHelpName,
     flag_descriptions::kBundledConnectionHelpDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBundledConnectionHelpFeature)},

    {"enable-query-in-omnibox", flag_descriptions::kQueryInOmniboxName,
     flag_descriptions::kQueryInOmniboxDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kQueryInOmnibox)},

    {"enable-viz-hit-test-surface-layer", flag_descriptions::kVizHitTestName,
     flag_descriptions::kVizHitTestDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableVizHitTestSurfaceLayer)},

#if BUILDFLAG(ENABLE_PDF)
#if defined(OS_CHROMEOS)
    {"pdf-annotations", flag_descriptions::kPdfAnnotations,
     flag_descriptions::kPdfAnnotationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPDFAnnotations)},
#endif  // defined(OS_CHROMEOS)

    {"pdf-form-save", flag_descriptions::kPdfFormSaveName,
     flag_descriptions::kPdfFormSaveDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kSaveEditedPDFForm)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
    {"harfbuzz-pdf-subsetter", flag_descriptions::kHarfBuzzPDFSubsetterName,
     flag_descriptions::kHarfBuzzPDFSubsetterDescription, kOsAll,
     FEATURE_VALUE_TYPE(printing::features::kHarfBuzzPDFSubsetter)},
#endif

    {"autofill-profile-client-validation",
     flag_descriptions::kAutofillProfileClientValidationName,
     flag_descriptions::kAutofillProfileClientValidationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillProfileClientValidation)},

    {"autofill-profile-server-validation",
     flag_descriptions::kAutofillProfileServerValidationName,
     flag_descriptions::kAutofillProfileServerValidationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillProfileServerValidation)},

    {"autofill-reject-company-birthyear",
     flag_descriptions::kAutofillRejectCompanyBirthyearName,
     flag_descriptions::kAutofillRejectCompanyBirthyearDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRejectCompanyBirthyear)},

    {"autofill-restrict-formless-form-extraction",
     flag_descriptions::kAutofillRestrictUnownedFieldsToFormlessCheckoutName,
     flag_descriptions::
         kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout)},

#if defined(OS_ANDROID)
    {"enable-start-surface", flag_descriptions::kStartSurfaceAndroidName,
     flag_descriptions::kStartSurfaceAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kStartSurfaceAndroid,
                                    kStartSurfaceAndroidVariations,
                                    "StartSurfaceAndroid")},

    {"enable-close-tab-suggestions",
     flag_descriptions::kCloseTabSuggestionsName,
     flag_descriptions::kCloseTabSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCloseTabSuggestions,
                                    kCloseTabSuggestionsStaleVariations,
                                    "CloseSuggestionsTab")},

    {"enable-horizontal-tab-switcher",
     flag_descriptions::kHorizontalTabSwitcherAndroidName,
     flag_descriptions::kHorizontalTabSwitcherAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHorizontalTabSwitcherAndroid)},

    {"enable-tab-grid-layout", flag_descriptions::kTabGridLayoutAndroidName,
     flag_descriptions::kTabGridLayoutAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabGridLayoutAndroid,
                                    kTabGridLayoutAndroidVariations,
                                    "TabGridLayoutAndroid")},

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
                                    "TabSwitcherOnReturn")},

    {"enable-tab-to-gts-animation",
     flag_descriptions::kTabToGTSAnimationAndroidName,
     flag_descriptions::kTabToGTSAnimationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabToGTSAnimation)},

    {"enable-tab-engagement-reporting",
     flag_descriptions::kTabEngagementReportingName,
     flag_descriptions::kTabEngagementReportingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabEngagementReportingAndroid)},
#endif  // OS_ANDROID

    {"enable-built-in-module-all", flag_descriptions::kBuiltInModuleAllName,
     flag_descriptions::kBuiltInModuleAllDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBuiltInModuleAll)},

    {"enable-display-locking", flag_descriptions::kEnableDisplayLockingName,
     flag_descriptions::kEnableDisplayLockingDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDisplayLocking)},

    {"enable-layout-ng", flag_descriptions::kEnableLayoutNGName,
     flag_descriptions::kEnableLayoutNGDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLayoutNG)},

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

    {"autofill-cache-query-responses",
     flag_descriptions::kAutofillCacheQueryResponsesName,
     flag_descriptions::kAutofillCacheQueryResponsesDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheQueryResponses)},

    {"autofill-enable-company-name",
     flag_descriptions::kAutofillEnableCompanyNameName,
     flag_descriptions::kAutofillEnableCompanyNameDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCompanyName)},

    {"autofill-enable-toolbar-status-chip",
     flag_descriptions::kAutofillEnableToolbarStatusChipName,
     flag_descriptions::kAutofillEnableToolbarStatusChipDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableToolbarStatusChip)},

    {"autofill-enforce-min-required-fields-for-heuristics",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForHeuristicsName,
     flag_descriptions::
         kAutofillEnforceMinRequiredFieldsForHeuristicsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics)},
    {"autofill-enforce-min-required-fields-for-query",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForQuery)},
    {"autofill-enforce-min-required-fields-for-upload",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForUpload)},
    {"autofill-no-local-save-on-upload-success",
     flag_descriptions::kAutofillNoLocalSaveOnUploadSuccessName,
     flag_descriptions::kAutofillNoLocalSaveOnUploadSuccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillNoLocalSaveOnUploadSuccess)},
    {"autofill-rich-metadata-queries",
     flag_descriptions::kAutofillRichMetadataQueriesName,
     flag_descriptions::kAutofillRichMetadataQueriesDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRichMetadataQueries)},
    {"enable-experimental-productivity-features",
     flag_descriptions::kExperimentalProductivityFeaturesName,
     flag_descriptions::kExperimentalProductivityFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalProductivityFeatures)},

#if defined(USE_AURA)
    {"touchpad-overscroll-history-navigation",
     flag_descriptions::kTouchpadOverscrollHistoryNavigationName,
     flag_descriptions::kTouchpadOverscrollHistoryNavigationDescription,
     kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTouchpadOverscrollHistoryNavigation)},
#endif

    {"disallow-unsafe-http-downloads",
     flag_descriptions::kDisallowUnsafeHttpDownloadsName,
     flag_descriptions::kDisallowUnsafeHttpDownloadsNameDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDisallowUnsafeHttpDownloads)},

    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         network::switches::kUnsafelyTreatInsecureOriginAsSecure,
         "")},

    {"treat-unsafe-downloads-as-active-content",
     flag_descriptions::kTreatUnsafeDownloadsAsActiveName,
     flag_descriptions::kTreatUnsafeDownloadsAsActiveDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTreatUnsafeDownloadsAsActive)},

#if defined(OS_CHROMEOS)
    {"enable-play-store-search", flag_descriptions::kEnablePlayStoreSearchName,
     flag_descriptions::kEnablePlayStoreSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnablePlayStoreAppSearch)},

    {"enable-app-data-search", flag_descriptions::kEnableAppDataSearchName,
     flag_descriptions::kEnableAppDataSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppDataSearch)},

    {"enable-app-grid-ghost", flag_descriptions::kEnableAppGridGhostName,
     flag_descriptions::kEnableAppGridGhostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppGridGhost)},

    {"enable-search-box-selection",
     flag_descriptions::kEnableSearchBoxSelectionName,
     flag_descriptions::kEnableSearchBoxSelectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableSearchBoxSelection)},
#endif  // OS_CHROMEOS

    {"enable-accessibility-expose-aria-annotations",
     flag_descriptions::kAccessibilityExposeARIAAnnotationsName,
     flag_descriptions::kAccessibilityExposeARIAAnnotationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAccessibilityExposeARIAAnnotations)},

    {"enable-accessibility-expose-display-none",
     flag_descriptions::kAccessibilityExposeDisplayNoneName,
     flag_descriptions::kAccessibilityExposeDisplayNoneDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAccessibilityExposeDisplayNone)},

    {"enable-accessibility-object-model",
     flag_descriptions::kEnableAccessibilityObjectModelName,
     flag_descriptions::kEnableAccessibilityObjectModelDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityObjectModel)},

#if defined(OS_ANDROID)
    {"cct-incognito", flag_descriptions::kCCTIncognitoName,
     flag_descriptions::kCCTIncognitoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognito)},
    {"cct-module", flag_descriptions::kCCTModuleName,
     flag_descriptions::kCCTModuleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModule)},
    {"cct-module-cache", flag_descriptions::kCCTModuleCacheName,
     flag_descriptions::kCCTModuleCacheDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCCTModuleCache,
                                    kCCTModuleCacheVariations,
                                    "CCTModule")},
    {"cct-module-custom-header", flag_descriptions::kCCTModuleCustomHeaderName,
     flag_descriptions::kCCTModuleCustomHeaderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModuleCustomHeader)},
    {"cct-module-custom-request-header",
     flag_descriptions::kCCTModuleCustomRequestHeaderName,
     flag_descriptions::kCCTModuleCustomRequestHeaderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModuleCustomRequestHeader)},
    {"cct-module-dex-loading", flag_descriptions::kCCTModuleDexLoadingName,
     flag_descriptions::kCCTModuleDexLoadingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModuleDexLoading)},
    {"cct-module-post-message", flag_descriptions::kCCTModulePostMessageName,
     flag_descriptions::kCCTModulePostMessageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModulePostMessage)},
    {"cct-module-use-intent-extras",
     flag_descriptions::kCCTModuleUseIntentExtrasName,
     flag_descriptions::kCCTModuleUseIntentExtrasDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModuleUseIntentExtras)},
#endif

#if !defined(OS_ANDROID)
    {"proactive-tab-freeze", flag_descriptions::kTabFreezeName,
     flag_descriptions::kTabFreezeDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kProactiveTabFreezeAndDiscard,
         kTabFreezeVariations,
         resource_coordinator::kProactiveTabFreezeAndDiscardFeatureName)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-arc-cups-api", flag_descriptions::kArcCupsApiName,
     flag_descriptions::kArcCupsApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kArcCupsApi)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"enable-native-controls",
     flag_descriptions::kEnableVideoPlayerNativeControlsName,
     flag_descriptions::kEnableVideoPlayerNativeControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kVideoPlayerNativeControls)},
#endif

#if defined(OS_ANDROID)
    {"background-task-component-update",
     flag_descriptions::kBackgroundTaskComponentUpdateName,
     flag_descriptions::kBackgroundTaskComponentUpdateDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackgroundTaskComponentUpdate)},
#endif

#if defined(OS_CHROMEOS)
    {"smart-text-selection", flag_descriptions::kSmartTextSelectionName,
     flag_descriptions::kSmartTextSelectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kSmartTextSelectionFeature)},
#endif  // OS_CHROMEOS

    {"allow-sxg-certs-without-extension",
     flag_descriptions::kAllowSignedHTTPExchangeCertsWithoutExtensionName,
     flag_descriptions::
         kAllowSignedHTTPExchangeCertsWithoutExtensionDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kAllowSignedHTTPExchangeCertsWithoutExtension)},

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
     flag_descriptions::kEnableAutofillAccountWalletStorageDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableAccountWalletStorage)},

#if defined(OS_CHROMEOS)
    {"enable-zero-state-suggestions",
     flag_descriptions::kEnableZeroStateSuggestionsName,
     flag_descriptions::kEnableZeroStateSuggestionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableZeroStateSuggestions)},
    {"enable-zero-state-app-reinstall-suggestions",
     flag_descriptions::kEnableAppReinstallZeroStateName,
     flag_descriptions::kEnableAppReinstallZeroStateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppReinstallZeroState)},
#endif  // OS_CHROMEOS

    {"enable-sync-device-info-in-transport-mode",
     flag_descriptions::kSyncDeviceInfoInTransportModeName,
     flag_descriptions::kSyncDeviceInfoInTransportModeDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncDeviceInfoInTransportMode)},

    {"enable-lookalike-url-navigation-suggestions",
     flag_descriptions::kLookalikeUrlNavigationSuggestionsName,
     flag_descriptions::kLookalikeUrlNavigationSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLookalikeUrlNavigationSuggestionsUI)},

    {"enable-resampling-input-events",
     flag_descriptions::kEnableResamplingInputEventsName,
     flag_descriptions::kEnableResamplingInputEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kResamplingInputEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingInputEvents")},

    {"enable-resampling-scroll-events",
     flag_descriptions::kEnableResamplingScrollEventsName,
     flag_descriptions::kEnableResamplingScrollEventsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kResamplingScrollEvents,
                                    kResamplingInputEventsFeatureVariations,
                                    "ResamplingScrollEvents")},

    {"enable-filtering-scroll-events",
     flag_descriptions::kFilteringScrollPredictionName,
     flag_descriptions::kFilteringScrollPredictionDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kFilteringScrollPrediction,
                                    kFilteringPredictionFeatureVariations,
                                    "FilteringScrollPrediction")},

    {"compositor-threaded-scrollbar-scrolling",
     flag_descriptions::kCompositorThreadedScrollbarScrollingName,
     flag_descriptions::kCompositorThreadedScrollbarScrollingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kCompositorThreadedScrollbarScrolling)},

#if defined(OS_CHROMEOS)
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
    {"happiness-tracking-surveys-for-desktop",
     flag_descriptions::kHappinessTrackingSurveysForDesktopName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktop)},

    {"happiness-tracking-surveys-for-desktop-demo",
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDemoDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktopDemo)},
#endif  // !defined(OS_ANDROID)

    {"enable-service-worker-imported-script-update-check",
     flag_descriptions::kServiceWorkerImportedScriptUpdateCheckName,
     flag_descriptions::kServiceWorkerImportedScriptUpdateCheckDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kServiceWorkerImportedScriptUpdateCheck)},

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"use-multilogin-endpoint", flag_descriptions::kUseMultiloginEndpointName,
     flag_descriptions::kUseMultiloginEndpointDescription,
     kOsMac | kOsWin | kOsLinux, FEATURE_VALUE_TYPE(kUseMultiloginEndpoint)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-usbguard", flag_descriptions::kUsbguardName,
     flag_descriptions::kUsbguardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kUsbguard)},
    {"enable-fs-nosymfollow", flag_descriptions::kFsNosymfollowName,
     flag_descriptions::kFsNosymfollowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFsNosymfollow)},
    {"enable-arc-unified-audio-focus",
     flag_descriptions::kEnableArcUnifiedAudioFocusName,
     flag_descriptions::kEnableArcUnifiedAudioFocusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUnifiedAudioFocusFeature)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescription, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoices)},
#endif
#if defined(OS_ANDROID)
    {"android-site-settings-ui-refresh",
     flag_descriptions::kAndroidSiteSettingsUIRefreshName,
     flag_descriptions::kAndroidSiteSettingsUIRefreshDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidSiteSettingsUIRefresh)},
    {"draw-vertically-edge-to-edge",
     flag_descriptions::kDrawVerticallyEdgeToEdgeName,
     flag_descriptions::kDrawVerticallyEdgeToEdgeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDrawVerticallyEdgeToEdge)},
#endif
#if defined(OS_ANDROID)
    {"enable-ephemeral-tab", flag_descriptions::kEphemeralTabName,
     flag_descriptions::kEphemeralTabDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEphemeralTab)},
    {"enable-ephemeral-tab-bottom-sheet",
     flag_descriptions::kEphemeralTabUsingBottomSheetName,
     flag_descriptions::kEphemeralTabUsingBottomSheetDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEphemeralTabUsingBottomSheet)},
    {"overlay-new-layout", flag_descriptions::kOverlayNewLayoutName,
     flag_descriptions::kOverlayNewLayoutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOverlayNewLayout)},
    {"safe-browsing-use-local-blacklists-v2",
     flag_descriptions::kSafeBrowsingUseLocalBlacklistsV2Name,
     flag_descriptions::kSafeBrowsingUseLocalBlacklistsV2Description,
     kOsAndroid, FEATURE_VALUE_TYPE(safe_browsing::kUseLocalBlacklistsV2)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"enable-assistant-dsp", flag_descriptions::kEnableGoogleAssistantDspName,
     flag_descriptions::kEnableGoogleAssistantDspDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kEnableDspHotword)},

    {"enable-assistant-app-support",
     flag_descriptions::kEnableAssistantAppSupportName,
     flag_descriptions::kEnableAssistantAppSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantAppSupport)},

    {"enable-assistant-media-session-integration",
     flag_descriptions::kEnableAssistantMediaSessionIntegrationName,
     flag_descriptions::kEnableAssistantMediaSessionIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableMediaSessionIntegration)},

    {"enable-assistant-launcher-ui",
     flag_descriptions::kEnableAssistantLauncherUIName,
     flag_descriptions::kEnableAssistantLauncherUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAssistantLauncherUI)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    {"click-to-call-open-dialer-directly",
     flag_descriptions::kClickToCallOpenDialerDirectlyName,
     flag_descriptions::kClickToCallOpenDialerDirectlyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kClickToCallOpenDialerDirectly)},

    {"click-to-call-receiver", flag_descriptions::kClickToCallReceiverName,
     flag_descriptions::kClickToCallReceiverDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(kClickToCallReceiver)},
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
    {"click-to-call-context-menu-selected-text",
     flag_descriptions::kClickToCallContextMenuForSelectedTextName,
     flag_descriptions::kClickToCallContextMenuForSelectedTextDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(kClickToCallContextMenuForSelectedText)},

    {"click-to-call-ui", flag_descriptions::kClickToCallUIName,
     flag_descriptions::kClickToCallUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kClickToCallUI)},

    {"click-to-call-detection-v2",
     flag_descriptions::kClickToCallDetectionV2Name,
     flag_descriptions::kClickToCallDetectionV2Description, kOsDesktop,
     FEATURE_VALUE_TYPE(kClickToCallDetectionV2)},
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"remote-copy-receiver", flag_descriptions::kRemoteCopyReceiverName,
     flag_descriptions::kRemoteCopyReceiverDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kRemoteCopyReceiver)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

    {"shared-clipboard-receiver",
     flag_descriptions::kSharedClipboardReceiverName,
     flag_descriptions::kSharedClipboardReceiverDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharedClipboardReceiver)},

    {"shared-clipboard-ui", flag_descriptions::kSharedClipboardUIName,
     flag_descriptions::kSharedClipboardUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharedClipboardUI)},

    {"enable-ambient-authentication-in-incognito",
     flag_descriptions::kEnableAmbientAuthenticationInIncognitoName,
     flag_descriptions::kEnableAmbientAuthenticationInIncognitoDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAmbientAuthenticationInIncognito)},

    {"enable-ambient-authentication-in-guest-session",
     flag_descriptions::kEnableAmbientAuthenticationInGuestSessionName,
     flag_descriptions::kEnableAmbientAuthenticationInGuestSessionDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableAmbientAuthenticationInGuestSession)},

    {"enable-send-tab-to-self-when-signed-in",
     flag_descriptions::kSendTabToSelfWhenSignedInName,
     flag_descriptions::kSendTabToSelfWhenSignedInDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfWhenSignedIn)},

    {"enable-data-reduction-proxy-with-network-service",
     flag_descriptions::kEnableDataReductionProxyNetworkServiceName,
     flag_descriptions::kEnableDataReductionProxyNetworkServiceDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(data_reduction_proxy::features::
                            kDataReductionProxyEnabledWithNetworkService)},

    {"enable-sharing-device-registration",
     flag_descriptions::kSharingDeviceRegistrationName,
     flag_descriptions::kSharingDeviceRegistrationDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingDeviceRegistration)},

    {"sharing-derive-vapid-key", flag_descriptions::kSharingDeriveVapidKeyName,
     flag_descriptions::kSharingDeriveVapidKeyDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingDeriveVapidKey)},

    {"sharing-use-device-info", flag_descriptions::kSharingUseDeviceInfoName,
     flag_descriptions::kSharingUseDeviceInfoDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingUseDeviceInfo)},

    {"sharing-peer-connection-receiver",
     flag_descriptions::kSharingPeerConnectionReceiverName,
     flag_descriptions::kSharingPeerConnectionReceiverDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingPeerConnectionReceiver)},

    {"sharing-peer-connection-sender",
     flag_descriptions::kSharingPeerConnectionSenderName,
     flag_descriptions::kSharingPeerConnectionSenderDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingPeerConnectionSender)},

    {"sharing-rename-devices", flag_descriptions::kSharingRenameDevicesName,
     flag_descriptions::kSharingRenameDevicesDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSharingRenameDevices)},

#if defined(OS_CHROMEOS)
    {"discover-app", flag_descriptions::kEnableDiscoverAppName,
     flag_descriptions::kEnableDiscoverAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDiscoverApp)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"ash-enable-pip-rounded-corners",
     flag_descriptions::kAshEnablePipRoundedCornersName,
     flag_descriptions::kAshEnablePipRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipRoundedCorners)},

    {"ash-swap-side-volume-buttons-for-orientation",
     flag_descriptions::kAshSwapSideVolumeButtonsForOrientationName,
     flag_descriptions::kAshSwapSideVolumeButtonsForOrientationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSwapSideVolumeButtonsForOrientation)},
#endif  // defined(OS_CHROMEOS)
    {"google-password-manager", flag_descriptions::kGooglePasswordManagerName,
     flag_descriptions::kGooglePasswordManagerDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kGooglePasswordManager)},

    {"enable-implicit-root-scroller",
     flag_descriptions::kEnableImplicitRootScrollerName,
     flag_descriptions::kEnableImplicitRootScrollerDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kImplicitRootScroller)},

    {"enable-cssom-view-scroll-coordinates",
     flag_descriptions::kEnableCSSOMViewScrollCoordinatesName,
     flag_descriptions::kEnableCSSOMViewScrollCoordinatesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCSSOMViewScrollCoordinates)},

    {"enable-text-fragment-anchor",
     flag_descriptions::kEnableTextFragmentAnchorName,
     flag_descriptions::kEnableTextFragmentAnchorDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kTextFragmentAnchor)},

#if defined(OS_CHROMEOS)
    {"enable-assistant-stereo-input",
     flag_descriptions::kEnableGoogleAssistantStereoInputName,
     flag_descriptions::kEnableGoogleAssistantStereoInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::assistant::features::kEnableStereoAudioInput)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
    {"force-enable-system-aec", flag_descriptions::kForceEnableSystemAecName,
     flag_descriptions::kForceEnableSystemAecDescription, kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kForceEnableSystemAec)},
#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_MACOSX)
    {"enable-custom-mac-paper-sizes",
     flag_descriptions::kEnableCustomMacPaperSizesName,
     flag_descriptions::kEnableCustomMacPaperSizesDescription, kOsMac,
     FEATURE_VALUE_TYPE(printing::features::kEnableCustomMacPaperSizes)},
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    {"enable-reopen-tab-in-product-help",
     flag_descriptions::kReopenTabInProductHelpName,
     flag_descriptions::kReopenTabInProductHelpDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHReopenTabFeature)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

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

    {"enable-paint-holding", flag_descriptions::kPaintHoldingName,
     flag_descriptions::kPaintHoldingDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPaintHolding)},

#if !defined(OS_ANDROID)
    {"app-management", flag_descriptions::kAppManagementName,
     flag_descriptions::kAppManagementDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAppManagement)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"app-service-ash", flag_descriptions::kAppServiceAshName,
     flag_descriptions::kAppServiceAshDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceAsh)},

    {"app-service-instance-registry",
     flag_descriptions::kAppServiceInstanceRegistryName,
     flag_descriptions::kAppServiceInstanceRegistryDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceInstanceRegistry)},

    {"app-service-intent-handling",
     flag_descriptions::kAppServiceIntentHandlingName,
     flag_descriptions::kAppServiceIntentHandlingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceIntentHandling)},

    {"app-service-shelf", flag_descriptions::kAppServiceShelfName,
     flag_descriptions::kAppServiceShelfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAppServiceShelf)},

    {"ash-enable-overview-rounded-corners",
     flag_descriptions::kAshEnableOverviewRoundedCornersName,
     flag_descriptions::kAshEnableOverviewRoundedCornersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableOverviewRoundedCorners)},

    {"ash-swiping-from-left-edge-to-go-back",
     flag_descriptions::kAshSwipingFromLeftEdgeToGoBackName,
     flag_descriptions::kAshSwipingFromLeftEdgeToGoBackDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSwipingFromLeftEdgeToGoBack)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},

    {"ash-drag-window-from-shelf",
     flag_descriptions::kAshDragWindowFromShelfName,
     flag_descriptions::kAshDragWindowFromShelfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragFromShelfToHomeOrOverview)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    {"d3d11-video-decoder", flag_descriptions::kD3D11VideoDecoderName,
     flag_descriptions::kD3D11VideoDecoderDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kD3D11VideoDecoder)},
#endif

#if defined(OS_ANDROID)
    {"autofill-assistant-chrome-entry",
     flag_descriptions::kAutofillAssistantChromeEntryName,
     flag_descriptions::kAutofillAssistantChromeEntryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill_assistant::features::kAutofillAssistantChromeEntry)},

    {"autofill-assistant-direct-actions",
     flag_descriptions::kAutofillAssistantDirectActionsName,
     flag_descriptions::kAutofillAssistantDirectActionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill_assistant::features::kAutofillAssistantDirectActions)},
#endif  // defined(OS_ANDROID)

    {"disable-best-effort-tasks",
     flag_descriptions::kDisableBestEffortTasksName,
     flag_descriptions::kDisableBestEffortTasksDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableBestEffortTasks)},
    {"enable-sync-uss-passwords",
     flag_descriptions::kEnableSyncUSSPasswordsName,
     flag_descriptions::kEnableSyncUSSPasswordsDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSPasswords)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
    {"web-contents-occlusion", flag_descriptions::kWebContentsOcclusionName,
     flag_descriptions::kWebContentsOcclusionDescription,
     kOsWin | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebContentsOcclusion)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    {"manual-password-generation-android",
     flag_descriptions::kManualPasswordGenerationAndroidName,
     flag_descriptions::kManualPasswordGenerationAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kManualPasswordGenerationAndroid)},
    {"mobile-identity-consistency",
     flag_descriptions::kMobileIdentityConsistencyName,
     flag_descriptions::kMobileIdentityConsistencyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(signin::kMiceFeature)},
#endif  // defined(OS_ANDROID)

    {"autofill-use-improved-label-disambiguation",
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationName,
     flag_descriptions::kAutofillUseImprovedLabelDisambiguationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseImprovedLabelDisambiguation)},

#if defined(OS_ANDROID)
    {"cct-target-translate-language",
     flag_descriptions::kCCTTargetTranslateLanguageName,
     flag_descriptions::kCCTTargetTranslateLanguageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTTargetTranslateLanguage)},
#endif

    {"enable-built-in-module-infra", flag_descriptions::kBuiltInModuleInfraName,
     flag_descriptions::kBuiltInModuleInfraDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBuiltInModuleInfra)},

    {"enable-built-in-module-kv-storage",
     flag_descriptions::kBuiltInModuleKvStorageName,
     flag_descriptions::kBuiltInModuleKvStorageDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBuiltInModuleKvStorage)},

    {"native-file-system-api", flag_descriptions::kNativeFileSystemAPIName,
     flag_descriptions::kNativeFileSystemAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kNativeFileSystemAPI)},

    {"file-handling-api", flag_descriptions::kFileHandlingAPIName,
     flag_descriptions::kFileHandlingAPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFileHandlingAPI)},

#if !defined(OS_ANDROID)
    {"enable-intent-picker", flag_descriptions::kIntentPickerName,
     flag_descriptions::kIntentPickerDescription, kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kIntentPicker)},
#endif  // !defined(OS_ANDROID)

#if defined(TOOLKIT_VIEWS)
    {"installable-ink-drop", flag_descriptions::kInstallableInkDropName,
     flag_descriptions::kInstallableInkDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::kInstallableInkDropFeature)},
#endif  // defined(TOOLKIT_VIEWS)

#if defined(OS_CHROMEOS)
    {"enable-assistant-launcher-integration",
     flag_descriptions::kEnableAssistantLauncherIntegrationName,
     flag_descriptions::kEnableAssistantLauncherIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAssistantSearch)},
#endif  // OS_CHROMEOS

    {"autofill-enable-local-card-migration-for-non-sync-user",
     flag_descriptions::kAutofillEnableLocalCardMigrationForNonSyncUserName,
     flag_descriptions::
         kAutofillEnableLocalCardMigrationForNonSyncUserDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableLocalCardMigrationForNonSyncUser)},

#if defined(TOOLKIT_VIEWS)
    {"enable-md-rounded-corners-on-dialogs",
     flag_descriptions::kEnableMDRoundedCornersOnDialogsName,
     flag_descriptions::kEnableMDRoundedCornersOnDialogsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(views::features::kEnableMDRoundedCornersOnDialogs)},
#endif  // defined(TOOLKIT_VIEWS)

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(ENABLE_PLUGINS)
    {"mime-handler-view-in-cross-process-frame",
     flag_descriptions::kMimeHandlerViewInCrossProcessFrameName,
     flag_descriptions::kMimeHandlerViewInCrossProcessFrameDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMimeHandlerViewInCrossProcessFrame)},
#endif

    {"strict-origin-isolation", flag_descriptions::kStrictOriginIsolationName,
     flag_descriptions::kStrictOriginIsolationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kStrictOriginIsolation)},

    {"autofill-no-local-save-on-unmask-success",
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessName,
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillNoLocalSaveOnUnmaskSuccess)},

#if defined(OS_ANDROID)
    {"enable-logging-js-console-messages",
     flag_descriptions::kLogJsConsoleMessagesName,
     flag_descriptions::kLogJsConsoleMessagesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLogJsConsoleMessages)},
#endif  // OS_ANDROID

    {"enable-skia-renderer", flag_descriptions::kSkiaRendererName,
     flag_descriptions::kSkiaRendererDescription,
     kOsLinux | kOsWin | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kUseSkiaRenderer)},

#if defined(OS_CHROMEOS)
    {"allow-ambient-eq", flag_descriptions::kAllowAmbientEQName,
     flag_descriptions::kAllowAmbientEQDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAllowAmbientEQ)},

    {"allow-disable-mouse-acceleration",
     flag_descriptions::kAllowDisableMouseAccelerationName,
     flag_descriptions::kAllowDisableMouseAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAllowDisableMouseAcceleration)},

    {"enable-streamlined-usb-printer-setup",
     flag_descriptions::kStreamlinedUsbPrinterSetupName,
     flag_descriptions::kStreamlinedUsbPrinterSetupDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kStreamlinedUsbPrinterSetup)},

    {"enable-media-session-notifications",
     flag_descriptions::kMediaSessionNotificationsName,
     flag_descriptions::kMediaSessionNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMediaSessionNotification)},

    {"enable-neural-stylus-palm-rejection",
     flag_descriptions::kEnableNeuralStylusPalmRejectionName,
     flag_descriptions::kEnableNeuralStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableNeuralPalmDetectionFilter)},

    {"enable-heuristic-stylus-palm-rejection",
     flag_descriptions::kEnableHeuristicStylusPalmRejectionName,
     flag_descriptions::kEnableHeuristicStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeuristicPalmDetectionFilter)},

    {"enable-hide-arc-media-notifications",
     flag_descriptions::kHideArcMediaNotificationsName,
     flag_descriptions::kHideArcMediaNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHideArcMediaNotifications)},

    {"enable-cups-printers-ui-overhaul",
     flag_descriptions::kCupsPrintersUiOverhaulName,
     flag_descriptions::kCupsPrintersUiOverhaulDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCupsPrintersUiOverhaul)},

    {"reduce-display-notifications",
     flag_descriptions::kReduceDisplayNotificationsName,
     flag_descriptions::kReduceDisplayNotificationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kReduceDisplayNotifications)},

    {"use-search-click-for-right-click",
     flag_descriptions::kUseSearchClickForRightClickName,
     flag_descriptions::kUseSearchClickForRightClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kUseSearchClickForRightClick)},

    {"enable-print-server-ui", flag_descriptions::kPrintServerUiName,
     flag_descriptions::kPrintServerUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kPrintServerUi)},
#endif  // OS_CHROMEOS

    {"autofill-off-no-server-data",
     flag_descriptions::kAutofillOffNoServerDataName,
     flag_descriptions::kAutofillOffNoServerDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillOffNoServerData)},

    {"enable-portals", flag_descriptions::kEnablePortalsName,
     flag_descriptions::kEnablePortalsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPortals)},
    {"enable-autofill-credit-card-authentication",
     flag_descriptions::kEnableAutofillCreditCardAuthenticationName,
     flag_descriptions::kEnableAutofillCreditCardAuthenticationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardAuthentication)},

#if defined(OS_MACOSX)
    {"mac-system-media-permissions-info-ui",
     flag_descriptions::kMacSystemMediaPermissionsInfoUiName,
     flag_descriptions::kMacSystemMediaPermissionsInfoUiDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSystemMediaPermissionsInfoUi)},
#endif  // defined(OS_MACOSX)

    {"storage-access-api", flag_descriptions::kStorageAccessAPIName,
     flag_descriptions::kStorageAccessAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kStorageAccessAPI)},

    {"same-site-by-default-cookies",
     flag_descriptions::kSameSiteByDefaultCookiesName,
     flag_descriptions::kSameSiteByDefaultCookiesDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kSameSiteByDefaultCookies)},

    {"enable-removing-all-third-party-cookies",
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesName,
     flag_descriptions::kEnableRemovingAllThirdPartyCookiesDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         browsing_data::features::kEnableRemovingAllThirdPartyCookies)},

    {"enable-send-tab-to-self-broadcast",
     flag_descriptions::kSendTabToSelfBroadcastName,
     flag_descriptions::kSendTabToSelfBroadcastDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfBroadcast)},

    {"cookies-without-same-site-must-be-secure",
     flag_descriptions::kCookiesWithoutSameSiteMustBeSecureName,
     flag_descriptions::kCookiesWithoutSameSiteMustBeSecureDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kCookiesWithoutSameSiteMustBeSecure)},

#if !defined(OS_ANDROID)
    {"enterprise-reporting-in-browser",
     flag_descriptions::kEnterpriseReportingInBrowserName,
     flag_descriptions::kEnterpriseReportingInBrowserDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnterpriseReportingInBrowser)},
#endif  // !defined(OS_ANDROID)

    {"enable-autofill-do-not-migrate-unsupported-local-cards",
     flag_descriptions::kEnableAutofillDoNotMigrateUnsupportedLocalCardsName,
     flag_descriptions::
         kEnableAutofillDoNotMigrateUnsupportedLocalCardsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillDoNotMigrateUnsupportedLocalCards)},
    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription, kOsMac | kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},

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

    {"allow-popups-during-page-unload",
     flag_descriptions::kAllowPopupsDuringPageUnloadName,
     flag_descriptions::kAllowPopupsDuringPageUnloadDescription,
     kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(features::kAllowPopupsDuringPageUnload)},

#if defined(OS_CHROMEOS)
    {"enable-advanced-ppd-attributes",
     flag_descriptions::kEnableAdvancedPpdAttributesName,
     flag_descriptions::kEnableAdvancedPpdAttributesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(printing::kAdvancedPpdAttributes)},
#endif  // defined(OS_CHROMEOS)

    {"allow-sync-xhr-in-page-dismissal",
     flag_descriptions::kAllowSyncXHRInPageDismissalName,
     flag_descriptions::kAllowSyncXHRInPageDismissalDescription,
     kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(blink::features::kAllowSyncXHRInPageDismissal)},

    {"form-controls-refresh", flag_descriptions::kFormControlsRefreshName,
     flag_descriptions::kFormControlsRefreshDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFormControlsRefresh)},

#if defined(OS_CHROMEOS)
    {"auto-screen-brightness", flag_descriptions::kAutoScreenBrightnessName,
     flag_descriptions::kAutoScreenBrightnessDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAutoScreenBrightness)},
    {"sync-wifi-configurations", flag_descriptions::kSyncWifiConfigurationsName,
     flag_descriptions::kSyncWifiConfigurationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(switches::kSyncWifiConfigurations)},
#endif  // defined(OS_CHROMEOS)

    {"audio-worklet-realtime-thread",
     flag_descriptions::kAudioWorkletRealtimeThreadName,
     flag_descriptions::kAudioWorkletRealtimeThreadDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAudioWorkletRealtimeThread)},

#if defined(OS_CHROMEOS)
    {"release-notes", flag_descriptions::kReleaseNotesName,
     flag_descriptions::kReleaseNotesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kReleaseNotes)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"smart-dim-model-v3", flag_descriptions::kSmartDimModelV3Name,
     flag_descriptions::kSmartDimModelV3Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSmartDimModelV3)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"split-settings", flag_descriptions::kSplitSettingsName,
     flag_descriptions::kSplitSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSplitSettings)},
#endif  // defined(OS_CHROMEOS)

    {"privacy-settings-redesign",
     flag_descriptions::kPrivacySettingsRedesignName,
     flag_descriptions::kPrivacySettingsRedesignDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kPrivacySettingsRedesign)},

#if defined(OS_CHROMEOS)
    {"gesture-properties-dbus-service",
     flag_descriptions::kEnableGesturePropertiesDBusServiceName,
     flag_descriptions::kEnableGesturePropertiesDBusServiceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kGesturePropertiesDBusService)},
#endif  // defined(OS_CHROMEOS)

    {"cookie-deprecation-messages",
     flag_descriptions::kCookieDeprecationMessagesName,
     flag_descriptions::kCookieDeprecationMessagesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCookieDeprecationMessages)},

    {"enable-caption-settings", flag_descriptions::kCaptionSettingsName,
     flag_descriptions::kCaptionSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCaptionSettings)},

    {"ev-details-in-page-info", flag_descriptions::kEvDetailsInPageInfoName,
     flag_descriptions::kEvDetailsInPageInfoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEvDetailsInPageInfo)},

    {"security-interstitials-dark-mode",
     flag_descriptions::kSecurityInterstitialsDarkModeName,
     flag_descriptions::kSecurityInterstitialsDarkModeDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         security_interstitials::kSecurityInterstitialsDarkMode)},

    {"enable-autofill-credit-card-upload-feedback",
     flag_descriptions::kEnableAutofillCreditCardUploadFeedbackName,
     flag_descriptions::kEnableAutofillCreditCardUploadFeedbackDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardUploadFeedback)},

    {"periodic-background-sync", flag_descriptions::kPeriodicBackgroundSyncName,
     flag_descriptions::kPeriodicBackgroundSyncDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPeriodicBackgroundSync)},

    {"font-src-local-matching", flag_descriptions::kFontSrcLocalMatchingName,
     flag_descriptions::kFontSrcLocalMatchingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFontSrcLocalMatching)},

#if defined(OS_CHROMEOS)
    {"enable-parental-controls-settings",
     flag_descriptions::kEnableParentalControlsSettingsName,
     flag_descriptions::kEnableParentalControlsSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kParentalControlsSettings)},
#endif  // defined(OS_CHROMEOS)

    {"mouse-subframe-no-implicit-capture",
     flag_descriptions::kMouseSubframeNoImplicitCaptureName,
     flag_descriptions::kMouseSubframeNoImplicitCaptureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kMouseSubframeNoImplicitCapture)},

#if defined(OS_ANDROID)
    {"touch-to-fill", flag_descriptions::kAutofillTouchToFillName,
     flag_descriptions::kAutofillTouchToFillDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillTouchToFill)},
#endif  // defined(OS_ANDROID)

    {"enable-sync-uss-nigori", flag_descriptions::kEnableSyncUSSNigoriName,
     flag_descriptions::kEnableSyncUSSNigoriDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSNigori)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"global-media-controls", flag_descriptions::kGlobalMediaControlsName,
     flag_descriptions::kGlobalMediaControlsDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControls)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_WIN)
    {"win-use-native-spellchecker",
     flag_descriptions::kWinUseBrowserSpellCheckerName,
     flag_descriptions::kWinUseBrowserSpellCheckerDescription, kOsWin,
     FEATURE_VALUE_TYPE(spellcheck::kWinUseBrowserSpellChecker)},
#endif  // BUILDFLAG(ENABLE_SPELLCHECK) && defined(OS_WIN)

    {"safety-tips", flag_descriptions::kSafetyTipName,
     flag_descriptions::kSafetyTipDescription, kOsAll,
     FEATURE_VALUE_TYPE(security_state::features::kSafetyTipUI)},

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"animated-avatar-button", flag_descriptions::kAnimatedAvatarButtonName,
     flag_descriptions::kAnimatedAvatarButtonDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kAnimatedAvatarButton)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if defined(OS_CHROMEOS)
    {"crostini-webui-installer", flag_descriptions::kCrostiniWebUIInstallerName,
     flag_descriptions::kCrostiniWebUIInstallerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniWebUIInstaller)},
    {"crostini-webui-upgrader", flag_descriptions::kCrostiniWebUIUpgraderName,
     flag_descriptions::kCrostiniWebUIUpgraderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniWebUIUpgrader)},
#endif  // OS_CHROMEOS

    {"turn-off-streaming-media-caching",
     flag_descriptions::kTurnOffStreamingMediaCachingName,
     flag_descriptions::kTurnOffStreamingMediaCachingDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTurnOffStreamingMediaCaching)},

#if defined(OS_ANDROID)
    {"password-manager-onboarding-android",
     flag_descriptions::kPasswordManagerOnboardingAndroidName,
     flag_descriptions::kPasswordManagerOnboardingAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordManagerOnboardingAndroid)},
#endif  // defined(OS_ANDROID)

    {"enable-cooperative-scheduling",
     flag_descriptions::kCooperativeSchedulingName,
     flag_descriptions::kCooperativeSchedulingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCooperativeScheduling)},

    {"enable-defer-all-script", flag_descriptions::kEnableDeferAllScriptName,
     flag_descriptions::kEnableDeferAllScriptDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kDeferAllScriptPreviews)},
    {"enable-defer-all-script-without-optimization-hints",
     flag_descriptions::kEnableDeferAllScriptWithoutOptimizationHintsName,
     flag_descriptions::
         kEnableDeferAllScriptWithoutOptimizationHintsDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         previews::switches::kEnableDeferAllScriptWithoutOptimizationHints)},

#if defined(OS_CHROMEOS)
    {"enable-edu-coexistence", flag_descriptions::kEnableEduCoexistenceName,
     flag_descriptions::kEnableEduCoexistenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEduCoexistence)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"enable-assistant-routines",
     flag_descriptions::kEnableAssistantRoutinesName,
     flag_descriptions::kEnableAssistantRoutinesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::assistant::features::kAssistantRoutines)},
#endif  // OS_CHROMEOS

#if defined(OS_CHROMEOS)
    {"gaia-action-buttons", flag_descriptions::kGaiaActionButtonsName,
     flag_descriptions::kGaiaActionButtonsDescription, kOsCrOSOwnerOnly,
     FEATURE_VALUE_TYPE(chromeos::features::kGaiaActionButtons)},
#endif  // defined(OS_CHROMEOS)

    {"notification-scheduler-debug-options",
     flag_descriptions::kNotificationSchedulerDebugOptionName,
     flag_descriptions::kNotificationSchedulerDebugOptionDescription,
     kOsAndroid, MULTI_VALUE_TYPE(kNotificationSchedulerChoices)},

    {"update-hover-at-begin-frame",
     flag_descriptions::kUpdateHoverAtBeginFrameName,
     flag_descriptions::kUpdateHoverAtBeginFrameDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUpdateHoverAtBeginFrame)},

#if defined(OS_ANDROID)
    {"usage-stats", flag_descriptions::kUsageStatsName,
     flag_descriptions::kUsageStatsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kUsageStatsFeature)},
#endif  // defined(OS_ANDROID)

    // This set of flags is used to temporary reinstate expired flags; see
    // //docs/flag_expiry.md for details.
    {"temporary-unexpire-flags-m78", flag_descriptions::kUnexpireFlagsM78Name,
     flag_descriptions::kUnexpireFlagsM78Description, kOsAll,
     FEATURE_VALUE_TYPE(flags::kUnexpireFlagsM78)},
    {"temporary-unexpire-flags-m80", flag_descriptions::kUnexpireFlagsM80Name,
     flag_descriptions::kUnexpireFlagsM80Description, kOsAll,
     FEATURE_VALUE_TYPE(flags::kUnexpireFlagsM80)},

#if defined(OS_CHROMEOS)
    {"lock-screen-media-controls",
     flag_descriptions::kLockScreenMediaControlsName,
     flag_descriptions::kLockScreenMediaControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenMediaControls)},
#endif  // defined(OS_CHROMEOS)

    {"policy-atomic-group-enabled",
     flag_descriptions::kPolicyAtomicGroupsEnabledName,
     flag_descriptions::kPolicyAtomicGroupsEnabledDescription, kOsAll,
     FEATURE_VALUE_TYPE(policy::features::kPolicyAtomicGroup)},

    {"enable-autofill-updated-card-unmask-prompt-ui",
     flag_descriptions::kEnableAutofillUpdatedCardUnmaskPromptUiName,
     flag_descriptions::kEnableAutofillUpdatedCardUnmaskPromptUiDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpdatedCardUnmaskPromptUi)},

    {"decode-jpeg-images-to-yuv",
     flag_descriptions::kDecodeJpeg420ImagesToYUVName,
     flag_descriptions::kDecodeJpeg420ImagesToYUVDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDecodeJpeg420ImagesToYUV)},

    {"decode-webp-images-to-yuv",
     flag_descriptions::kDecodeLossyWebPImagesToYUVName,
     flag_descriptions::kDecodeLossyWebPImagesToYUVDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDecodeLossyWebPImagesToYUV)},

    {"dns-over-https", flag_descriptions::kDnsOverHttpsName,
     flag_descriptions::kDnsOverHttpsDescription,
     kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDnsOverHttps)},

#if defined(OS_ANDROID)
    {"tab-switcher-longpress-menu",
     flag_descriptions::kTabSwitcherLongpressMenuName,
     flag_descriptions::kTabSwitcherLongpressMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabSwitcherLongpressMenu)},
#endif  // defined(OS_ANDROID)

    {"web-bundles", flag_descriptions::kWebBundlesName,
     flag_descriptions::kWebBundlesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebBundles)},

#if defined(OS_ANDROID)
    {"darken-websites-checkbox-in-themes-setting",
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingName,
     flag_descriptions::kDarkenWebsitesCheckboxInThemesSettingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kDarkenWebsitesCheckboxInThemesSetting)},
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"profile-menu-revamp", flag_descriptions::kProfileMenuRevampName,
     flag_descriptions::kProfileMenuRevampDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kProfileMenuRevamp)},
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

    {"password-leak-detection", flag_descriptions::kPasswordLeakDetectionName,
     flag_descriptions::kPasswordLeakDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kLeakDetection)},
    {"enable-autofill-upi-vpa", flag_descriptions::kAutofillSaveAndFillVPAName,
     flag_descriptions::kAutofillSaveAndFillVPADescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveAndFillVPA)},

    {"quiet-notification-prompts",
     flag_descriptions::kQuietNotificationPromptsName,
     flag_descriptions::kQuietNotificationPromptsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kQuietNotificationPrompts,
                                    kQuietNotificationPromptsVariations,
                                    "QuietNotificationPrompts")},

#if defined(OS_ANDROID)
    {"context-menu-search-with-google-lens",
     flag_descriptions::kContextMenuSearchWithGoogleLensName,
     flag_descriptions::kContextMenuSearchWithGoogleLensDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextMenuSearchWithGoogleLens)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"zero-state-files", flag_descriptions::kZeroStateFilesName,
     flag_descriptions::kZeroStateFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableZeroStateMixedTypesRanker)},

    {"new-overview-tablet-layout",
     flag_descriptions::kNewOverviewTabletLayoutName,
     flag_descriptions::kNewOverviewTabletLayoutDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNewOverviewLayout)},

    {"aggregated-ml-app-ranking",
     flag_descriptions::kAggregatedMlAppRankingName,
     flag_descriptions::kAggregatedMlAppRankingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAggregatedMlAppRanking)},

    {"scalable-app-list", flag_descriptions::kScalableAppListName,
     flag_descriptions::kScalableAppListDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kScalableAppList)},

    {"fuzzy-app-search", flag_descriptions::kFuzzyAppSearchName,
     flag_descriptions::kFuzzyAppSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableFuzzyAppSearch)},

    {"aggregated-ml-search-ranking",
     flag_descriptions::kAggregatedMlSearchRankingName,
     flag_descriptions::kAggregatedMlSearchRankingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAggregatedMlSearchRanking)},

#endif  // defined(OS_CHROMEOS)

    {"passwords-account-storage",
     flag_descriptions::kEnablePasswordsAccountStorageName,
     flag_descriptions::kEnablePasswordsAccountStorageDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnablePasswordsAccountStorage)},

#if !defined(OS_ANDROID)
    {"improved-cookie-controls", flag_descriptions::kImprovedCookieControlsName,
     flag_descriptions::kImprovedCookieControlsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(content_settings::kImprovedCookieControls)},

    {"improved-cookie-controls-for-third-party-cookie-blocking",
     flag_descriptions::kImprovedCookieControlsForThirdPartyCookieBlockingName,
     flag_descriptions::
         kImprovedCookieControlsForThirdPartyCookieBlockingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         content_settings::kImprovedCookieControlsForThirdPartyCookieBlocking)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"sync-clipboard-service", flag_descriptions::kSyncClipboardServiceName,
     flag_descriptions::kSyncClipboardServiceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSyncClipboardServiceFeature)},
#endif  // OS_WIN || OS_MACOSX || OS_LINUX

#if !defined(OS_ANDROID)
    {"accessibility-internals-page-improvements",
     flag_descriptions::kAccessibilityInternalsPageImprovementsName,
     flag_descriptions::kAccessibilityInternalsPageImprovementsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAccessibilityInternalsPageImprovements)},
#endif

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

#if defined(OS_ANDROID)
    {"android-setup-search-engine",
     flag_descriptions::kAndroidSetupSearchEngineName,
     flag_descriptions::kAndroidSetupSearchEngineDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidSetupSearchEngine)},
    {"enable-clipboard-provider-text-suggestions",
     flag_descriptions::kEnableClipboardProviderTextSuggestionsName,
     flag_descriptions::kEnableClipboardProviderTextSuggestionsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kEnableClipboardProviderTextSuggestions)},
    {"omnibox-remove-suggestions-from-clipboard",
     flag_descriptions::kOmniboxRemoveSuggestionsFromClipboardName,
     flag_descriptions::kOmniboxRemoveSuggestionsFromClipboardDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxRemoveSuggestionsFromClipboard)},
#endif  // defined(OS_ANDROID)

    {"percent-based-scrolling", flag_descriptions::kPercentBasedScrollingName,
     flag_descriptions::kPercentBasedScrollingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPercentBasedScrolling)},

#if !defined(OS_ANDROID)
    {"show-legacy-tls-warnings", flag_descriptions::kLegacyTLSWarningsName,
     flag_descriptions::kLegacyTLSWarningsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(security_state::features::kLegacyTLSWarnings)},
#endif

#if defined(OS_CHROMEOS)
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

#if defined(OS_CHROMEOS)
    {"exo-pointer-lock", flag_descriptions::kExoPointerLockName,
     flag_descriptions::kExoPointerLockDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kExoPointerLock)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
    {"metal", flag_descriptions::kMetalName,
     flag_descriptions::kMetalDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMetal)},
#endif

    {"enable-de-jelly", flag_descriptions::kEnableDeJellyName,
     flag_descriptions::kEnableDeJellyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableDeJelly)},

#if defined(OS_CHROMEOS)
    {"enable-cros-action-recorder",
     flag_descriptions::kEnableCrOSActionRecorderName,
     flag_descriptions::kEnableCrOSActionRecorderDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kEnableCrOSActionRecorderChoices)},
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
    {"mix-browser-type-tabs", flag_descriptions::kMixBrowserTypeTabsName,
     flag_descriptions::kMixBrowserTypeTabsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMixBrowserTypeTabs)},

    {"mixed-content-setting", flag_descriptions::kMixedContentSiteSettingName,
     flag_descriptions::kMixedContentSiteSettingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMixedContentSiteSetting)},
#endif  // !defined(OS_ANDROID)

    {"enable-desktop-minimal-ui", flag_descriptions::kDesktopMinimalUIName,
     flag_descriptions::kDesktopMinimalUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopMinimalUI)},

    {"enable-media-internals-devtools",
     flag_descriptions::kMediaInspectorLoggingName,
     flag_descriptions::kMediaInspectorLoggingDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kMediaInspectorLogging)},

#if defined(OS_ANDROID)
    {"enable-games-hub", flag_descriptions::kGamesHubName,
     flag_descriptions::kGamesHubDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(games::features::kGamesHub)},
#endif  // defined(OS_ANDROID)

    {"enable-heavy-ad-intervention",
     flag_descriptions::kHeavyAdInterventionName,
     flag_descriptions::kHeavyAdInterventionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHeavyAdIntervention)},

    {"heavy-ad-privacy-mitigations-opt-out",
     flag_descriptions::kHeavyAdPrivacyMitigationsOptOutName,
     flag_descriptions::kHeavyAdPrivacyMitigationsOptOutDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kHeavyAdPrivacyMitigations)},

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    {"enable-ftp", flag_descriptions::kEnableFtpName,
     flag_descriptions::kEnableFtpDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFtpProtocol)},
#endif

#if defined(OS_CHROMEOS)
    {"crostini-use-buster-image",
     flag_descriptions::kCrostiniUseBusterImageName,
     flag_descriptions::kCrostiniUseBusterImageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrostiniUseBusterImage)},
    {"arc-application-zoom", flag_descriptions::kArcApplicationZoomName,
     flag_descriptions::kArcApplicationZoomDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableApplicationZoomFeature)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    {"enable-home-page-location-policy",
     flag_descriptions::kHomepageLocationName,
     flag_descriptions::kHomepageLocationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHomepageLocation)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"split-settings-sync", flag_descriptions::kSplitSettingsSyncName,
     flag_descriptions::kSplitSettingsSyncDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSplitSettingsSync)},
    {"media-app", flag_descriptions::kMediaAppName,
     flag_descriptions::kMediaAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMediaApp)},
#endif  // defined(OS_CHROMEOS)

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

 private:
  // flags_ui::FlagsState::Delegate:
  bool ShouldExcludeFlag(const FeatureEntry& entry) override {
    return flags::IsFlagExpired(entry.internal_name);
  }

  std::unique_ptr<flags_ui::FlagsState> flags_state_;

  DISALLOW_COPY_AND_ASSIGN(FlagsStateSingleton);
};

bool ShouldSkipNonDeprecatedFeatureEntry(const FeatureEntry& entry) {
  return ~entry.supported_platforms & kDeprecated;
}

bool SkipConditionalFeatureEntry(const FeatureEntry& entry) {
  version_info::Channel channel = chrome::GetChannel();
#if defined(OS_CHROMEOS)
  // Don't expose mash on stable channel.
  if (!strcmp("mash", entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  // enable-ui-devtools is only available on for non Stable channels.
  if (!strcmp(ui_devtools::switches::kEnableUiDevTools, entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }
#endif  // defined(OS_CHROMEOS)

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

#if defined(OS_WIN)
  // HDR mode works, but displays everything horribly wrong prior to windows 10.
  if (!strcmp("enable-hdr", entry.internal_name) &&
      base::win::GetVersion() < base::win::Version::WIN10) {
    return true;
  }
#endif  // OS_WIN

  if (!strcmp("dns-over-https", entry.internal_name) &&
      chrome_browser_net::ShouldDisableDohForManaged()) {
    return true;
  }

  if (flags::IsFlagExpired(entry.internal_name))
    return true;

  return false;
}

// Records a set of feature switches (prefixed with "--").
void ReportAboutFlagsHistogramSwitches(const std::string& uma_histogram_name,
                                       const std::set<std::string>& switches) {
  for (const std::string& flag : switches) {
    int uma_id = about_flags::testing::kBadSwitchFormatHistogramId;
    if (base::StartsWith(flag, "--", base::CompareCase::SENSITIVE)) {
      // Skip '--' before switch name.
      std::string switch_name(flag.substr(2));

      // Kill value, if any.
      const size_t value_pos = switch_name.find('=');
      if (value_pos != std::string::npos)
        switch_name.resize(value_pos);

      uma_id = GetSwitchUMAId(switch_name);
    } else {
      NOTREACHED() << "ReportAboutFlagsHistogram(): flag '" << flag
                   << "' has incorrect format.";
    }
    DVLOG(1) << "ReportAboutFlagsHistogram(): histogram='" << uma_histogram_name
             << "' '" << flag << "', uma_id=" << uma_id;
    base::UmaHistogramSparse(uma_histogram_name, uma_id);
  }
}

// Records a set of FEATURE_VALUE_TYPE features (suffixed with ":enabled" or
// "disabled", depending on their state).
void ReportAboutFlagsHistogramFeatures(const std::string& uma_histogram_name,
                                       const std::set<std::string>& features) {
  for (const std::string& feature : features) {
    int uma_id = GetSwitchUMAId(feature);
    DVLOG(1) << "ReportAboutFlagsHistogram(): histogram='" << uma_histogram_name
             << "' '" << feature << "', uma_id=" << uma_id;
    base::UmaHistogramSparse(uma_histogram_name, uma_id);
  }
}

}  // namespace

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

bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline,
    std::set<base::CommandLine::StringType>* out_difference) {
  const char* extra_flag_sentinel_begin_flag_name = nullptr;
  const char* extra_flag_sentinel_end_flag_name = nullptr;
#if defined(OS_CHROMEOS)
  // Put the flags between --policy-switches--begin and --policy-switches-end on
  // ChromeOS.
  extra_flag_sentinel_begin_flag_name =
      chromeos::switches::kPolicySwitchesBegin;
  extra_flag_sentinel_end_flag_name = chromeos::switches::kPolicySwitchesEnd;
#endif  // OS_CHROMEOS
  return flags_ui::FlagsState::AreSwitchesIdenticalToCurrentCommandLine(
      new_cmdline, active_cmdline, out_difference,
      extra_flag_sentinel_begin_flag_name, extra_flag_sentinel_end_flag_name);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&SkipConditionalFeatureEntry));
}

void GetFlagFeatureEntriesForDeprecatedPage(
    flags_ui::FlagsStorage* flags_storage,
    flags_ui::FlagAccess access,
    base::ListValue* supported_entries,
    base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&ShouldSkipNonDeprecatedFeatureEntry));
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

void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage) {
  std::set<std::string> switches;
  std::set<std::string> features;
  FlagsStateSingleton::GetFlagsState()->GetSwitchesAndFeaturesFromFlags(
      flags_storage, &switches, &features);
  ReportAboutFlagsHistogram("Launch.FlagsAtStartup", switches, features);
}

base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name) {
  return static_cast<base::HistogramBase::Sample>(
      base::HashMetricName(switch_name));
}

void ReportAboutFlagsHistogram(const std::string& uma_histogram_name,
                               const std::set<std::string>& switches,
                               const std::set<std::string>& features) {
  ReportAboutFlagsHistogramSwitches(uma_histogram_name, switches);
  ReportAboutFlagsHistogramFeatures(uma_histogram_name, features);
}

namespace testing {

const base::HistogramBase::Sample kBadSwitchFormatHistogramId = 0;

std::vector<FeatureEntry>* GetEntriesForTesting() {
  static base::NoDestructor<std::vector<FeatureEntry>> entries;
  return entries.get();
}

const FeatureEntry* GetFeatureEntries(size_t* count) {
  if (!GetEntriesForTesting()->empty()) {
    *count = GetEntriesForTesting()->size();
    return GetEntriesForTesting()->data();
  }
  *count = base::size(kFeatureEntries);
  return kFeatureEntries;
}

void SetFeatureEntries(const std::vector<FeatureEntry>& entries) {
  GetEntriesForTesting()->clear();
  for (const auto& entry : entries)
    GetEntriesForTesting()->push_back(entry);
  FlagsStateSingleton::GetInstance()->RebuildState(*GetEntriesForTesting());
}

}  // namespace testing

}  // namespace about_flags
