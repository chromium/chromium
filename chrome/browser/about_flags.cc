// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <set>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "components/assist_ranker/predictor_config_definitions.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browsing_data/core/features.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/public/common/download_features.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/favicon/core/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_tiles/constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/omnibox/browser/toolbar_field_trial.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/printing/browser/features.h"
#include "components/safe_browsing/features.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/switches.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_switches.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_switches.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/suggestions/features.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "components/unified_consent/feature.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "device/base/features.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/midi/midi_switches.h"
#include "net/base/features.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/service_manager/sandbox/switches.h"
#include "third_party/blink/public/common/experiments/memory_ablation_experiment.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "third_party/libaom/av1_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/views/views_switches.h"

#include "services/service_manager/sandbox/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#else  // OS_ANDROID
#include "chrome/browser/media/router/media_router_feature.h"
#include "ui/message_center/public/cpp/features.h"
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_features.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#endif  // OS_CHROMEOS

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // OS_MACOSX

#if BUILDFLAG(ENABLE_APP_LIST)
#include "ash/public/cpp/app_list/app_list_switches.h"
#endif  // ENABLE_APP_LIST

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#endif  // ENABLE_EXTENSIONS

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif  // USE_OZONE

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/titlebar_config.h"
#endif  // OS_WIN

using flags_ui::FeatureEntry;
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

const FeatureEntry::Choice kTouchEventFeatureDetectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kTouchEventFeatureDetection,
     switches::kTouchEventFeatureDetectionEnabled},
    {flags_ui::kGenericExperimentChoiceAutomatic,
     switches::kTouchEventFeatureDetection,
     switches::kTouchEventFeatureDetectionAuto}};

#if defined(USE_AURA)
const FeatureEntry::Choice kOverscrollStartThresholdChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kOverscrollStartThreshold133Percent,
     switches::kOverscrollStartThreshold, "133"},
    {flag_descriptions::kOverscrollStartThreshold166Percent,
     switches::kOverscrollStartThreshold, "166"},
    {flag_descriptions::kOverscrollStartThreshold200Percent,
     switches::kOverscrollStartThreshold, "200"}};

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

#if BUILDFLAG(ENABLE_NACL)
const FeatureEntry::Choice kNaClDebugMaskChoices[] = {
    // Secure shell can be used on ChromeOS for forwarding the TCP port opened
    // by
    // debug stub to a remote machine. Since secure shell uses NaCl, we usually
    // want to avoid debugging that. The PNaCl translator is also a NaCl module,
    // so by default we want to avoid debugging that.
    // NOTE: As the default value must be the empty string, the mask excluding
    // the PNaCl translator and secure shell is substituted elsewhere.
    {flag_descriptions::kNaclDebugMaskChoiceExcludeUtilsPnacl, "", ""},
    {flag_descriptions::kNaclDebugMaskChoiceDebugAll, switches::kNaClDebugMask,
     "*://*"},
    {flag_descriptions::kNaclDebugMaskChoiceIncludeDebug,
     switches::kNaClDebugMask, "*://*/*debug.nmf"}};
#endif  // ENABLE_NACL

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

const FeatureEntry::Choice kShowSavedCopyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kEnableShowSavedCopyPrimary,
     error_page::switches::kShowSavedCopy,
     error_page::switches::kEnableShowSavedCopyPrimary},
    {flag_descriptions::kEnableShowSavedCopySecondary,
     error_page::switches::kShowSavedCopy,
     error_page::switches::kEnableShowSavedCopySecondary},
    {flag_descriptions::kDisableShowSavedCopy,
     error_page::switches::kShowSavedCopy,
     error_page::switches::kDisableShowSavedCopy}};

const FeatureEntry::Choice kDefaultTileWidthChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kDefaultTileWidthShort, switches::kDefaultTileWidth,
     "128"},
    {flag_descriptions::kDefaultTileWidthTall, switches::kDefaultTileWidth,
     "256"},
    {flag_descriptions::kDefaultTileWidthGrande, switches::kDefaultTileWidth,
     "512"},
    {flag_descriptions::kDefaultTileWidthVenti, switches::kDefaultTileWidth,
     "1024"}};

const FeatureEntry::Choice kDefaultTileHeightChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kDefaultTileHeightShort, switches::kDefaultTileHeight,
     "128"},
    {flag_descriptions::kDefaultTileHeightTall, switches::kDefaultTileHeight,
     "256"},
    {flag_descriptions::kDefaultTileHeightGrande, switches::kDefaultTileHeight,
     "512"},
    {flag_descriptions::kDefaultTileHeightVenti, switches::kDefaultTileHeight,
     "1024"}};

#if defined(OS_WIN)
const FeatureEntry::Choice kUseAngleChoices[] = {
    {flag_descriptions::kUseAngleDefault, "", ""},
    {flag_descriptions::kUseAngleGL, switches::kUseANGLE,
     gl::kANGLEImplementationOpenGLName},
    {flag_descriptions::kUseAngleD3D11, switches::kUseANGLE,
     gl::kANGLEImplementationD3D11Name},
    {flag_descriptions::kUseAngleD3D9, switches::kUseANGLE,
     gl::kANGLEImplementationD3D9Name}};
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const FeatureEntry::FeatureParam kAccountConsistencyDice[] = {
    {kAccountConsistencyFeatureMethodParameter,
     kAccountConsistencyFeatureMethodDice}};

const FeatureEntry::FeatureParam kAccountConsistencyDiceMigration[] = {
    {kAccountConsistencyFeatureMethodParameter,
     kAccountConsistencyFeatureMethodDiceMigration}};

const FeatureEntry::FeatureParam kAccountConsistencyDiceFixAuthErrors[] = {
    {kAccountConsistencyFeatureMethodParameter,
     kAccountConsistencyFeatureMethodDiceFixAuthErrors}};

const FeatureEntry::FeatureVariation kAccountConsistencyFeatureVariations[] = {
    {"Dice", kAccountConsistencyDice, arraysize(kAccountConsistencyDice),
     nullptr /* variation_id */},
    {"Dice (migration)", kAccountConsistencyDiceMigration,
     arraysize(kAccountConsistencyDiceMigration), nullptr /* variation_id */},
    {"Dice (fix auth errors)", kAccountConsistencyDiceFixAuthErrors,
     arraysize(kAccountConsistencyDiceFixAuthErrors),
     nullptr /* variation_id */}};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

const FeatureEntry::Choice kSimpleCacheBackendChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kUseSimpleCacheBackend, "off"},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kUseSimpleCacheBackend, "on"}};

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

const FeatureEntry::Choice kChromeHomeSwipeLogicChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kChromeHomeSwipeLogicRestrictArea,
     switches::kChromeHomeSwipeLogicType, "restrict-area"},
    {flag_descriptions::kChromeHomeSwipeLogicVelocity,
     switches::kChromeHomeSwipeLogicType, "velocity"},
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

const FeatureEntry::Choice kNumRasterThreadsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kNumRasterThreadsOne, switches::kNumRasterThreads, "1"},
    {flag_descriptions::kNumRasterThreadsTwo, switches::kNumRasterThreads, "2"},
    {flag_descriptions::kNumRasterThreadsThree, switches::kNumRasterThreads,
     "3"},
    {flag_descriptions::kNumRasterThreadsFour, switches::kNumRasterThreads,
     "4"}};

const FeatureEntry::Choice kGpuRasterizationMSAASampleCountChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kGpuRasterizationMsaaSampleCountZero,
     switches::kGpuRasterizationMSAASampleCount, "0"},
    {flag_descriptions::kGpuRasterizationMsaaSampleCountTwo,
     switches::kGpuRasterizationMSAASampleCount, "2"},
    {flag_descriptions::kGpuRasterizationMsaaSampleCountFour,
     switches::kGpuRasterizationMSAASampleCount, "4"},
    {flag_descriptions::kGpuRasterizationMsaaSampleCountEight,
     switches::kGpuRasterizationMSAASampleCount, "8"},
    {flag_descriptions::kGpuRasterizationMsaaSampleCountSixteen,
     switches::kGpuRasterizationMSAASampleCount, "16"},
};

const FeatureEntry::Choice kMHTMLGeneratorOptionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMhtmlSkipNostoreMain, switches::kMHTMLGeneratorOption,
     switches::kMHTMLSkipNostoreMain},
    {flag_descriptions::kMhtmlSkipNostoreAll, switches::kMHTMLGeneratorOption,
     switches::kMHTMLSkipNostoreAll},
};

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

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kMemoryPressureThresholdChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kConservativeThresholds,
     chromeos::switches::kMemoryPressureThresholds,
     chromeos::switches::kConservativeThreshold},
    {flag_descriptions::kAggressiveCacheDiscardThresholds,
     chromeos::switches::kMemoryPressureThresholds,
     chromeos::switches::kAggressiveCacheDiscardThreshold},
    {flag_descriptions::kAggressiveTabDiscardThresholds,
     chromeos::switches::kMemoryPressureThresholds,
     chromeos::switches::kAggressiveTabDiscardThreshold},
    {flag_descriptions::kAggressiveThresholds,
     chromeos::switches::kMemoryPressureThresholds,
     chromeos::switches::kAggressiveThreshold},
};
#endif  // OS_CHROMEOS

const FeatureEntry::FeatureParam
    kLookalikeUrlNavigationSuggestionsMetricsOnly[] = {
        {"metrics_only", "true"},
};

const FeatureEntry::FeatureVariation
    kLookalikeUrlNavigationSuggestionsVariants[] = {
        {"With Metrics Only", kLookalikeUrlNavigationSuggestionsMetricsOnly,
         base::size(kLookalikeUrlNavigationSuggestionsMetricsOnly)}};

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

const FeatureEntry::Choice kNewTabButtonPositionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kNewTabButtonPositionOppositeCaption,
     switches::kNewTabButtonPosition,
     switches::kNewTabButtonPositionOppositeCaption},
    {flag_descriptions::kNewTabButtonPositionLeading,
     switches::kNewTabButtonPosition, switches::kNewTabButtonPositionLeading},
    {flag_descriptions::kNewTabButtonPositionAfterTabs,
     switches::kNewTabButtonPosition, switches::kNewTabButtonPositionAfterTabs},
    {flag_descriptions::kNewTabButtonPositionTrailing,
     switches::kNewTabButtonPosition, switches::kNewTabButtonPositionTrailing}};

#if defined(OS_CHROMEOS)
const FeatureEntry::Choice kAshShelfColorChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled, ash::switches::kAshShelfColor,
     ash::switches::kAshShelfColorEnabled},
    {flags_ui::kGenericExperimentChoiceDisabled, ash::switches::kAshShelfColor,
     ash::switches::kAshShelfColorDisabled}};

const FeatureEntry::Choice kAshShelfColorSchemeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kAshShelfColorSchemeLightVibrant,
     ash::switches::kAshShelfColorScheme,
     ash::switches::kAshShelfColorSchemeLightVibrant},
    {flag_descriptions::kAshShelfColorSchemeNormalVibrant,
     ash::switches::kAshShelfColorScheme,
     ash::switches::kAshShelfColorSchemeNormalVibrant},
    {flag_descriptions::kAshShelfColorSchemeDarkVibrant,
     ash::switches::kAshShelfColorScheme,
     ash::switches::kAshShelfColorSchemeDarkVibrant},
    {flag_descriptions::kAshShelfColorSchemeLightMuted,
     ash::switches::kAshShelfColorScheme,
     ash::switches::kAshShelfColorSchemeLightMuted},
    {flag_descriptions::kAshShelfColorSchemeNormalMuted,
     ash::switches::kAshShelfColorScheme,
     ash::switches::kAshShelfColorSchemeNormalMuted},
    {flag_descriptions::kAshShelfColorSchemeDarkMuted,
     ash::switches::kAshShelfColorScheme,
     ash::switches::kAshShelfColorSchemeDarkMuted},
};

const FeatureEntry::Choice kAshMaterialDesignInkDropAnimationSpeed[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kMaterialDesignInkDropAnimationFast,
     switches::kMaterialDesignInkDropAnimationSpeed,
     switches::kMaterialDesignInkDropAnimationSpeedFast},
    {flag_descriptions::kMaterialDesignInkDropAnimationSlow,
     switches::kMaterialDesignInkDropAnimationSpeed,
     switches::kMaterialDesignInkDropAnimationSpeedSlow}};

const FeatureEntry::Choice kDataSaverPromptChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     chromeos::switches::kEnableDataSaverPrompt, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     chromeos::switches::kDisableDataSaverPrompt, ""},
    {flag_descriptions::kDatasaverPromptDemoMode,
     chromeos::switches::kEnableDataSaverPrompt,
     chromeos::switches::kDataSaverPromptDemoMode},
};

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

const FeatureEntry::Choice kV8CacheOptionsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kV8CacheOptions,
     "none"},
    {flag_descriptions::kV8CacheOptionsCode, switches::kV8CacheOptions, "code"},
};

const FeatureEntry::Choice kSavePreviousDocumentResourcesChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kSavePreviousDocumentResourcesNever,
     switches::kSavePreviousDocumentResources, "never"},
    {flag_descriptions::kSavePreviousDocumentResourcesUntilOnDOMContentLoaded,
     switches::kSavePreviousDocumentResources, "onDOMContentLoaded"},
    {flag_descriptions::kSavePreviousDocumentResourcesUntilOnLoad,
     switches::kSavePreviousDocumentResources, "onload"},
};

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
const FeatureEntry::Choice kAshUiModeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kUiModeTablet, ash::switches::kAshUiMode,
     ash::switches::kAshUiModeTablet},
    {flag_descriptions::kUiModeClamshell, ash::switches::kAshUiMode,
     ash::switches::kAshUiModeClamshell},
    {flag_descriptions::kUiModeAuto, ash::switches::kAshUiMode,
     ash::switches::kAshUiModeAuto},
};
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam
    kContentSuggestionsCategoryOrderFeatureVariationGeneral[] = {
        {ntp_snippets::kCategoryOrderParameter,
         ntp_snippets::kCategoryOrderGeneral},
};

const FeatureEntry::FeatureParam
    kContentSuggestionsCategoryOrderFeatureVariationEMOriented[] = {
        {ntp_snippets::kCategoryOrderParameter,
         ntp_snippets::kCategoryOrderEmergingMarketsOriented},
};

const FeatureEntry::FeatureVariation
    kContentSuggestionsCategoryOrderFeatureVariations[] = {
        {"(general)", kContentSuggestionsCategoryOrderFeatureVariationGeneral,
         arraysize(kContentSuggestionsCategoryOrderFeatureVariationGeneral),
         nullptr},
        {"(emerging markets oriented)",
         kContentSuggestionsCategoryOrderFeatureVariationEMOriented,
         arraysize(kContentSuggestionsCategoryOrderFeatureVariationEMOriented),
         nullptr}};

const FeatureEntry::FeatureParam
    kContentSuggestionsCategoryRankerFeatureVariationConstant[] = {
        {ntp_snippets::kCategoryRankerParameter,
         ntp_snippets::kCategoryRankerConstantRanker},
};

const FeatureEntry::FeatureParam
    kContentSuggestionsCategoryRankerFeatureVariationClickBased[] = {
        {ntp_snippets::kCategoryRankerParameter,
         ntp_snippets::kCategoryRankerClickBasedRanker},
};

const FeatureEntry::FeatureVariation
    kContentSuggestionsCategoryRankerFeatureVariations[] = {
        {"(constant)",
         kContentSuggestionsCategoryRankerFeatureVariationConstant,
         arraysize(kContentSuggestionsCategoryRankerFeatureVariationConstant),
         nullptr},
        {"(click based)",
         kContentSuggestionsCategoryRankerFeatureVariationClickBased,
         arraysize(kContentSuggestionsCategoryRankerFeatureVariationClickBased),
         nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kForceFetchedSuggestionsNotifications[] = {
    {"force_fetched_suggestions_notifications", "true"},
    {"enable_fetched_suggestions_notifications", "true"}};

const FeatureEntry::FeatureVariation
    kContentSuggestionsNotificationsFeatureVariations[] = {
        {"(notify always, server side)", nullptr, 0, "3313312"},
        {"(notify always, client side)", kForceFetchedSuggestionsNotifications,
         arraysize(kForceFetchedSuggestionsNotifications), nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
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

const FeatureEntry::Choice kSiteIsolationTrialOptOutChoices[] = {
    {flag_descriptions::kSiteIsolationTrialOptOutChoiceDefault, "", ""},
    {flag_descriptions::kSiteIsolationTrialOptOutChoiceOptOut,
     switches::kDisableSiteIsolationTrials, ""},
};

const FeatureEntry::Choice kTLS13VariantChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kTLS13VariantDisabled, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The Draft 18 variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The Experiment variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The RecordType variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The NoSessionID variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The Experiment2 variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The Experiment3 variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    // The Draft22 variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    {flag_descriptions::kTLS13VariantDraft23, switches::kTLS13Variant,
     switches::kTLS13VariantDraft23},
    // The Draft28 variant was deprecated.
    {flag_descriptions::kTLS13VariantDeprecated, switches::kTLS13Variant,
     switches::kTLS13VariantDisabled},
    {flag_descriptions::kTLS13VariantFinal, switches::kTLS13Variant,
     switches::kTLS13VariantFinal},
};

#if !defined(OS_ANDROID)
const FeatureEntry::Choice kEnableAudioFocusChoices[] = {
    {flag_descriptions::kEnableAudioFocusDisabled, "", ""},
    {flag_descriptions::kEnableAudioFocusEnabled,
     media_session::switches::kEnableAudioFocus, ""},
#if BUILDFLAG(ENABLE_PLUGINS)
    {flag_descriptions::kEnableAudioFocusEnabledDuckFlash,
     media_session::switches::kEnableAudioFocus,
     media_session::switches::kEnableAudioFocusDuckFlash},
#endif  // BUILDFLAG(ENABLE_PLUGINS)
    {flag_descriptions::kEnableAudioFocusEnabledNoEnforce,
     media_session::switches::kEnableAudioFocus,
     media_session::switches::kEnableAudioFocusNoEnforce},
};
#endif  // !defined(OS_ANDROID)

const FeatureEntry::Choice kForceColorProfileChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kForceColorProfileSRGB,
     switches::kForceDisplayColorProfile, "srgb"},
    {flag_descriptions::kForceColorProfileP3,
     switches::kForceDisplayColorProfile, "display-p3-d65"},
    {flag_descriptions::kForceColorProfileColorSpin,
     switches::kForceDisplayColorProfile, "color-spin-gamma24"},
    {flag_descriptions::kForceColorProfileHdr,
     switches::kForceDisplayColorProfile, "scrgb-linear"},
};

const FeatureEntry::FeatureParam kAutofillPreviewStyleBlackOnBlue050[] = {
    {blink::features::kAutofillPreviewStyleExperimentBgColorParameterName,
     "#E8F0FE"},
    {blink::features::kAutofillPreviewStyleExperimentColorParameterName,
     "#000000"},
};
const FeatureEntry::FeatureParam kAutofillPreviewStyleBlackOnBlue100[] = {
    {blink::features::kAutofillPreviewStyleExperimentBgColorParameterName,
     "#D2E3FC"},
    {blink::features::kAutofillPreviewStyleExperimentColorParameterName,
     "#000000"},
};
const FeatureEntry::FeatureParam kAutofillPreviewStyleBlue900OnBlue050[] = {
    {blink::features::kAutofillPreviewStyleExperimentBgColorParameterName,
     "#E8F0FE"},
    {blink::features::kAutofillPreviewStyleExperimentColorParameterName,
     "#174EA6"},
};
const FeatureEntry::FeatureParam kAutofillPreviewStyleBlackOnGreen050[] = {
    {blink::features::kAutofillPreviewStyleExperimentBgColorParameterName,
     "#E6F4EA"},
    {blink::features::kAutofillPreviewStyleExperimentColorParameterName,
     "#000000"},
};
const FeatureEntry::FeatureParam kAutofillPreviewStyleBlackOnYellow050[] = {
    {blink::features::kAutofillPreviewStyleExperimentBgColorParameterName,
     "#FEF7E0"},
    {blink::features::kAutofillPreviewStyleExperimentColorParameterName,
     "#000000"},
};

const FeatureEntry::FeatureVariation kAutofillPreviewStyleVariations[] = {
    {"(Black on GoogleBlue050)", kAutofillPreviewStyleBlackOnBlue050,
     base::size(kAutofillPreviewStyleBlackOnBlue050), nullptr},
    {"(Black on GoogleBlue100)", kAutofillPreviewStyleBlackOnBlue100,
     base::size(kAutofillPreviewStyleBlackOnBlue100), nullptr},
    {"(GoogleBlue900 on GoogleBlue050)", kAutofillPreviewStyleBlue900OnBlue050,
     base::size(kAutofillPreviewStyleBlue900OnBlue050), nullptr},
    {"(Black on GoogleGreen050)", kAutofillPreviewStyleBlackOnGreen050,
     base::size(kAutofillPreviewStyleBlackOnGreen050), nullptr},
    {"(Black on GoogleYellow050)", kAutofillPreviewStyleBlackOnYellow050,
     base::size(kAutofillPreviewStyleBlackOnYellow050), nullptr}};

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
const FeatureEntry::FeatureParam kAutofillPrimaryInfoStyleMedium[] = {
    {autofill::kAutofillForcedFontWeightParameterName,
     autofill::kAutofillForcedFontWeightParameterMedium},
};
const FeatureEntry::FeatureParam kAutofillPrimaryInfoStyleBold[] = {
    {autofill::kAutofillForcedFontWeightParameterName,
     autofill::kAutofillForcedFontWeightParameterBold},
};

const FeatureEntry::FeatureVariation kAutofillPrimaryInfoStyleVariations[] = {
    {"(medium)", kAutofillPrimaryInfoStyleMedium,
     base::size(kAutofillPrimaryInfoStyleMedium), nullptr},
    {"(bold)", kAutofillPrimaryInfoStyleBold,
     base::size(kAutofillPrimaryInfoStyleBold), nullptr},
};

const FeatureEntry::FeatureParam kPedalSuggestionInSuggestion[] = {
    {OmniboxFieldTrial::kPedalSuggestionModeParam, "in_suggestion"}};
const FeatureEntry::FeatureParam kPedalSuggestionDedicated[] = {
    {OmniboxFieldTrial::kPedalSuggestionModeParam, "dedicated"}};
const FeatureEntry::FeatureVariation kPedalSuggestionVariations[] = {
    {"In Suggestion (Side Button)", kPedalSuggestionInSuggestion,
     base::size(kPedalSuggestionInSuggestion), nullptr},
    {"Dedicated Suggestion Line", kPedalSuggestionDedicated,
     base::size(kPedalSuggestionDedicated), nullptr},
};
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

const FeatureEntry::Choice kAutoplayPolicyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kAutoplayPolicyNoUserGestureRequired,
     switches::kAutoplayPolicy,
     switches::autoplay::kNoUserGestureRequiredPolicy},
#if defined(OS_ANDROID)
    {flag_descriptions::kAutoplayPolicyUserGestureRequired,
     switches::kAutoplayPolicy, switches::autoplay::kUserGestureRequiredPolicy},
#else
    {flag_descriptions::kAutoplayPolicyUserGestureRequiredForCrossOrigin,
     switches::kAutoplayPolicy,
     switches::autoplay::kUserGestureRequiredForCrossOriginPolicy},
#endif
    {flag_descriptions::kAutoplayPolicyDocumentUserActivation,
     switches::kAutoplayPolicy,
     switches::autoplay::kDocumentUserActivationRequiredPolicy},
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
                  arraysize(kForceEffectiveConnectionTypeChoices),
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
         arraysize(kAutofillKeyboardAccessoryFeatureVariationAnimationDuration),
         nullptr},
        {"Limit label width",
         kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth,
         arraysize(kAutofillKeyboardAccessoryFeatureVariationLimitLabelWidth),
         nullptr},
        {"Show hint", kAutofillKeyboardAccessoryFeatureVariationShowHint,
         arraysize(kAutofillKeyboardAccessoryFeatureVariationShowHint),
         nullptr},
        {"Animate with hint",
         kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint,
         arraysize(kAutofillKeyboardAccessoryFeatureVariationAnimateWithHint),
         nullptr}};
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kPersistentMenuItemEnabled[] = {
    {"persistent_menu_item_enabled", "true"}};

const FeatureEntry::FeatureVariation kDataReductionMainMenuFeatureVariations[] =
    {{"(persistent)", kPersistentMenuItemEnabled,
      arraysize(kPersistentMenuItemEnabled), nullptr}};
#endif  // OS_ANDROID

const FeatureEntry::FeatureParam kDetectingHeavyPagesLowThresholdEnabled[] = {
    {"PageCapMiB", "1"},
    {"MediaPageCapMiB", "1"}};

const FeatureEntry::FeatureVariation kDetectingHeavyPagesFeatureVariations[] = {
    {"(Low)", kDetectingHeavyPagesLowThresholdEnabled,
     base::size(kDetectingHeavyPagesLowThresholdEnabled), nullptr}};

const FeatureEntry::Choice kEnableOutOfProcessHeapProfilingChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flag_descriptions::kEnableOutOfProcessHeapProfilingModeMinimal,
     heap_profiling::kMemlog, heap_profiling::kMemlogModeMinimal},
    {flag_descriptions::kEnableOutOfProcessHeapProfilingModeAll,
     heap_profiling::kMemlog, heap_profiling::kMemlogModeAll},
    {flag_descriptions::kEnableOutOfProcessHeapProfilingModeBrowser,
     heap_profiling::kMemlog, heap_profiling::kMemlogModeBrowser},
    {flag_descriptions::kEnableOutOfProcessHeapProfilingModeGpu,
     heap_profiling::kMemlog, heap_profiling::kMemlogModeGpu},
    {flag_descriptions::kEnableOutOfProcessHeapProfilingModeAllRenderers,
     heap_profiling::kMemlog, heap_profiling::kMemlogModeAllRenderers},
    {flag_descriptions::kEnableOutOfProcessHeapProfilingModeManual,
     heap_profiling::kMemlog, heap_profiling::kMemlogModeManual},
};

const FeatureEntry::Choice kOOPHPStackModeChoices[] = {
    {flags_ui::kGenericExperimentChoiceDisabled, "", ""},
    {flag_descriptions::kOOPHPStackModeNative, heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeNative},
    {flag_descriptions::kOOPHPStackModeNativeWithThreadNames,
     heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeNativeWithThreadNames},
    {flag_descriptions::kOOPHPStackModePseudo, heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModePseudo},
    {flag_descriptions::kOOPHPStackModeMixed, heap_profiling::kMemlogStackMode,
     heap_profiling::kMemlogStackModeMixed},
};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         arraysize(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         arraysize(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         arraysize(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         arraysize(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         arraysize(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         arraysize(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         arraysize(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::Choice kAsyncImageDecodingChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     cc::switches::kDisableCheckerImaging, ""},
};

#if defined(OS_ANDROID)
const FeatureEntry::Choice kThirdPartyDoodlesChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"(force simple test doodle)",
     search_provider_logos::switches::kThirdPartyDoodleURL,
     "https://www.gstatic.com/chrome/ntp/doodle_test/third_party_simple.json"},
    {"(force animated test doodle)",
     search_provider_logos::switches::kThirdPartyDoodleURL,
     "https://www.gstatic.com/chrome/ntp/doodle_test/"
     "third_party_animated.json"},
};

const FeatureEntry::Choice kUseDdljsonApiChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"(force test doodle 0)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android0.json"},
    {"(force test doodle 1)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android1.json"},
    {"(force test doodle 2)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android2.json"},
    {"(force test doodle 3)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android3.json"},
    {"(force test doodle 4)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json"},
};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam kMarkHttpAsDangerous[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerous}};
const FeatureEntry::FeatureParam kMarkHttpAsWarning[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterWarning}};
const FeatureEntry::FeatureParam kMarkHttpAsWarningAndDangerousOnFormEdits[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::
         kMarkHttpAsParameterWarningAndDangerousOnFormEdits}};
const FeatureEntry::FeatureParam
    kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards[] = {
        {security_state::features::kMarkHttpAsFeatureParameterName,
         security_state::features::
             kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}};

const FeatureEntry::FeatureVariation kMarkHttpAsFeatureVariations[] = {
    {"(mark as actively dangerous)", kMarkHttpAsDangerous,
     arraysize(kMarkHttpAsDangerous), nullptr},
    {"(mark with a Not Secure warning)", kMarkHttpAsWarning,
     arraysize(kMarkHttpAsWarning), nullptr},
    {"(mark with a Not Secure warning and dangerous on form edits)",
     kMarkHttpAsWarningAndDangerousOnFormEdits,
     arraysize(kMarkHttpAsWarningAndDangerousOnFormEdits), nullptr},
    {"(mark with a Not Secure warning and dangerous on passwords and credit "
     "card fields)",
     kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards,
     arraysize(kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards),
     nullptr}};

#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_VR)
const FeatureEntry::FeatureParam kWebXrRenderPathChoiceClientWait[] = {
    {features::kWebXrRenderPathParamName,
     features::kWebXrRenderPathParamValueClientWait}};
const FeatureEntry::FeatureParam kWebXrRenderPathChoiceGpuFence[] = {
    {features::kWebXrRenderPathParamName,
     features::kWebXrRenderPathParamValueGpuFence}};
const FeatureEntry::FeatureParam kWebXrRenderPathChoiceSharedBuffer[] = {
    {features::kWebXrRenderPathParamName,
     features::kWebXrRenderPathParamValueSharedBuffer}};

const FeatureEntry::FeatureVariation kWebXrRenderPathVariations[] = {
    {flag_descriptions::kWebXrRenderPathChoiceClientWaitDescription,
     kWebXrRenderPathChoiceClientWait,
     arraysize(kWebXrRenderPathChoiceClientWait), nullptr},
    {flag_descriptions::kWebXrRenderPathChoiceGpuFenceDescription,
     kWebXrRenderPathChoiceGpuFence, arraysize(kWebXrRenderPathChoiceGpuFence),
     nullptr},
    {flag_descriptions::kWebXrRenderPathChoiceSharedBufferDescription,
     kWebXrRenderPathChoiceSharedBuffer,
     arraysize(kWebXrRenderPathChoiceSharedBuffer), nullptr}};
#endif  // defined(OS_ANDROID) && BUILDFLAG(ENABLE_VR)

const FeatureEntry::FeatureParam kUnifiedConsentShowBump[] = {
    {unified_consent::kUnifiedConsentShowBumpParameter, "true"}};
const FeatureEntry::FeatureVariation kUnifiedConsentVariations[] = {
    {"(with consent bump)", kUnifiedConsentShowBump,
     arraysize(kUnifiedConsentShowBump), nullptr}};

const FeatureEntry::FeatureParam kSimplifyHttpsIndicatorEvToSecure[] = {
    {toolbar::features::kSimplifyHttpsIndicatorParameterName,
     toolbar::features::kSimplifyHttpsIndicatorParameterEvToSecure}};
const FeatureEntry::FeatureParam kSimplifyHttpsIndicatorSecureToLock[] = {
    {toolbar::features::kSimplifyHttpsIndicatorParameterName,
     toolbar::features::kSimplifyHttpsIndicatorParameterSecureToLock}};
const FeatureEntry::FeatureParam kSimplifyHttpsIndicatorBothToLock[] = {
    {toolbar::features::kSimplifyHttpsIndicatorParameterName,
     toolbar::features::kSimplifyHttpsIndicatorParameterBothToLock}};
const FeatureEntry::FeatureParam kSimplifyHttpsIndicatorKeepSecureChip[] = {
    {toolbar::features::kSimplifyHttpsIndicatorParameterName,
     toolbar::features::kSimplifyHttpsIndicatorParameterKeepSecureChip}};

const FeatureEntry::FeatureVariation kSimplifyHttpsIndicatorVariations[] = {
    {"(show Secure chip for EV pages)", kSimplifyHttpsIndicatorEvToSecure,
     base::size(kSimplifyHttpsIndicatorEvToSecure), nullptr},
    {"(show Lock icon for non-EV HTTPS pages)",
     kSimplifyHttpsIndicatorSecureToLock,
     base::size(kSimplifyHttpsIndicatorSecureToLock), nullptr},
    {"(show Lock icon for all HTTPS pages)", kSimplifyHttpsIndicatorBothToLock,
     base::size(kSimplifyHttpsIndicatorBothToLock), nullptr},
    {"(show Secure chip for non-EV HTTPS pages)",
     kSimplifyHttpsIndicatorKeepSecureChip,
     base::size(kSimplifyHttpsIndicatorKeepSecureChip), nullptr}};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishHeuristic[] = {
    {language::kOverrideModelKey, language::kOverrideModelHeuristicValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureParam kTranslateForceTriggerOnEnglishGeo[] = {
    {language::kOverrideModelKey, language::kOverrideModelGeoValue},
    {language::kEnforceRankerKey, "false"}};
const FeatureEntry::FeatureVariation
    kTranslateForceTriggerOnEnglishVariations[] = {
        {"(Heuristic model without Ranker)",
         kTranslateForceTriggerOnEnglishHeuristic,
         arraysize(kTranslateForceTriggerOnEnglishHeuristic), nullptr},
        {"(Geo model without Ranker)", kTranslateForceTriggerOnEnglishGeo,
         arraysize(kTranslateForceTriggerOnEnglishGeo), nullptr},
};
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kProactiveTabFreezeAndDiscard_FreezeOnly[] = {
    {resource_coordinator::
         kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam,
     "false"}};
const FeatureEntry::FeatureParam
    kProactiveTabFreezeAndDiscard_FreezeAndDiscard[] = {
        {resource_coordinator::
             kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam,
         "true"}};
const FeatureEntry::FeatureParam
    kProactiveTabFreezeAndDiscard_DisableHeuristics[] = {
        {resource_coordinator::
             kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam,
         "true"},
        {resource_coordinator::
             kProactiveTabFreezeAndDiscard_DisableHeuristicsParam,
         "true"}};
const FeatureEntry::FeatureVariation kProactiveTabFreezeAndDiscardVariations[] =
    {{"Freeze only", kProactiveTabFreezeAndDiscard_FreezeOnly,
      base::size(kProactiveTabFreezeAndDiscard_FreezeOnly), nullptr},
     {"Freeze and discard", kProactiveTabFreezeAndDiscard_FreezeAndDiscard,
      base::size(kProactiveTabFreezeAndDiscard_FreezeAndDiscard), nullptr},
     {"Freeze and discard, heuristics disabled",
      kProactiveTabFreezeAndDiscard_DisableHeuristics,
      base::size(kProactiveTabFreezeAndDiscard_DisableHeuristics), nullptr}};
#endif

const FeatureEntry::FeatureParam kGamepadPollingRate100Hz[] = {
    {features::kGamepadPollingIntervalParamKey, "10"}};
const FeatureEntry::FeatureParam kGamepadPollingRate125Hz[] = {
    {features::kGamepadPollingIntervalParamKey, "8"}};
const FeatureEntry::FeatureParam kGamepadPollingRate200Hz[] = {
    {features::kGamepadPollingIntervalParamKey, "5"}};
const FeatureEntry::FeatureParam kGamepadPollingRate250Hz[] = {
    {features::kGamepadPollingIntervalParamKey, "4"}};

const FeatureEntry::FeatureVariation kGamepadPollingRateVariations[] = {
    {"(100 Hz)", kGamepadPollingRate100Hz, base::size(kGamepadPollingRate100Hz),
     nullptr},
    {"(125 Hz)", kGamepadPollingRate125Hz, base::size(kGamepadPollingRate125Hz),
     nullptr},
    {"(200 Hz)", kGamepadPollingRate200Hz, base::size(kGamepadPollingRate200Hz),
     nullptr},
    {"(250 Hz)", kGamepadPollingRate250Hz, base::size(kGamepadPollingRate250Hz),
     nullptr},
};

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kNewNetErrorPageUIContentList = {
    features::kNewNetErrorPageUIAlternateParameterName,
    features::kNewNetErrorPageUIAlternateContentList};
const FeatureEntry::FeatureParam kNewNetErrorPageUIContentPreview = {
    features::kNewNetErrorPageUIAlternateParameterName,
    features::kNewNetErrorPageUIAlternateContentPreview};

const FeatureEntry::FeatureVariation kNewNetErrorPageUIVariations[] = {
    {"Content List", &kNewNetErrorPageUIContentList, 1, nullptr},
    {"Content Preview", &kNewNetErrorPageUIContentPreview, 1, nullptr}};

const FeatureEntry::FeatureParam kExploreSitesExperimental = {
    chrome::android::explore_sites::kExploreSitesVariationParameterName,
    chrome::android::explore_sites::kExploreSitesVariationExperimental};
const FeatureEntry::FeatureVariation kExploreSitesVariations[] = {
    {"Experimental", &kExploreSitesExperimental, 1, nullptr}};
#endif  // defined(OS_ANDROID)

const FeatureEntry::FeatureParam
    kAutofillCreditCardLocalCardMigrationWithoutSettingsPage[] = {
        {autofill::features::kAutofillCreditCardLocalCardMigrationParameterName,
         autofill::features::
             kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage}};

const FeatureEntry::FeatureVariation
    kAutofillCreditCardLocalCardMigrationVariations[] = {
        {"(without settings page)",
         kAutofillCreditCardLocalCardMigrationWithoutSettingsPage,
         base::size(kAutofillCreditCardLocalCardMigrationWithoutSettingsPage),
         nullptr}};

const FeatureEntry::FeatureParam kResamplingInputEventsLSQEnabled[] = {
    {"predictor", "lsq"}};

const FeatureEntry::FeatureParam kResamplingInputEventsKalmanEnabled[] = {
    {"predictor", "kalman"}};

const FeatureEntry::FeatureVariation kResamplingInputEventsFeatureVariations[] =
    {{"lsq", kResamplingInputEventsLSQEnabled,
      base::size(kResamplingInputEventsLSQEnabled), nullptr},
     {"kalman", kResamplingInputEventsKalmanEnabled,
      base::size(kResamplingInputEventsKalmanEnabled), nullptr}};

#if !defined(OS_ANDROID)
const FeatureEntry::FeatureParam kAutofillDropdownLayoutLeadingIcon[] = {
    {autofill::kAutofillDropdownLayoutParameterName,
     autofill::kAutofillDropdownLayoutParameterLeadingIcon}};
const FeatureEntry::FeatureParam kAutofillDropdownLayoutTrailingIcon[] = {
    {autofill::kAutofillDropdownLayoutParameterName,
     autofill::kAutofillDropdownLayoutParameterTrailingIcon}};
const FeatureEntry::FeatureParam kAutofillDropdownLayoutTwoLinesLeadingIcon[] =
    {{autofill::kAutofillDropdownLayoutParameterName,
      autofill::kAutofillDropdownLayoutParameterTwoLinesLeadingIcon}};

const FeatureEntry::FeatureVariation kAutofillDropdownLayoutVariations[] = {
    {"(leading icon)", kAutofillDropdownLayoutLeadingIcon,
     base::size(kAutofillDropdownLayoutLeadingIcon), nullptr},
    {"(trailing icon)", kAutofillDropdownLayoutTrailingIcon,
     base::size(kAutofillDropdownLayoutLeadingIcon), nullptr},
    {"(two line leading icon)", kAutofillDropdownLayoutTwoLinesLeadingIcon,
     base::size(kAutofillDropdownLayoutTwoLinesLeadingIcon), nullptr}};
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
const FeatureEntry::FeatureParam kBottomOfflineIndicatorEnabled[] = {
    {"bottom_offline_indicator", "true"}};

const FeatureEntry::FeatureVariation kOfflineIndicatorFeatureVariations[] = {
    {"(bottom)", kBottomOfflineIndicatorEnabled,
     base::size(kBottomOfflineIndicatorEnabled), nullptr}};
#endif  // OS_ANDROID

// RECORDING USER METRICS FOR FLAGS:
// -----------------------------------------------------------------------------
// The first line of the entry is the internal name.
//
// To add a new entry, add to the end of kFeatureEntries. There are two
// distinct types of entries:
// . SINGLE_VALUE: entry is either on or off. Use the SINGLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// See the documentation of FeatureEntry for details on the fields.
//
// Usage of about:flags is logged on startup via the "Launch.FlagsAtStartup"
// UMA histogram. This histogram shows the number of startups with a given flag
// enabled. If you'd like to see user counts instead, make sure to switch to
// to "count users" view on the dashboard. When adding new entries, the enum
// "LoginCustomFlags" must be updated in histograms/enums.xml. See note in
// enums.xml and don't forget to run AboutFlagsHistogramTest unit test
// to calculate and verify checksum.
//
// When adding a new choice, add it to the end of the list.
const FeatureEntry kFeatureEntries[] = {
    {"ignore-gpu-blacklist", flag_descriptions::kIgnoreGpuBlacklistName,
     flag_descriptions::kIgnoreGpuBlacklistDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kIgnoreGpuBlacklist)},
    {"enable-canvas-2d-image-chromium",
     flag_descriptions::kCanvas2DImageChromiumName,
     flag_descriptions::kCanvas2DImageChromiumDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kCanvas2DImageChromium)},
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
    {"show-overdraw-feedback", flag_descriptions::kShowOverdrawFeedbackName,
     flag_descriptions::kShowOverdrawFeedbackDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kShowOverdrawFeedback)},
    {"enable-draw-occlusion", flag_descriptions::kEnableDrawOcclusionName,
     flag_descriptions::kEnableDrawOcclusionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableDrawOcclusion)},
    {"ui-disable-partial-swap", flag_descriptions::kUiPartialSwapName,
     flag_descriptions::kUiPartialSwapDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kUIDisablePartialSwap)},
    {"disable-webrtc-hw-decoding", flag_descriptions::kWebrtcHwDecodingName,
     flag_descriptions::kWebrtcHwDecodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWDecoding)},
    {"disable-webrtc-hw-encoding", flag_descriptions::kWebrtcHwEncodingName,
     flag_descriptions::kWebrtcHwEncodingDescription, kOsAndroid | kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableWebRtcHWEncoding)},
    {"enable-webrtc-hw-h264-encoding",
     flag_descriptions::kWebrtcHwH264EncodingName,
     flag_descriptions::kWebrtcHwH264EncodingDescription, kOsAndroid | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebRtcHWH264Encoding)},
    {"enable-webrtc-hw-vp8-encoding",
     flag_descriptions::kWebrtcHwVP8EncodingName,
     flag_descriptions::kWebrtcHwVP8EncodingDescription, kOsAndroid | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebRtcHWVP8Encoding)},
#if !defined(OS_ANDROID)
    {"enable-webrtc-remote-event-log",
     flag_descriptions::kWebRtcRemoteEventLogName,
     flag_descriptions::kWebRtcRemoteEventLogDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebRtcRemoteEventLog)},
#endif
    {"enable-webrtc-srtp-aes-gcm", flag_descriptions::kWebrtcSrtpAesGcmName,
     flag_descriptions::kWebrtcSrtpAesGcmDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcSrtpAesGcm)},
    {"enable-webrtc-srtp-encrypted-headers",
     flag_descriptions::kWebrtcSrtpEncryptedHeadersName,
     flag_descriptions::kWebrtcSrtpEncryptedHeadersDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcSrtpEncryptedHeaders)},
    {"enable-webrtc-stun-origin", flag_descriptions::kWebrtcStunOriginName,
     flag_descriptions::kWebrtcStunOriginDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebRtcStunOrigin)},
    {"WebRtcUseEchoCanceller3", flag_descriptions::kWebrtcEchoCanceller3Name,
     flag_descriptions::kWebrtcEchoCanceller3Description, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcUseEchoCanceller3)},
    {"enable-webrtc-hybrid-agc", flag_descriptions::kWebrtcHybridAgcName,
     flag_descriptions::kWebrtcHybridAgcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebRtcHybridAgc)},
    {"enable-webrtc-new-encode-cpu-load-estimator",
     flag_descriptions::kWebrtcNewEncodeCpuLoadEstimatorName,
     flag_descriptions::kWebrtcNewEncodeCpuLoadEstimatorDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kNewEncodeCpuLoadEstimator)},
    {"webrtc-unified-plan-by-default",
     flag_descriptions::kWebrtcUnifiedPlanByDefaultName,
     flag_descriptions::kWebrtcUnifiedPlanByDefaultDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kRTCUnifiedPlanByDefault)},
#if defined(OS_ANDROID)
    {"clear-old-browsing-data", flag_descriptions::kClearOldBrowsingDataName,
     flag_descriptions::kClearOldBrowsingDataDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kClearOldBrowsingData)},
    {"enable-osk-overscroll", flag_descriptions::kEnableOskOverscrollName,
     flag_descriptions::kEnableOskOverscrollDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableOSKOverscroll)},
    {"enable-new-contacts-picker", flag_descriptions::kNewContactsPickerName,
     flag_descriptions::kNewContactsPickerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNewContactsPicker)},
    {"enable-new-photo-picker", flag_descriptions::kNewPhotoPickerName,
     flag_descriptions::kNewPhotoPickerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNewPhotoPicker)},
    {"enable-usermedia-screen-capturing",
     flag_descriptions::kMediaScreenCaptureName,
     flag_descriptions::kMediaScreenCaptureDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kUserMediaScreenCapturing)},
    {"enable-surfacecontrol", flag_descriptions::kAndroidSurfaceControl,
     flag_descriptions::kAndroidSurfaceControlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidSurfaceControl)},
#endif  // OS_ANDROID
// Native client is compiled out if ENABLE_NACL is not set.
#if BUILDFLAG(ENABLE_NACL)
    {"enable-nacl", flag_descriptions::kNaclName,
     flag_descriptions::kNaclDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
    {"enable-nacl-debug", flag_descriptions::kNaclDebugName,
     flag_descriptions::kNaclDebugDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableNaClDebug)},
    {"force-pnacl-subzero", flag_descriptions::kPnaclSubzeroName,
     flag_descriptions::kPnaclSubzeroDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kForcePNaClSubzero)},
    {"nacl-debug-mask", flag_descriptions::kNaclDebugMaskName,
     flag_descriptions::kNaclDebugMaskDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kNaClDebugMaskChoices)},
#endif  // ENABLE_NACL
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extension-apis", flag_descriptions::kExperimentalExtensionApisName,
     flag_descriptions::kExperimentalExtensionApisDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableExperimentalExtensionApis)},
    {"extensions-on-chrome-urls",
     flag_descriptions::kExtensionsOnChromeUrlsName,
     flag_descriptions::kExtensionsOnChromeUrlsDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif  // ENABLE_EXTENSIONS
    {"enable-fast-unload", flag_descriptions::kFastUnloadName,
     flag_descriptions::kFastUnloadDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableFastUnload)},
    {"enable-history-entry-requires-user-gesture",
     flag_descriptions::kHistoryRequiresUserGestureName,
     flag_descriptions::kHistoryRequiresUserGestureDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kHistoryEntryRequiresUserGesture)},
    {"disable-pushstate-throttle",
     flag_descriptions::kDisablePushStateThrottleName,
     flag_descriptions::kDisablePushStateThrottleDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisablePushStateThrottle)},
    {"disable-hyperlink-auditing", flag_descriptions::kHyperlinkAuditingName,
     flag_descriptions::kHyperlinkAuditingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kNoPings)},
#if defined(OS_ANDROID)
    {"contextual-search", flag_descriptions::kContextualSearchName,
     flag_descriptions::kContextualSearchDescription, kOsAndroid,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableContextualSearch,
                               switches::kDisableContextualSearch)},
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
    {"contextual-search-unity-integration",
     flag_descriptions::kContextualSearchUnityIntegrationName,
     flag_descriptions::kContextualSearchUnityIntegrationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kContextualSearchUnityIntegration)},
    {"explore-sites", flag_descriptions::kExploreSitesName,
     flag_descriptions::kExploreSitesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kExploreSites,
                                    kExploreSitesVariations,
                                    "ExploreSites")},
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
    {   // See http://crbug.com/120416 for how to remove this flag.
     "save-page-as-mhtml", flag_descriptions::kSavePageAsMhtmlName,
     flag_descriptions::kSavePageAsMhtmlDescription, kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(switches::kSavePageAsMHTML)},
    {"mhtml-generator-option", flag_descriptions::kMhtmlGeneratorOptionName,
     flag_descriptions::kMhtmlGeneratorOptionDescription,
     kOsMac | kOsWin | kOsLinux,
     MULTI_VALUE_TYPE(kMHTMLGeneratorOptionChoices)},
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
    {"enable-webassembly", flag_descriptions::kEnableWasmName,
     flag_descriptions::kEnableWasmDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssembly)},
    {"enable-webassembly-baseline", flag_descriptions::kEnableWasmBaselineName,
     flag_descriptions::kEnableWasmBaselineDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyBaseline)},
    {"enable-webassembly-threads", flag_descriptions::kEnableWasmThreadsName,
     flag_descriptions::kEnableWasmThreadsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyThreads)},
    {"shared-array-buffer", flag_descriptions::kEnableSharedArrayBufferName,
     flag_descriptions::kEnableSharedArrayBufferDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSharedArrayBuffer)},
    {"enable-future-v8-vm-features", flag_descriptions::kV8VmFutureName,
     flag_descriptions::kV8VmFutureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8VmFuture)},
    {"enable-v8-orinoco", flag_descriptions::kV8OrinocoName,
     flag_descriptions::kV8OrinocoDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8Orinoco)},
    {"harmony-await-optimization", flag_descriptions::kAwaitOptimizationName,
     flag_descriptions::kAwaitOptimizationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kAwaitOptimization)},
    {"disable-software-rasterizer", flag_descriptions::kSoftwareRasterizerName,
     flag_descriptions::kSoftwareRasterizerDescription,
#if BUILDFLAG(ENABLE_SWIFTSHADER)
     kOsAll,
#else   // BUILDFLAG(ENABLE_SWIFTSHADER)
     0,
#endif  // BUILDFLAG(ENABLE_SWIFTSHADER)
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableSoftwareRasterizer)},
    {"enable-gpu-rasterization", flag_descriptions::kGpuRasterizationName,
     flag_descriptions::kGpuRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"enable-oop-rasterization", flag_descriptions::kOopRasterizationName,
     flag_descriptions::kOopRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableOopRasterizationChoices)},
    {"gpu-rasterization-msaa-sample-count",
     flag_descriptions::kGpuRasterizationMsaaSampleCountName,
     flag_descriptions::kGpuRasterizationMsaaSampleCountDescription, kOsAll,
     MULTI_VALUE_TYPE(kGpuRasterizationMSAASampleCountChoices)},
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-ble-advertising-in-apps",
     flag_descriptions::kBleAdvertisingInExtensionsName,
     flag_descriptions::kBleAdvertisingInExtensionsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableBLEAdvertising)},
#endif  // ENABLE_EXTENSIONS
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
    {"new-tab-button-position", flag_descriptions::kNewTabButtonPosition,
     flag_descriptions::kNewTabButtonPositionDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kNewTabButtonPositionChoices)},
    {"single-tab-mode", flag_descriptions::kSingleTabMode,
     flag_descriptions::kSingleTabModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSingleTabMode)},
    {"site-settings", flag_descriptions::kSiteSettings,
     flag_descriptions::kSiteSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSiteSettings)},
    {"touch-events", flag_descriptions::kTouchEventsName,
     flag_descriptions::kTouchEventsDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTouchEventFeatureDetectionChoices)},
    {"disable-touch-adjustment", flag_descriptions::kTouchAdjustmentName,
     flag_descriptions::kTouchAdjustmentDescription,
     kOsWin | kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableTouchAdjustment)},
#if defined(OS_CHROMEOS)
    {"network-portal-notification",
     flag_descriptions::kNetworkPortalNotificationName,
     flag_descriptions::kNetworkPortalNotificationDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableNetworkPortalNotification,
         chromeos::switches::kDisableNetworkPortalNotification)},
    {"enable-captive-portal-random-url",
     flag_descriptions::kEnableCaptivePortalRandomUrl,
     flag_descriptions::kEnableCaptivePortalRandomUrlDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableCaptivePortalRandomUrl)},
    {"disable-explicit-dma-fences",
     flag_descriptions::kDisableExplicitDmaFencesName,
     flag_descriptions::kDisableExplicitDmaFencesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableExplicitDmaFences)},
#endif  // OS_CHROMEOS
#if BUILDFLAG(ENABLE_PLUGINS)
    {"allow-nacl-socket-api", flag_descriptions::kAllowNaclSocketApiName,
     flag_descriptions::kAllowNaclSocketApiDescription, kOsDesktop,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kAllowNaClSocketAPI, "*")},
#endif  // ENABLE_PLUGINS
#if defined(OS_CHROMEOS)
    {"ash-enable-cursor-motion-blur",
     flag_descriptions::kEnableCursorMotionBlurName,
     flag_descriptions::kEnableCursorMotionBlurDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshEnableCursorMotionBlur)},
    {"ash-enable-docked-magnifier",
     flag_descriptions::kEnableDockedMagnifierName,
     flag_descriptions::kEnableDockedMagnifierDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDockedMagnifier)},
    {"ash-enable-night-light", flag_descriptions::kEnableNightLightName,
     flag_descriptions::kEnableNightLightDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNightLight)},
    {"ash-enable-notification-scroll-bar",
     flag_descriptions::kEnableNotificationScrollBarName,
     flag_descriptions::kEnableNotificationScrollBarDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNotificationScrollBar)},
    {"allow-touchpad-three-finger-click",
     flag_descriptions::kAllowTouchpadThreeFingerClickName,
     flag_descriptions::kAllowTouchpadThreeFingerClickDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchpadThreeFingerClick)},
    {"ash-enable-unified-desktop",
     flag_descriptions::kAshEnableUnifiedDesktopName,
     flag_descriptions::kAshEnableUnifiedDesktopDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUnifiedDesktop)},
    {
        "disable-office-editing-component-app",
        flag_descriptions::kOfficeEditingComponentAppName,
        flag_descriptions::kOfficeEditingComponentAppDescription, kOsCrOS,
        SINGLE_DISABLE_VALUE_TYPE(
            chromeos::switches::kDisableOfficeEditingComponentApp),
    },
    {"enable_android_messages_integration",
     flag_descriptions::kAndroidMessagesIntegrationName,
     flag_descriptions::kAndroidMessagesIntegrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAndroidMessagesIntegration)},
    {"enable_android_messages_prod_endpoint",
     flag_descriptions::kAndroidMessagesProdEndpointName,
     flag_descriptions::kAndroidMessagesProdEndpointDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kAndroidMessagesProdEndpoint)},
    {
        "enable-background-blur", flag_descriptions::kEnableBackgroundBlurName,
        flag_descriptions::kEnableBackgroundBlurDescription, kOsCrOS,
        FEATURE_VALUE_TYPE(app_list_features::kEnableBackgroundBlur),
    },
    {"enable-touchable-app-context-menu",
     flag_descriptions::kTouchableAppContextMenuName,
     flag_descriptions::kTouchableAppContextMenuDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTouchableAppContextMenu)},
    {"enable-notification-indicator",
     flag_descriptions::kNotificationIndicatorName,
     flag_descriptions::kNotificationIndicatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNotificationIndicator)},
    {"enable-app-list-search-autocomplete",
     flag_descriptions::kEnableAppListSearchAutocompleteName,
     flag_descriptions::kEnableAppListSearchAutocompleteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppListSearchAutocomplete)},
    {
        "enable-pinch", flag_descriptions::kPinchScaleName,
        flag_descriptions::kPinchScaleDescription, kOsLinux | kOsWin | kOsCrOS,
        ENABLE_DISABLE_VALUE_TYPE(switches::kEnablePinch,
                                  switches::kDisablePinch),
    },
    {"enable_unified_multidevice_settings",
     flag_descriptions::kEnableUnifiedMultiDeviceSettingsName,
     flag_descriptions::kEnableUnifiedMultiDeviceSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableUnifiedMultiDeviceSettings)},
    {"enable_unified_multidevice_setup",
     flag_descriptions::kEnableUnifiedMultiDeviceSetupName,
     flag_descriptions::kEnableUnifiedMultiDeviceSetupDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kEnableUnifiedMultiDeviceSetup)},
    {"enable-video-player-chromecast-support",
     flag_descriptions::kVideoPlayerChromecastSupportName,
     flag_descriptions::kVideoPlayerChromecastSupportDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kEnableVideoPlayerChromecastSupport)},
    {"hide-active-apps-from-shelf",
     flag_descriptions::kHideActiveAppsFromShelfName,
     flag_descriptions::kHideActiveAppsFromShelfDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kHideActiveAppsFromShelf)},
    {"instant-tethering", flag_descriptions::kTetherName,
     flag_descriptions::kTetherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kInstantTethering)},
    {"multidevice-service", flag_descriptions::kMultiDeviceApiName,
     flag_descriptions::kMultiDeviceApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMultiDeviceApi)},
    {"mash", flag_descriptions::kUseMashName,
     flag_descriptions::kUseMashDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMash)},
    {"newblue", flag_descriptions::kNewblueName,
     flag_descriptions::kNewblueDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(device::kNewblueDaemon)},
    {"unfiltered-bluetooth-devices",
     flag_descriptions::kUnfilteredBluetoothDevicesName,
     flag_descriptions::kUnfilteredBluetoothDevicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(device::kUnfilteredBluetoothDevices)},
    {"shelf-hover-previews", flag_descriptions::kShelfHoverPreviewsName,
     flag_descriptions::kShelfHoverPreviewsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kShelfHoverPreviews)},
    {"show-taps", flag_descriptions::kShowTapsName,
     flag_descriptions::kShowTapsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShowTaps)},
    {"show-touch-hud", flag_descriptions::kShowTouchHudName,
     flag_descriptions::kShowTouchHudDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {"single-process-mash", flag_descriptions::kSingleProcessMashName,
     flag_descriptions::kSingleProcessMashDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSingleProcessMash)},
#endif  // OS_CHROMEOS
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
#if defined(OS_WIN)
    {"enable-hdr", flag_descriptions::kEnableHDRName,
     flag_descriptions::kEnableHDRDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kHighDynamicRange)},
#endif  // OS_WIN
#if defined(OS_CHROMEOS)
    {
        "ash-debug-shortcuts", flag_descriptions::kDebugShortcutsName,
        flag_descriptions::kDebugShortcutsDescription, kOsAll,
        SINGLE_VALUE_TYPE(ash::switches::kAshDebugShortcuts),
    },
    {
        "ash-enable-mirrored-screen",
        flag_descriptions::kAshEnableMirroredScreenName,
        flag_descriptions::kAshEnableMirroredScreenDescription, kOsCrOS,
        SINGLE_VALUE_TYPE(ash::switches::kAshEnableMirroredScreen),
    },
    {"ash-shelf-color", flag_descriptions::kAshShelfColorName,
     flag_descriptions::kAshShelfColorDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAshShelfColorChoices)},
    {"ash-shelf-color-scheme", flag_descriptions::kAshShelfColorScheme,
     flag_descriptions::kAshShelfColorSchemeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAshShelfColorSchemeChoices)},
    {"material-design-ink-drop-animation-speed",
     flag_descriptions::kMaterialDesignInkDropAnimationSpeedName,
     flag_descriptions::kMaterialDesignInkDropAnimationSpeedDescription,
     kOsCrOS, MULTI_VALUE_TYPE(kAshMaterialDesignInkDropAnimationSpeed)},
    {"ui-slow-animations", flag_descriptions::kUiSlowAnimationsName,
     flag_descriptions::kUiSlowAnimationsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kUISlowAnimations)},
    {"ui-show-composited-layer-borders",
     flag_descriptions::kUiShowCompositedLayerBordersName,
     flag_descriptions::kUiShowCompositedLayerBordersDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kUiShowCompositedLayerBordersChoices)},
    {"disable-cloud-import", flag_descriptions::kCloudImportName,
     flag_descriptions::kCloudImportDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableCloudImport)},
    {"enable-request-tablet-site", flag_descriptions::kRequestTabletSiteName,
     flag_descriptions::kRequestTabletSiteDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableRequestTabletSite)},
#endif  // OS_CHROMEOS
    {"debug-packed-apps", flag_descriptions::kDebugPackedAppName,
     flag_descriptions::kDebugPackedAppDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kDebugPackedApps)},
    {"automatic-password-generation",
     flag_descriptions::kAutomaticPasswordGenerationName,
     flag_descriptions::kAutomaticPasswordGenerationDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutomaticPasswordGeneration)},
    {"new-password-form-parsing",
     flag_descriptions::kNewPasswordFormParsingName,
     flag_descriptions::kNewPasswordFormParsingDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kNewPasswordFormParsing)},
    {"new-password-form-parsing-for-saving",
     flag_descriptions::kNewPasswordFormParsingForSavingName,
     flag_descriptions::kNewPasswordFormParsingForSavingDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kNewPasswordFormParsingForSaving)},
    {"enable-show-autofill-signatures",
     flag_descriptions::kShowAutofillSignaturesName,
     flag_descriptions::kShowAutofillSignaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(autofill::switches::kShowAutofillSignatures)},
    {"AffiliationBasedMatching",
     flag_descriptions::kAffiliationBasedMatchingName,
     flag_descriptions::kAffiliationBasedMatchingDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kAffiliationBasedMatching)},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription,
     kOsAndroid | kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
#if defined(USE_AURA)
    {"overscroll-history-navigation",
     flag_descriptions::kOverscrollHistoryNavigationName,
     flag_descriptions::kOverscrollHistoryNavigationDescription, kOsAura,
     FEATURE_VALUE_TYPE(features::kOverscrollHistoryNavigation)},
    {"overscroll-start-threshold",
     flag_descriptions::kOverscrollStartThresholdName,
     flag_descriptions::kOverscrollStartThresholdDescription, kOsAura,
     MULTI_VALUE_TYPE(kOverscrollStartThresholdChoices)},
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
    {"enable-navigation-tracing",
     flag_descriptions::kEnableNavigationTracingName,
     flag_descriptions::kEnableNavigationTracingDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNavigationTracing)},
    {"trace-upload-url", flag_descriptions::kTraceUploadUrlName,
     flag_descriptions::kTraceUploadUrlDescription, kOsAll,
     MULTI_VALUE_TYPE(kTraceUploadURL)},
    {"enable-service-worker-servicification",
     flag_descriptions::kServiceWorkerServicificationName,
     flag_descriptions::kServiceWorkerServicificationDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kServiceWorkerServicification)},
    {"enable-suggestions-with-substring-match",
     flag_descriptions::kSuggestionsWithSubStringMatchName,
     flag_descriptions::kSuggestionsWithSubStringMatchDescription, kOsAll,
     SINGLE_VALUE_TYPE(
         autofill::switches::kEnableSuggestionsWithSubstringMatch)},
    {"lcd-text-aa", flag_descriptions::kLcdTextName,
     flag_descriptions::kLcdTextDescription, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableLCDText,
                               switches::kDisableLCDText)},
    {"enable-offer-store-unmasked-wallet-cards",
     flag_descriptions::kOfferStoreUnmaskedWalletCardsName,
     flag_descriptions::kOfferStoreUnmaskedWalletCardsDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(
         autofill::switches::kEnableOfferStoreUnmaskedWalletCards,
         autofill::switches::kDisableOfferStoreUnmaskedWalletCards)},
    {"enable-offline-auto-reload", flag_descriptions::kOfflineAutoReloadName,
     flag_descriptions::kOfflineAutoReloadDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReload,
                               switches::kDisableOfflineAutoReload)},
    {"enable-offline-auto-reload-visible-only",
     flag_descriptions::kOfflineAutoReloadVisibleOnlyName,
     flag_descriptions::kOfflineAutoReloadVisibleOnlyDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableOfflineAutoReloadVisibleOnly,
                               switches::kDisableOfflineAutoReloadVisibleOnly)},
    {"show-saved-copy", flag_descriptions::kShowSavedCopyName,
     flag_descriptions::kShowSavedCopyDescription, kOsAll,
     MULTI_VALUE_TYPE(kShowSavedCopyChoices)},
    {"default-tile-width", flag_descriptions::kDefaultTileWidthName,
     flag_descriptions::kDefaultTileWidthDescription, kOsAll,
     MULTI_VALUE_TYPE(kDefaultTileWidthChoices)},
    {"default-tile-height", flag_descriptions::kDefaultTileHeightName,
     flag_descriptions::kDefaultTileHeightDescription, kOsAll,
     MULTI_VALUE_TYPE(kDefaultTileHeightChoices)},
#if defined(OS_CHROMEOS)
    {"enable-virtual-keyboard", flag_descriptions::kVirtualKeyboardName,
     flag_descriptions::kVirtualKeyboardDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(keyboard::switches::kEnableVirtualKeyboard)},
    {"virtual-keyboard-overscroll",
     flag_descriptions::kVirtualKeyboardOverscrollName,
     flag_descriptions::kVirtualKeyboardOverscrollDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         keyboard::switches::kDisableVirtualKeyboardOverscroll)},
    {"input-view", flag_descriptions::kInputViewName,
     flag_descriptions::kInputViewDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableInputView)},
    {"disable-new-korean-ime", flag_descriptions::kNewKoreanImeName,
     flag_descriptions::kNewKoreanImeDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableNewKoreanIme)},
    {"enable-physical-keyboard-autocorrect",
     flag_descriptions::kPhysicalKeyboardAutocorrectName,
     flag_descriptions::kPhysicalKeyboardAutocorrectDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnablePhysicalKeyboardAutocorrect,
         chromeos::switches::kDisablePhysicalKeyboardAutocorrect)},
    {"disable-voice-input", flag_descriptions::kVoiceInputName,
     flag_descriptions::kVoiceInputDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableVoiceInput)},
    {"gesture-typing", flag_descriptions::kGestureTypingName,
     flag_descriptions::kGestureTypingDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableGestureTyping)},
    {"gesture-editing", flag_descriptions::kGestureEditingName,
     flag_descriptions::kGestureEditingDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(keyboard::switches::kDisableGestureEditing)},
#endif  // OS_CHROMEOS
    {"enable-simple-cache-backend", flag_descriptions::kSimpleCacheBackendName,
     flag_descriptions::kSimpleCacheBackendDescription,
     kOsWin | kOsMac | kOsLinux | kOsCrOS,
     MULTI_VALUE_TYPE(kSimpleCacheBackendChoices)},
    {"enable-tcp-fast-open", flag_descriptions::kTcpFastOpenName,
     flag_descriptions::kTcpFastOpenDescription,
     kOsLinux | kOsCrOS | kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableTcpFastOpen)},
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    {"device-discovery-notifications",
     flag_descriptions::kDeviceDiscoveryNotificationsName,
     flag_descriptions::kDeviceDiscoveryNotificationsDescription, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableDeviceDiscoveryNotifications,
                               switches::kDisableDeviceDiscoveryNotifications)},
    {"enable-print-preview-register-promos",
     flag_descriptions::kPrintPreviewRegisterPromosName,
     flag_descriptions::kPrintPreviewRegisterPromosDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnablePrintPreviewRegisterPromos)},
#endif  // BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#if defined(OS_WIN)
    {"enable-cloud-print-xps", flag_descriptions::kCloudPrintXpsName,
     flag_descriptions::kCloudPrintXpsDescription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kEnableCloudPrintXps)},
#endif  // OS_WIN
#if BUILDFLAG(ENABLE_SPELLCHECK)
    {"enable-spelling-feedback-field-trial",
     flag_descriptions::kSpellingFeedbackFieldTrialName,
     flag_descriptions::kSpellingFeedbackFieldTrialDescription, kOsAll,
     SINGLE_VALUE_TYPE(
         spellcheck::switches::kEnableSpellingFeedbackFieldTrial)},
#endif  // ENABLE_SPELLCHECK
    {"enable-webgl-draft-extensions",
     flag_descriptions::kWebglDraftExtensionsName,
     flag_descriptions::kWebglDraftExtensionsDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGLDraftExtensions)},
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"account-consistency", flag_descriptions::kAccountConsistencyName,
     flag_descriptions::kAccountConsistencyDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAccountConsistencyFeature,
                                    kAccountConsistencyFeatureVariations,
                                    "AccountConsistencyVariations")},
#endif
#if BUILDFLAG(ENABLE_APP_LIST)
    {"reset-app-list-install-state",
     flag_descriptions::kResetAppListInstallStateName,
     flag_descriptions::kResetAppListInstallStateDescription,
     kOsMac | kOsWin | kOsLinux,
     SINGLE_VALUE_TYPE(app_list::switches::kResetAppListInstallState)},
#endif  // BUILDFLAG(ENABLE_APP_LIST)
#if defined(OS_ANDROID)
    {"enable-special-locale", flag_descriptions::kEnableSpecialLocaleName,
     flag_descriptions::kEnableSpecialLocaleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSpecialLocaleFeature)},
    {"enable-accessibility-tab-switcher",
     flag_descriptions::kAccessibilityTabSwitcherName,
     flag_descriptions::kAccessibilityTabSwitcherDescription, kOsAndroid,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityTabSwitcher)},
    {"enable-android-autofill-accessibility",
     flag_descriptions::kAndroidAutofillAccessibilityName,
     flag_descriptions::kAndroidAutofillAccessibilityDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidAutofillAccessibility)},
#endif  // OS_ANDROID
    {"enable-zero-copy", flag_descriptions::kZeroCopyName,
     flag_descriptions::kZeroCopyDescription, kOsAll,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableZeroCopy,
                               switches::kDisableZeroCopy)},
#if defined(OS_CHROMEOS)
    {"enable-first-run-ui-transitions",
     flag_descriptions::kFirstRunUiTransitionsName,
     flag_descriptions::kFirstRunUiTransitionsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableFirstRunUITransitions)},
#endif  // OS_CHROMEOS
#if defined(OS_MACOSX)
    {"bookmark-apps", flag_descriptions::kNewBookmarkAppsName,
     flag_descriptions::kNewBookmarkAppsDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kBookmarkApps)},
    {"disable-hosted-apps-in-windows",
     flag_descriptions::kHostedAppsInWindowsName,
     flag_descriptions::kHostedAppsInWindowsDescription, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableHostedAppsInWindows,
                               switches::kDisableHostedAppsInWindows)},
    {"create-app-windows-in-app",
     flag_descriptions::kCreateAppWindowsInAppShimProcessName,
     flag_descriptions::kCreateAppWindowsInAppShimProcessDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kHostWindowsInAppShimProcess)},
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
    {"disable-pull-to-refresh-effect",
     flag_descriptions::kPullToRefreshEffectName,
     flag_descriptions::kPullToRefreshEffectDescription, kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisablePullToRefreshEffect)},
    {"translate-force-trigger-on-english",
     flag_descriptions::kTranslateForceTriggerOnEnglishName,
     flag_descriptions::kTranslateForceTriggerOnEnglishDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         language::kOverrideTranslateTriggerInIndia,
         kTranslateForceTriggerOnEnglishVariations,
         language::kOverrideTranslateTriggerInIndia.name)},
    {"translate-explicit-ask",
     flag_descriptions::kTranslateExplicitLanguageAskName,
     flag_descriptions::kTranslateExplicitLanguageAskDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(language::kExplicitLanguageAsk)},
#endif  // OS_ANDROID
    {"translate-ranker-enforcement",
     flag_descriptions::kTranslateRankerEnforcementName,
     flag_descriptions::kTranslateRankerEnforcementDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTranslateRankerEnforcement)},
    {"translate", flag_descriptions::kTranslateUIName,
     flag_descriptions::kTranslateUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kTranslateUI)},
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
    {"chrome-home-swipe-logic", flag_descriptions::kChromeHomeSwipeLogicName,
     flag_descriptions::kChromeHomeSwipeLogicDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kChromeHomeSwipeLogicChoices)},
    {"enable-chrome-memex", flag_descriptions::kChromeMemexName,
     flag_descriptions::kChromeMemexDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeMemexFeature)},
    {"force-enable-home-page-button", flag_descriptions::kHomePageButtonName,
     flag_descriptions::kHomePageButtonDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHomePageButtonForceEnabled)},
    {"enable-ntp-button", flag_descriptions::kNtpButtonName,
     flag_descriptions::kNtpButtonDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNTPButton)},
    {"enable-homepage-tile", flag_descriptions::kHomepageTileName,
     flag_descriptions::kHomepageTileDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHomepageTile)},
#endif  // OS_ANDROID
#if defined(OS_ANDROID)
    {"enable-tab-modal-js-dialog-android",
     flag_descriptions::kTabModalJsDialogName,
     flag_descriptions::kTabModalJsDialogDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabModalJsDialog)},
    {"enable-modal-permission-dialog-view",
     flag_descriptions::kModalPermissionDialogViewName,
     flag_descriptions::kModalPermissionDialogViewDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kModalPermissionDialogView)},
#endif  // OS_ANDROID
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeChoiceName,
     flag_descriptions::kInProductHelpDemoModeChoiceDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"num-raster-threads", flag_descriptions::kNumRasterThreadsName,
     flag_descriptions::kNumRasterThreadsDescription, kOsAll,
     MULTI_VALUE_TYPE(kNumRasterThreadsChoices)},
    {"disable-cast-streaming-hw-encoding",
     flag_descriptions::kCastStreamingHwEncodingName,
     flag_descriptions::kCastStreamingHwEncodingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableCastStreamingHWEncoding)},
    {"disable-threaded-scrolling", flag_descriptions::kThreadedScrollingName,
     flag_descriptions::kThreadedScrollingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableThreadedScrolling)},
    {"extension-content-verification",
     flag_descriptions::kExtensionContentVerificationName,
     flag_descriptions::kExtensionContentVerificationDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionContentVerificationChoices)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"extension-active-script-permission",
     flag_descriptions::kUserConsentForExtensionScriptsName,
     flag_descriptions::kUserConsentForExtensionScriptsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(extensions_features::kRuntimeHostPermissions)},
#endif  // ENABLE_EXTENSIONS
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-embedded-extension-options",
     flag_descriptions::kEmbeddedExtensionOptionsName,
     flag_descriptions::kEmbeddedExtensionOptionsDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(extensions::switches::kEnableEmbeddedExtensionOptions)},
#endif  // ENABLE_EXTENSIONS
#if !defined(OS_ANDROID)
#if defined(OS_CHROMEOS)
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // OS_CHROMEOS
    {"enable-message-center-new-style-notification",
     flag_descriptions::kMessageCenterNewStyleNotificationName,
     flag_descriptions::kMessageCenterNewStyleNotificationDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(message_center::kNewStyleNotifications)},
    {"enable-policy-tool", flag_descriptions::kEnablePolicyToolName,
     flag_descriptions::kEnablePolicyToolDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPolicyTool)},
#endif  // !OS_ANDROID
#if defined(OS_CHROMEOS)
    {"memory-pressure-thresholds",
     flag_descriptions::kMemoryPressureThresholdName,
     flag_descriptions::kMemoryPressureThresholdDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kMemoryPressureThresholdChoices)},
    {"wake-on-wifi-packet", flag_descriptions::kWakeOnPacketsName,
     flag_descriptions::kWakeOnPacketsDescription, kOsCrOSOwnerOnly,
     SINGLE_VALUE_TYPE(chromeos::switches::kWakeOnWifiPacket)},
#endif  // OS_CHROMEOS
    {"enable-memory-coordinator", flag_descriptions::kMemoryCoordinatorName,
     flag_descriptions::kMemoryCoordinatorDescription,
     kOsAndroid | kOsCrOS | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kMemoryCoordinator)},
    {"reduced-referrer-granularity",
     flag_descriptions::kReducedReferrerGranularityName,
     flag_descriptions::kReducedReferrerGranularityDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kReducedReferrerGranularity)},
#if defined(OS_CHROMEOS)
    {"disable-new-zip-unpacker", flag_descriptions::kNewZipUnpackerName,
     flag_descriptions::kNewZipUnpackerDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableNewZIPUnpacker)},
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
    {"enable-credit-card-assist", flag_descriptions::kCreditCardAssistName,
     flag_descriptions::kCreditCardAssistDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCreditCardAssist)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS)
    {"disable-captive-portal-bypass-proxy",
     flag_descriptions::kCaptivePortalBypassProxyName,
     flag_descriptions::kCaptivePortalBypassProxyDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kDisableCaptivePortalBypassProxy)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"enable-wifi-credential-sync", flag_descriptions::kWifiCredentialSyncName,
     flag_descriptions::kWifiCredentialSyncDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableWifiCredentialSync)},
    {"enable-potentially-annoying-security-features",
     flag_descriptions::kExperimentalSecurityFeaturesName,
     flag_descriptions::kExperimentalSecurityFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnablePotentiallyAnnoyingSecurityFeatures)},
#endif  // OS_CHROMEOS
    {"ssl-committed-interstitials",
     flag_descriptions::kSSLCommittedInterstitialsName,
     flag_descriptions::kSSLCommittedInterstitialsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSSLCommittedInterstitials)},
    {"enable-site-per-process", flag_descriptions::kStrictSiteIsolationName,
     flag_descriptions::kStrictSiteIsolationDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kSitePerProcess)},
    {"site-isolation-trial-opt-out",
     flag_descriptions::kSiteIsolationTrialOptOutName,
     flag_descriptions::kSiteIsolationTrialOptOutDescription, kOsAll,
     MULTI_VALUE_TYPE(kSiteIsolationTrialOptOutChoices)},
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
    {"enable-data-reduction-proxy-server-experiment",
     flag_descriptions::kEnableDataReductionProxyServerExperimentName,
     flag_descriptions::kEnableDataReductionProxyServerExperimentDescription,
     kOsAll, MULTI_VALUE_TYPE(kDataReductionProxyServerExperiment)},
#if defined(OS_ANDROID)
    {"enable-data-reduction-proxy-savings-promo",
     flag_descriptions::kEnableDataReductionProxySavingsPromoName,
     flag_descriptions::kEnableDataReductionProxySavingsPromoDescription,
     kOsAndroid,
     SINGLE_VALUE_TYPE(data_reduction_proxy::switches::
                           kEnableDataReductionProxySavingsPromo)},
    {"enable-data-reduction-proxy-main-menu",
     flag_descriptions::kEnableDataReductionProxyMainMenuName,
     flag_descriptions::kEnableDataReductionProxyMainMenuDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         data_reduction_proxy::features::kDataReductionMainMenu,
         kDataReductionMainMenuFeatureVariations,
         "DataReductionProxyMainMenu")},
    {"enable-offline-previews", flag_descriptions::kEnableOfflinePreviewsName,
     flag_descriptions::kEnableOfflinePreviewsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(previews::features::kOfflinePreviews)},
    {"enable-previews-android-omnibox-ui",
     flag_descriptions::kEnablePreviewsAndroidOmniboxUIName,
     flag_descriptions::kEnablePreviewsAndroidOmniboxUIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(previews::features::kAndroidOmniboxPreviewsBadge)},
    {"enable-lite-page-server-previews",
     flag_descriptions::kEnableLitePageServerPreviewsName,
     flag_descriptions::kEnableLitePageServerPreviewsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(previews::features::kLitePageServerPreviews)},
#endif  // OS_ANDROID
    {"enable-client-lo-fi", flag_descriptions::kEnableClientLoFiName,
     flag_descriptions::kEnableClientLoFiDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kClientLoFi)},
    {"enable-noscript-previews", flag_descriptions::kEnableNoScriptPreviewsName,
     flag_descriptions::kEnableNoScriptPreviewsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kNoScriptPreviews)},
    {"enable-resource-loading-hints",
     flag_descriptions::kEnableResourceLoadingHintsName,
     flag_descriptions::kEnableResourceLoadingHintsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kResourceLoadingHints)},
    {"enable-optimization-hints",
     flag_descriptions::kEnableOptimizationHintsName,
     flag_descriptions::kEnableOptimizationHintsDescription, kOsAll,
     FEATURE_VALUE_TYPE(previews::features::kOptimizationHints)},
    {"enable-heavy-page-capping",
     flag_descriptions::kEnableHeavyPageCappingName,
     flag_descriptions::kEnableHeavyPageCappingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(data_use_measurement::page_load_capping::
                                        features::kDetectingHeavyPages,
                                    kDetectingHeavyPagesFeatureVariations,
                                    "DetectingHeavyPages")},
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
#if !defined(OS_ANDROID)
    {"enable-app-banners", flag_descriptions::kAppBannersName,
     flag_descriptions::kAppBannersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAppBanners)},
#endif  // !OS_ANDROID
    {"enable-experimental-app-banners",
     flag_descriptions::kExperimentalAppBannersName,
     flag_descriptions::kExperimentalAppBannersDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalAppBanners)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kBypassAppBannerEngagementChecks)},
    {"enable-desktop-pwas", flag_descriptions::kEnableDesktopPWAsName,
     flag_descriptions::kEnableDesktopPWAsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAWindowing)},
    {"enable-desktop-pwas-link-capturing",
     flag_descriptions::kEnableDesktopPWAsLinkCapturingName,
     flag_descriptions::kEnableDesktopPWAsLinkCapturingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsLinkCapturing)},
    {"enable-system-webapps", flag_descriptions::kEnableSystemWebAppsName,
     flag_descriptions::kEnableSystemWebAppsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemWebApps)},
    {"enable-desktop-pwas-stay-in-window",
     flag_descriptions::kDesktopPWAsStayInWindowName,
     flag_descriptions::kDesktopPWAsStayInWindowDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsStayInWindow)},
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
#endif  // !OS_ANDROID
// Since Drive Search is not available when app list is disabled, flag guard
// enable-drive-search-in-chrome-launcher flag.
#if BUILDFLAG(ENABLE_APP_LIST)
    {"enable-drive-search-in-app-launcher",
     flag_descriptions::kDriveSearchInChromeLauncherName,
     flag_descriptions::kDriveSearchInChromeLauncherDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         app_list::switches::kEnableDriveSearchInChromeLauncher,
         app_list::switches::kDisableDriveSearchInChromeLauncher)},
#endif  // BUILDFLAG(ENABLE_APP_LIST)
#if defined(OS_CHROMEOS)
    {"disable-mtp-write-support", flag_descriptions::kMtpWriteSupportName,
     flag_descriptions::kMtpWriteSupportDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableMtpWriteSupport)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"enable-datasaver-prompt", flag_descriptions::kDatasaverPromptName,
     flag_descriptions::kDatasaverPromptDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kDataSaverPromptChoices)},
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
    {"autofill-keyboard-accessory-view",
     flag_descriptions::kAutofillAccessoryViewName,
     flag_descriptions::kAutofillAccessoryViewDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(autofill::kAutofillKeyboardAccessory,
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
    {"mac-v2-sandbox", flag_descriptions::kMacV2SandboxName,
     flag_descriptions::kMacV2SandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacV2Sandbox)},
    {"mac-views-task-manager", flag_descriptions::kMacViewsTaskManagerName,
     flag_descriptions::kMacViewsTaskManagerDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kViewsTaskManager)},
#endif  // OS_MACOSX
    {"enable-gamepad-extensions", flag_descriptions::kGamepadExtensionsName,
     flag_descriptions::kGamepadExtensionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGamepadExtensions)},
    {"enable-gamepad-vibration", flag_descriptions::kGamepadVibrationName,
     flag_descriptions::kGamepadVibrationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGamepadVibration)},
    {"enable-webvr", flag_descriptions::kWebvrName,
     flag_descriptions::kWebvrDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebVR)},
    {"webxr", flag_descriptions::kWebXrName,
     flag_descriptions::kWebXrDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXr)},
    {"webxr-gamepad-support", flag_descriptions::kWebXrGamepadSupportName,
     flag_descriptions::kWebXrGamepadSupportDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrGamepadSupport)},
    {"webxr-orientation-sensor-device",
     flag_descriptions::kWebXrOrientationSensorDeviceName,
     flag_descriptions::kWebXrOrientationSensorDeviceDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrOrientationSensorDevice)},
    {"webxr-hit-test", flag_descriptions::kWebXrHitTestName,
     flag_descriptions::kWebXrHitTestDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebXrHitTest)},
#if BUILDFLAG(ENABLE_VR)
    {"webvr-vsync-align", flag_descriptions::kWebVrVsyncAlignName,
     flag_descriptions::kWebVrVsyncAlignDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kWebVrVsyncAlign)},
#if defined(OS_ANDROID)
    {"webxr-render-path", flag_descriptions::kWebXrRenderPathName,
     flag_descriptions::kWebXrRenderPathDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kWebXrRenderPath,
                                    kWebXrRenderPathVariations,
                                    "WebXrRenderPath")},
    // TODO(crbug.com/731802): Only these should be #if defined(OS_ANDROID).
    {"vr-browsing-tabs-view", flag_descriptions::kVrBrowsingTabsViewName,
     flag_descriptions::kVrBrowsingTabsViewDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kVrBrowsingTabsView)},
#endif  // OS_ANDROID
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
#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
    {"xr-sandbox", flag_descriptions::kXRSandboxName,
     flag_descriptions::kXRSandboxDescription, kOsWin,
     FEATURE_VALUE_TYPE(service_manager::features::kXRSandbox)},
#endif  // ENABLE_ISOLATED_XR_SERVICE
#endif  // ENABLE_VR
#if defined(OS_CHROMEOS)
    {"disable-accelerated-mjpeg-decode",
     flag_descriptions::kAcceleratedMjpegDecodeName,
     flag_descriptions::kAcceleratedMjpegDecodeDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedMjpegDecode)},
#endif  // OS_CHROMEOS
    {"v8-cache-options", flag_descriptions::kV8CacheOptionsName,
     flag_descriptions::kV8CacheOptionsDescription, kOsAll,
     MULTI_VALUE_TYPE(kV8CacheOptionsChoices)},
    {"keyboard-lock-api", flag_descriptions::kKeyboardLockApiName,
     flag_descriptions::kKeyboardLockApiDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kKeyboardLockAPI)},
    {"system-keyboard-lock", flag_descriptions::kSystemKeyboardLockName,
     flag_descriptions::kSystemKeyboardLockDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSystemKeyboardLock)},
#if defined(OS_ANDROID)
    {"progress-bar-throttle", flag_descriptions::kProgressBarThrottleName,
     flag_descriptions::kProgressBarThrottleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kProgressBarThrottleFeature)},
#endif  // OS_ANDROID
    {"save-previous-document-resources-until",
     flag_descriptions::kSavePreviousDocumentResourcesName,
     flag_descriptions::kSavePreviousDocumentResourcesDescription, kOsAll,
     MULTI_VALUE_TYPE(kSavePreviousDocumentResourcesChoices)},
#if defined(OS_ANDROID)
    {"offline-bookmarks", flag_descriptions::kOfflineBookmarksName,
     flag_descriptions::kOfflineBookmarksDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflineBookmarksFeature)},
    {"offline-pages-load-signal-collecting",
     flag_descriptions::kOfflinePagesLoadSignalCollectingName,
     flag_descriptions::kOfflinePagesLoadSignalCollectingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesLoadSignalCollectingFeature)},
    {"offline-pages-sharing", flag_descriptions::kOfflinePagesSharingName,
     flag_descriptions::kOfflinePagesSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesSharingFeature)},
    {"offline-pages-live-page-sharing",
     flag_descriptions::kOfflinePagesLivePageSharingName,
     flag_descriptions::kOfflinePagesLivePageSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kOfflinePagesLivePageSharingFeature)},
    {"offline-pages-prefetching",
     flag_descriptions::kOfflinePagesPrefetchingName,
     flag_descriptions::kOfflinePagesPrefetchingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kPrefetchingOfflinePagesFeature)},
    {"offline-pages-limitless-prefetching",
     flag_descriptions::kOfflinePagesLimitlessPrefetchingName,
     flag_descriptions::kOfflinePagesLimitlessPrefetchingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesLimitlessPrefetchingFeature)},
    {"background-loader-for-downloads",
     flag_descriptions::kBackgroundLoaderForDownloadsName,
     flag_descriptions::kBackgroundLoaderForDownloadsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(offline_pages::kBackgroundLoaderForDownloadsFeature)},
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
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         offline_pages::kOfflineIndicatorFeature,
         kOfflineIndicatorFeatureVariations,
         offline_pages::kOfflineIndicatorFeature.name)},
    {"offline-indicator-always-http-probe",
     flag_descriptions::kOfflineIndicatorAlwaysHttpProbeName,
     flag_descriptions::kOfflineIndicatorAlwaysHttpProbeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflineIndicatorAlwaysHttpProbeFeature)},
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
    {"trace-export-events-to-etw",
     flag_descriptions::kTraceExportEventsToEtwName,
     flag_descriptions::kTraceExportEventsToEtwDesription, kOsWin,
     SINGLE_VALUE_TYPE(switches::kTraceExportEventsToETW)},
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
    {"enable-appcontainer", flag_descriptions::kEnableAppcontainerName,
     flag_descriptions::kEnableAppcontainerDescription, kOsWin,
     ENABLE_DISABLE_VALUE_TYPE(
         service_manager::switches::kEnableAppContainer,
         service_manager::switches::kDisableAppContainer)},
#endif  // OS_WIN
#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
#endif  // TOOLKIT_VIEWS || OS_ANDROID
    {"enable-md-incognito-ntp",
     flag_descriptions::kMaterialDesignIncognitoNTPName,
     flag_descriptions::kMaterialDesignIncognitoNTPDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kMaterialDesignIncognitoNTP)},
    {"safe-search-url-reporting",
     flag_descriptions::kSafeSearchUrlReportingName,
     flag_descriptions::kSafeSearchUrlReportingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSafeSearchUrlReporting)},
    {"force-ui-direction", flag_descriptions::kForceUiDirectionName,
     flag_descriptions::kForceUiDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceUIDirectionChoices)},
    {"force-text-direction", flag_descriptions::kForceTextDirectionName,
     flag_descriptions::kForceTextDirectionDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceTextDirectionChoices)},
#if defined(OS_WIN) || defined(OS_LINUX)
    {"enable-input-ime-api", flag_descriptions::kEnableInputImeApiName,
     flag_descriptions::kEnableInputImeApiDescription, kOsWin | kOsLinux,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableInputImeAPI,
                               switches::kDisableInputImeAPI)},
#endif  // OS_WIN || OS_LINUX
    {"enable-origin-trials", flag_descriptions::kOriginTrialsName,
     flag_descriptions::kOriginTrialsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kOriginTrials)},
    {"enable-brotli", flag_descriptions::kEnableBrotliName,
     flag_descriptions::kEnableBrotliDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBrotliEncoding)},
    {"enable-image-capture-api", flag_descriptions::kEnableImageCaptureAPIName,
     flag_descriptions::kEnableImageCaptureAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kImageCaptureAPI)},
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
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kMarketUrlForTesting,
         "https://play.google.com/store/apps/details?id=com.android.chrome")},
#endif  // OS_ANDROID
#if defined(OS_WIN) || defined(OS_MACOSX)
    {"automatic-tab-discarding", flag_descriptions::kAutomaticTabDiscardingName,
     flag_descriptions::kAutomaticTabDiscardingDescription, kOsWin | kOsMac,
     FEATURE_VALUE_TYPE(features::kAutomaticTabDiscarding)},
#endif  // OS_WIN || OS_MACOSX
    {"tls13-variant", flag_descriptions::kTLS13VariantName,
     flag_descriptions::kTLS13VariantDescription, kOsAll,
     MULTI_VALUE_TYPE(kTLS13VariantChoices)},
    {"enforce-tls13-downgrade", flag_descriptions::kEnforceTLS13DowngradeName,
     flag_descriptions::kEnforceTLS13DowngradeDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnforceTLS13Downgrade)},
    {"enable-scroll-anchor-serialization",
     flag_descriptions::kEnableScrollAnchorSerializationName,
     flag_descriptions::kEnableScrollAnchorSerializationDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kScrollAnchorSerialization)},
    {"disable-audio-support-for-desktop-share",
     flag_descriptions::kDisableAudioForDesktopShareName,
     flag_descriptions::kDisableAudioForDesktopShareDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kDisableAudioSupportForDesktopShare)},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"tab-for-desktop-share", flag_descriptions::kDisableTabForDesktopShareName,
     flag_descriptions::kDisableTabForDesktopShareDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kDisableTabForDesktopShare)},
#endif  // ENABLE_EXTENSIONS
#if defined(OS_ANDROID)
    {"keep-prefetched-content-suggestions",
     flag_descriptions::kKeepPrefetchedContentSuggestionsName,
     flag_descriptions::kKeepPrefetchedContentSuggestionsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kKeepPrefetchedContentSuggestions)},
    {"content-suggestions-category-order",
     flag_descriptions::kContentSuggestionsCategoryOrderName,
     flag_descriptions::kContentSuggestionsCategoryOrderDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_snippets::kCategoryOrder,
         kContentSuggestionsCategoryOrderFeatureVariations,
         ntp_snippets::kCategoryOrder.name)},
    {"content-suggestions-category-ranker",
     flag_descriptions::kContentSuggestionsCategoryRankerName,
     flag_descriptions::kContentSuggestionsCategoryRankerDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_snippets::kCategoryRanker,
         kContentSuggestionsCategoryRankerFeatureVariations,
         ntp_snippets::kCategoryRanker.name)},
    {"content-suggestions-debug-log",
     flag_descriptions::kContentSuggestionsDebugLogName,
     flag_descriptions::kContentSuggestionsDebugLogDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kContentSuggestionsDebugLog)},
    {"enable-ntp-snippets-increased-visibility",
     flag_descriptions::kEnableNtpSnippetsVisibilityName,
     flag_descriptions::kEnableNtpSnippetsVisibilityDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kIncreasedVisibility)},
    {"contextual-suggestions-button",
     flag_descriptions::kContextualSuggestionsButtonName,
     flag_descriptions::kContextualSuggestionsButtonDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(contextual_suggestions::kContextualSuggestionsButton)},
    {"enable-contextual-suggestions-alternate-card-layout",
     flag_descriptions::kContextualSuggestionsAlternateCardLayoutName,
     flag_descriptions::kContextualSuggestionsAlternateCardLayoutDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         contextual_suggestions::kContextualSuggestionsAlternateCardLayout)},
    {"contextual-suggestions-iph-reverse-scroll",
     flag_descriptions::kContextualSuggestionsIPHReverseScrollName,
     flag_descriptions::kContextualSuggestionsIPHReverseScrollDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         contextual_suggestions::kContextualSuggestionsIPHReverseScroll)},
    {"contextual-suggestions-opt-out",
     flag_descriptions::kContextualSuggestionsOptOutName,
     flag_descriptions::kContextualSuggestionsOptOutDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(contextual_suggestions::kContextualSuggestionsOptOut)},
    {"enable-content-suggestions-new-favicon-server",
     flag_descriptions::kEnableContentSuggestionsNewFaviconServerName,
     flag_descriptions::kEnableContentSuggestionsNewFaviconServerDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kPublisherFaviconsFromNewServerFeature)},
    {"enable-content-suggestions-thumbnail-dominant-color",
     flag_descriptions::kEnableContentSuggestionsThumbnailDominantColorName,
     flag_descriptions::
         kEnableContentSuggestionsThumbnailDominantColorDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kContentSuggestionsThumbnailDominantColor)},
    {"interest-feed-content-suggestions",
     flag_descriptions::kInterestFeedContentSuggestionsName,
     flag_descriptions::kInterestFeedContentSuggestionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedContentSuggestions)},
    {"enable-ntp-remote-suggestions",
     flag_descriptions::kEnableNtpRemoteSuggestionsName,
     flag_descriptions::kEnableNtpRemoteSuggestionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_snippets::kArticleSuggestionsFeature,
         kRemoteSuggestionsFeatureVariations,
         ntp_snippets::kArticleSuggestionsFeature.name)},
    {"enable-ntp-asset-download-suggestions",
     flag_descriptions::kEnableNtpAssetDownloadSuggestionsName,
     flag_descriptions::kEnableNtpAssetDownloadSuggestionsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAssetDownloadSuggestionsFeature)},
    {"enable-ntp-offline-page-download-suggestions",
     flag_descriptions::kEnableNtpOfflinePageDownloadSuggestionsName,
     flag_descriptions::kEnableNtpOfflinePageDownloadSuggestionsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kOfflinePageDownloadSuggestionsFeature)},
    {"enable-ntp-bookmark-suggestions",
     flag_descriptions::kEnableNtpBookmarkSuggestionsName,
     flag_descriptions::kEnableNtpBookmarkSuggestionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_snippets::kBookmarkSuggestionsFeature)},
    {"enable-ntp-suggestions-notifications",
     flag_descriptions::kEnableNtpSuggestionsNotificationsName,
     flag_descriptions::kEnableNtpSuggestionsNotificationsDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_snippets::kNotificationsFeature,
         kContentSuggestionsNotificationsFeatureVariations,
         ntp_snippets::kNotificationsFeature.name)},
    {"simplified-ntp", flag_descriptions::kSimplifiedNtpName,
     flag_descriptions::kSimplifiedNtpDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSimplifiedNTP)},
    {"enable-site-exploration-ui", flag_descriptions::kSiteExplorationUiName,
     flag_descriptions::kSiteExplorationUiDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(ntp_tiles::kSiteExplorationUiFeature)},
#endif  // OS_ANDROID
    {"user-activation-v2", flag_descriptions::kUserActivationV2Name,
     flag_descriptions::kUserActivationV2Description, kOsAll,
     FEATURE_VALUE_TYPE(features::kUserActivationV2)},
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    {"enable-webrtc-h264-with-openh264-ffmpeg",
     flag_descriptions::kWebrtcH264WithOpenh264FfmpegName,
     flag_descriptions::kWebrtcH264WithOpenh264FfmpegDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(content::kWebRtcH264WithOpenH264FFmpeg)},
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#if defined(OS_ANDROID)
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
    {"protect-sync-credential", flag_descriptions::kProtectSyncCredentialName,
     flag_descriptions::kProtectSyncCredentialDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kProtectSyncCredential)},
    {"ProtectSyncCredentialOnReauth",
     flag_descriptions::kProtectSyncCredentialOnReauthName,
     flag_descriptions::kProtectSyncCredentialOnReauthDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kProtectSyncCredentialOnReauth)},
    {"PasswordImport", flag_descriptions::kPasswordImportName,
     flag_descriptions::kPasswordImportDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordImport)},
#if defined(OS_ANDROID)
    {"password-search", flag_descriptions::kPasswordSearchMobileName,
     flag_descriptions::kPasswordSearchMobileDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordSearchMobile)},
    {"passwords-keyboard-accessory",
     flag_descriptions::kPasswordsKeyboardAccessoryName,
     flag_descriptions::kPasswordsKeyboardAccessoryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordsKeyboardAccessory)},
#endif  // OS_ANDROID
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    {"passwords-migrate-linux-to-login-db",
     flag_descriptions::kPasswordsMigrateLinuxToLoginDBName,
     flag_descriptions::kPasswordsMigrateLinuxToLoginDBDescription, kOsLinux,
     FEATURE_VALUE_TYPE(password_manager::features::kMigrateLinuxToLoginDB)},
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
    {"enable-experimental-accessibility-features",
     flag_descriptions::kExperimentalAccessibilityFeaturesName,
     flag_descriptions::kExperimentalAccessibilityFeaturesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kEnableExperimentalAccessibilityFeatures)},
    {"opt-in-ime-menu", flag_descriptions::kEnableImeMenuName,
     flag_descriptions::kEnableImeMenuDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kOptInImeMenu)},
    {"disable-system-timezone-automatic-detection",
     flag_descriptions::kDisableSystemTimezoneAutomaticDetectionName,
     flag_descriptions::kDisableSystemTimezoneAutomaticDetectionDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         chromeos::switches::kDisableSystemTimezoneAutomaticDetectionPolicy)},
    {"enable-bulk-printers", flag_descriptions::kBulkPrintersName,
     flag_descriptions::kBulkPrintersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kBulkPrinters)},
    {"disable-cros-component", flag_descriptions::kCrOSComponentName,
     flag_descriptions::kCrOSComponentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCrOSComponent)},
    {"enable-encryption-migration",
     flag_descriptions::kEnableEncryptionMigrationName,
     flag_descriptions::kEnableEncryptionMigrationDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableEncryptionMigration,
         chromeos::switches::kDisableEncryptionMigration)},
#endif  // OS_CHROMEOS
#if !defined(OS_ANDROID) && defined(GOOGLE_CHROME_BUILD)
    {"enable-google-branded-context-menu",
     flag_descriptions::kGoogleBrandedContextMenuName,
     flag_descriptions::kGoogleBrandedContextMenuDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kEnableGoogleBrandedContextMenu)},
#endif  // !OS_ANDROID && GOOGLE_CHROME_BUILD
#if defined(OS_MACOSX)
    {"enable-fullscreen-in-tab-detaching",
     flag_descriptions::kTabDetachingInFullscreenName,
     flag_descriptions::kTabDetachingInFullscreenDescription, kOsMac,
     ENABLE_DISABLE_VALUE_TYPE(switches::kEnableFullscreenTabDetaching,
                               switches::kDisableFullscreenTabDetaching)},
    {"enable-content-fullscreen", flag_descriptions::kContentFullscreenName,
     flag_descriptions::kContentFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kContentFullscreen)},
    {"enable-fullscreen-toolbar-reveal",
     flag_descriptions::kFullscreenToolbarRevealName,
     flag_descriptions::kFullscreenToolbarRevealDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kFullscreenToolbarReveal)},
#endif  // OS_MACOSX
    {"remove-navigation-history",
     flag_descriptions::kRemoveNavigationHistoryName,
     flag_descriptions::kRemoveNavigationHistoryDescription, kOsAll,
     FEATURE_VALUE_TYPE(browsing_data::features::kRemoveNavigationHistory)},
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
    {"enable-font-cache-scaling", flag_descriptions::kFontCacheScalingName,
     flag_descriptions::kFontCacheScalingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFontCacheScaling)},
    {"enable-framebusting-needs-sameorigin-or-usergesture",
     flag_descriptions::kFramebustingName,
     flag_descriptions::kFramebustingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFramebustingNeedsSameOriginOrUserGesture)},
    {"web-payments", flag_descriptions::kWebPaymentsName,
     flag_descriptions::kWebPaymentsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebPayments)},
    {"web-payments-modifiers", flag_descriptions::kWebPaymentsModifiersName,
     flag_descriptions::kWebPaymentsModifiersDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsModifiers)},
    {"service-worker-payment-apps",
     flag_descriptions::kServiceWorkerPaymentAppsName,
     flag_descriptions::kServiceWorkerPaymentAppsDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(features::kServiceWorkerPaymentApps)},
    {"enable-web-payments-single-app-ui-skip",
     flag_descriptions::kEnableWebPaymentsSingleAppUiSkipName,
     flag_descriptions::kEnableWebPaymentsSingleAppUiSkipDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsSingleAppUiSkip)},
    {"just-in-time-service-worker-payment-app",
     flag_descriptions::kJustInTimeServiceWorkerPaymentAppName,
     flag_descriptions::kJustInTimeServiceWorkerPaymentAppDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsJustInTimePaymentApp)},
#if defined(OS_ANDROID)
    {"enable-android-pay-integration-v1",
     flag_descriptions::kEnableAndroidPayIntegrationV1Name,
     flag_descriptions::kEnableAndroidPayIntegrationV1Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidPayIntegrationV1)},
    {"enable-android-pay-integration-v2",
     flag_descriptions::kEnableAndroidPayIntegrationV2Name,
     flag_descriptions::kEnableAndroidPayIntegrationV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidPayIntegrationV2)},
    {"enable-web-payments-method-section-order-v2",
     flag_descriptions::kEnableWebPaymentsMethodSectionOrderV2Name,
     flag_descriptions::kEnableWebPaymentsMethodSectionOrderV2Description,
     kOsAndroid,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsMethodSectionOrderV2)},
    {"android-payment-apps", flag_descriptions::kAndroidPaymentAppsName,
     flag_descriptions::kAndroidPaymentAppsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidPaymentApps)},
    {"pay-with-google-v1", flag_descriptions::kPayWithGoogleV1Name,
     flag_descriptions::kPayWithGoogleV1Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPayWithGoogleV1)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS)
    {"disable-eol-notification", flag_descriptions::kEolNotificationName,
     flag_descriptions::kEolNotificationDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisableEolNotification)},
#endif  // OS_CHROMEOS
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
    {"new-audio-rendering-mixing-strategy",
     flag_descriptions::kNewAudioRenderingMixingStrategyName,
     flag_descriptions::kNewAudioRenderingMixingStrategyDescription,
     kOsWin | kOsMac | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(media::kNewAudioRenderingMixingStrategy)},
    {"enable-new-remote-playback-pipeline",
     flag_descriptions::kNewRemotePlaybackPipelineName,
     flag_descriptions::kNewRemotePlaybackPipelineDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kNewRemotePlaybackPipeline)},
    {"enable-surfaces-for-videos",
     flag_descriptions::kUseSurfaceLayerForVideoName,
     flag_descriptions::kUseSurfaceLayerForVideoDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kUseSurfaceLayerForVideo)},
#if defined(OS_CHROMEOS)
    {"quick-unlock-pin", flag_descriptions::kQuickUnlockPinName,
     flag_descriptions::kQuickUnlockPinDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kQuickUnlockPin)},
    {"quick-unlock-fingerprint", flag_descriptions::kQuickUnlockFingerprint,
     flag_descriptions::kQuickUnlockFingerprintDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kQuickUnlockFingerprint)},
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
    {"no-credit-card-abort", flag_descriptions::kNoCreditCardAbort,
     flag_descriptions::kNoCreditCardAbortDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNoCreditCardAbort)},
#endif  // OS_ANDROID
#if defined(OS_CHROMEOS)
    {"enable-emoji-handwriting-voice-on-ime-menu",
     flag_descriptions::kEnableEhvInputName,
     flag_descriptions::kEnableEhvInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEHVInputOnImeMenu)},
#endif  // OS_CHROMEOS
#if defined(OS_CHROMEOS)
    {"arc-available-for-child", flag_descriptions::kArcAvailableForChildName,
     flag_descriptions::kArcAvailableForChildDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kAvailableForChildAccountFeature)},
    {"arc-boot-completed-broadcast", flag_descriptions::kArcBootCompleted,
     flag_descriptions::kArcBootCompletedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kBootCompletedBroadcastFeature)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-input-method", flag_descriptions::kArcInputMethodName,
     flag_descriptions::kArcInputMethodDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableInputMethodFeature)},
    {"arc-native-bridge-experiment",
     flag_descriptions::kArcNativeBridgeExperimentName,
     flag_descriptions::kArcNativeBridgeExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeExperimentFeature)},
    {"arc-usb-host", flag_descriptions::kArcUsbHostName,
     flag_descriptions::kArcUsbHostDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUsbHostFeature)},
    {"arc-vpn", flag_descriptions::kArcVpnName,
     flag_descriptions::kArcVpnDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kVpnFeature)},
#endif  // OS_CHROMEOS
    {"enable-generic-sensor", flag_descriptions::kEnableGenericSensorName,
     flag_descriptions::kEnableGenericSensorDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensor)},
    {"enable-generic-sensor-extra-classes",
     flag_descriptions::kEnableGenericSensorExtraClassesName,
     flag_descriptions::kEnableGenericSensorExtraClassesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kGenericSensorExtraClasses)},
    {"expensive-background-timer-throttling",
     flag_descriptions::kExpensiveBackgroundTimerThrottlingName,
     flag_descriptions::kExpensiveBackgroundTimerThrottlingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExpensiveBackgroundTimerThrottling)},
#if defined(OS_CHROMEOS)
    {"enumerate-audio-devices",
     flag_descriptions::kEnableEnumeratingAudioDevicesName,
     flag_descriptions::kEnableEnumeratingAudioDevicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnumerateAudioDevices)},
#endif  // OS_CHROMEOS
#if !defined(OS_ANDROID)
    {"enable-audio-focus", flag_descriptions::kEnableAudioFocusName,
     flag_descriptions::kEnableAudioFocusDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kEnableAudioFocusChoices)},
#endif  // !OS_ANDROID
#if defined(OS_ANDROID)
    {"modal-permission-prompts", flag_descriptions::kModalPermissionPromptsName,
     flag_descriptions::kModalPermissionPromptsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kModalPermissionPrompts)},
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    {"enable-new-print-preview", flag_descriptions::kEnableNewPrintPreview,
     flag_descriptions::kEnableNewPrintPreviewDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNewPrintPreview)},
    {"enable-nup-printing", flag_descriptions::kEnableNupPrintingName,
     flag_descriptions::kEnableNupPrintingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kNupPrinting)},
    {"enable-cloud-printer-handler",
     flag_descriptions::kCloudPrinterHandlerName,
     flag_descriptions::kCloudPrinterHandlerDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCloudPrinterHandler)},
#endif
#if defined(OS_ANDROID)
    {"concurrent-background-loading-on-svelte",
     flag_descriptions::kOfflinePagesSvelteConcurrentLoadingName,
     flag_descriptions::kOfflinePagesSvelteConcurrentLoadingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         offline_pages::kOfflinePagesSvelteConcurrentLoadingFeature)},
#endif  // !defined(OS_ANDROID)
    {"cross-process-guests",
     flag_descriptions::kCrossProcessGuestViewIsolationName,
     flag_descriptions::kCrossProcessGuestViewIsolationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kGuestViewCrossProcessFrames)},
#if defined(OS_ANDROID)
    {"video-fullscreen-orientation-lock",
     flag_descriptions::kVideoFullscreenOrientationLockName,
     flag_descriptions::kVideoFullscreenOrientationLockDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(media::kVideoFullscreenOrientationLock)},
    {"video-rotate-to-fullscreen",
     flag_descriptions::kVideoRotateToFullscreenName,
     flag_descriptions::kVideoRotateToFullscreenDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(media::kVideoRotateToFullscreen)},
#endif
    {"enable-nostate-prefetch", flag_descriptions::kNostatePrefetchName,
     flag_descriptions::kNostatePrefetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(prerender::kNoStatePrefetchFeature)},

#if defined(OS_CHROMEOS)
    {switches::kEnableUiDevTools, flag_descriptions::kUiDevToolsName,
     flag_descriptions::kUiDevToolsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kEnableUiDevTools)},
#endif  // defined(OS_CHROMEOS)

    {"enable-autofill-credit-card-ablation-experiment",
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDisplayName,
     flag_descriptions::kEnableAutofillCreditCardAblationExperimentDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillCreditCardAblationExperiment)},
    {"enable-autofill-credit-card-local-card-migration",
     flag_descriptions::kEnableAutofillCreditCardLocalCardMigrationName,
     flag_descriptions::kEnableAutofillCreditCardLocalCardMigrationDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillCreditCardLocalCardMigration,
         kAutofillCreditCardLocalCardMigrationVariations,
         "AutofillLocalCardMigration")},
    {"enable-autofill-credit-card-upload-editable-cardholder-name",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableCardholderNameName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableCardholderNameDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableCardholderName)},
    {"enable-autofill-credit-card-upload-google-pay-on-android-branding",
     flag_descriptions::
         kEnableAutofillCreditCardUploadGooglePayOnAndroidBrandingName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadGooglePayOnAndroidBrandingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamUseGooglePayBrandingOnMobile)},
    {"enable-autofill-local-card-migration-show-feedback",
     flag_descriptions::kEnableAutofillLocalCardMigrationShowFeedbackName,
     flag_descriptions::
         kEnableAutofillLocalCardMigrationShowFeedbackDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillLocalCardMigrationShowFeedback)},
    {"enable-autofill-native-dropdown-views",
     flag_descriptions::kEnableAutofillNativeDropdownViewsName,
     flag_descriptions::kEnableAutofillNativeDropdownViewsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillExpandedPopupViews)},
    {"enable-autofill-save-card-dialog-unlabeled-expiration-date",
     flag_descriptions::
         kEnableAutofillSaveCardDialogUnlabeledExpirationDateName,
     flag_descriptions::
         kEnableAutofillSaveCardDialogUnlabeledExpirationDateDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardDialogUnlabeledExpirationDate)},
    {"enable-autofill-save-card-sign-in-after-local-save",
     flag_descriptions::kEnableAutofillSaveCardSignInAfterLocalSaveName,
     flag_descriptions::kEnableAutofillSaveCardSignInAfterLocalSaveDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardSignInAfterLocalSave)},
    {"enable-autofill-save-credit-card-uses-strike-system",
     flag_descriptions::kEnableAutofillSaveCreditCardUsesStrikeSystemName,
     flag_descriptions::
         kEnableAutofillSaveCreditCardUsesStrikeSystemDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCreditCardUsesStrikeSystem)},
    {"enable-autofill-send-experiment-ids-in-payments-rpcs",
     flag_descriptions::kEnableAutofillSendExperimentIdsInPaymentsRPCsName,
     flag_descriptions::
         kEnableAutofillSendExperimentIdsInPaymentsRPCsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSendExperimentIdsInPaymentsRPCs)},

#if defined(OS_WIN)
    {"windows10-custom-titlebar",
     flag_descriptions::kWindows10CustomTitlebarName,
     flag_descriptions::kWindows10CustomTitlebarDescription, kOsWin,
     FEATURE_VALUE_TYPE(kWindows10CustomTitlebar)},
#endif  // OS_WIN

#if defined(OS_ANDROID)
    {"enable-autofill-manual-fallback",
     flag_descriptions::kAutofillManualFallbackAndroidName,
     flag_descriptions::kAutofillManualFallbackAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillManualFallbackAndroid)},

    {"enable-autofill-refresh-style",
     flag_descriptions::kEnableAutofillRefreshStyleName,
     flag_descriptions::kEnableAutofillRefreshStyleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRefreshStyleAndroid)},
    {"lsd-permission-prompt", flag_descriptions::kLsdPermissionPromptName,
     flag_descriptions::kLsdPermissionPromptDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kLsdPermissionPrompt)},
    {"language-settings", flag_descriptions::kLanguagesPreferenceName,
     flag_descriptions::kLanguagesPreferenceDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kLanguagesPreference)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableTouchCalibrationSetting)},
#endif  // defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
    {"file-manager-touch-mode", flag_descriptions::kFileManagerTouchModeName,
     flag_descriptions::kFileManagerTouchModeDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kEnableFileManagerTouchMode,
         chromeos::switches::kDisableFileManagerTouchMode)},
    {"android-files-in-files-app",
     flag_descriptions::kShowAndroidFilesInFilesAppName,
     flag_descriptions::kShowAndroidFilesInFilesAppDescription, kOsCrOS,
     ENABLE_DISABLE_VALUE_TYPE(
         chromeos::switches::kShowAndroidFilesInFilesApp,
         chromeos::switches::kHideAndroidFilesInFilesApp)},
    {"crostini-files", flag_descriptions::kCrostiniFilesName,
     flag_descriptions::kCrostiniFilesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kCrostiniFiles)},
#endif  // OS_CHROMEOS

#if defined(OS_WIN)
    {"gdi-text-printing", flag_descriptions::kGdiTextPrinting,
     flag_descriptions::kGdiTextPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kGdiTextPrinting)},
    {"postscript-printing", flag_descriptions::kDisablePostscriptPrinting,
     flag_descriptions::kDisablePostscriptPrintingDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kDisablePostScriptPrinting)},
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
    {"force-enable-stylus-tools",
     flag_descriptions::kForceEnableStylusToolsName,
     flag_descriptions::kForceEnableStylusToolsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshForceEnableStylusTools)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
    {"new-usb-backend", flag_descriptions::kNewUsbBackendName,
     flag_descriptions::kNewUsbBackendDescription, kOsWin,
     FEATURE_VALUE_TYPE(device::kNewUsbBackend)},
    {"enable-desktop-ios-promotions",
     flag_descriptions::kEnableDesktopIosPromotionsName,
     flag_descriptions::kEnableDesktopIosPromotionsDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kDesktopIOSPromotion)},
#endif  // defined(OS_WIN)

    {"enable-zero-suggest-redirect-to-chrome",
     flag_descriptions::kEnableZeroSuggestRedirectToChromeName,
     flag_descriptions::kEnableZeroSuggestRedirectToChromeDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestRedirectToChrome)},
    {"left-to-right-urls", flag_descriptions::kLeftToRightUrlsName,
     flag_descriptions::kLeftToRightUrlsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kLeftToRightUrls)},

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    {"omnibox-new-answer-layout",
     flag_descriptions::kOmniboxNewAnswerLayoutName,
     flag_descriptions::kOmniboxNewAnswerLayoutDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxNewAnswerLayout)},
    {"omnibox-reverse-answers", flag_descriptions::kOmniboxReverseAnswersName,
     flag_descriptions::kOmniboxReverseAnswersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxReverseAnswers)},
    {"omnibox-rich-entity-suggestions",
     flag_descriptions::kOmniboxRichEntitySuggestionsName,
     flag_descriptions::kOmniboxRichEntitySuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxRichEntitySuggestions)},
    {"omnibox-tail-suggestions", flag_descriptions::kOmniboxTailSuggestionsName,
     flag_descriptions::kOmniboxTailSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTailSuggestions)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"omnibox-pedal-suggestions",
     flag_descriptions::kOmniboxPedalSuggestionsName,
     flag_descriptions::kOmniboxPedalSuggestionsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxPedalSuggestions,
                                    kPedalSuggestionVariations,
                                    "PedalSuggestionVariations")},
    {"enable-new-app-menu-icon", flag_descriptions::kEnableNewAppMenuIconName,
     flag_descriptions::kEnableNewAppMenuIconDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAnimatedAppMenuIcon)},
    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescriptions, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDocumentProvider)},
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID)
    {"enable-custom-feedback-ui",
     flag_descriptions::kEnableCustomFeedbackUiName,
     flag_descriptions::kEnableCustomFeedbackUiDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCustomFeedbackUi)},
#endif  // OS_ANDROID

    {"enable-speculative-service-worker-start-on-query-input",
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputName,
     flag_descriptions::kSpeculativeServiceWorkerStartOnQueryInputDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kSpeculativeServiceWorkerStartOnQueryInput)},

    // NOTE: This feature is generic and marked kOsAll but is used only in CrOS
    // for AndroidMessagesIntegration feature.
    {"enable-service-worker-long-running-message",
     flag_descriptions::kServiceWorkerLongRunningMessageName,
     flag_descriptions::kServiceWorkerLongRunningMessageDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kServiceWorkerLongRunningMessage)},

#if defined(OS_MACOSX)
    {"tab-strip-keyboard-focus", flag_descriptions::kTabStripKeyboardFocusName,
     flag_descriptions::kTabStripKeyboardFocusDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kTabStripKeyboardFocus)},
#endif

#if defined(OS_CHROMEOS)
    {"ChromeVoxArcSupport", flag_descriptions::kChromeVoxArcSupportName,
     flag_descriptions::kChromeVoxArcSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kChromeVoxArcSupport)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
    {"force-tablet-mode", flag_descriptions::kUiModeName,
     flag_descriptions::kUiModeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAshUiModeChoices)},
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
    {"enable-command-line-on-non-rooted-devices",
     flag_descriptions::kEnableCommandLineOnNonRootedName,
     flag_descriptions::kEnableCommandLineOnNoRootedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCommandLineOnNonRooted)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"enable-custom-context-menu",
     flag_descriptions::kEnableCustomContextMenuName,
     flag_descriptions::kEnableCustomContextMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCustomContextMenu)},
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
    {"ash-disable-smooth-screen-rotation",
     flag_descriptions::kAshDisableSmoothScreenRotationName,
     flag_descriptions::kAshDisableSmoothScreenRotationDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(ash::switches::kAshDisableSmoothScreenRotation)},
#endif  // OS_CHROMEOS

    {"omnibox-display-title-for-current-url",
     flag_descriptions::kOmniboxDisplayTitleForCurrentUrlName,
     flag_descriptions::kOmniboxDisplayTitleForCurrentUrlDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kDisplayTitleForCurrentUrl)},

    {"force-color-profile", flag_descriptions::kForceColorProfileName,
     flag_descriptions::kForceColorProfileDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceColorProfileChoices)},

#if defined(OS_CHROMEOS)
    {"use-monitor-color-space", flag_descriptions::kUseMonitorColorSpaceName,
     flag_descriptions::kUseMonitorColorSpaceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kUseMonitorColorSpace)},

    {"quick-unlock-pin-signin", flag_descriptions::kQuickUnlockPinSignin,
     flag_descriptions::kQuickUnlockPinSigninDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kQuickUnlockPinSignin)},
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
    {"enable-webnfc", flag_descriptions::kEnableWebNfcName,
     flag_descriptions::kEnableWebNfcDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kWebNfc)},
#endif

#if defined(OS_ANDROID)
    {"enable-clipboard-provider",
     flag_descriptions::kEnableOmniboxClipboardProviderName,
     flag_descriptions::kEnableOmniboxClipboardProviderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kEnableClipboardProvider)},
#endif

    {"autoplay-policy", flag_descriptions::kAutoplayPolicyName,
     flag_descriptions::kAutoplayPolicyDescription, kOsAll,
     MULTI_VALUE_TYPE(kAutoplayPolicyChoices)},

    {"force-effective-connection-type",
     flag_descriptions::kForceEffectiveConnectionTypeName,
     flag_descriptions::kForceEffectiveConnectionTypeDescription, kOsAll,
     MULTI_VALUE_TYPE(kForceEffectiveConnectionTypeChoices)},

    {"sampling-heap-profiler", flag_descriptions::kSamplingHeapProfilerName,
     flag_descriptions::kSamplingHeapProfilerDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kSamplingHeapProfiler)},

    {"memlog", flag_descriptions::kEnableOutOfProcessHeapProfilingName,
     flag_descriptions::kEnableOutOfProcessHeapProfilingDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableOutOfProcessHeapProfilingChoices)},

    {"memlog-keep-small-allocations",
     flag_descriptions::kOutOfProcessHeapProfilingKeepSmallAllocations,
     flag_descriptions::
         kOutOfProcessHeapProfilingKeepSmallAllocationsDescription,
     kOsAll, SINGLE_VALUE_TYPE(heap_profiling::kMemlogKeepSmallAllocations)},

    {"memlog-sampling", flag_descriptions::kOutOfProcessHeapProfilingSampling,
     flag_descriptions::kOutOfProcessHeapProfilingSamplingDescription, kOsAll,
     SINGLE_VALUE_TYPE(heap_profiling::kMemlogSampling)},

    {"memlog-stack-mode", flag_descriptions::kOOPHPStackModeName,
     flag_descriptions::kOOPHPStackModeDescription, kOsAll,
     MULTI_VALUE_TYPE(kOOPHPStackModeChoices)},

    {"omnibox-ui-elide-suggestion-url-after-host",
     flag_descriptions::kOmniboxUIElideSuggestionUrlAfterHostName,
     flag_descriptions::kOmniboxUIElideSuggestionUrlAfterHostDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentElideSuggestionUrlAfterHost)},

    {"omnibox-ui-hide-steady-state-url-scheme",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlSchemeName,
     flag_descriptions::kOmniboxUIHideSteadyStateUrlSchemeDescription, kOsAll,
     FEATURE_VALUE_TYPE(toolbar::features::kHideSteadyStateUrlScheme)},

    {"omnibox-ui-hide-steady-state-url-trivial-subdomains",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName,
     flag_descriptions::
         kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         toolbar::features::kHideSteadyStateUrlTrivialSubdomains)},

    {"omnibox-ui-hide-steady-state-url-path-query-and-ref",
     flag_descriptions::kOmniboxUIHideSteadyStateUrlPathQueryAndRefName,
     flag_descriptions::kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(toolbar::features::kHideSteadyStateUrlPathQueryAndRef)},

    {"omnibox-ui-jog-textfield-on-popup",
     flag_descriptions::kOmniboxUIJogTextfieldOnPopupName,
     flag_descriptions::kOmniboxUIJogTextfieldOnPopupDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentJogTextfieldOnPopup)},

    {"omnibox-ui-show-suggestion-favicons",
     flag_descriptions::kOmniboxUIShowSuggestionFaviconsName,
     flag_descriptions::kOmniboxUIShowSuggestionFaviconsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentShowSuggestionFavicons)},

    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},

    {"omnibox-ui-swap-title-and-url",
     flag_descriptions::kOmniboxUISwapTitleAndUrlName,
     flag_descriptions::kOmniboxUISwapTitleAndUrlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentSwapTitleAndUrl)},

    {"use-suggestions-even-if-few",
     flag_descriptions::kUseSuggestionsEvenIfFewFeatureName,
     flag_descriptions::kUseSuggestionsEvenIfFewFeatureDescription, kOsAll,
     FEATURE_VALUE_TYPE(suggestions::kUseSuggestionsEvenIfFewFeature)},

    {"use-new-accept-language-header",
     flag_descriptions::kUseNewAcceptLanguageHeaderName,
     flag_descriptions::kUseNewAcceptLanguageHeaderDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUseNewAcceptLanguageHeader)},

#if !defined(OS_ANDROID)
    {"use-google-local-ntp", flag_descriptions::kUseGoogleLocalNtpName,
     flag_descriptions::kUseGoogleLocalNtpDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kUseGoogleLocalNtp)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_MACOSX)
    {"mac-rtl", flag_descriptions::kMacRTLName,
     flag_descriptions::kMacRTLDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacRTL)},
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
    {"enable-floating-virtual-keyboard",
     flag_descriptions::kEnableFloatingVirtualKeyboardName,
     flag_descriptions::kEnableFloatingVirtualKeyboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableFloatingVirtualKeyboard)},
    {"enable-stylus-virtual-keyboard",
     flag_descriptions::kEnableStylusVirtualKeyboardName,
     flag_descriptions::kEnableStylusVirtualKeyboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableStylusVirtualKeyboard)},
    {"enable-fullscreen-handwriting-virtual-keyboard",
     flag_descriptions::kEnableFullscreenHandwritingVirtualKeyboardName,
     flag_descriptions::kEnableFullscreenHandwritingVirtualKeyboardDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableFullscreenHandwritingVirtualKeyboard)},
    {"enable-per-user-timezone", flag_descriptions::kEnablePerUserTimezoneName,
     flag_descriptions::kEnablePerUserTimezoneDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(chromeos::switches::kDisablePerUserTimezone)},
    {"enable-virtual-keyboard-md-ui",
     flag_descriptions::kEnableVirtualKeyboardMdUiName,
     flag_descriptions::kEnableVirtualKeyboardMdUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableVirtualKeyboardMdUi)},
    {"enable-virtual-keyboard-ukm",
     flag_descriptions::kEnableVirtualKeyboardUkmName,
     flag_descriptions::kEnableVirtualKeyboardUkmDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableVirtualKeyboardUkm)},
#endif  // OS_CHROMEOS

#if !defined(OS_ANDROID)
    {"enable-picture-in-picture",
     flag_descriptions::kEnablePictureInPictureName,
     flag_descriptions::kEnablePictureInPictureDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kPictureInPicture)},
#endif  // !defined(OS_ANDROID)

#if defined(TOOLKIT_VIEWS)
    {"enable-experimental-fullscreen-exit-ui",
     flag_descriptions::kExperimentalFullscreenExitUIName,
     flag_descriptions::kExperimentalFullscreenExitUIDescription,
     kOsWin | kOsLinux | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kFullscreenExitUI)},
#endif  // defined(TOOLKIT_VIEWS)

    {"network-service", flag_descriptions::kEnableNetworkServiceName,
     flag_descriptions::kEnableNetworkServiceDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kNetworkService)},

    {"network-service-in-process",
     flag_descriptions::kEnableNetworkServiceInProcessName,
     flag_descriptions::kEnableNetworkServiceInProcessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kNetworkServiceInProcess)},

    {"out-of-blink-cors", flag_descriptions::kEnableOutOfBlinkCORSName,
     flag_descriptions::kEnableOutOfBlinkCORSDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kOutOfBlinkCORS)},

#if defined(OS_ANDROID)
    {"use-ddljson-api", flag_descriptions::kUseDdljsonApiName,
     flag_descriptions::kUseDdljsonApiDescription, kOsAll,
     MULTI_VALUE_TYPE(kUseDdljsonApiChoices)},

    {"spannable-inline-autocomplete",
     flag_descriptions::kSpannableInlineAutocompleteName,
     flag_descriptions::kSpannableInlineAutocompleteDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSpannableInlineAutocomplete)},
#endif  // defined(OS_ANDROID)

    {"enable-resource-load-scheduler",
     flag_descriptions::kResourceLoadSchedulerName,
     flag_descriptions::kResourceLoadSchedulerDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kResourceLoadScheduler)},

#if defined(OS_ANDROID)
    {"omnibox-spare-renderer", flag_descriptions::kOmniboxSpareRendererName,
     flag_descriptions::kOmniboxSpareRendererDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOmniboxSpareRenderer)},
#endif

    {"enable-async-image-decoding", flag_descriptions::kAsyncImageDecodingName,
     flag_descriptions::kAsyncImageDecodingDescription, kOsAll,
     MULTI_VALUE_TYPE(kAsyncImageDecodingChoices)},

#if defined(OS_CHROMEOS)
    {"disable-lock-screen-apps", flag_descriptions::kDisableLockScreenAppsName,
     flag_descriptions::kDisableLockScreenAppsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kDisableLockScreenApps)},
    {"double-tap-to-zoom-in-tablet-mode",
     flag_descriptions::kDoubleTapToZoomInTabletModeName,
     flag_descriptions::kDoubleTapToZoomInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDoubleTapToZoomInTabletMode)},
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    {"dont-prefetch-libraries", flag_descriptions::kDontPrefetchLibrariesName,
     flag_descriptions::kDontPrefetchLibrariesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDontPrefetchLibraries)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"pwa-improved-splash-screen",
     flag_descriptions::kPwaImprovedSplashScreenName,
     flag_descriptions::kPwaImprovedSplashScreenDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPwaImprovedSplashScreen)},
#endif  // OS_ANDROID

#if defined(OS_ANDROID)
    {"pwa-persistent-notification",
     flag_descriptions::kPwaPersistentNotificationName,
     flag_descriptions::kPwaPersistentNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPwaPersistentNotification)},
#endif  // OS_ANDROID

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

#if defined(OS_ANDROID)
    {"third-party-doodles", flag_descriptions::kThirdPartyDoodlesName,
     flag_descriptions::kThirdPartyDoodlesDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kThirdPartyDoodlesChoices)},
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"doodles-on-local-ntp", flag_descriptions::kDoodlesOnLocalNtpName,
     flag_descriptions::kDoodlesOnLocalNtpDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDoodlesOnLocalNtp)},
#endif  // !defined(OS_ANDROID)

    {"sound-content-setting", flag_descriptions::kSoundContentSettingName,
     flag_descriptions::kSoundContentSettingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSoundContentSetting)},

#if DCHECK_IS_CONFIGURABLE
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, kOsWin,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // DCHECK_IS_CONFIGURABLE

#if defined(OS_CHROMEOS)
    {"slide-top-chrome-with-page-scrolls",
     flag_descriptions::kSlideTopChromeWithPageScrollsName,
     flag_descriptions::kSlideTopChromeWithPageScrollsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSlideTopChromeWithPageScrolls)},
    {"sys-internals", flag_descriptions::kSysInternalsName,
     flag_descriptions::kSysInternalsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSysInternals)},
#endif  // defined(OS_CHROMEOS)

    {"enable-improved-language-settings",
     flag_descriptions::kImprovedLanguageSettingsName,
     flag_descriptions::kImprovedLanguageSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kImprovedLanguageSettings)},

    {"enable-regional-locales-as-display-ui",
     flag_descriptions::kRegionalLocalesAsDisplayUIName,
     flag_descriptions::kRegionalLocalesAsDisplayUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(translate::kRegionalLocalesAsDisplayUI)},

    {"enable-v8-context-snapshot", flag_descriptions::kV8ContextSnapshotName,
     flag_descriptions::kV8ContextSnapshotDescription,
     kOsMac | kOsWin | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kV8ContextSnapshot)},

    {"enable-pixel-canvas-recording",
     flag_descriptions::kEnablePixelCanvasRecordingName,
     flag_descriptions::kEnablePixelCanvasRecordingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnablePixelCanvasRecording)},

#if defined(OS_CHROMEOS)
    {"disable-tablet-splitview", flag_descriptions::kDisableTabletSplitViewName,
     flag_descriptions::kDisableTabletSplitViewDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshDisableTabletSplitView)},
#endif  // defined(OS_CHROMEOS)

    {"enable-parallel-downloading", flag_descriptions::kParallelDownloadingName,
     flag_descriptions::kParallelDownloadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kParallelDownloading)},

    {"enable-html-base-username-detector",
     flag_descriptions::kHtmlBasedUsernameDetectorName,
     flag_descriptions::kHtmlBasedUsernameDetectorDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kHtmlBasedUsernameDetector)},

#if defined(OS_ANDROID)
    {"enable-async-dns", flag_descriptions::kAsyncDnsName,
     flag_descriptions::kAsyncDnsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAsyncDns)},
#endif  // defined(OS_ANDROID)

    {"enable-overflow-icons-for-media-controls",
     flag_descriptions::kOverflowIconsForMediaControlsName,
     flag_descriptions::kOverflowIconsForMediaControlsDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kOverflowIconsForMediaControls)},

#if defined(OS_ANDROID)
    {"enable-downloads-location-change",
     flag_descriptions::kDownloadsLocationChangeName,
     flag_descriptions::kDownloadsLocationChangeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDownloadsLocationChange)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"download-progress-infobar",
     flag_descriptions::kDownloadProgressInfoBarName,
     flag_descriptions::kDownloadProgressInfoBarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDownloadProgressInfoBar)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"download-home-v2", flag_descriptions::kDownloadHomeV2Name,
     flag_descriptions::kDownloadHomeV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDownloadHomeV2)},
#endif

#if defined(OS_ANDROID)
    {"new-net-error-page-ui", flag_descriptions::kNewNetErrorPageUIName,
     flag_descriptions::kNewNetErrorPageUIDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kNewNetErrorPageUI,
                                    kNewNetErrorPageUIVariations,
                                    "OfflineContentOnDinoPage")},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"auto-fetch-on-net-error-page",
     flag_descriptions::kAutoFetchOnNetErrorPageName,
     flag_descriptions::kAutoFetchOnNetErrorPageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAutoFetchOnNetErrorPage)},
#endif  // defined(OS_ANDROID)

    {"enable-block-tab-unders", flag_descriptions::kBlockTabUndersName,
     flag_descriptions::kBlockTabUndersDescription, kOsAll,
     FEATURE_VALUE_TYPE(TabUnderNavigationThrottle::kBlockTabUnders)},

    {"top-sites-from-site-engagement",
     flag_descriptions::kTopSitesFromSiteEngagementName,
     flag_descriptions::kTopSitesFromSiteEngagementDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kTopSitesFromSiteEngagement)},

#if defined(OS_POSIX)
    {"enable-ntlm-v2", flag_descriptions::kNtlmV2EnabledName,
     flag_descriptions::kNtlmV2EnabledDescription,
     kOsMac | kOsLinux | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kNtlmV2Enabled)},
#endif  // defined(OS_POSIX)

    {"stop-non-timers-in-background",
     flag_descriptions::kStopNonTimersInBackgroundName,
     flag_descriptions::kStopNonTimersInBackgroundDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kStopNonTimersInBackground)},

    {"stop-in-background", flag_descriptions::kStopInBackgroundName,
     flag_descriptions::kStopInBackgroundDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kStopInBackground)},

#if defined(OS_CHROMEOS)
    {"ash-keyboard-shortcut-viewer-app",
     flag_descriptions::kAshKeyboardShortcutViewerAppName,
     flag_descriptions::kAshKeyboardShortcutViewerAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kKeyboardShortcutViewerApp)},

    {"ash-disable-login-dim-and-blur",
     flag_descriptions::kAshDisableLoginDimAndBlurName,
     flag_descriptions::kAshDisableLoginDimAndBlurDescription, kOsCrOS,
     SINGLE_DISABLE_VALUE_TYPE(ash::switches::kAshDisableLoginDimAndBlur)},
#endif  // OS_CHROMEOS

    {"clipboard-content-setting",
     flag_descriptions::kClipboardContentSettingName,
     flag_descriptions::kClipboardContentSettingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kClipboardContentSetting)},

#if defined(OS_CHROMEOS)
    {"native-smb", flag_descriptions::kNativeSmbName,
     flag_descriptions::kNativeSmbDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNativeSmb)},
#endif  // defined(OS_CHROMEOS)

    {"enable-modern-media-controls",
     flag_descriptions::kUseModernMediaControlsName,
     flag_descriptions::kUseModernMediaControlsDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kUseModernMediaControls)},

    {"enable-network-logging-to-file",
     flag_descriptions::kEnableNetworkLoggingToFileName,
     flag_descriptions::kEnableNetworkLoggingToFileDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kLogNetLog)},

#if defined(OS_CHROMEOS)
    {"disable-multi-mirroring", flag_descriptions::kDisableMultiMirroringName,
     flag_descriptions::kDisableMultiMirroringDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableMultiMirroring)},
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_AV1_DECODER)
    {"enable-av1-decoder", flag_descriptions::kAv1DecoderName,
     flag_descriptions::kAv1DecoderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kAv1Decoder)},
#endif  // ENABLE_AV1_DECODER

#if defined(OS_ANDROID)
    {"grant-notifications-to-dse",
     flag_descriptions::kGrantNotificationsToDSEName,
     flag_descriptions::kGrantNotificationsToDSENameDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kGrantNotificationsToDSE)},
#endif

    {"enable-mark-http-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         security_state::features::kMarkHttpAsFeature,
         kMarkHttpAsFeatureVariations,
         "HTTPBadPhase3")},

    {"enable-web-authentication-api",
     flag_descriptions::kEnableWebAuthenticationAPIName,
     flag_descriptions::kEnableWebAuthenticationAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAuth)},

#if !defined(OS_ANDROID)
    {"enable-web-authentication-testing-api",
     flag_descriptions::kEnableWebAuthenticationTestingAPIName,
     flag_descriptions::kEnableWebAuthenticationTestingAPIDescription,
     kOsDesktop, SINGLE_VALUE_TYPE(switches::kEnableWebAuthTestingAPI)},
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"enable-web-authentication-ctap2-support",
     flag_descriptions::kEnableWebAuthenticationCtap2SupportName,
     flag_descriptions::kEnableWebAuthenticationCtap2SupportDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(device::kNewCtap2Device)},
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
    {"enable-web-authentication-cable-support",
     flag_descriptions::kEnableWebAuthenticationCableSupportName,
     flag_descriptions::kEnableWebAuthenticationCableSupportDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(features::kWebAuthCable)},
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"enable-sole-integration", flag_descriptions::kSoleIntegrationName,
     flag_descriptions::kSoleIntegrationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSoleIntegration)},
#endif  // defined(OS_ANDROID)

    {"enable-viz-display-compositor",
     flag_descriptions::kVizDisplayCompositorName,
     flag_descriptions::kVizDisplayCompositorDescription,
     kOsLinux | kOsWin | kOsMac | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kVizDisplayCompositor)},

    {"unified-consent", flag_descriptions::kUnifiedConsentName,
     flag_descriptions::kUnifiedConsentDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(unified_consent::kUnifiedConsent,
                                    kUnifiedConsentVariations,
                                    "UnifiedConsent")},

    {"force-unified-consent-bump",
     flag_descriptions::kForceUnifiedConsentBumpName,
     flag_descriptions::kForceUnifiedConsentBumpDescription, kOsAll,
     FEATURE_VALUE_TYPE(unified_consent::kForceUnifiedConsentBump)},

    {"simplify-https-indicator", flag_descriptions::kSimplifyHttpsIndicatorName,
     flag_descriptions::kSimplifyHttpsIndicatorDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(toolbar::features::kSimplifyHttpsIndicator,
                                    kSimplifyHttpsIndicatorVariations,
                                    "SimplifyHttpsIndicator")},

#if defined(OS_CHROMEOS)
    {"ash-enable-trilinear-filtering",
     flag_descriptions::kAshEnableTrilinearFilteringName,
     flag_descriptions::kAshEnableTrilinearFilteringDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTrilinearFiltering)},
#endif  // OS_CHROMEOS

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

#if defined(OS_CHROMEOS)
    {"enable-experimental-crostini-ui",
     flag_descriptions::kExperimentalCrostiniUIName,
     flag_descriptions::kExperimentalCrostiniUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kExperimentalCrostiniUI)},
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
    {"enable-omnibox-voice-search-always-visible",
     flag_descriptions::kOmniboxVoiceSearchAlwaysVisibleName,
     flag_descriptions::kOmniboxVoiceSearchAlwaysVisibleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kOmniboxVoiceSearchAlwaysVisible)},
#endif  // OS_ANDROID

    {"enable-signed-http-exchange", flag_descriptions::kSignedHTTPExchangeName,
     flag_descriptions::kSignedHTTPExchangeDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSignedHTTPExchange)},

#if defined(OS_CHROMEOS)
    {"enable-oobe-recommend-apps-screen",
     flag_descriptions::kEnableOobeRecommendAppsScreenName,
     flag_descriptions::kEnableOobeRecommendAppsScreenDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kOobeRecommendAppsScreen)},
#endif  // OS_CHROMEOS

    {"enable-query-in-omnibox", flag_descriptions::kQueryInOmniboxName,
     flag_descriptions::kQueryInOmniboxDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kQueryInOmnibox)},

    {"enable-viz-hit-test-draw-quad",
     flag_descriptions::kVizHitTestDrawQuadName,
     flag_descriptions::kVizHitTestDrawQuadDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableVizHitTestDrawQuad)},

    {"pdf-isolation", flag_descriptions::kPdfIsolationName,
     flag_descriptions::kPdfIsolationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPdfIsolation)},

#if BUILDFLAG(ENABLE_PRINTING)
    {"use-pdf-compositor-service-for-print",
     flag_descriptions::kUsePdfCompositorServiceName,
     flag_descriptions::kUsePdfCompositorServiceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(printing::features::kUsePdfCompositorServiceForPrint)},
#endif

    {"autofill-dynamic-forms", flag_descriptions::kAutofillDynamicFormsName,
     flag_descriptions::kAutofillDynamicFormsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillDynamicForms)},

    {"autofill-preview-style",
     flag_descriptions::kAutofillPreviewStyleExperimentName,
     flag_descriptions::kAutofillPreviewStyleExperimentDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kAutofillPreviewStyleExperiment,
         kAutofillPreviewStyleVariations,
         "AutofillPreviewStyle")},

    {"autofill-prefilled-fields",
     flag_descriptions::kAutofillPrefilledFieldsName,
     flag_descriptions::kAutofillPrefilledFieldsDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPrefilledFields)},

    {"autofill-rationalize-repeated-server-predictions",
     flag_descriptions::kAutofillRationalizeRepeatedServerPredictionsName,
     flag_descriptions::
         kAutofillRationalizeRepeatedServerPredictionsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRationalizeRepeatedServerPredictions)},

    {"autofill-restrict-formless-form-extraction",
     flag_descriptions::kAutofillRestrictUnownedFieldsToFormlessCheckoutName,
     flag_descriptions::
         kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout)},

#if defined(TOOLKIT_VIEWS)
    {"views-cast-dialog", flag_descriptions::kViewsCastDialogName,
     flag_descriptions::kViewsCastDialogDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kViewsCastDialog)},
#endif  // defined(TOOLKIT_VIEWS)

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS)
    {"enable-emoji-context-menu",
     flag_descriptions::kEnableEmojiContextMenuName,
     flag_descriptions::kEnableEmojiContextMenuDescription,
     kOsMac | kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableEmojiContextMenu)},
#endif  // OS_MACOSX || OS_WIN || OS_CHROMEOS

    {"SupervisedUserCommittedInterstitials",
     flag_descriptions::kSupervisedUserCommittedInterstitialsName,
     flag_descriptions::kSupervisedUserCommittedInterstitialsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kSupervisedUserCommittedInterstitials)},

#if defined(OS_ANDROID)
    {"enable-horizontal-tab-switcher",
     flag_descriptions::kHorizontalTabSwitcherAndroidName,
     flag_descriptions::kHorizontalTabSwitcherAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kHorizontalTabSwitcherAndroid)},
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
    {"enable-home-launcher", flag_descriptions::kEnableHomeLauncherName,
     flag_descriptions::kEnableHomeLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableHomeLauncher)},

    {"enable-apps-grid-gap", flag_descriptions::kEnableAppsGridGapFeatureName,
     flag_descriptions::kEnableAppsGridGapFeatureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppsGridGapFeature)},

    {"enable-new-style-launcher",
     flag_descriptions::kEnableNewStyleLauncherName,
     flag_descriptions::kEnableNewStyleLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableNewStyleLauncher)},
#endif  // OS_CHROMEOS

#if defined(OS_WIN)
    {"increase-input-audio-buffer-size",
     flag_descriptions::kIncreaseInputAudioBufferSize,
     flag_descriptions::kIncreaseInputAudioBufferSizeDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kIncreaseInputAudioBufferSize)},
#endif  // OS_WIN

    {"enable-layered-api", flag_descriptions::kLayeredAPIName,
     flag_descriptions::kLayeredAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLayeredAPI)},

    {"enable-layout-ng", flag_descriptions::kEnableLayoutNGName,
     flag_descriptions::kEnableLayoutNGDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLayoutNG)},

    {"enable-lazy-image-loading",
     flag_descriptions::kEnableLazyImageLoadingName,
     flag_descriptions::kEnableLazyImageLoadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLazyImageLoading)},

    {"enable-lazy-frame-loading",
     flag_descriptions::kEnableLazyFrameLoadingName,
     flag_descriptions::kEnableLazyFrameLoadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLazyFrameLoading)},

#if defined(OS_CHROMEOS)
    {"enable-settings-shortcut-search",
     flag_descriptions::kEnableSettingsShortcutSearchName,
     flag_descriptions::kEnableSettingsShortcutSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableSettingsShortcutSearch)},
#endif  // OS_CHROMEOS

    {"autofill-cache-query-responses",
     flag_descriptions::kAutofillCacheQueryResponsesName,
     flag_descriptions::kAutofillCacheQueryResponsesDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheQueryResponses)},

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    {"autofill-primary-info-style",
     flag_descriptions::kAutofillPrimaryInfoStyleExperimentName,
     flag_descriptions::kAutofillPrimaryInfoStyleExperimentDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::kAutofillPrimaryInfoStyleExperiment,
         kAutofillPrimaryInfoStyleVariations,
         "AutofillPrimaryInfoStyleExperiment")},
#endif  // defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)

    {"autofill-enable-company-name",
     flag_descriptions::kAutofillEnableCompanyNameName,
     flag_descriptions::kAutofillEnableCompanyNameDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCompanyName)},
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
    {"single-click-autofill", flag_descriptions::kSingleClickAutofillName,
     flag_descriptions::kSingleClickAutofillDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kSingleClickAutofill)},
    {"enable-sync-user-consent-separate-type",
     flag_descriptions::kEnableSyncUserConsentSeparateTypeName,
     flag_descriptions::kEnableSyncUserConsentSeparateTypeDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUserConsentSeparateType)},
    {"enable-sync-uss-sessions", flag_descriptions::kEnableSyncUSSSessionsName,
     flag_descriptions::kEnableSyncUSSSessionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSSessions)},
    {"enable-experimental-productivity-features",
     flag_descriptions::kExperimentalProductivityFeaturesName,
     flag_descriptions::kExperimentalProductivityFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalProductivityFeatures)},

#if defined(OS_CHROMEOS)
    {"enable-overview-swipe-to-close",
     flag_descriptions::kEnableOverviewSwipeToCloseName,
     flag_descriptions::kEnableOverviewSwipeToCloseDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOverviewSwipeToClose)},
#endif  // OS_CHROMEOS

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    {"ntp-backgrounds", flag_descriptions::kNtpBackgroundsName,
     flag_descriptions::kNtpBackgroundsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNtpBackgrounds)},

    {"ntp-custom-links", flag_descriptions::kNtpCustomLinksName,
     flag_descriptions::kNtpCustomLinksDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_tiles::kNtpCustomLinks)},

    {"ntp-icons", flag_descriptions::kNtpIconsName,
     flag_descriptions::kNtpIconsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNtpIcons)},

    {"ntp-ui-md", flag_descriptions::kNtpUIMdName,
     flag_descriptions::kNtpUIMdDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kNtpUIMd)},
#endif  // OS_WIN || OS_MACOSX || OS_LINUX

#if defined(OS_ANDROID)
    {"enable-display-cutout-api", flag_descriptions::kDisplayCutoutAPIName,
     flag_descriptions::kDisplayCutoutAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDisplayCutoutAPI)},
#endif  // OS_ANDROID

#if defined(USE_AURA)
    {"touchpad-overscroll-history-navigation",
     flag_descriptions::kTouchpadOverscrollHistoryNavigationName,
     flag_descriptions::kTouchpadOverscrollHistoryNavigationDescription,
     kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kTouchpadOverscrollHistoryNavigation)},
#endif

    {"enable-recurrent-interstitial",
     flag_descriptions::kRecurrentInterstitialName,
     flag_descriptions::kRecurrentInterstitialDescription, kOsAll,
     FEATURE_VALUE_TYPE(kRecurrentInterstitialFeature)},

    {"disallow-unsafe-http-downloads",
     flag_descriptions::kDisallowUnsafeHttpDownloadsName,
     flag_descriptions::kDisallowUnsafeHttpDownloadsNameDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDisallowUnsafeHttpDownloads)},

    {"enable-websocket-auth-connection-reuse",
     flag_descriptions::kWebSocketHandshakeReuseConnectionName,
     flag_descriptions::kWebSocketHandshakeReuseConnectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::WebSocketBasicHandshakeStream::
                            kWebSocketHandshakeReuseConnection)},
    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kUnsafelyTreatInsecureOriginAsSecure,
                            "")},

#if defined(OS_CHROMEOS)
    {"enable-app-shortcut-search",
     flag_descriptions::kEnableAppShortcutSearchName,
     flag_descriptions::kEnableAppShortcutSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppShortcutSearch)},

    {"enable-play-store-search", flag_descriptions::kEnablePlayStoreSearchName,
     flag_descriptions::kEnablePlayStoreSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnablePlayStoreAppSearch)},

    {"enable-app-data-search", flag_descriptions::kEnableAppDataSearchName,
     flag_descriptions::kEnableAppDataSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableAppDataSearch)},

    {"enable-drag-tabs-in-tablet-mode",
     flag_descriptions::kEnableDragTabsInTabletModeName,
     flag_descriptions::kEnableDragTabsInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragTabsInTabletMode)},

    {"enable-drag-apps-in-tablet-mode",
     flag_descriptions::kEnableDragAppsInTabletModeName,
     flag_descriptions::kEnableDragAppsInTabletModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDragAppsInTabletMode)},
#endif  // OS_CHROMEOS

    {"enable-accessibility-object-model",
     flag_descriptions::kEnableAccessibilityObjectModelName,
     flag_descriptions::kEnableAccessibilityObjectModelDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableAccessibilityObjectModel)},

    {"enable-autoplay-ignore-web-audio",
     flag_descriptions::kEnableAutoplayIgnoreWebAudioName,
     flag_descriptions::kEnableAutoplayIgnoreWebAudioDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kAutoplayIgnoreWebAudio)},

    {"upcoming-ui-features", flag_descriptions::kExperimentalUiName,
     flag_descriptions::kExperimentalUiDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kExperimentalUi)},

#if defined(OS_ANDROID)
    {"enable-media-controls-expand-gesture",
     flag_descriptions::kEnableMediaControlsExpandGestureName,
     flag_descriptions::kEnableMediaControlsExpandGestureDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(media::kMediaControlsExpandGesture)},
#endif

#if defined(OS_ANDROID)
    {"cct-module", flag_descriptions::kCCTModuleName,
     flag_descriptions::kCCTModuleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTModule)},
    {"cct-module-cache", flag_descriptions::kCCTModuleCacheName,
     flag_descriptions::kCCTModuleCacheDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCCTModuleCache,
                                    kCCTModuleCacheVariations,
                                    "CCTModule")},
#endif

    {"enable-css-fragment-identifiers",
     flag_descriptions::kEnableCSSFragmentIdentifiersName,
     flag_descriptions::kEnableCSSFragmentIdentifiersDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCSSFragmentIdentifiers)},

#if !defined(OS_ANDROID)
    {"infinite-session-restore", flag_descriptions::kInfiniteSessionRestoreName,
     flag_descriptions::kInfiniteSessionRestoreDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kInfiniteSessionRestore)},
    {"page-almost-idle", flag_descriptions::kPageAlmostIdleName,
     flag_descriptions::kPageAlmostIdleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPageAlmostIdle)},
    {"proactive-tab-freeze-and-discard",
     flag_descriptions::kProactiveTabFreezeAndDiscardName,
     flag_descriptions::kProactiveTabFreezeAndDiscardDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kProactiveTabFreezeAndDiscard,
         kProactiveTabFreezeAndDiscardVariations,
         resource_coordinator::kProactiveTabFreezeAndDiscardFeatureName)},
    {"site-characteristics-database",
     flag_descriptions::kSiteCharacteristicsDatabaseName,
     flag_descriptions::kSiteCharacteristicsDatabaseDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSiteCharacteristicsDatabase)},
#endif

#if defined(OS_MACOSX)
    {"enable-text-suggestions-touch-bar",
     flag_descriptions::kTextSuggestionsTouchBarName,
     flag_descriptions::kTextSuggestionsTouchBarDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kTextSuggestionsTouchBar)},
#endif

#if defined(OS_CHROMEOS)
    {"enable-arc-cups-api", flag_descriptions::kArcCupsApiName,
     flag_descriptions::kArcCupsApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kArcCupsApi)},
#endif  // OS_CHROMEOS

    {"gamepad-polling-rate", flag_descriptions::kGamepadPollingRateName,
     flag_descriptions::kGamepadPollingRateDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kGamepadPollingInterval,
                                    kGamepadPollingRateVariations,
                                    "GamepadPollingInterval")},

#if defined(OS_CHROMEOS)
    {"enable-drive-fs", flag_descriptions::kEnableDriveFsName,
     flag_descriptions::kEnableDriveFsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDriveFs)},
#endif  // OS_CHROMEOS

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

#if defined(OS_MACOSX)
    {"enable-web-authentication-touch-id",
     flag_descriptions::kEnableWebAuthenticationTouchIdName,
     flag_descriptions::kEnableWebAuthenticationTouchIdDescription, kOsMac,
     FEATURE_VALUE_TYPE(device::kWebAuthTouchId)},
#endif

    {"enable-autofill-account-wallet-storage",
     flag_descriptions::kEnableAutofillAccountWalletStorageName,
     flag_descriptions::kEnableAutofillAccountWalletStorageDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableAccountWalletStorage)},

#if defined(OS_CHROMEOS)
    {"enable-continue-reading", flag_descriptions::kEnableContinueReadingName,
     flag_descriptions::kEnableContinueReadingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableContinueReading)},
    {"enable-zero-state-suggestions",
     flag_descriptions::kEnableZeroStateSuggestionsName,
     flag_descriptions::kEnableZeroStateSuggestionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableZeroStateSuggestions)},
#endif  // OS_CHROMEOS

    {"enable-bloated-renderer-detection",
     flag_descriptions::kEnableBloatedRendererDetectionName,
     flag_descriptions::kEnableBloatedRendererDetectionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBloatedRendererDetection)},
    {"enable-sync-uss-bookmarks",
     flag_descriptions::kEnableSyncUSSBookmarksName,
     flag_descriptions::kEnableSyncUSSBookmarksDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSBookmarks)},

#if defined(OS_ANDROID)
    {"incognito-strings", flag_descriptions::kIncognitoStringsName,
     flag_descriptions::kIncognitoStringsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kIncognitoStrings)},
#endif

    {"enable-lookalike-url-navigation-suggestions",
     flag_descriptions::kLookalikeUrlNavigationSuggestionsName,
     flag_descriptions::kLookalikeUrlNavigationSuggestionsDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kLookalikeUrlNavigationSuggestions,
         kLookalikeUrlNavigationSuggestionsVariants,
         "LookalikeUrlNavigationSuggestions")},

    {"sync-standalone-transport",
     flag_descriptions::kSyncStandaloneTransportName,
     flag_descriptions::kSyncStandaloneTransportDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncStandaloneTransport)},

#if defined(OS_CHROMEOS)
    {"enable-chromevox-developer-option",
     flag_descriptions::kEnableChromevoxDeveloperOptionName,
     flag_descriptions::kEnableChromevoxDeveloperOptionDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(chromeos::switches::kEnableChromevoxDeveloperOption)},
#endif

    {"sync-USS-autofill-profile",
     flag_descriptions::kSyncUSSAutofillProfileName,
     flag_descriptions::kSyncUSSAutofillProfileDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSAutofillProfile)},

    {"sync-USS-autofill-wallet-data",
     flag_descriptions::kSyncUSSAutofillWalletDataName,
     flag_descriptions::kSyncUSSAutofillWalletDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncUSSAutofillWalletData)},

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

    {"enable-autoplay-unified-sound-settings",
     flag_descriptions::kEnableAutoplayUnifiedSoundSettingsName,
     flag_descriptions::kEnableAutoplayUnifiedSoundSettingsDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(media::kAutoplayWhitelistSettings)},

#if defined(OS_CHROMEOS)
    {"enable-chromeos-account-manager",
     flag_descriptions::kEnableChromeOsAccountManagerName,
     flag_descriptions::kEnableChromeOsAccountManagerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::switches::kAccountManager)},
#endif

#if !defined(OS_ANDROID)
    {"autofill-dropdown-layout", flag_descriptions::kAutofillDropdownLayoutName,
     flag_descriptions::kAutofillDropdownLayoutDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(autofill::kAutofillDropdownLayoutExperiment,
                                    kAutofillDropdownLayoutVariations,
                                    "AutofillDropdownLayout")},
#endif  // OS_ANDROID

#if defined(OS_CHROMEOS)
    {"enable-vaapi-jpeg-image-decode-acceleration",
     flag_descriptions::kVaapiJpegImageDecodeAccelerationName,
     flag_descriptions::kVaapiJpegImageDecodeAccelerationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kVaapiJpegImageDecodeAcceleration)},

    {"enable-home-launcher-gestures",
     flag_descriptions::kEnableHomeLauncherGesturesName,
     flag_descriptions::kEnableHomeLauncherGesturesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(app_list_features::kEnableHomeLauncherGestures)},
#endif

#if !defined(OS_ANDROID)
    {"happiness-tarcking-surveys-for-desktop",
     flag_descriptions::kHappinessTrackingSurveysForDesktopName,
     flag_descriptions::kHappinessTrackingSurveysForDesktopDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHappinessTrackingSurveysForDesktop)},
#endif  // !defined(OS_ANDROID)

    {"enable-service-worker-imported-script-update-check",
     flag_descriptions::kServiceWorkerImportedScriptUpdateCheckName,
     flag_descriptions::kServiceWorkerImportedScriptUpdateCheckDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kServiceWorkerImportedScriptUpdateCheck)},

    {"sync-support-secondary-account",
     flag_descriptions::kSyncSupportSecondaryAccountName,
     flag_descriptions::kSyncSupportSecondaryAccountDescription, kOsAll,
     FEATURE_VALUE_TYPE(switches::kSyncSupportSecondaryAccount)},

    {"use-multilogin-endpoint", flag_descriptions::kUseMultiloginEndpointName,
     flag_descriptions::kUseMultiloginEndpointDescription, kOsAll,
     FEATURE_VALUE_TYPE(kUseMultiloginEndpoint)},

#if defined(OS_CHROMEOS)
    {"enable-usbguard", flag_descriptions::kUsbguardName,
     flag_descriptions::kUsbguardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kUsbguard)},
    {"enable-shill-sandboxing", flag_descriptions::kShillSandboxingName,
     flag_descriptions::kShillSandboxingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kShillSandboxing)},

    {"enable-arc-unified-audio-focus",
     flag_descriptions::kEnableArcUnifiedAudioFocusName,
     flag_descriptions::kEnableArcUnifiedAudioFocusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableUnifiedAudioFocusFeature)},
#endif  // defined(OS_CHROMEOS)

// NOTE: Adding a new flag requires adding a corresponding entry to enum
// "LoginCustomFlags" in tools/metrics/histograms/enums.xml. See "Flag
// Histograms" in tools/metrics/histograms/README.md (run the
// AboutFlagsHistogramTest unit test to verify this process).

#if defined(OS_WIN)
    {"use-angle", flag_descriptions::kUseAngleName,
     flag_descriptions::kUseAngleDescription, kOsWin,
     MULTI_VALUE_TYPE(kUseAngleChoices)},
#endif
#if defined(OS_ANDROID)
    {"android-site-settings-ui", flag_descriptions::kAndroidSiteSettingsUIName,
     flag_descriptions::kAndroidSiteSettingsUIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidSiteSettingsUI)},
    {"translate-android-manual-trigger",
     flag_descriptions::kTranslateAndroidManualTriggerName,
     flag_descriptions::kTranslateAndroidManualTriggerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(translate::kTranslateAndroidManualTrigger)},
#endif
    {"fcm-invalidations", flag_descriptions::kFCMInvalidationsName,
     flag_descriptions::kFCMInvalidationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(invalidation::switches::kFCMInvalidations)},
#if defined(OS_ANDROID)
    {"enable-ephemeral-tab", flag_descriptions::kEphemeralTabName,
     flag_descriptions::kEphemeralTabDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEphemeralTab)},
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
    {"safe-browsing-use-local-blacklists-v2",
     flag_descriptions::kSafeBrowsingUseLocalBlacklistsV2Name,
     flag_descriptions::kSafeBrowsingUseLocalBlacklistsV2Description,
     kOsAndroid, FEATURE_VALUE_TYPE(safe_browsing::kUseLocalBlacklistsV2)},
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    {"enable-native-google-assistant",
     flag_descriptions::kEnableGoogleAssistantName,
     flag_descriptions::kEnableGoogleAssistantDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::switches::kAssistantFeature)},

    {"enable-media-session-ash-media-keys",
     flag_descriptions::kEnableMediaSessionAshMediaKeysName,
     flag_descriptions::kEnableMediaSessionAshMediaKeysDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMediaSessionAccelerators)},
#endif  // defined(OS_CHROMEOS)

    {"enable-blink-heap-unified-garbage-collection",
     flag_descriptions::kEnableBlinkHeapUnifiedGarbageCollectionName,
     flag_descriptions::kEnableBlinkHeapUnifiedGarbageCollectionDescription,
     kOsAll, FEATURE_VALUE_TYPE(features::kBlinkHeapUnifiedGarbageCollection)},

    {"enable-incognito-window-counter",
     flag_descriptions::kEnableIncognitoWindowCounterName,
     flag_descriptions::kEnableIncognitoWindowCounterDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnableIncognitoWindowCounter)},

    {"enable-send-tab-to-self", flag_descriptions::kSendTabToSelfName,
     flag_descriptions::kSendTabToSelfDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSendTabToSelf)},

#if defined(OS_CHROMEOS)
    {"ash-enable-notification-expansion-animation",
     flag_descriptions::kEnableNotificationExpansionAnimationName,
     flag_descriptions::kEnableNotificationExpansionAnimationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNotificationExpansionAnimation)},

#endif  // defined(OS_CHROMEOS)
};

class FlagsStateSingleton {
 public:
  FlagsStateSingleton()
      : flags_state_(kFeatureEntries, arraysize(kFeatureEntries)) {}
  ~FlagsStateSingleton() {}

  static FlagsStateSingleton* GetInstance() {
    return base::Singleton<FlagsStateSingleton>::get();
  }

  static flags_ui::FlagsState* GetFlagsState() {
    return &GetInstance()->flags_state_;
  }

 private:
  flags_ui::FlagsState flags_state_;

  DISALLOW_COPY_AND_ASSIGN(FlagsStateSingleton);
};

bool SkipConditionalFeatureEntry(const FeatureEntry& entry) {
  version_info::Channel channel = chrome::GetChannel();
#if defined(OS_CHROMEOS)
  // Don't expose mash on stable channel.
  if (!strcmp("mash", entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  // enable-ui-devtools is only available on for non Stable channels.
  if (!strcmp(switches::kEnableUiDevTools, entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  // enable-experimental-crostini-ui is only available for boards that have
  // VM support, which is controlled by the Crostini feature.
  if (!strcmp("enable-experimental-crostini-ui", entry.internal_name) &&
      !base::FeatureList::IsEnabled(features::kCrostini)) {
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
      base::win::GetVersion() < base::win::Version::VERSION_WIN10) {
    return true;
  }
#endif  // OS_WIN

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
  const std::set<std::string> switches =
      FlagsStateSingleton::GetFlagsState()->GetSwitchesFromFlags(flags_storage);
  const std::set<std::string> features =
      FlagsStateSingleton::GetFlagsState()->GetFeaturesFromFlags(flags_storage);
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

const FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = arraysize(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing

}  // namespace about_flags
