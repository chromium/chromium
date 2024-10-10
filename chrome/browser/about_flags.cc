// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Instructions for adding new entries to this file:
// https://chromium.googlesource.com/chromium/src/+/main/docs/how_to_add_your_feature_flag.md#step-2_adding-the-feature-flag-to-the-chrome_flags-ui

#include "chrome/browser/about_flags.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/ip_protection/ip_protection_switches.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/permissions/notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/site_isolation/about_flags.h"
#include "chrome/browser/task_manager/task_manager_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/browser/web_applications/features.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/assist_ranker/predictor_config_definitions.h"
#include "components/autofill/content/common/content_autofill_features.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/flag_descriptions.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/contextual_search_field_trial.h"
#include "components/contextual_search/core/browser/public.h"
#include "components/data_sharing/public/features.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/download/public/common/download_features.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/manta/features.h"
#include "components/mirroring/service/mirroring_features.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/page_image_service/features.h"
#include "components/page_info/core/features.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/performance_manager/public/features.h"
#include "components/permissions/features.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/remote_cocoa/app_shim/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_state/core/security_state.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/sensitive_content/features.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/sharing_message/features.h"
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_isolation/features.h"
#include "components/soda/soda_features.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "components/translate/core/common/translate_util.h"
#include "components/trusted_vault/features.h"
#include "components/ui_devtools/switches.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/common/switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/bluetooth/bluez/bluez_features.h"
#include "device/bluetooth/chromeos_platform_features.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/fido/features.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "flag_descriptions.h"
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
#include "services/media_session/public/cpp/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "storage/browser/quota/quota_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ozone_buildflags.h"
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
#include "url/url_features.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/allocator/buildflags.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/process/process.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/browsing_data/core/features.h"
#include "components/external_intents/android/external_intents_features.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/messages/android/messages_feature.h"
#include "components/payments/content/android/payment_feature_map.h"
#include "components/translate/content/android/translate_message.h"
#include "ui/android/ui_android_features.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "components/user_notes/user_notes_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_suggest/item_suggest_cache.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/components/memory/swap_configuration.h"
#include "chromeos/ash/components/standalone_browser/channel_util.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/app_restore/features.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "components/metrics/structured/structured_metrics_features.h"  // nogncheck
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "remoting/host/chromeos/features.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/events/ozone/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#include "chrome/common/webui_url_constants.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/cws_info_service.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_features.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/ozone/public/ozone_switches.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/win/mica_titlebar.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
// This causes a gn error on Android builds, because gn does not understand
// buildflags.
#include "components/user_education/common/user_education_features.h"  // nogncheck
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/ui_base_features.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/views/views_features.h"
#include "ui/views/views_switches.h"
#endif  // defined(TOOLKIT_VIEWS)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/unexportable_keys/features.h"  // nogncheck
#endif

using flags_ui::FeatureEntry;
using flags_ui::kDeprecated;
using flags_ui::kOsAndroid;
using flags_ui::kOsCrOS;
using flags_ui::kOsCrOSOwnerOnly;
using flags_ui::kOsLacros;
using flags_ui::kOsLinux;
using flags_ui::kOsMac;
using flags_ui::kOsWin;

namespace about_flags {

namespace {

const unsigned kOsAll =
    kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsAndroid | kOsLacros;
const unsigned kOsDesktop = kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsLacros;

#if defined(USE_AURA)
const unsigned kOsAura = kOsWin | kOsLinux | kOsCrOS | kOsLacros;
#endif  // USE_AURA

#if defined(USE_AURA)
const FeatureEntry::Choice kPullToRefreshChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kPullToRefresh, "0"},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kPullToRefresh, "1"},
    {flag_descriptions::kPullToRefreshEnabledTouchscreen,
     switches::kPullToRefresh, "2"}};
#endif  // USE_AURA

const FeatureEntry::Choice kEnableBenchmarkingChoices[] = {
    {flag_descriptions::kEnableBenchmarkingChoiceDisabled, "", ""},
    {flag_descriptions::kEnableBenchmarkingChoiceDefaultFeatureStates,
     variations::switches::kEnableBenchmarking, ""},
    {flag_descriptions::kEnableBenchmarkingChoiceMatchFieldTrialTestingConfig,
     variations::switches::kEnableBenchmarking,
     variations::switches::kEnableFieldTrialTestingConfig},
};

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
     blink::switches::kTouchTextSelectionStrategy,
     blink::switches::kTouchTextSelectionStrategy_Character},
    {flag_descriptions::kTouchSelectionStrategyDirection,
     blink::switches::kTouchTextSelectionStrategy,
     blink::switches::kTouchTextSelectionStrategy_Direction}};

#if BUILDFLAG(IS_WIN)
const FeatureEntry::FeatureParam kMediaFoundationClearStrategyUseFrameServer[] =
    {{"strategy", "frame-server"}};

const FeatureEntry::FeatureParam
    kMediaFoundationClearStrategyUseDirectComposition[] = {
        {"strategy", "direct-composition"}};

const FeatureEntry::FeatureParam kMediaFoundationClearStrategyUseDynamic[] = {
    {"strategy", "dynamic"}};

const FeatureEntry::FeatureVariation kMediaFoundationClearStrategyVariations[] =
    {{"Direct Composition", kMediaFoundationClearStrategyUseDirectComposition,
      std::size(kMediaFoundationClearStrategyUseDirectComposition), nullptr},
     {"Frame Server", kMediaFoundationClearStrategyUseFrameServer,
      std::size(kMediaFoundationClearStrategyUseFrameServer), nullptr},
     {"Dynamic", kMediaFoundationClearStrategyUseDynamic,
      std::size(kMediaFoundationClearStrategyUseDynamic), nullptr}};

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
const FeatureEntry::Choice kExtensionsToolbarZeroStateChoices[] = {
    {flag_descriptions::kExtensionsToolbarZeroStateChoicesDisabled, "", ""},
    {flag_descriptions::kExtensionsToolbarZeroStateVistWebStore,
     switches::kExtensionsToolbarZeroStateVariation,
     switches::kExtensionsToolbarZeroStateSingleWebStoreLink},
    {flag_descriptions::kExtensionsToolbarZeroStateExploreExtensionsByCategory,
     switches::kExtensionsToolbarZeroStateVariation,
     switches::kExtensionsToolbarZeroStateExploreExtensionsByCategory},
};
#endif  // ENABLE_EXTENSIONS

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
#if BUILDFLAG(IS_OZONE_X11)
    {flag_descriptions::kOzonePlatformHintChoiceX11,
     switches::kOzonePlatformHint, "x11"},
#endif
#if BUILDFLAG(IS_OZONE_WAYLAND)
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
#if BUILDFLAG(ENABLE_ARCORE)
    {flag_descriptions::kWebXrRuntimeChoiceArCore, switches::kWebXrForceRuntime,
     switches::kWebXrRuntimeArCore},
#endif
#if BUILDFLAG(ENABLE_CARDBOARD)
    {flag_descriptions::kWebXrRuntimeChoiceCardboard,
     switches::kWebXrForceRuntime, switches::kWebXrRuntimeCardboard},
#endif
#if BUILDFLAG(ENABLE_OPENXR)
    {flag_descriptions::kWebXrRuntimeChoiceOpenXR, switches::kWebXrForceRuntime,
     switches::kWebXrRuntimeOpenXr},
#endif  // ENABLE_OPENXR
    {flag_descriptions::kWebXrRuntimeChoiceOrientationSensors,
     switches::kWebXrForceRuntime, switches::kWebXrRuntimeOrientationSensors},
};
#endif  // ENABLE_VR

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kAndroidDefaultFontFamilyDevTesting[] = {
    {"dev_testing", "true"}};

const FeatureEntry::FeatureVariation kAndroidDefaultFontFamilyVariations[] = {
    {"Use dev testing font families", kAndroidDefaultFontFamilyDevTesting,
     std::size(kAndroidDefaultFontFamilyDevTesting), nullptr}};

const FeatureEntry::FeatureParam kCCTMinimizedDefaultIcon[] = {
    {"icon_variant", "0"}};
const FeatureEntry::FeatureParam kCCTMinimizedAlternativeIcon[] = {
    {"icon_variant", "1"}};

const FeatureEntry::FeatureVariation kCCTMinimizedIconVariations[] = {
    {"Use default minimize icon", kCCTMinimizedDefaultIcon,
     std::size(kCCTMinimizedDefaultIcon), nullptr},
    {"Use alternative minimize icon", kCCTMinimizedAlternativeIcon,
     std::size(kCCTMinimizedAlternativeIcon), nullptr}};

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

const FeatureEntry::FeatureParam kCCTBottomBarButtonBalancedWithHomeParam[] = {
    {"google_bottom_bar_button_list", "0,10,3,2"}};
const FeatureEntry::FeatureParam kCCTBottomBarButtonsBalancedWithCustomParam[] =
    {{"google_bottom_bar_button_list", "0,3,8,2"}};
const FeatureEntry::FeatureParam kCCTBottomBarButtonsBalancedWithSearchParam[] =
    {{"google_bottom_bar_button_list", "0,3,9,2"}};
const FeatureEntry::FeatureParam kCCTBottomBarHomeInSpotlightParam[] = {
    {"google_bottom_bar_button_list", "10,10,3,2"}};
const FeatureEntry::FeatureParam kCCTBottomBarCustomInSpotlightParam[] = {
    {"google_bottom_bar_button_list", "8,8,3,2"}};
const FeatureEntry::FeatureParam kCCTBottomBarSearchInSpotlightParam[] = {
    {"google_bottom_bar_button_list", "9,9,3,2"}};

const FeatureEntry::FeatureVariation kCCTGoogleBottomBarVariations[] = {
    {"Balanced with home button", kCCTBottomBarButtonBalancedWithHomeParam,
     std::size(kCCTBottomBarButtonBalancedWithHomeParam), nullptr},
    {"Balanced with custom button", kCCTBottomBarButtonsBalancedWithCustomParam,
     std::size(kCCTBottomBarButtonsBalancedWithCustomParam), nullptr},
    {"Balanced with search button", kCCTBottomBarButtonsBalancedWithSearchParam,
     std::size(kCCTBottomBarButtonsBalancedWithSearchParam), nullptr},
    {"home button in spotlight", kCCTBottomBarHomeInSpotlightParam,
     std::size(kCCTBottomBarHomeInSpotlightParam), nullptr},
    {"custom button in spotlight", kCCTBottomBarCustomInSpotlightParam,
     std::size(kCCTBottomBarCustomInSpotlightParam), nullptr},
    {"search button in spotlight", kCCTBottomBarSearchInSpotlightParam,
     std::size(kCCTBottomBarSearchInSpotlightParam), nullptr},
};

const FeatureEntry::FeatureParam kCCTDoubleDeckerBottomBarParam[] = {
    {"google_bottom_bar_variant_layout", "1"}};
const FeatureEntry::FeatureParam kCCTSingleDeckerBottomBarParam[] = {
    {"google_bottom_bar_variant_layout", "2"}};
const FeatureEntry::FeatureParam
    kCCTSingleDeckerBottomBarWithButtonsOnRightParam[] = {
        {"google_bottom_bar_variant_layout", "3"}};

const FeatureEntry::FeatureVariation
    kCCTGoogleBottomBarVariantLayoutsVariations[] = {
        {"Double decker", kCCTDoubleDeckerBottomBarParam,
         std::size(kCCTDoubleDeckerBottomBarParam), nullptr},
        {"Single decker", kCCTSingleDeckerBottomBarParam,
         std::size(kCCTSingleDeckerBottomBarParam), nullptr},
        {"Single decker with button(s) on right",
         kCCTSingleDeckerBottomBarWithButtonsOnRightParam,
         std::size(kCCTSingleDeckerBottomBarWithButtonsOnRightParam), nullptr},
};

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

const FeatureEntry::FeatureParam kOmahaMinSdkVersionAndroidMinSdk1[] = {
    {"min_sdk_version", "1"}};
const FeatureEntry::FeatureParam kOmahaMinSdkVersionAndroidMinSdk1000[] = {
    {"min_sdk_version", "1000"}};
const FeatureEntry::FeatureVariation kOmahaMinSdkVersionAndroidVariations[] = {
    {flag_descriptions::kOmahaMinSdkVersionAndroidMinSdk1Description,
     kOmahaMinSdkVersionAndroidMinSdk1,
     std::size(kOmahaMinSdkVersionAndroidMinSdk1), nullptr},
    {flag_descriptions::kOmahaMinSdkVersionAndroidMinSdk1000Description,
     kOmahaMinSdkVersionAndroidMinSdk1000,
     std::size(kOmahaMinSdkVersionAndroidMinSdk1000), nullptr},
};

const FeatureEntry::FeatureParam
    kOptimizationGuidePersonalizedFetchingAllowPageInsights[] = {
        {"allowed_contexts", "CONTEXT_PAGE_INSIGHTS_HUB"}};
const FeatureEntry::FeatureVariation
    kOptimizationGuidePersonalizedFetchingAllowPageInsightsVariations[] = {
        {"for Page Insights",
         kOptimizationGuidePersonalizedFetchingAllowPageInsights,
         std::size(kOptimizationGuidePersonalizedFetchingAllowPageInsights),
         nullptr}};

const FeatureEntry::FeatureParam kAccessibilityPageZoomNoOSAdjustment[] = {
    {"AdjustForOSLevel", "false"},
};
const FeatureEntry::FeatureParam kAccessibilityPageZoomWithOSAdjustment[] = {
    {"AdjustForOSLevel", "true"},
};

const FeatureEntry::FeatureVariation kAccessibilityPageZoomVariations[] = {
    {"- With OS Adjustment", kAccessibilityPageZoomWithOSAdjustment,
     std::size(kAccessibilityPageZoomWithOSAdjustment), nullptr},
    {"- No OS Adjustment (default)", kAccessibilityPageZoomNoOSAdjustment,
     std::size(kAccessibilityPageZoomNoOSAdjustment), nullptr},
};

#endif  // BUILDFLAG(IS_ANDROID)

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

const FeatureEntry::FeatureParam kForceDark_TransparencyAndNumColors[] = {
    {"classifier_policy", "transparency_and_num_colors"}};

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
    {"with selective image inversion based on transparency and number of "
     "colors",
     kForceDark_TransparencyAndNumColors,
     std::size(kForceDark_TransparencyAndNumColors), nullptr}};
#endif  // !BUILDFLAG(IS_CHROMEOS)

const FeatureEntry::FeatureParam
    kWebIdentityDigitalIdentityCredentialNoDialogParam[] = {
        {"dialog", "no_dialog"}};
const FeatureEntry::FeatureParam
    kWebIdentityDigitalIdentityCredentialDefaultParam[] = {
        {"dialog", "default"}};
const FeatureEntry::FeatureParam
    kWebIdentityDigitalIdentityCredentialLowRiskDialogParam[] = {
        {"dialog", "low_risk"}};
const FeatureEntry::FeatureParam
    kWebIdentityDigitalIdentityCredentialHighRiskDialogParam[] = {
        {"dialog", "high_risk"}};
const FeatureEntry::FeatureVariation
    kWebIdentityDigitalIdentityCredentialVariations[] = {
        {"with dialog depending on what credentials are requested",
         kWebIdentityDigitalIdentityCredentialDefaultParam,
         std::size(kWebIdentityDigitalIdentityCredentialDefaultParam), nullptr},
        {"without dialog", kWebIdentityDigitalIdentityCredentialNoDialogParam,
         std::size(kWebIdentityDigitalIdentityCredentialNoDialogParam),
         nullptr},
        {"with confirmation dialog with mild warning before sending identity "
         "request to Android OS",
         kWebIdentityDigitalIdentityCredentialLowRiskDialogParam,
         std::size(kWebIdentityDigitalIdentityCredentialLowRiskDialogParam),
         nullptr},
        {"with confirmation dialog with severe warning before sending "
         "identity request to Android OS",
         kWebIdentityDigitalIdentityCredentialHighRiskDialogParam,
         std::size(kWebIdentityDigitalIdentityCredentialHighRiskDialogParam),
         nullptr}};

const FeatureEntry::FeatureParam kClipboardMaximumAge60Seconds[] = {
    {"UIClipboardMaximumAge", "60"}};
const FeatureEntry::FeatureParam kClipboardMaximumAge90Seconds[] = {
    {"UIClipboardMaximumAge", "90"}};
const FeatureEntry::FeatureParam kClipboardMaximumAge120Seconds[] = {
    {"UIClipboardMaximumAge", "120"}};
const FeatureEntry::FeatureParam kClipboardMaximumAge150Seconds[] = {
    {"UIClipboardMaximumAge", "150"}};
const FeatureEntry::FeatureParam kClipboardMaximumAge180Seconds[] = {
    {"UIClipboardMaximumAge", "180"}};

const FeatureEntry::FeatureVariation kClipboardMaximumAgeVariations[] = {
    {"Enabled 60 seconds", kClipboardMaximumAge60Seconds,
     std::size(kClipboardMaximumAge60Seconds), nullptr},
    {"Enabled 90 seconds", kClipboardMaximumAge90Seconds,
     std::size(kClipboardMaximumAge90Seconds), nullptr},
    {"Enabled 120 seconds", kClipboardMaximumAge120Seconds,
     std::size(kClipboardMaximumAge120Seconds), nullptr},
    {"Enabled 150 seconds", kClipboardMaximumAge150Seconds,
     std::size(kClipboardMaximumAge150Seconds), nullptr},
    {"Enabled 180 seconds", kClipboardMaximumAge180Seconds,
     std::size(kClipboardMaximumAge180Seconds), nullptr},
};

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

const FeatureEntry::FeatureParam kSearchPrefetchWithoutHoldback[] = {
    {"prefetch_holdback", "false"}};
const FeatureEntry::FeatureParam kSearchPrefetchWithHoldback[] = {
    {"prefetch_holdback", "true"}};

const FeatureEntry::FeatureVariation
    kSearchPrefetchServicePrefetchingVariations[] = {
        {"without holdback", kSearchPrefetchWithoutHoldback,
         std::size(kSearchPrefetchWithoutHoldback), nullptr},
        {"with holdback", kSearchPrefetchWithHoldback,
         std::size(kSearchPrefetchWithHoldback), nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kArcVmMemorySizeShift_200[] = {
    {"shift_mib", "-200"}};
const FeatureEntry::FeatureParam kArcVmMemorySizeShift_500[] = {
    {"shift_mib", "-500"}};
const FeatureEntry::FeatureParam kArcVmMemorySizeShift_800[] = {
    {"shift_mib", "-800"}};

const FeatureEntry::FeatureVariation kArcVmMemorySizeVariations[] = {
    {"shift -200MiB", kArcVmMemorySizeShift_200,
     std::size(kArcVmMemorySizeShift_200), nullptr},
    {"shift -500MiB", kArcVmMemorySizeShift_500,
     std::size(kArcVmMemorySizeShift_500), nullptr},
    {"shift -800MiB", kArcVmMemorySizeShift_800,
     std::size(kArcVmMemorySizeShift_800), nullptr},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const FeatureEntry::Choice kEnableGpuRasterizationChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableGpuRasterization, ""},
    {flags_ui::kGenericExperimentChoiceDisabled,
     switches::kDisableGpuRasterization, ""},
};

const FeatureEntry::Choice kTopChromeTouchUiChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceAutomatic, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiAuto},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiDisabled},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kTopChromeTouchUi,
     switches::kTopChromeTouchUiEnabled}};

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kToastWith6Seconds[] = {
    {"toast_timeout", "6s"},
    {"toast_without_action_timeout", "6s"},
    {"toast_demo_mode", "true"}};
const FeatureEntry::FeatureParam kToastWith8Seconds[] = {
    {"toast_timeout", "8s"},
    {"toast_without_action_timeout", "8s"},
    {"toast_demo_mode", "true"}};
const FeatureEntry::FeatureParam kToastWith10Seconds[] = {
    {"toast_timeout", "10s"},
    {"toast_without_action_timeout", "10s"},
    {"toast_demo_mode", "true"}};
const FeatureEntry::FeatureParam kToastWith12Seconds[] = {
    {"toast_timeout", "12s"},
    {"toast_without_action_timeout", "12s"},
    {"toast_demo_mode", "true"}};

const FeatureEntry::FeatureVariation kToastVariations[] = {
    {"with 6s", kToastWith6Seconds, std::size(kToastWith6Seconds), nullptr},
    {"with 8s", kToastWith8Seconds, std::size(kToastWith8Seconds), nullptr},
    {"with 10s", kToastWith10Seconds, std::size(kToastWith10Seconds), nullptr},
    {"with 12s", kToastWith12Seconds, std::size(kToastWith12Seconds), nullptr}};
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)

const FeatureEntry::FeatureParam kRoundedWindowRadius8 = {
    chromeos::features::kRoundedWindowsRadius, "8"};

const FeatureEntry::FeatureParam kRoundedWindowRadius10 = {
    chromeos::features::kRoundedWindowsRadius, "10"};

const FeatureEntry::FeatureParam kRoundedWindowRadius12 = {
    chromeos::features::kRoundedWindowsRadius, "12"};

const FeatureEntry::FeatureParam kRoundedWindowRadius14 = {
    chromeos::features::kRoundedWindowsRadius, "14"};

const FeatureEntry::FeatureParam kRoundedWindowRadius16 = {
    chromeos::features::kRoundedWindowsRadius, "16"};

const FeatureEntry::FeatureParam kRoundedWindowRadius18 = {
    chromeos::features::kRoundedWindowsRadius, "18"};

const FeatureEntry::FeatureVariation kRoundedWindowsRadiusVariation[] = {
    {"8", &kRoundedWindowRadius8, 1, nullptr},
    {"10", &kRoundedWindowRadius10, 1, nullptr},
    {"12", &kRoundedWindowRadius12, 1, nullptr},
    {"14", &kRoundedWindowRadius14, 1, nullptr},
    {"16", &kRoundedWindowRadius16, 1, nullptr},
    {"18", &kRoundedWindowRadius18, 1, nullptr},
};

const FeatureEntry::FeatureParam
    kArcRoundedWindowCompatStrategyLeftRightBottomGesture = {
        arc::kRoundedWindowCompatStrategy,
        arc::kRoundedWindowCompatStrategy_LeftRightBottomGesture};

const FeatureEntry::FeatureParam
    kArcRoundedWindowCompatStrategyBottomOnlyGesture = {
        arc::kRoundedWindowCompatStrategy,
        arc::kRoundedWindowCompatStrategy_BottomOnlyGesture};

const FeatureEntry::FeatureVariation kArcRoundedWindowCompatVariation[] = {
    {"Left-Right-Bottom Gesture Exclusion",
     &kArcRoundedWindowCompatStrategyLeftRightBottomGesture, 1, nullptr},
    {"Bottom-only Gesture Exclusion",
     &kArcRoundedWindowCompatStrategyBottomOnlyGesture, 1, nullptr},
};

const FeatureEntry::FeatureParam kZinkEnableRecommended[] = {
    {"BorealisZinkGlDriverParam", "ZinkEnableRecommended"}};
const FeatureEntry::FeatureParam kZinkEnableAll[] = {
    {"BorealisZinkGlDriverParam", "ZinkEnableAll"}};

const FeatureEntry::FeatureVariation kBorealisZinkGlDriverVariations[] = {
    {"for recommended apps", kZinkEnableRecommended,
     std::size(kZinkEnableRecommended), nullptr},
    {"for all apps", kZinkEnableAll, std::size(kZinkEnableAll), nullptr}};

const char kPreferDcheckInternalName[] = "prefer-dcheck";

const char kLacrosAvailabilityIgnoreInternalName[] =
    "lacros-availability-ignore";
const char kLacrosStabilityInternalName[] = "lacros-stability";
const char kLacrosWaylandLoggingInternalName[] = "lacros-wayland-logging";
const char kArcEnableVirtioBlkForDataInternalName[] =
    "arc-enable-virtio-blk-for-data";

const FeatureEntry::Choice kPreferDcheckChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {component_updater::kPreferDcheckOptIn,
     component_updater::kPreferDcheckSwitch,
     component_updater::kPreferDcheckOptIn},
    {component_updater::kPreferDcheckOptOut,
     component_updater::kPreferDcheckSwitch,
     component_updater::kPreferDcheckOptOut},
};

const FeatureEntry::Choice kLacrosStabilityChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {ash::standalone_browser::kLacrosStabilityChannelCanary,
     ash::standalone_browser::kLacrosStabilitySwitch,
     ash::standalone_browser::kLacrosStabilityChannelCanary},
    {ash::standalone_browser::kLacrosStabilityChannelDev,
     ash::standalone_browser::kLacrosStabilitySwitch,
     ash::standalone_browser::kLacrosStabilityChannelDev},
    {ash::standalone_browser::kLacrosStabilityChannelBeta,
     ash::standalone_browser::kLacrosStabilitySwitch,
     ash::standalone_browser::kLacrosStabilityChannelBeta},
    {ash::standalone_browser::kLacrosStabilityChannelStable,
     ash::standalone_browser::kLacrosStabilitySwitch,
     ash::standalone_browser::kLacrosStabilityChannelStable},
};

const char kLacrosSelectionInternalName[] = "lacros-selection";
const char kProjectorServerSideSpeechRecognition[] =
    "enable-projector-server-side-speech-recognition";

const FeatureEntry::Choice kLacrosSelectionChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flag_descriptions::kLacrosSelectionStatefulDescription,
     ash::standalone_browser::kLacrosSelectionSwitch,
     ash::standalone_browser::kLacrosSelectionStateful},
    {flag_descriptions::kLacrosSelectionRootfsDescription,
     ash::standalone_browser::kLacrosSelectionSwitch,
     ash::standalone_browser::kLacrosSelectionRootfs},
};

const char kLacrosSelectionPolicyIgnoreInternalName[] =
    "lacros-selection-ignore";

const FeatureEntry::Choice kLacrosAvailabilityPolicyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {ash::standalone_browser::kLacrosAvailabilityPolicyUserChoice,
     ash::standalone_browser::kLacrosAvailabilityPolicySwitch,
     ash::standalone_browser::kLacrosAvailabilityPolicyUserChoice},
    {ash::standalone_browser::kLacrosAvailabilityPolicyLacrosDisabled,
     ash::standalone_browser::kLacrosAvailabilityPolicySwitch,
     ash::standalone_browser::kLacrosAvailabilityPolicyLacrosDisabled},
    {ash::standalone_browser::kLacrosAvailabilityPolicyLacrosOnly,
     ash::standalone_browser::kLacrosAvailabilityPolicySwitch,
     ash::standalone_browser::kLacrosAvailabilityPolicyLacrosOnly},
};

const FeatureEntry::Choice kLacrosDataBackwardMigrationModePolicyChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyNone,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyNone},
    {crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyKeepNone,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyKeepNone},
    {crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyKeepSafeData,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyKeepSafeData},
    {crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyKeepAll,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
     crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyKeepAll},
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

const FeatureEntry::Choice kIpProtectionProxyOptOutChoices[] = {
    {flag_descriptions::kIpProtectionProxyOptOutChoiceDefault, "", ""},
    {flag_descriptions::kIpProtectionProxyOptOutChoiceOptOut,
     switches::kDisableIpProtectionProxy, ""},
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
    {flag_descriptions::kForceColorProfileRec2020,
     switches::kForceDisplayColorProfile, "rec2020"},
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

const FeatureEntry::FeatureParam
    kOptimizationGuideOnDeviceModelBypassPerfParams[] = {
        {"compatible_on_device_performance_classes", "*"},
        {"on_device_retract_unsafe_content", "true"},
};
const FeatureEntry::FeatureParam
    kOptimizationGuideOnDeviceModelBypassTextSafetyParams[] = {
        {"on_device_retract_unsafe_content", "false"},
};
const FeatureEntry::FeatureParam
    kOptimizationGuideOnDeviceModelBypassPerfAndTextSafetyParams[] = {
        {"compatible_on_device_performance_classes", "*"},
        {"on_device_retract_unsafe_content", "false"},
};
const FeatureEntry::FeatureVariation
    kOptimizationGuideOnDeviceModelVariations[] = {
        {"BypassPerfRequirement",
         kOptimizationGuideOnDeviceModelBypassPerfParams,
         std::size(kOptimizationGuideOnDeviceModelBypassPerfParams), nullptr},
        {"BypassTextSafety",
         kOptimizationGuideOnDeviceModelBypassTextSafetyParams,
         std::size(kOptimizationGuideOnDeviceModelBypassTextSafetyParams),
         nullptr},
        {"BypassPerfAndTextSafety",
         kOptimizationGuideOnDeviceModelBypassPerfAndTextSafetyParams,
         std::size(
             kOptimizationGuideOnDeviceModelBypassPerfAndTextSafetyParams),
         nullptr},
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
const FeatureEntry::FeatureParam kHistoryEmbeddingsAtKeywordAcceleration[]{
    {"AtKeywordAcceleration", "true"},
};
const FeatureEntry::FeatureVariation kHistoryEmbeddingsVariations[] = {
    {"with AtKeywordAcceleration", kHistoryEmbeddingsAtKeywordAcceleration,
     std::size(kHistoryEmbeddingsAtKeywordAcceleration), nullptr},
};

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

const FeatureEntry::FeatureParam kJourneysShowAllVisitsParams[] = {
    {"JourneysLocaleOrLanguageAllowlist", "*"},
    // To show all visits, set the number of visits above the fold to a very
    // high number.
    {"JourneysNumVisitsToAlwaysShowAboveTheFold", "200"},
};
const FeatureEntry::FeatureParam kJourneysAllLocalesParams[] = {
    {"JourneysLocaleOrLanguageAllowlist", "*"},
};
const FeatureEntry::FeatureVariation kJourneysVariations[] = {
    {"No 'Show More' - Show all visits", kJourneysShowAllVisitsParams,
     std::size(kJourneysShowAllVisitsParams), nullptr},
    {"All Supported Locales", kJourneysAllLocalesParams,
     std::size(kJourneysAllLocalesParams), nullptr},
};

const FeatureEntry::FeatureVariation
    kImageServiceOptimizationGuideSalientImagesVariations[] = {
        {"High Performance Canonicalization", nullptr, 0, "3362133"},
};

const FeatureEntry::FeatureParam kSidePanelJourneysOpensFromOmniboxParams[] = {
    {"SidePanelJourneysOpensFromOmnibox", "true"},
};
const FeatureEntry::FeatureVariation
    kSidePanelJourneysOpensFromOmniboxVariations[] = {
        {"Omnibox opens Side Panel Journeys",
         kSidePanelJourneysOpensFromOmniboxParams,
         std::size(kSidePanelJourneysOpensFromOmniboxParams), nullptr},
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
const FeatureEntry::FeatureParam kShortcutBoostSingleUrl[] = {
    {"ShortcutBoostSearchScore", "0"},
    {"ShortcutBoostNonTopHitThreshold", "0"},
    {"ShortcutBoostGroupWithSearches", "false"},
};
const FeatureEntry::FeatureParam kShortcutBoostMultipleUrls[] = {
    {"ShortcutBoostSearchScore", "0"},
    {"ShortcutBoostNonTopHitThreshold", "2"},
    {"ShortcutBoostGroupWithSearches", "true"},
};
const FeatureEntry::FeatureParam kShortcutBoostMultipleSearchesAndUrls[] = {
    {"ShortcutBoostSearchScore", "1414"},
    {"ShortcutBoostNonTopHitThreshold", "2"},
    {"ShortcutBoostNonTopHitSearchThreshold", "3"},
    {"ShortcutBoostGroupWithSearches", "true"},
};

const FeatureEntry::FeatureVariation kOmniboxShortcutBoostVariations[] = {
    {"Single URL", kShortcutBoostSingleUrl, std::size(kShortcutBoostSingleUrl),
     nullptr},
    {"Multiple URLs", kShortcutBoostMultipleUrls,
     std::size(kShortcutBoostMultipleUrls), nullptr},
    {"Multiple Searches and URLs", kShortcutBoostMultipleSearchesAndUrls,
     std::size(kShortcutBoostMultipleSearchesAndUrls), nullptr},
};

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

const FeatureEntry::FeatureParam
    kOmniboxDriveSuggestionsIgnoreWhenDebouncing[] = {
        {"DocumentProviderIgnoreWhenDebouncing", "true"}};
const FeatureEntry::FeatureVariation kOmniboxDriveSuggestionsVariations[] = {
    {"ignore when debouncing", kOmniboxDriveSuggestionsIgnoreWhenDebouncing,
     std::size(kOmniboxDriveSuggestionsIgnoreWhenDebouncing), nullptr}};

const FeatureEntry::FeatureParam kOmniboxStarterPackExpansionPreProdUrl[] = {
    {"StarterPackGeminiUrlOverride", "https://gemini.google.com/corp/prompt"}};
const FeatureEntry::FeatureParam kOmniboxStarterPackExpansionStagingUrl[] = {
    {"StarterPackGeminiUrlOverride",
     "https://gemini.google.com/staging/prompt"}};
const FeatureEntry::FeatureVariation kOmniboxStarterPackExpansionVariations[] =
    {{"pre-prod url", kOmniboxStarterPackExpansionPreProdUrl,
      std::size(kOmniboxStarterPackExpansionPreProdUrl), nullptr},
     {"staging url", kOmniboxStarterPackExpansionStagingUrl,
      std::size(kOmniboxStarterPackExpansionStagingUrl), nullptr}};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

const FeatureEntry::FeatureParam kOmniboxMlUrlScoringEnabledWithFixes[] = {
    {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
    {"MlUrlScoringShortcutDocumentSignals", "true"},
};
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringUnlimitedNumCandidates[] =
    {
        {"MlUrlScoringUnlimitedNumCandidates", "true"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};
// Sets Bookmark(1), History Quick(4), History URL(8), Shortcuts(64),
// Document(512), and History Fuzzy(65536) providers max matches to 10.
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringMaxMatchesByProvider10[] =
    {
        {"MlUrlScoringMaxMatchesByProvider",
         "1:10,4:10,8:10,64:10,512:10,65536:10"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};
// Enables ML scoring for Search suggestions.
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringWithSearches[] = {
    {"MlUrlScoring_EnableMlScoringForSearches", "true"},
};
// Enables ML scoring for verbatim URL suggestions.
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringWithVerbatimURLs[] = {
    {"MlUrlScoring_EnableMlScoringForVerbatimUrls", "true"},
};
// Enables ML scoring for both Search and verbatim URL suggestions.
const FeatureEntry::FeatureParam
    kOmniboxMlUrlScoringWithSearchesAndVerbatimURLs[] = {
        {"MlUrlScoring_EnableMlScoringForSearches", "true"},
        {"MlUrlScoring_EnableMlScoringForVerbatimUrls", "true"},
};

const FeatureEntry::FeatureVariation kOmniboxMlUrlScoringVariations[] = {
    {"Enabled with fixes", kOmniboxMlUrlScoringEnabledWithFixes,
     std::size(kOmniboxMlUrlScoringEnabledWithFixes), nullptr},
    {"unlimited suggestion candidates",
     kOmniboxMlUrlScoringUnlimitedNumCandidates,
     std::size(kOmniboxMlUrlScoringUnlimitedNumCandidates), nullptr},
    {"Increase provider max limit to 10",
     kOmniboxMlUrlScoringMaxMatchesByProvider10,
     std::size(kOmniboxMlUrlScoringMaxMatchesByProvider10), nullptr},
    {"with scoring of Search suggestions", kOmniboxMlUrlScoringWithSearches,
     std::size(kOmniboxMlUrlScoringWithSearches), nullptr},
    {"with scoring of verbatim URL suggestions",
     kOmniboxMlUrlScoringWithVerbatimURLs,
     std::size(kOmniboxMlUrlScoringWithVerbatimURLs), nullptr},
    {"with scoring of Search & verbatim URL suggestions",
     kOmniboxMlUrlScoringWithSearchesAndVerbatimURLs,
     std::size(kOmniboxMlUrlScoringWithSearchesAndVerbatimURLs), nullptr},
};

const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1300;0.14,1398;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1400"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingDemotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1250;0.14,1348;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1350"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1350;0.14,1448;1,1472"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1450"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy100[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1400;0.14,1498;1,1522"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1500"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingMobileMapping[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,590;0.006,790;0.082,1290;0.443,1360;0.464,1400;0.987,1425;1,1530"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1340"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};

const FeatureEntry::FeatureVariation
    kMlUrlPiecewiseMappedSearchBlendingVariations[] = {
        {"adjusted by 0", kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0,
         std::size(kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0), nullptr},
        {"demoted by 50", kMlUrlPiecewiseMappedSearchBlendingDemotedBy50,
         std::size(kMlUrlPiecewiseMappedSearchBlendingDemotedBy50), nullptr},
        {"promoted by 50", kMlUrlPiecewiseMappedSearchBlendingPromotedBy50,
         std::size(kMlUrlPiecewiseMappedSearchBlendingPromotedBy50), nullptr},
        {"promoted by 100", kMlUrlPiecewiseMappedSearchBlendingPromotedBy100,
         std::size(kMlUrlPiecewiseMappedSearchBlendingPromotedBy100), nullptr},
        {"mobile mapping", kMlUrlPiecewiseMappedSearchBlendingMobileMapping,
         std::size(kMlUrlPiecewiseMappedSearchBlendingMobileMapping), nullptr},
};

const FeatureEntry::FeatureParam kMlUrlSearchBlendingStable[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlending", "false"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedConservativeUrls[] =
    {
        {"MlUrlSearchBlending_StableSearchBlending", "false"},
        {"MlUrlSearchBlending_MappedSearchBlending", "true"},
        {"MlUrlSearchBlending_MappedSearchBlendingMin", "0"},
        {"MlUrlSearchBlending_MappedSearchBlendingMax", "2000"},
        {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1000"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedModerateUrls[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "false"},
    {"MlUrlSearchBlending_MappedSearchBlending", "true"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedAggressiveUrls[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "false"},
    {"MlUrlSearchBlending_MappedSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlendingMin", "1000"},
    {"MlUrlSearchBlending_MappedSearchBlendingMax", "4000"},
    {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1500"},
};

const FeatureEntry::FeatureVariation kMlUrlSearchBlendingVariations[] = {
    {"Stable", kMlUrlSearchBlendingStable,
     std::size(kMlUrlSearchBlendingStable), nullptr},
    {"Mapped conservative urls", kMlUrlSearchBlendingMappedConservativeUrls,
     std::size(kMlUrlSearchBlendingMappedConservativeUrls), nullptr},
    {"Mapped moderate urls", kMlUrlSearchBlendingMappedModerateUrls,
     std::size(kMlUrlSearchBlendingMappedModerateUrls), nullptr},
    {"Mapped aggressive urls", kMlUrlSearchBlendingMappedAggressiveUrls,
     std::size(kMlUrlSearchBlendingMappedAggressiveUrls), nullptr},
};

const FeatureEntry::FeatureParam
    kMostVitedTilesNewScoring_DecayStaircaseCap10[] = {
        {history::kMvtScoringParamRecencyFactor,
         history::kMvtScoringParamRecencyFactor_DecayStaircase},
        {history::kMvtScoringParamDailyVisitCountCap, "10"},
};
const FeatureEntry::FeatureVariation kMostVisitedTilesNewScoringVariations[] = {
    {"Decay Staircase, Cap 10", kMostVitedTilesNewScoring_DecayStaircaseCap10,
     std::size(kMostVitedTilesNewScoring_DecayStaircaseCap10), nullptr},
};

const FeatureEntry::FeatureVariation kUrlScoringModelVariations[] = {
    {"Small model (desktop)", nullptr, 0, nullptr},
    {"Full model (desktop)", nullptr, 0, "3380045"},
    {"Small model (ios)", nullptr, 0, "3379590"},
    {"Full model (ios)", nullptr, 0, "3380197"},
    {"Small model (android)", nullptr, 0, "3381543"},
    {"Full model (android)", nullptr, 0, "3381544"},
};

const FeatureEntry::FeatureParam
    kOmniboxZeroSuggestPrefetchDebouncingMinimalFromLastRun[] = {
        {"ZeroSuggestPrefetchDebounceDelay", "300"},
        {"ZeroSuggestPrefetchDebounceFromLastRun", "true"},
};
const FeatureEntry::FeatureParam
    kOmniboxZeroSuggestPrefetchDebouncingMinimalFromLastRequest[] = {
        {"ZeroSuggestPrefetchDebounceDelay", "300"},
        {"ZeroSuggestPrefetchDebounceFromLastRun", "false"},
};
const FeatureEntry::FeatureParam
    kOmniboxZeroSuggestPrefetchDebouncingModerateFromLastRun[] = {
        {"ZeroSuggestPrefetchDebounceDelay", "600"},
        {"ZeroSuggestPrefetchDebounceFromLastRun", "true"},
};
const FeatureEntry::FeatureParam
    kOmniboxZeroSuggestPrefetchDebouncingModerateFromLastRequest[] = {
        {"ZeroSuggestPrefetchDebounceDelay", "600"},
        {"ZeroSuggestPrefetchDebounceFromLastRun", "false"},
};
const FeatureEntry::FeatureParam
    kOmniboxZeroSuggestPrefetchDebouncingAggressiveFromLastRun[] = {
        {"ZeroSuggestPrefetchDebounceDelay", "900"},
        {"ZeroSuggestPrefetchDebounceFromLastRun", "true"},
};
const FeatureEntry::FeatureParam
    kOmniboxZeroSuggestPrefetchDebouncingAggressiveFromLastRequest[] = {
        {"ZeroSuggestPrefetchDebounceDelay", "900"},
        {"ZeroSuggestPrefetchDebounceFromLastRun", "false"},
};

const FeatureEntry::FeatureVariation
    kOmniboxZeroSuggestPrefetchDebouncingVariations[] = {
        {"Minimal debouncing relative to last run",
         kOmniboxZeroSuggestPrefetchDebouncingMinimalFromLastRun,
         std::size(kOmniboxZeroSuggestPrefetchDebouncingMinimalFromLastRun),
         nullptr},
        {"Minimal debouncing relative to last request",
         kOmniboxZeroSuggestPrefetchDebouncingMinimalFromLastRequest,
         std::size(kOmniboxZeroSuggestPrefetchDebouncingMinimalFromLastRequest),
         nullptr},
        {"Moderate debouncing relative to last run",
         kOmniboxZeroSuggestPrefetchDebouncingModerateFromLastRun,
         std::size(kOmniboxZeroSuggestPrefetchDebouncingModerateFromLastRun),
         nullptr},
        {"Moderate debouncing relative to last request",
         kOmniboxZeroSuggestPrefetchDebouncingModerateFromLastRequest,
         std::size(
             kOmniboxZeroSuggestPrefetchDebouncingModerateFromLastRequest),
         nullptr},
        {"Aggressive debouncing relative to last run",
         kOmniboxZeroSuggestPrefetchDebouncingAggressiveFromLastRun,
         std::size(kOmniboxZeroSuggestPrefetchDebouncingAggressiveFromLastRun),
         nullptr},
        {"Aggressive debouncing relative to last request",
         kOmniboxZeroSuggestPrefetchDebouncingAggressiveFromLastRequest,
         std::size(
             kOmniboxZeroSuggestPrefetchDebouncingAggressiveFromLastRequest),
         nullptr},
};

#if BUILDFLAG(IS_ANDROID)
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment1[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, ""}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment2[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "false"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, ""}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment3[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "false"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, ""}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment4[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, "reviews"}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment5[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, "call"}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment6[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, "directions"}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment7[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "true"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "false"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, "call"}};
constexpr FeatureEntry::FeatureParam kOmniboxActionsInSuggestTreatment8[] = {
    {OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name, "false"},
    {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name, "false"},
    {OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name, "call"}};

constexpr FeatureEntry::FeatureVariation kOmniboxActionsInSuggestVariants[] = {
    {"T1: Promote, Reviews, Directions, Calls",
     kOmniboxActionsInSuggestTreatment1,
     std::size(kOmniboxActionsInSuggestTreatment1), "t3366528"},
    {"T2: Reviews, Directions, Calls", kOmniboxActionsInSuggestTreatment2,
     std::size(kOmniboxActionsInSuggestTreatment2), "t3366528"},
    {"T3: Promote, Calls, Directions, Reviews",
     kOmniboxActionsInSuggestTreatment3,
     std::size(kOmniboxActionsInSuggestTreatment3), "t3366528"},
    {"T4: Promote, Directions, Calls", kOmniboxActionsInSuggestTreatment4,
     std::size(kOmniboxActionsInSuggestTreatment4), "t3366528"},
    {"T5: Promote, Reviews, Directions", kOmniboxActionsInSuggestTreatment5,
     std::size(kOmniboxActionsInSuggestTreatment5), "t3366528"},
    {"T6: Promote, Reviews, Calls", kOmniboxActionsInSuggestTreatment6,
     std::size(kOmniboxActionsInSuggestTreatment6), "t3366528"},
    {"T7: Promote, Directions, Reviews", kOmniboxActionsInSuggestTreatment7,
     std::size(kOmniboxActionsInSuggestTreatment7), "t3366528"},
    {"T8: Directions, Reviews", kOmniboxActionsInSuggestTreatment8,
     std::size(kOmniboxActionsInSuggestTreatment8), "t3366528"},
};

constexpr FeatureEntry::FeatureParam kOmniboxAnswerActionsCounterfactual[] = {
    {OmniboxFieldTrial::kAnswerActionsCounterfactual.name, "true"}};
constexpr FeatureEntry::FeatureParam kOmniboxAnswerActionsTreatment1[] = {
    {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "true"},
    {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "false"}};

constexpr FeatureEntry::FeatureParam kOmniboxAnswerActionsTreatment2[] = {
    {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "true"},
    {OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "false"}};

constexpr FeatureEntry::FeatureParam kOmniboxAnswerActionsTreatment3[] = {
    {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "false"}};

constexpr FeatureEntry::FeatureParam kOmniboxAnswerActionsTreatment4[] = {
    {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "true"},
    {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "true"}};

constexpr FeatureEntry::FeatureParam kOmniboxAnswerActionsTreatment5[] = {
    {OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.name, "false"},
    {OmniboxFieldTrial::kAnswerActionsShowRichCard.name, "true"}};

constexpr FeatureEntry::FeatureVariation kOmniboxAnswerActionsVariants[] = {
    {"Counterfactual: fetch without rendering ",
     kOmniboxAnswerActionsCounterfactual,
     std::size(kOmniboxAnswerActionsCounterfactual), "t3379046"},
    {"T1: Show chips above keyboard when there are no url matches",
     kOmniboxAnswerActionsTreatment1,
     std::size(kOmniboxAnswerActionsTreatment1), "t3379047"},
    {"T2: Show chips at position 0", kOmniboxAnswerActionsTreatment2,
     std::size(kOmniboxAnswerActionsTreatment2), "t3379048"},
    {"T3: Show chips at position 0 when there are no url matches",
     kOmniboxAnswerActionsTreatment3,
     std::size(kOmniboxAnswerActionsTreatment3), "t3379049"},
    {"T4: Show rich card above keyboard when there are no url matches",
     kOmniboxAnswerActionsTreatment4,
     std::size(kOmniboxAnswerActionsTreatment4), "t3379050"},
    {"T5: Show rich card at position 0 when there are no url matches",
     kOmniboxAnswerActionsTreatment5,
     std::size(kOmniboxAnswerActionsTreatment5), "t3379051"},
};

#endif  // BUILDFLAG(IS_ANDROID)

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

const FeatureEntry::FeatureParam kRepeatableQueries_6Searches_90Days[] = {
    {"RepeatableQueriesIgnoreDuplicateVisits", "true"},
    {"RepeatableQueriesMinVisitCount", "6"},
};
const FeatureEntry::FeatureParam kRepeatableQueries_12Searches_90Days[] = {
    {"RepeatableQueriesIgnoreDuplicateVisits", "true"},
    {"RepeatableQueriesMinVisitCount", "12"},
};
const FeatureEntry::FeatureParam kRepeatableQueries_6Searches_7Days[] = {
    {"RepeatableQueriesIgnoreDuplicateVisits", "true"},
    {"RepeatableQueriesMinVisitCount", "6"},
    {"RepeatableQueriesMaxAgeDays", "7"},
};
const FeatureEntry::FeatureParam kRepeatableQueries_12Searches_7Days[] = {
    {"RepeatableQueriesIgnoreDuplicateVisits", "true"},
    {"RepeatableQueriesMinVisitCount", "12"},
    {"RepeatableQueriesMaxAgeDays", "7"},
};

const FeatureEntry::FeatureVariation kOrganicRepeatableQueriesVariations[] = {
    {"6+ uses, once in last 90d", kRepeatableQueries_6Searches_90Days,
     std::size(kRepeatableQueries_6Searches_90Days), nullptr},
    {"12+ uses, once in last 90d", kRepeatableQueries_12Searches_90Days,
     std::size(kRepeatableQueries_12Searches_90Days), nullptr},
    {"6+ uses, once in last 7d", kRepeatableQueries_6Searches_7Days,
     std::size(kRepeatableQueries_6Searches_7Days), nullptr},
    {"12+ uses, once in last 7d", kRepeatableQueries_12Searches_7Days,
     std::size(kRepeatableQueries_12Searches_7Days), nullptr},
};
#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kMinimumTabWidthSettingPinned[] = {
    {tabs::kMinimumTabWidthFeatureParameterName, "54"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingMedium[] = {
    {tabs::kMinimumTabWidthFeatureParameterName, "72"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingLarge[] = {
    {tabs::kMinimumTabWidthFeatureParameterName, "140"}};
const FeatureEntry::FeatureParam kMinimumTabWidthSettingFull[] = {
    {tabs::kMinimumTabWidthFeatureParameterName, "256"}};

const FeatureEntry::FeatureVariation kTabScrollingVariations[] = {
    {" - tabs shrink to pinned tab width", kMinimumTabWidthSettingPinned,
     std::size(kMinimumTabWidthSettingPinned), nullptr},
    {" - tabs shrink to a medium width", kMinimumTabWidthSettingMedium,
     std::size(kMinimumTabWidthSettingMedium), nullptr},
    {" - tabs shrink to a large width", kMinimumTabWidthSettingLarge,
     std::size(kMinimumTabWidthSettingLarge), nullptr},
    {" - tabs don't shrink", kMinimumTabWidthSettingFull,
     std::size(kMinimumTabWidthSettingFull), nullptr}};
#endif
const FeatureEntry::FeatureParam kTabScrollingButtonPositionRight[] = {
    {features::kTabScrollingButtonPositionParameterName, "0"}};
const FeatureEntry::FeatureParam kTabScrollingButtonPositionLeft[] = {
    {features::kTabScrollingButtonPositionParameterName, "1"}};
const FeatureEntry::FeatureParam kTabScrollingButtonPositionSplit[] = {
    {features::kTabScrollingButtonPositionParameterName, "2"}};

const FeatureEntry::FeatureVariation kTabScrollingButtonPositionVariations[] = {
    {" - to the right of the tabstrip", kTabScrollingButtonPositionRight,
     std::size(kTabScrollingButtonPositionRight), nullptr},
    {" - to the left of the tabstrip", kTabScrollingButtonPositionLeft,
     std::size(kTabScrollingButtonPositionLeft), nullptr},
    {" - on both sides of the tabstrip", kTabScrollingButtonPositionSplit,
     std::size(kTabScrollingButtonPositionSplit), nullptr}};

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kTabScrollingWithDraggingWithConstantSpeed[] =
    {{tabs::kTabScrollingWithDraggingModeName, "1"}};
const FeatureEntry::FeatureParam kTabScrollingWithDraggingWithVariableSpeed[] =
    {{tabs::kTabScrollingWithDraggingModeName, "2"}};

const FeatureEntry::FeatureVariation kTabScrollingWithDraggingVariations[] = {
    {" - tabs scrolling with constant speed",
     kTabScrollingWithDraggingWithConstantSpeed,
     std::size(kTabScrollingWithDraggingWithConstantSpeed), nullptr},
    {" - tabs scrolling with variable speed region",
     kTabScrollingWithDraggingWithVariableSpeed,
     std::size(kTabScrollingWithDraggingWithVariableSpeed), nullptr}};

const FeatureEntry::FeatureParam kScrollableTabStripOverflowDivider[] = {
    {tabs::kScrollableTabStripOverflowModeName, "1"}};
const FeatureEntry::FeatureParam kScrollableTabStripOverflowFade[] = {
    {tabs::kScrollableTabStripOverflowModeName, "2"}};
const FeatureEntry::FeatureParam kScrollableTabStripOverflowShadow[] = {
    {tabs::kScrollableTabStripOverflowModeName, "3"}};

const FeatureEntry::FeatureVariation kScrollableTabStripOverflowVariations[] = {
    {" - Divider", kScrollableTabStripOverflowDivider,
     std::size(kScrollableTabStripOverflowDivider), nullptr},  // Divider
    {" - Fade", kScrollableTabStripOverflowFade,
     std::size(kScrollableTabStripOverflowFade), nullptr},  // Fade
    {" - Shadow", kScrollableTabStripOverflowShadow,
     std::size(kScrollableTabStripOverflowShadow), nullptr},  // Shadow
};
#endif

const FeatureEntry::FeatureParam kChromeLabsEnabledInFlags[] = {
    {features::kChromeLabsActivationParameterName, "100"}};

const FeatureEntry::FeatureVariation kChromeLabsVariations[] = {
    {" use this one!", kChromeLabsEnabledInFlags,
     std::size(kChromeLabsEnabledInFlags), nullptr}};

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kNtpCalendarModuleFakeData[] = {
    {ntp_features::kNtpCalendarModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpCalendarModuleVariations[] = {
    {"- Fake Data", kNtpCalendarModuleFakeData,
     std::size(kNtpCalendarModuleFakeData), nullptr},
};

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

// The following is Code-based RBD variation.
const flags_ui::FeatureEntry::FeatureParam kCodeBasedRBDEnabled[] = {
    {commerce::kCodeBasedRuleDiscountParam, "true"}};

const FeatureEntry::FeatureVariation kCodeBasedRBDVariations[] = {
    {"code-based RBD", kCodeBasedRBDEnabled, std::size(kCodeBasedRBDEnabled),
     "t3362898"},
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

const FeatureEntry::FeatureParam kNtpOutlookCalendarModuleFakeData[] = {
    {ntp_features::kNtpOutlookCalendarModuleDataParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpOutlookCalendarModuleVariations[] = {
    {"- Fake Data", kNtpOutlookCalendarModuleFakeData,
     std::size(kNtpOutlookCalendarModuleFakeData), nullptr},
};

const FeatureEntry::FeatureParam kNtpMiddleSlotPromoDismissalFakeData[] = {
    {ntp_features::kNtpMiddleSlotPromoDismissalParam, "fake"}};
const FeatureEntry::FeatureVariation kNtpMiddleSlotPromoDismissalVariations[] =
    {
        {"- Fake Data", kNtpMiddleSlotPromoDismissalFakeData,
         std::size(kNtpMiddleSlotPromoDismissalFakeData), nullptr},
};

const FeatureEntry::FeatureParam
    kNtpRealboxCr23NoShadowExpandedStateBgMatchesSteadyState[]{
        {"kNtpRealboxCr23ExpandedStateBgMatchesOmnibox", "false"},
        {"kNtpRealboxCr23SteadyStateShadow", "false"}};
const FeatureEntry::FeatureParam
    kNtpRealboxCr23ShadowExpandedStateBgMatchesOmnibox[]{
        {"kNtpRealboxCr23ExpandedStateBgMatchesOmnibox", "true"},
        {"kNtpRealboxCr23SteadyStateShadow", "true"}};
const FeatureEntry::FeatureParam
    kNtpRealboxCr23ShadowExpandedStateBgMatchesSteadyState[]{
        {"kNtpRealboxCr23ExpandedStateBgMatchesOmnibox", "false"},
        {"kNtpRealboxCr23SteadyStateShadow", "true"}};

const FeatureEntry::FeatureVariation kNtpRealboxCr23ThemingVariations[] = {
    {" - Steady state shadow",
     kNtpRealboxCr23ShadowExpandedStateBgMatchesOmnibox,
     std::size(kNtpRealboxCr23ShadowExpandedStateBgMatchesOmnibox), nullptr},
    {" - No steady state shadow + Dark mode background color matches steady"
     "state",
     kNtpRealboxCr23NoShadowExpandedStateBgMatchesSteadyState,
     std::size(kNtpRealboxCr23NoShadowExpandedStateBgMatchesSteadyState),
     nullptr},
    {" -  Steady state shadow + Dark mode background color matches steady "
     "state",
     kNtpRealboxCr23ShadowExpandedStateBgMatchesSteadyState,
     std::size(kNtpRealboxCr23ShadowExpandedStateBgMatchesSteadyState),
     nullptr},
};

const FeatureEntry::FeatureParam kNtpSafeBrowsingModuleFastCooldown[] = {
    {ntp_features::kNtpSafeBrowsingModuleCooldownPeriodDaysParam, "0.001"},
    {ntp_features::kNtpSafeBrowsingModuleCountMaxParam, "1"}};
const FeatureEntry::FeatureVariation kNtpSafeBrowsingModuleVariations[] = {
    {"(Fast Cooldown)", kNtpSafeBrowsingModuleFastCooldown,
     std::size(kNtpSafeBrowsingModuleFastCooldown), nullptr},
};

const FeatureEntry::FeatureParam kNtpMostRelevantTabResumptionModuleFakeData[] =
    {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam, "Fake Data"}};
const FeatureEntry::FeatureParam
    kNtpMostRelevantTabResumptionModuleFakeDataMostRecent[] = {
        {ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
         "Fake Data - Most Recent Decorator"}};
const FeatureEntry::FeatureParam
    kNtpMostRelevantTabResumptionModuleFakeDataFrequentlyVisitedAtTime[] = {
        {ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
         "Fake Data - Frequently Visited At Time Decorator"}};
const FeatureEntry::FeatureParam
    kNtpMostRelevantTabResumptionModuleFakeDataJustVisited[] = {
        {ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
         "Fake Data - Just Visited Decorator"}};
const FeatureEntry::FeatureParam kNtpMostRelevantTabResumptionModuleTabData[] =
    {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam, "1,2"}};
const FeatureEntry::FeatureParam
    kNtpMostRelevantTabResumptionModuleRemoteTabData[] = {
        {ntp_features::kNtpMostRelevantTabResumptionModuleDataParam, "2"}};
const FeatureEntry::FeatureParam
    kNtpMostRelevantTabResumptionModuleVisitData[] = {
        {ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
         "1,2,3,4"}};
// Most relevant tab resumption module data params may be expressed as a comma
// separated value consisting of the integer representations of the
// `FetchOptions::URLType` enumeration, to specify what URL types should be
// provided as options to the Visited URL Ranking Service's APIs.
const FeatureEntry::FeatureVariation
    kNtpMostRelevantTabResumptionModuleVariations[] = {
        {"- Fake Data", kNtpMostRelevantTabResumptionModuleFakeData,
         std::size(kNtpMostRelevantTabResumptionModuleFakeData), nullptr},
        {"- Fake Data - Most Recent Decorator",
         kNtpMostRelevantTabResumptionModuleFakeDataMostRecent,
         std::size(kNtpMostRelevantTabResumptionModuleFakeData), nullptr},
        {"- Fake Data - Frequently Visited At Time Decorator",
         kNtpMostRelevantTabResumptionModuleFakeDataFrequentlyVisitedAtTime,
         std::size(kNtpMostRelevantTabResumptionModuleFakeData), nullptr},
        {"- Fake Data - Just Visited Decorator",
         kNtpMostRelevantTabResumptionModuleFakeDataJustVisited,
         std::size(kNtpMostRelevantTabResumptionModuleFakeData), nullptr},
        {"- Tabs Only", kNtpMostRelevantTabResumptionModuleTabData,
         std::size(kNtpMostRelevantTabResumptionModuleTabData), nullptr},
        {"- Remote Tabs Only", kNtpMostRelevantTabResumptionModuleRemoteTabData,
         std::size(kNtpMostRelevantTabResumptionModuleRemoteTabData), nullptr},
        {"- All Visits", kNtpMostRelevantTabResumptionModuleVisitData,
         std::size(kNtpMostRelevantTabResumptionModuleVisitData), nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kPerformanceInterventionStringVersion1[] = {
    {"intervention_dialog_version", "1"}};
const FeatureEntry::FeatureParam kPerformanceInterventionStringVersion2[] = {
    {"intervention_dialog_version", "2"}};
const FeatureEntry::FeatureParam kPerformanceInterventionStringVersion3[] = {
    {"intervention_dialog_version", "3"}};

const FeatureEntry::FeatureVariation
    kPerformanceInterventionStringVariations[] = {
        {"String version 1", kPerformanceInterventionStringVersion1,
         std::size(kPerformanceInterventionStringVersion1), nullptr},
        {"String version 2", kPerformanceInterventionStringVersion2,
         std::size(kPerformanceInterventionStringVersion2), nullptr},
        {"String version 3", kPerformanceInterventionStringVersion3,
         std::size(kPerformanceInterventionStringVersion3), nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kContextualSearchSuppressShortViewWith300Dp[] =
    {{"contextual_search_minimum_page_height_dp", "300"}};
const FeatureEntry::FeatureParam kContextualSearchSuppressShortViewWith400Dp[] =
    {{"contextual_search_minimum_page_height_dp", "400"}};
const FeatureEntry::FeatureParam kContextualSearchSuppressShortViewWith500Dp[] =
    {{"contextual_search_minimum_page_height_dp", "500"}};
const FeatureEntry::FeatureParam kContextualSearchSuppressShortViewWith600Dp[] =
    {{"contextual_search_minimum_page_height_dp", "600"}};
const FeatureEntry::FeatureVariation
    kContextualSearchSuppressShortViewVariations[] = {
        {"(300 dp)", kContextualSearchSuppressShortViewWith300Dp,
         std::size(kContextualSearchSuppressShortViewWith300Dp), nullptr},
        {"(400 dp)", kContextualSearchSuppressShortViewWith400Dp,
         std::size(kContextualSearchSuppressShortViewWith400Dp), nullptr},
        {"(500 dp)", kContextualSearchSuppressShortViewWith500Dp,
         std::size(kContextualSearchSuppressShortViewWith500Dp), nullptr},
        {"(600 dp)", kContextualSearchSuppressShortViewWith600Dp,
         std::size(kContextualSearchSuppressShortViewWith600Dp), nullptr},
};

const FeatureEntry::FeatureParam kRichAutocompletionFullUrlThreeMinChar[] = {
    {"rich_autocomplete_full_url", "true"},
    {"rich_autocomplete_minimum_characters", "3"}};
const FeatureEntry::FeatureParam kRichAutocompletionNoFullUrlThreeMinChar[] = {
    {"rich_autocomplete_full_url", "false"},
    {"rich_autocomplete_minimum_characters", "3"}};
const FeatureEntry::FeatureParam kRichAutocompletionFullUrlFourMinChar[] = {
    {"rich_autocomplete_full_url", "true"},
    {"rich_autocomplete_minimum_characters", "4"}};
const FeatureEntry::FeatureParam kRichAutocompletionNoFullUrlFourMinChar[] = {
    {"rich_autocomplete_full_url", "false"},
    {"rich_autocomplete_minimum_characters", "4"}};
const FeatureEntry::FeatureVariation kRichAutocompletionAndroidVariations[] = {
    {"(full url, 3 chars at least)", kRichAutocompletionFullUrlThreeMinChar,
     std::size(kRichAutocompletionFullUrlThreeMinChar), nullptr},
    {"(no full url, 3 chars at least)",
     kRichAutocompletionNoFullUrlThreeMinChar,
     std::size(kRichAutocompletionNoFullUrlThreeMinChar), nullptr},
    {"(full url, 4 chars at least)", kRichAutocompletionFullUrlFourMinChar,
     std::size(kRichAutocompletionFullUrlFourMinChar), nullptr},
    {"(no full url, 4 chars at least)", kRichAutocompletionNoFullUrlFourMinChar,
     std::size(kRichAutocompletionNoFullUrlFourMinChar), nullptr},
};

#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kStartSurfaceReturnTime_Immediate[] = {
    {"start_surface_return_time_seconds", "0"},
    {"start_surface_return_time_on_tablet_seconds", "0"}};
const FeatureEntry::FeatureParam kStartSurfaceReturnTime_10Seconds[] = {
    {"start_surface_return_time_seconds", "10"},
    {"start_surface_return_time_on_tablet_seconds", "10"}};
const FeatureEntry::FeatureParam kStartSurfaceReturnTime_1Minute[] = {
    {"start_surface_return_time_seconds", "60"},
    {"start_surface_return_time_on_tablet_seconds", "60"}};
const FeatureEntry::FeatureParam kStartSurfaceReturnTime_5Minute[] = {
    {"start_surface_return_time_seconds", "300"},
    {"start_surface_return_time_on_tablet_seconds", "300"}};
const FeatureEntry::FeatureParam kStartSurfaceReturnTime_60Minute[] = {
    {"start_surface_return_time_seconds", "3600"},
    {"start_surface_return_time_on_tablet_seconds", "3600"}};
const FeatureEntry::FeatureVariation kStartSurfaceReturnTimeVariations[] = {
    {"Immediate", kStartSurfaceReturnTime_Immediate,
     std::size(kStartSurfaceReturnTime_Immediate), nullptr},
    {"10 seconds", kStartSurfaceReturnTime_10Seconds,
     std::size(kStartSurfaceReturnTime_10Seconds), nullptr},
    {"1 minute", kStartSurfaceReturnTime_1Minute,
     std::size(kStartSurfaceReturnTime_1Minute), nullptr},
    {"5 minute", kStartSurfaceReturnTime_5Minute,
     std::size(kStartSurfaceReturnTime_5Minute), nullptr},
    {"60 minute", kStartSurfaceReturnTime_60Minute,
     std::size(kStartSurfaceReturnTime_60Minute), nullptr},
};

const FeatureEntry::FeatureParam kMagicStackAndroid_show_all_modules[] = {
    {"show_all_modules", "true"}};

const FeatureEntry::FeatureVariation kMagicStackAndroidVariations[] = {
    {"Show all modules", kMagicStackAndroid_show_all_modules,
     std::size(kMagicStackAndroid_show_all_modules), nullptr},
};

const FeatureEntry::FeatureParam kEducationalTipModule_force_default_browser[] =
    {{"force_default_browser", "true"}};
const FeatureEntry::FeatureParam kEducationalTipModule_force_tab_group[] = {
    {"force_tab_group", "true"}};
const FeatureEntry::FeatureParam kEducationalTipModule_force_tab_group_sync[] =
    {{"force_tab_group_sync", "true"}};
const FeatureEntry::FeatureParam kEducationalTipModule_force_quick_delete[] = {
    {"force_quick_delete", "true"}};

const FeatureEntry::FeatureVariation kEducationalTipModuleVariations[] = {
    {"Show default browser promo", kEducationalTipModule_force_default_browser,
     std::size(kEducationalTipModule_force_default_browser), nullptr},
    {"Show tab group promo", kEducationalTipModule_force_tab_group,
     std::size(kEducationalTipModule_force_tab_group), nullptr},
    {"Show tab group sync promo", kEducationalTipModule_force_tab_group_sync,
     std::size(kEducationalTipModule_force_tab_group_sync), nullptr},
    {"Show quick delete promo", kEducationalTipModule_force_quick_delete,
     std::size(kEducationalTipModule_force_quick_delete), nullptr},
};

const FeatureEntry::FeatureParam
    kAccountReauthenticationRecentTimeWindow_0Minutes[] = {
        {"account_reauthentication_recent_time_window_minutes", "0"},
};
const FeatureEntry::FeatureParam
    kAccountReauthenticationRecentTimeWindow_1Minutes[] = {
        {"account_reauthentication_recent_time_window_minutes", "1"},
};
const FeatureEntry::FeatureParam
    kAccountReauthenticationRecentTimeWindow_5Minutes[] = {
        {"account_reauthentication_recent_time_window_minutes", "5"},
};
const FeatureEntry::FeatureParam
    kAccountReauthenticationRecentTimeWindow_10Minutes[] = {
        {"account_reauthentication_recent_time_window_minutes", "10"},
};
const FeatureEntry::FeatureVariation
    kAccountReauthenticationRecentTimeWindowVariations[] = {
        {"0 minutes", kAccountReauthenticationRecentTimeWindow_0Minutes,
         std::size(kAccountReauthenticationRecentTimeWindow_0Minutes), nullptr},
        {"1 minutes", kAccountReauthenticationRecentTimeWindow_1Minutes,
         std::size(kAccountReauthenticationRecentTimeWindow_1Minutes), nullptr},
        {"5 minutes", kAccountReauthenticationRecentTimeWindow_5Minutes,
         std::size(kAccountReauthenticationRecentTimeWindow_5Minutes), nullptr},
        {"10 minutes", kAccountReauthenticationRecentTimeWindow_10Minutes,
         std::size(kAccountReauthenticationRecentTimeWindow_10Minutes),
         nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kLogoPolish_large[] = {
    {"polish_logo_size_large", "true"},
    {"polish_logo_size_medium", "false"}};

const FeatureEntry::FeatureParam kLogoPolish_medium[] = {
    {"polish_logo_size_large", "false"},
    {"polish_logo_size_medium", "true"}};

const FeatureEntry::FeatureParam kLogoPolish_small[] = {
    {"polish_logo_size_large", "false"},
    {"polish_logo_size_medium", "false"}};

const FeatureEntry::FeatureVariation kLogoPolishVariations[] = {
    {"Logo height is large", kLogoPolish_large, std::size(kLogoPolish_large),
     nullptr},
    {"Logo height is medium", kLogoPolish_medium, std::size(kLogoPolish_medium),
     nullptr},
    {"Logo height is small", kLogoPolish_small, std::size(kLogoPolish_small),
     nullptr},
};

const FeatureEntry::FeatureParam kFeedPositionAndroid_pull_up_feed[] = {
    {"pull_up_feed", "true"}};

const FeatureEntry::FeatureParam
    kFeedPositionAndroid_push_down_feed_target_feed_active[] = {
        {"feed_active_targeting", "active"}};

const FeatureEntry::FeatureParam
    kFeedPositionAndroid_push_down_feed_target_non_feed_active[] = {
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
    {"Pull up Feed", kFeedPositionAndroid_pull_up_feed,
     std::size(kFeedPositionAndroid_pull_up_feed), nullptr},
    {"Push down Feed with targeting Feed active users",
     kFeedPositionAndroid_push_down_feed_target_feed_active,
     std::size(kFeedPositionAndroid_push_down_feed_target_feed_active),
     nullptr},
    {"Push down Feed with targeting non-Feed active users",
     kFeedPositionAndroid_push_down_feed_target_non_feed_active,
     std::size(kFeedPositionAndroid_push_down_feed_target_non_feed_active),
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

const FeatureEntry::FeatureParam kTabResumptionModule_defaul_app_filter[] = {
    {"show_see_more", "true"},
    {"use_default_app_filter", "true"},
};
const FeatureEntry::FeatureParam kTabResumptionModule_salient_image[] = {
    {"show_see_more", "true"},
    {"use_default_app_filter", "true"},
    {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_single_tile_with_salient_image[] = {
        {"max_tiles_number", "1"},
        {"show_see_more", "true"},
        {"use_default_app_filter", "true"},
        {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_single_tile_with_salient_image_show_default_reason[] =
        {
            {"max_tiles_number", "1"},     {"show_default_reason", "true"},
            {"show_see_more", "true"},     {"use_default_app_filter", "true"},
            {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_combine_tabs_with_salient_image[] = {
        {"show_see_more", "true"},
        {"show_tabs_in_one_module", "true"},
        {"use_default_app_filter", "true"},
        {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_combine_tabs_with_salient_image_single_tab_show_default_reason
        [] = {
            {"max_tiles_number", "1"},
            {"show_default_reason", "true"},
            {"show_see_more", "true"},
            {"show_tabs_in_one_module", "true"},
            {"use_default_app_filter", "true"},
            {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam kTabResumptionModule_enable_v2_arm1[] = {
    {"disable_blend", "true"},          {"enable_v2", "true"},
    {"show_see_more", "true"},          {"show_tabs_in_one_module", "true"},
    {"use_default_app_filter", "true"},
};
const FeatureEntry::FeatureParam kTabResumptionModule_enable_v2_arm2[] = {
    {"enable_v2", "true"},
    {"show_see_more", "true"},
    {"show_tabs_in_one_module", "true"},
    {"use_default_app_filter", "true"},
    {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam kTabResumptionModule_enable_v2_arm3[] = {
    {"disable_blend", "true"},           {"enable_v2", "true"},
    {"max_tiles_number", "1"},           {"show_see_more", "true"},
    {"show_tabs_in_one_module", "true"}, {"use_default_app_filter", "true"},
    {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_enable_v2_arm3_show_default_reason[] = {
        {"disable_blend", "true"},          {"enable_v2", "true"},
        {"max_tiles_number", "1"},          {"show_default_reason", "true"},
        {"show_see_more", "true"},          {"show_tabs_in_one_module", "true"},
        {"use_default_app_filter", "true"}, {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam kTabResumptionModule_enable_v2_arm4[] = {
    {"disable_blend", "true"},          {"enable_v2", "true"},
    {"show_see_more", "true"},          {"show_tabs_in_one_module", "true"},
    {"use_default_app_filter", "true"}, {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam kTabResumptionModule_enable_v2_ml[] = {
    {"enable_v2", "true"},
    {"fetch_history_backend", "true"},
    {"fetch_local_tabs_backend", "true"},
    {"show_see_more", "true"},
    {"show_tabs_in_one_module", "true"},
    {"use_default_app_filter", "true"},
    {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_enable_v2_ml_show_decorator_default_reason[] = {
        {"enable_v2", "true"},
        {"fetch_history_backend", "true"},
        {"fetch_local_tabs_backend", "true"},
        {"max_tiles_number", "1"},
        {"show_default_reason", "true"},
        {"show_see_more", "true"},
        {"show_tabs_in_one_module", "true"},
        {"use_default_app_filter", "true"},
        {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_enable_v2_ml_show_decorator_visited_x_ago[] = {
        {"enable_v2", "true"},
        {"fetch_history_backend", "true"},
        {"fetch_local_tabs_backend", "true"},
        {"override_decoration", "1"},  // kVisitedXAgo
        {"max_tiles_number", "1"},
        {"show_default_reason", "true"},
        {"show_see_more", "true"},
        {"show_tabs_in_one_module", "true"},
        {"use_default_app_filter", "true"},
        {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_enable_v2_ml_show_decorator_most_visited[] = {
        {"enable_v2", "true"},
        {"fetch_history_backend", "true"},
        {"fetch_local_tabs_backend", "true"},
        {"override_decoration", "2"},  // kMostRecent
        {"max_tiles_number", "1"},
        {"show_default_reason", "true"},
        {"show_see_more", "true"},
        {"show_tabs_in_one_module", "true"},
        {"use_default_app_filter", "true"},
        {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_enable_v2_ml_show_decorator_frequently_visited[] = {
        {"enable_v2", "true"},
        {"fetch_history_backend", "true"},
        {"fetch_local_tabs_backend", "true"},
        {"override_decoration", "3"},  // kFrequentlyVisited
        {"max_tiles_number", "1"},
        {"show_default_reason", "true"},
        {"show_see_more", "true"},
        {"show_tabs_in_one_module", "true"},
        {"use_default_app_filter", "true"},
        {"use_salient_image", "true"},
};
const FeatureEntry::FeatureParam
    kTabResumptionModule_enable_v2_ml_show_decorator_frequently_visited_at_time
        [] = {
            {"enable_v2", "true"},
            {"fetch_history_backend", "true"},
            {"fetch_local_tabs_backend", "true"},
            {"override_decoration", "4"},  // kFrequentlyVisitedAtTime
            {"max_tiles_number", "1"},
            {"show_default_reason", "true"},
            {"show_see_more", "true"},
            {"show_tabs_in_one_module", "true"},
            {"use_default_app_filter", "true"},
            {"use_salient_image", "true"},
};
const FeatureEntry::FeatureVariation kTabResumptionModuleAndroidVariations[] = {
    {"Default app filter", kTabResumptionModule_defaul_app_filter,
     std::size(kTabResumptionModule_defaul_app_filter), nullptr},
    {"Default app filter + Salient image", kTabResumptionModule_salient_image,
     std::size(kTabResumptionModule_salient_image), nullptr},
    {"Default app filter + Salient image + single tile",
     kTabResumptionModule_single_tile_with_salient_image,
     std::size(kTabResumptionModule_single_tile_with_salient_image), nullptr},
    {"Default app filter + Salient image + one Tab module",
     kTabResumptionModule_combine_tabs_with_salient_image,
     std::size(kTabResumptionModule_combine_tabs_with_salient_image), nullptr},
    {"V2 Arm1", kTabResumptionModule_enable_v2_arm1,
     std::size(kTabResumptionModule_enable_v2_arm1), nullptr},
    {"V2 Arm2", kTabResumptionModule_enable_v2_arm2,
     std::size(kTabResumptionModule_enable_v2_arm2), nullptr},
    {"V2 Arm3", kTabResumptionModule_enable_v2_arm3,
     std::size(kTabResumptionModule_enable_v2_arm3), nullptr},
    {"V2 Arm4", kTabResumptionModule_enable_v2_arm4,
     std::size(kTabResumptionModule_enable_v2_arm4), nullptr},
    {"V2 ML (collect data)", kTabResumptionModule_enable_v2_ml,
     std::size(kTabResumptionModule_enable_v2_ml), nullptr},
    {"Default app filter + Salient image + single tile + default reason",
     kTabResumptionModule_single_tile_with_salient_image_show_default_reason,
     std::size(
         kTabResumptionModule_single_tile_with_salient_image_show_default_reason),
     nullptr},
    {"Default app filter + one Tab module + single tab + default reason",
     kTabResumptionModule_combine_tabs_with_salient_image_single_tab_show_default_reason,
     std::size(
         kTabResumptionModule_combine_tabs_with_salient_image_single_tab_show_default_reason),
     nullptr},
    {"V2 Arm3 with default reason",
     kTabResumptionModule_enable_v2_arm3_show_default_reason,
     std::size(kTabResumptionModule_enable_v2_arm3_show_default_reason),
     nullptr},
    {"V2 ML: with default reason",
     kTabResumptionModule_enable_v2_ml_show_decorator_default_reason,
     std::size(kTabResumptionModule_enable_v2_ml_show_decorator_default_reason),
     nullptr},
    {"V2 ML: visited x ago decorator",
     kTabResumptionModule_enable_v2_ml_show_decorator_visited_x_ago,
     std::size(kTabResumptionModule_enable_v2_ml_show_decorator_visited_x_ago),
     nullptr},
    {"V2 ML: most visited decorator",
     kTabResumptionModule_enable_v2_ml_show_decorator_most_visited,
     std::size(kTabResumptionModule_enable_v2_ml_show_decorator_most_visited),
     nullptr},
    {"V2 ML: frequently visited decorator",
     kTabResumptionModule_enable_v2_ml_show_decorator_frequently_visited,
     std::size(
         kTabResumptionModule_enable_v2_ml_show_decorator_frequently_visited),
     nullptr},
    {"V2 ML: frequently visited at time decorator",
     kTabResumptionModule_enable_v2_ml_show_decorator_frequently_visited_at_time,
     std::size(
         kTabResumptionModule_enable_v2_ml_show_decorator_frequently_visited_at_time),
     nullptr},
};

const FeatureEntry::FeatureParam
    kMostVitedTilesReselect_enable_partial_match_arm1[] = {
        {"lax_scheme_host", "true"},
        {"lax_ref", "true"},
        {"lax_query", "false"},
        {"lax_path", "false"},
};
const FeatureEntry::FeatureParam
    kMostVitedTilesReselect_enable_partial_match_arm2[] = {
        {"lax_scheme_host", "true"},
        {"lax_ref", "true"},
        {"lax_query", "true"},
        {"lax_path", "false"},
};
const FeatureEntry::FeatureParam
    kMostVitedTilesReselect_enable_partial_match_arm3[] = {
        {"lax_scheme_host", "true"},
        {"lax_ref", "true"},
        {"lax_query", "true"},
        {"lax_path", "true"},
};
const FeatureEntry::FeatureVariation kMostVisitedTilesReselectVariations[] = {
    {"Partial match Arm 1", kMostVitedTilesReselect_enable_partial_match_arm1,
     std::size(kMostVitedTilesReselect_enable_partial_match_arm1), nullptr},
    {"Partial match Arm 2", kMostVitedTilesReselect_enable_partial_match_arm2,
     std::size(kMostVitedTilesReselect_enable_partial_match_arm2), nullptr},
    {"Partial match Arm 3", kMostVitedTilesReselect_enable_partial_match_arm3,
     std::size(kMostVitedTilesReselect_enable_partial_match_arm3), nullptr},
};

const FeatureEntry::FeatureParam
    kNotificationPermissionRationale_show_dialog_next_start[] = {
        {"always_show_rationale_before_requesting_permission", "true"},
        {"permission_request_interval_days", "0"},
};

const FeatureEntry::FeatureVariation
    kNotificationPermissionRationaleVariations[] = {
        {"- Show rationale UI on next startup",
         kNotificationPermissionRationale_show_dialog_next_start,
         std::size(kNotificationPermissionRationale_show_dialog_next_start),
         nullptr},
};

const FeatureEntry::FeatureParam kWebFeedAwareness_new_animation[] = {
    {"awareness_style", "new_animation"}};
const FeatureEntry::FeatureParam kWebFeedAwareness_new_animation_no_limit[] = {
    {"awareness_style", "new_animation_no_limit"}};

const FeatureEntry::FeatureParam kWebFeedAwareness_IPH[] = {
    {"awareness_style", "IPH"}};

const FeatureEntry::FeatureVariation kWebFeedAwarenessVariations[] = {
    {"new animation", kWebFeedAwareness_new_animation,
     std::size(kWebFeedAwareness_new_animation), nullptr},
    {"new animation rate limit off", kWebFeedAwareness_new_animation_no_limit,
     std::size(kWebFeedAwareness_new_animation_no_limit), nullptr},
    {"IPH and dot", kWebFeedAwareness_IPH, std::size(kWebFeedAwareness_IPH),
     nullptr},
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

const FeatureEntry::FeatureParam kPhotoPickerAdoptionStudyActionGetContent[] = {
    {"use_action_get_content", "true"}};
const FeatureEntry::FeatureParam kPhotoPickerAdoptionStudyActionPickImages[] = {
    {"use_action_pick_images", "true"}};
const FeatureEntry::FeatureParam
    kPhotoPickerAdoptionStudyActionPickImagesPlus[] = {
        {"use_action_pick_images_plus", "true"}};
const FeatureEntry::FeatureParam
    kPhotoPickerAdoptionStudyChromePickerWithoutBrowse[] = {
        {"chrome_picker_suppress_browse", "true"}};

const FeatureEntry::FeatureVariation
    kPhotoPickerAdoptionStudyFeatureVariations[] = {
        {"(Android Picker w/ACTION_GET_CONTENT)",
         kPhotoPickerAdoptionStudyActionGetContent,
         std::size(kPhotoPickerAdoptionStudyActionGetContent), nullptr},
        {"(Android Picker w/ACTION_PICK_IMAGES)",
         kPhotoPickerAdoptionStudyActionPickImages,
         std::size(kPhotoPickerAdoptionStudyActionPickImages), nullptr},
        {"(Android Picker w/ACTION_PICK_IMAGES Plus)",
         kPhotoPickerAdoptionStudyActionPickImagesPlus,
         std::size(kPhotoPickerAdoptionStudyActionPickImagesPlus), nullptr},
        {"(Chrome Picker without Browse)",
         kPhotoPickerAdoptionStudyChromePickerWithoutBrowse,
         std::size(kPhotoPickerAdoptionStudyChromePickerWithoutBrowse),
         nullptr}};

const FeatureEntry::FeatureParam kAuxiliarySearchDonation_MaxDonation_20[] = {
    {chrome::android::kAuxiliarySearchMaxBookmarksCountParam.name, "20"},
    {chrome::android::kAuxiliarySearchMaxTabsCountParam.name, "20"}};
const FeatureEntry::FeatureParam kAuxiliarySearchDonation_MaxDonation_100[] = {
    {chrome::android::kAuxiliarySearchMaxBookmarksCountParam.name, "100"},
    {chrome::android::kAuxiliarySearchMaxTabsCountParam.name, "100"}};
const FeatureEntry::FeatureParam kAuxiliarySearchDonation_MaxDonation_200[] = {
    {chrome::android::kAuxiliarySearchMaxBookmarksCountParam.name, "200"},
    {chrome::android::kAuxiliarySearchMaxTabsCountParam.name, "200"}};
const FeatureEntry::FeatureParam kAuxiliarySearchDonation_MaxDonation_500[] = {
    {chrome::android::kAuxiliarySearchMaxBookmarksCountParam.name, "500"},
    {chrome::android::kAuxiliarySearchMaxTabsCountParam.name, "500"}};
const FeatureEntry::FeatureVariation kAuxiliarySearchDonationVariations[] = {
    {"50 counts", kAuxiliarySearchDonation_MaxDonation_20,
     std::size(kAuxiliarySearchDonation_MaxDonation_20), nullptr},
    {"100 counts", kAuxiliarySearchDonation_MaxDonation_100,
     std::size(kAuxiliarySearchDonation_MaxDonation_100), nullptr},
    {"200 counts", kAuxiliarySearchDonation_MaxDonation_200,
     std::size(kAuxiliarySearchDonation_MaxDonation_200), nullptr},
    {"500 counts", kAuxiliarySearchDonation_MaxDonation_500,
     std::size(kAuxiliarySearchDonation_MaxDonation_500), nullptr},
};

const FeatureEntry::FeatureParam kBoardingPassDetectorUrl_AA[] = {
    {features::kBoardingPassDetectorUrlParamName,
     "https://www.aa.com/checkin/viewMobileBoardingPass"}};
const FeatureEntry::FeatureParam kBoardingPassDetectorUrl_All[] = {
    {features::kBoardingPassDetectorUrlParamName,
     "https://www.aa.com/checkin/viewMobileBoardingPass,https://united.com"}};
const FeatureEntry::FeatureParam kBoardingPassDetectorUrl_Test[] = {
    {features::kBoardingPassDetectorUrlParamName, "http"}};
const FeatureEntry::FeatureVariation kBoardingPassDetectorVariations[] = {
    {"AA", kBoardingPassDetectorUrl_AA, std::size(kBoardingPassDetectorUrl_AA),
     nullptr},
    {"All", kBoardingPassDetectorUrl_All,
     std::size(kBoardingPassDetectorUrl_All), nullptr},
    {"Test", kBoardingPassDetectorUrl_Test,
     std::size(kBoardingPassDetectorUrl_Test), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/991082,1015377): Remove after proper support for back/forward
// cache is implemented.
const FeatureEntry::FeatureParam kBackForwardCache_ForceCaching[] = {
    {"TimeToLiveInBackForwardCacheInSeconds", "300"},
    {"should_ignore_blocklists", "true"}};

const FeatureEntry::FeatureVariation kBackForwardCacheVariations[] = {
    {"force caching all pages (experimental)", kBackForwardCache_ForceCaching,
     std::size(kBackForwardCache_ForceCaching), nullptr},
};

const FeatureEntry::FeatureParam kRenderDocument_Subframe[] = {
    {"level", "subframe"}};
const FeatureEntry::FeatureParam kRenderDocument_AllFrames[] = {
    {"level", "all-frames"}};

const FeatureEntry::FeatureVariation kRenderDocumentVariations[] = {
    {"Swap RenderFrameHosts on same-site navigations from subframes and "
     "crashed frames (experimental)",
     kRenderDocument_Subframe, std::size(kRenderDocument_Subframe), nullptr},
    {"Swap RenderFrameHosts on same-site navigations from any frame "
     "(experimental)",
     kRenderDocument_AllFrames, std::size(kRenderDocument_AllFrames), nullptr},
};

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
#endif  // BUILDFLAG(IS_ANDROID)

// The choices for --enable-download-warning-improvements. This really should
// just be a SINGLE_VALUE_TYPE, but it is misleading to have the choices be
// labeled "Disabled"/"Enabled". So instead this is made to be a
// MULTI_VALUE_TYPE with choices "Default"/"Enabled".
const FeatureEntry::Choice kDownloadWarningImprovementsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled,
     switches::kEnableDownloadWarningImprovements, ""},
};

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
const FeatureEntry::Choice kForceControlFaceAeChoices[] = {
    {"Default", "", ""},
    {"Enable", media::switches::kForceControlFaceAe, "enable"},
    {"Disable", media::switches::kForceControlFaceAe, "disable"}};

const FeatureEntry::Choice kAutoFramingOverrideChoices[] = {
    {"Default", "", ""},
    {"Force enabled", media::switches::kAutoFramingOverride,
     media::switches::kAutoFramingForceEnabled},
    {"Force disabled", media::switches::kAutoFramingOverride,
     media::switches::kAutoFramingForceDisabled}};

const FeatureEntry::Choice kCameraSuperResOverrideChoices[] = {
    {"Default", "", ""},
    {"Enabled", media::switches::kCameraSuperResOverride,
     media::switches::kCameraSuperResForceEnabled},
    {"Disabled", media::switches::kCameraSuperResOverride,
     media::switches::kCameraSuperResForceDisabled}};

const FeatureEntry::Choice kFaceRetouchOverrideChoices[] = {
    {"Default", "", ""},
    {"Enabled with relighting", media::switches::kFaceRetouchOverride,
     media::switches::kFaceRetouchForceEnabledWithRelighting},
    {"Enabled without relighting", media::switches::kFaceRetouchOverride,
     media::switches::kFaceRetouchForceEnabledWithoutRelighting},
    {"Disabled", media::switches::kFaceRetouchOverride,
     media::switches::kFaceRetouchForceDisabled}};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kCrostiniContainerChoices[] = {
    {"Default", "", ""},
    {"Buster", crostini::kCrostiniContainerFlag, "buster"},
    {"Bullseye", crostini::kCrostiniContainerFlag, "bullseye"},
    {"Bookworm", crostini::kCrostiniContainerFlag, "bookworm"},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_10[] = {
    {"confidence_threshold", "10"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_20[] = {
    {"confidence_threshold", "20"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_30[] = {
    {"confidence_threshold", "30"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_40[] = {
    {"confidence_threshold", "40"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_50[] = {
    {"confidence_threshold", "50"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_60[] = {
    {"confidence_threshold", "60"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_70[] = {
    {"confidence_threshold", "70"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_80[] = {
    {"confidence_threshold", "80"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_90[] = {
    {"confidence_threshold", "90"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_100[] = {
    {"confidence_threshold", "100"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_110[] = {
    {"confidence_threshold", "110"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchConfidence_120[] = {
    {"confidence_threshold", "120"}};

const FeatureEntry::FeatureVariation
    kLauncherLocalImageSearchConfidenceVariations[] = {
        {"threshold 10", kLauncherLocalImageSearchConfidence_10,
         std::size(kLauncherLocalImageSearchConfidence_10), nullptr},
        {"threshold 20", kLauncherLocalImageSearchConfidence_20,
         std::size(kLauncherLocalImageSearchConfidence_20), nullptr},
        {"threshold 30", kLauncherLocalImageSearchConfidence_30,
         std::size(kLauncherLocalImageSearchConfidence_30), nullptr},
        {"threshold 40", kLauncherLocalImageSearchConfidence_40,
         std::size(kLauncherLocalImageSearchConfidence_40), nullptr},
        {"threshold 50", kLauncherLocalImageSearchConfidence_50,
         std::size(kLauncherLocalImageSearchConfidence_50), nullptr},
        {"threshold 60", kLauncherLocalImageSearchConfidence_60,
         std::size(kLauncherLocalImageSearchConfidence_60), nullptr},
        {"threshold 70", kLauncherLocalImageSearchConfidence_70,
         std::size(kLauncherLocalImageSearchConfidence_70), nullptr},
        {"threshold 80", kLauncherLocalImageSearchConfidence_80,
         std::size(kLauncherLocalImageSearchConfidence_80), nullptr},
        {"threshold 90", kLauncherLocalImageSearchConfidence_90,
         std::size(kLauncherLocalImageSearchConfidence_90), nullptr},
        {"threshold 100", kLauncherLocalImageSearchConfidence_100,
         std::size(kLauncherLocalImageSearchConfidence_100), nullptr},
        {"threshold 110", kLauncherLocalImageSearchConfidence_110,
         std::size(kLauncherLocalImageSearchConfidence_110), nullptr},
        {"threshold 120", kLauncherLocalImageSearchConfidence_120,
         std::size(kLauncherLocalImageSearchConfidence_120), nullptr}};

const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_10[] = {
    {"relevance_threshold", "0.1"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_20[] = {
    {"relevance_threshold", "0.2"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_30[] = {
    {"relevance_threshold", "0.3"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_40[] = {
    {"relevance_threshold", "0.4"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_50[] = {
    {"relevance_threshold", "0.5"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_60[] = {
    {"relevance_threshold", "0.6"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_70[] = {
    {"relevance_threshold", "0.7"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_80[] = {
    {"relevance_threshold", "0.8"}};
const FeatureEntry::FeatureParam kLauncherLocalImageSearchRelevance_90[] = {
    {"relevance_threshold", "0.9"}};

const FeatureEntry::FeatureVariation
    kLauncherLocalImageSearchRelevanceVariations[] = {
        {"threshold 0.1", kLauncherLocalImageSearchRelevance_10,
         std::size(kLauncherLocalImageSearchRelevance_10), nullptr},
        {"threshold 0.2", kLauncherLocalImageSearchRelevance_20,
         std::size(kLauncherLocalImageSearchRelevance_20), nullptr},
        {"threshold 0.3", kLauncherLocalImageSearchRelevance_30,
         std::size(kLauncherLocalImageSearchRelevance_30), nullptr},
        {"threshold 0.4", kLauncherLocalImageSearchRelevance_40,
         std::size(kLauncherLocalImageSearchRelevance_40), nullptr},
        {"threshold 0.5", kLauncherLocalImageSearchRelevance_50,
         std::size(kLauncherLocalImageSearchRelevance_50), nullptr},
        {"threshold 0.6", kLauncherLocalImageSearchRelevance_60,
         std::size(kLauncherLocalImageSearchRelevance_60), nullptr},
        {"threshold 0.7", kLauncherLocalImageSearchRelevance_70,
         std::size(kLauncherLocalImageSearchRelevance_70), nullptr},
        {"threshold 0.8", kLauncherLocalImageSearchRelevance_80,
         std::size(kLauncherLocalImageSearchRelevance_80), nullptr},
        {"threshold 0.9", kLauncherLocalImageSearchRelevance_90,
         std::size(kLauncherLocalImageSearchRelevance_90), nullptr}};

const FeatureEntry::FeatureParam kEolIncentiveOffer[] = {
    {"incentive_type", "offer"}};
const FeatureEntry::FeatureParam kEolIncentiveNoOffer[] = {
    {"incentive_type", "no_offer"}};

const FeatureEntry::FeatureVariation kEolIncentiveVariations[] = {
    {"with offer", kEolIncentiveOffer, std::size(kEolIncentiveOffer), nullptr},
    {"with no offer", kEolIncentiveNoOffer, std::size(kEolIncentiveNoOffer),
     nullptr}};

const FeatureEntry::FeatureParam kCampbell9dot[] = {{"icon", "9dot"}};
const FeatureEntry::FeatureParam kCampbellHero[] = {{"icon", "hero"}};
const FeatureEntry::FeatureParam kCampbellAction[] = {{"icon", "action"}};
const FeatureEntry::FeatureParam kCampbellText[] = {{"icon", "text"}};

const FeatureEntry::FeatureVariation kCampbellGlyphVariations[] = {
    {"9dot", kCampbell9dot, std::size(kCampbell9dot), nullptr},
    {"hero", kCampbellHero, std::size(kCampbellHero), nullptr},
    {"action", kCampbellAction, std::size(kCampbellAction), nullptr},
    {"text", kCampbellText, std::size(kCampbellText), nullptr}};

const FeatureEntry::FeatureParam kCaptureModeEducationShortcutNudge[] = {
    {"CaptureModeEducationParam", "ShortcutNudge"}};
const FeatureEntry::FeatureParam kCaptureModeEducationShortcutTutorial[] = {
    {"CaptureModeEducationParam", "ShortcutTutorial"}};
const FeatureEntry::FeatureParam kCaptureModeEducationQuickSettingsNudge[] = {
    {"CaptureModeEducationParam", "QuickSettingsNudge"}};

const FeatureEntry::FeatureVariation kCaptureModeEducationVariations[] = {
    {"Shortcut Nudge", kCaptureModeEducationShortcutNudge,
     std::size(kCaptureModeEducationShortcutNudge), nullptr},
    {"Shortcut Tutorial", kCaptureModeEducationShortcutTutorial,
     std::size(kCaptureModeEducationShortcutTutorial), nullptr},
    {"Quick Settings Nudge", kCaptureModeEducationQuickSettingsNudge,
     std::size(kCaptureModeEducationQuickSettingsNudge), nullptr}};

const FeatureEntry::FeatureParam
    kSystemShortcutBehaviorIgnoreCommonVdiShortcuts[] = {
        {"behavior_type", "ignore_common_vdi_shortcuts"}};
const FeatureEntry::FeatureParam
    kSystemShortcutBehaviorIgnoreCommonVdiShortcutsFullscreenOnly[] = {
        {"behavior_type", "ignore_common_vdi_shortcut_fullscreen_only"}};
const FeatureEntry::FeatureParam
    kSystemShortcutBehaviorAllowSearchBasedPassthrough[] = {
        {"behavior_type", "allow_search_based_passthrough"}};
const FeatureEntry::FeatureParam
    kSystemShortcutBehaviorAllowSearchBasedPassthroughFullscreenOnly[] = {
        {"behavior_type", "allow_search_based_passthrough_fullscreen_only"}};

const FeatureEntry::FeatureVariation kSystemShortcutBehaviorVariations[] = {
    {"Ignore Common VDI Shortcuts",
     kSystemShortcutBehaviorIgnoreCommonVdiShortcuts,
     std::size(kSystemShortcutBehaviorIgnoreCommonVdiShortcuts), nullptr},
    {"Ignore Common VDI Shortcuts while Fullscreen",
     kSystemShortcutBehaviorIgnoreCommonVdiShortcutsFullscreenOnly,
     std::size(kSystemShortcutBehaviorIgnoreCommonVdiShortcutsFullscreenOnly),
     nullptr},
    {"Allow Search Based Passthrough",
     kSystemShortcutBehaviorAllowSearchBasedPassthrough,
     std::size(kSystemShortcutBehaviorAllowSearchBasedPassthrough), nullptr},
    {"Allow Search Based Passthrough while Fullscreen",
     kSystemShortcutBehaviorAllowSearchBasedPassthroughFullscreenOnly,
     std::size(
         kSystemShortcutBehaviorAllowSearchBasedPassthroughFullscreenOnly),
     nullptr},
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kTaskManagerEndProcessDisabledForExtensionInternalName[] =
    "enable-task-manager-end-process-disabled-for-extension";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kWallpaperFastRefreshInternalName[] = "wallpaper-fast-refresh";
constexpr char kWallpaperGooglePhotosSharedAlbumsInternalName[] =
    "wallpaper-google-photos-shared-albums";
constexpr char kGlanceablesTimeManagementClassroomStudentViewInternalName[] =
    "glanceables-time-management-classroom-student-view";
constexpr char kGlanceablesTimeManagementTasksViewInternalName[] =
    "glanceables-time-management-tasks-view";
constexpr char kBackgroundListeningName[] = "background-listening";
constexpr char kBorealisBigGlInternalName[] = "borealis-big-gl";
constexpr char kBorealisDGPUInternalName[] = "borealis-dgpu";
constexpr char kBorealisEnableUnsupportedHardwareInternalName[] =
    "borealis-enable-unsupported-hardware";
constexpr char kBorealisForceBetaClientInternalName[] =
    "borealis-force-beta-client";
constexpr char kBorealisForceDoubleScaleInternalName[] =
    "borealis-force-double-scale";
constexpr char kBorealisLinuxModeInternalName[] = "borealis-linux-mode";
// This differs slightly from its symbol's name since "enabled" is used
// internally to refer to whether borealis is installed or not.
constexpr char kBorealisPermittedInternalName[] = "borealis-enabled";
constexpr char kBorealisProvisionInternalName[] = "borealis-provision";
constexpr char kBorealisScaleClientByDPIInternalName[] =
    "borealis-scale-client-by-dpi";
constexpr char kBorealisZinkGlDriverInternalName[] = "borealis-zink-gl-driver";
constexpr char kClipboardHistoryLongpressInternalName[] =
    "clipboard-history-longpress";
constexpr char kClipboardHistoryRefreshInternalName[] =
    "clipboard-history-refresh";
constexpr char kClipboardHistoryUrlTitlesInternalName[] =
    "clipboard-history-url-titles";
constexpr char kBluetoothUseFlossInternalName[] = "bluetooth-use-floss";
constexpr char kBluetoothUseLLPrivacyInternalName[] = "bluetooth-use-llprivacy";
constexpr char kSeaPenInternalName[] = "sea-pen";
constexpr char kAssistantIphInternalName[] = "assistant-iph";
constexpr char kGrowthCampaigns[] = "growth-campaigns";
constexpr char kGrowthCampaignsTestTag[] = "campaigns-test-tag";
constexpr char kVcTrayMicIndicatorInternalName[] = "vc-tray-mic-indicator";
constexpr char kVcTrayTitleHeaderInternalName[] = "vc-tray-title-header";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kLensOverlayNoOmniboxEntryPoint[] = {
    {"omnibox-entry-point", "false"},
};
const FeatureEntry::FeatureParam kLensOverlayResponsiveOmniboxEntryPoint[] = {
    {"omnibox-entry-point", "true"},
    {"omnibox-entry-point-always-visible", "false"},
};
const FeatureEntry::FeatureParam kLensOverlayPersistentOmniboxEntryPoint[] = {
    {"omnibox-entry-point", "true"},
    {"omnibox-entry-point-always-visible", "true"},
};

const FeatureEntry::FeatureVariation kLensOverlayVariations[] = {
    {"with no omnibox entry point", kLensOverlayNoOmniboxEntryPoint,
     std::size(kLensOverlayNoOmniboxEntryPoint), nullptr},
    {"with responsive chip omnibox entry point",
     kLensOverlayResponsiveOmniboxEntryPoint,
     std::size(kLensOverlayResponsiveOmniboxEntryPoint), nullptr},
    {"with persistent icon omnibox entry point",
     kLensOverlayPersistentOmniboxEntryPoint,
     std::size(kLensOverlayPersistentOmniboxEntryPoint), nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kContextualSearchboxWithPageContent[] = {
    {"use-pdfs-as-context", "true"},
};

const FeatureEntry::FeatureVariation kContextualSearchboxVariations[] = {
    {"with page content", kContextualSearchboxWithPageContent,
     std::size(kContextualSearchboxWithPageContent), nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::Choice kAlwaysEnableHdcpChoices[] = {
    {flag_descriptions::kAlwaysEnableHdcpDefault, "", ""},
    {flag_descriptions::kAlwaysEnableHdcpType0,
     ash::switches::kAlwaysEnableHdcp, "type0"},
    {flag_descriptions::kAlwaysEnableHdcpType1,
     ash::switches::kAlwaysEnableHdcp, "type1"},
};

const FeatureEntry::Choice kPrintingPpdChannelChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {ash::switches::kPrintingPpdChannelProduction,
     ash::switches::kPrintingPpdChannel,
     ash::switches::kPrintingPpdChannelProduction},
    {ash::switches::kPrintingPpdChannelStaging,
     ash::switches::kPrintingPpdChannel,
     ash::switches::kPrintingPpdChannelStaging},
    {ash::switches::kPrintingPpdChannelDev, ash::switches::kPrintingPpdChannel,
     ash::switches::kPrintingPpdChannelDev},
    {ash::switches::kPrintingPpdChannelLocalhost,
     ash::switches::kPrintingPpdChannel,
     ash::switches::kPrintingPpdChannelLocalhost}};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Feature variations for kIsolateSandboxedIframes.
#if !BUILDFLAG(IS_ANDROID)
// TODO(wjmaclean): Add FeatureParams for a per-frame grouping when support
// for it is added.
const FeatureEntry::FeatureParam kIsolateSandboxedIframesGroupingPerSite{
    "grouping", "per-site"};
const FeatureEntry::FeatureParam kIsolateSandboxedIframesGroupingPerOrigin{
    "grouping", "per-origin"};
const FeatureEntry::FeatureParam kIsolateSandboxedIframesGroupingPerDocument{
    "grouping", "per-document"};
const FeatureEntry::FeatureVariation
    kIsolateSandboxedIframesGroupingVariations[] = {
        {"with grouping by URL's site",
         &kIsolateSandboxedIframesGroupingPerSite, 1, nullptr},
        {"with grouping by URL's origin",
         &kIsolateSandboxedIframesGroupingPerOrigin, 1, nullptr},
        {"with each sandboxed frame document in its own process",
         &kIsolateSandboxedIframesGroupingPerDocument, 1, nullptr},
};
#endif  // !BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam kWebRtcApmDownmixMethodAverage[] = {
    {"method", "average"}};
const FeatureEntry::FeatureParam kWebRtcApmDownmixMethodFirstChannel[] = {
    {"method", "first"}};
const FeatureEntry::FeatureVariation kWebRtcApmDownmixMethodVariations[] = {
    {"- Average all the input channels", kWebRtcApmDownmixMethodAverage,
     std::size(kWebRtcApmDownmixMethodAverage), nullptr},
    {"- Use first channel", kWebRtcApmDownmixMethodFirstChannel,
     std::size(kWebRtcApmDownmixMethodFirstChannel), nullptr}};

const FeatureEntry::FeatureParam
    kSafetyCheckUnusedSitePermissionsNoDelayParam[] = {
        {"unused-site-permissions-no-delay-for-testing", "true"}};

const FeatureEntry::FeatureParam
    kSafetyCheckUnusedSitePermissionsWithDelayParam[] = {
        {"unused-site-permissions-with-delay-for-testing", "true"}};

const FeatureEntry::FeatureVariation
    kSafetyCheckUnusedSitePermissionsVariations[] = {
        {"for testing no delay", kSafetyCheckUnusedSitePermissionsNoDelayParam,
         std::size(kSafetyCheckUnusedSitePermissionsNoDelayParam), nullptr},
        {"for testing with delay",
         kSafetyCheckUnusedSitePermissionsWithDelayParam,
         std::size(kSafetyCheckUnusedSitePermissionsWithDelayParam), nullptr},
};

const FeatureEntry::FeatureParam kSafetyHub_NoDelay[] = {
    {features::kPasswordCheckNotificationIntervalName, "0d"},
    {features::kRevokedPermissionsNotificationIntervalName, "0d"},
    {features::kNotificationPermissionsNotificationIntervalName, "0d"},
    {features::kSafeBrowsingNotificationIntervalName, "0d"}};
const FeatureEntry::FeatureParam kSafetyHub_WithDelay[] = {
    {features::kPasswordCheckNotificationIntervalName, "0d"},
    {features::kRevokedPermissionsNotificationIntervalName, "5m"},
    {features::kNotificationPermissionsNotificationIntervalName, "5m"},
    {features::kSafeBrowsingNotificationIntervalName, "5m"}};
const FeatureEntry::FeatureVariation kSafetyHubVariations[] = {
    {"for testing no delay", kSafetyHub_NoDelay, std::size(kSafetyHub_NoDelay),
     nullptr},
    {"for testing with delay", kSafetyHub_WithDelay,
     std::size(kSafetyHub_WithDelay), nullptr},
};

const FeatureEntry::FeatureParam kTPCPhaseOutFacilitatedTestingControl1[] = {
    {tpcd::experiment::kForceEligibleForTestingName, "false"},
    {tpcd::experiment::kDisable3PCookiesName, "false"},
    {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
    {features::kCookieDeprecationLabelName, "fake_control_1.1"},
    {tpcd::experiment::kVersionName, "9990"}};
const FeatureEntry::FeatureParam kTPCPhaseOutFacilitatedTestingLabelOnly[] = {
    {tpcd::experiment::kForceEligibleForTestingName, "false"},
    {tpcd::experiment::kDisable3PCookiesName, "false"},
    {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
    {features::kCookieDeprecationLabelName, "fake_label_only_1.1"},
    {tpcd::experiment::kVersionName, "9991"}};
const FeatureEntry::FeatureParam kTPCPhaseOutFacilitatedTestingTreatment[] = {
    {tpcd::experiment::kForceEligibleForTestingName, "false"},
    {tpcd::experiment::kDisable3PCookiesName, "true"},
    {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
    {features::kCookieDeprecationLabelName, "fake_treatment_1.1"},
    {tpcd::experiment::kVersionName, "9992"}};
const FeatureEntry::FeatureParam kTPCPhaseOutFacilitatedTestingControl2[] = {
    {tpcd::experiment::kForceEligibleForTestingName, "false"},
    {tpcd::experiment::kDisable3PCookiesName, "true"},
    {features::kCookieDeprecationTestingDisableAdsAPIsName, "true"},
    {features::kCookieDeprecationLabelName, "fake_control_2"},
    {tpcd::experiment::kVersionName, "9993"}};
const FeatureEntry::FeatureParam kTPCPhaseOutFacilitatedTestingForceControl1[] =
    {{tpcd::experiment::kForceEligibleForTestingName, "true"},
     {tpcd::experiment::kDisable3PCookiesName, "false"},
     {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
     {features::kCookieDeprecationLabelName, "fake_control_1.1"},
     {tpcd::experiment::kVersionName, "9994"}};
const FeatureEntry::FeatureParam
    kTPCPhaseOutFacilitatedTestingForceLabelOnly[] = {
        {tpcd::experiment::kForceEligibleForTestingName, "true"},
        {tpcd::experiment::kDisable3PCookiesName, "false"},
        {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
        {features::kCookieDeprecationLabelName, "fake_label_only_1.1"},
        {tpcd::experiment::kVersionName, "9995"}};
const FeatureEntry::FeatureParam
    kTPCPhaseOutFacilitatedTestingForceTreatment[] = {
        {tpcd::experiment::kForceEligibleForTestingName, "true"},
        {tpcd::experiment::kDisable3PCookiesName, "true"},
        {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
        {features::kCookieDeprecationLabelName, "fake_treatment_1.1"},
        {tpcd::experiment::kVersionName, "9996"}};
const FeatureEntry::FeatureParam kTPCPhaseOutFacilitatedTestingForceControl2[] =
    {{tpcd::experiment::kForceEligibleForTestingName, "true"},
     {tpcd::experiment::kDisable3PCookiesName, "true"},
     {features::kCookieDeprecationTestingDisableAdsAPIsName, "true"},
     {features::kCookieDeprecationLabelName, "fake_control_2"},
     {tpcd::experiment::kVersionName, "9997"}};

const FeatureEntry::FeatureVariation
    kTPCPhaseOutFacilitatedTestingVariations[] = {
        {"Control 1", kTPCPhaseOutFacilitatedTestingControl1,
         std::size(kTPCPhaseOutFacilitatedTestingControl1), nullptr},
        {"LabelOnly", kTPCPhaseOutFacilitatedTestingLabelOnly,
         std::size(kTPCPhaseOutFacilitatedTestingLabelOnly), nullptr},
        {"Treatment", kTPCPhaseOutFacilitatedTestingTreatment,
         std::size(kTPCPhaseOutFacilitatedTestingTreatment), nullptr},
        {"Control 2", kTPCPhaseOutFacilitatedTestingControl2,
         std::size(kTPCPhaseOutFacilitatedTestingControl2), nullptr},
        {"Force Control 1", kTPCPhaseOutFacilitatedTestingForceControl1,
         std::size(kTPCPhaseOutFacilitatedTestingForceControl1), nullptr},
        {"Force LabelOnly", kTPCPhaseOutFacilitatedTestingForceLabelOnly,
         std::size(kTPCPhaseOutFacilitatedTestingForceLabelOnly), nullptr},
        {"Force Treatment", kTPCPhaseOutFacilitatedTestingForceTreatment,
         std::size(kTPCPhaseOutFacilitatedTestingForceTreatment), nullptr},
        {"Force Control 2", kTPCPhaseOutFacilitatedTestingForceControl2,
         std::size(kTPCPhaseOutFacilitatedTestingForceControl2), nullptr},
};

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kTabStateFlatBufferMigrateStaleTabs[] = {
    {"migrate_stale_tabs", "true"}};

const FeatureEntry::FeatureVariation kTabStateFlatBufferVariations[] = {
    {"Migrate Stale Tabs", kTabStateFlatBufferMigrateStaleTabs,
     std::size(kTabStateFlatBufferMigrateStaleTabs), nullptr}};
#endif

const FeatureEntry::FeatureParam
    kTpcdHeuristicsGrants_CurrentInteraction_ShortRedirect_MainFrameInitiator
        [] = {
            {content_settings::features::kTpcdReadHeuristicsGrantsName, "true"},
            {tpcd::experiment::
                 kTpcdWritePopupCurrentInteractionHeuristicsGrantsName,
             "30d"},
            {tpcd::experiment::kTpcdBackfillPopupHeuristicsGrantsName, "30d"},
            {tpcd::experiment::kTpcdPopupHeuristicEnableForIframeInitiatorName,
             "none"},
            {tpcd::experiment::kTpcdWriteRedirectHeuristicGrantsName, "15m"},
            {tpcd::experiment::kTpcdRedirectHeuristicRequireABAFlowName,
             "true"},
            {tpcd::experiment::
                 kTpcdRedirectHeuristicRequireCurrentInteractionName,
             "true"}};
const FeatureEntry::FeatureParam
    kTpcdHeuristicsGrants_CurrentInteraction_LongRedirect_MainFrameInitiator[] =
        {{content_settings::features::kTpcdReadHeuristicsGrantsName, "true"},
         {tpcd::experiment::
              kTpcdWritePopupCurrentInteractionHeuristicsGrantsName,
          "30d"},
         {tpcd::experiment::kTpcdBackfillPopupHeuristicsGrantsName, "30d"},
         {tpcd::experiment::kTpcdPopupHeuristicEnableForIframeInitiatorName,
          "none"},
         {tpcd::experiment::kTpcdWriteRedirectHeuristicGrantsName, "30d"},
         {tpcd::experiment::kTpcdRedirectHeuristicRequireABAFlowName, "true"},
         {tpcd::experiment::kTpcdRedirectHeuristicRequireCurrentInteractionName,
          "true"}};
const FeatureEntry::FeatureParam
    kTpcdHeuristicsGrants_CurrentInteraction_ShortRedirect_AllFrameInitiator[] =
        {{content_settings::features::kTpcdReadHeuristicsGrantsName, "true"},
         {tpcd::experiment::
              kTpcdWritePopupCurrentInteractionHeuristicsGrantsName,
          "30d"},
         {tpcd::experiment::kTpcdBackfillPopupHeuristicsGrantsName, "30d"},
         {tpcd::experiment::kTpcdPopupHeuristicEnableForIframeInitiatorName,
          "all"},
         {tpcd::experiment::kTpcdWriteRedirectHeuristicGrantsName, "15m"},
         {tpcd::experiment::kTpcdRedirectHeuristicRequireABAFlowName, "true"},
         {tpcd::experiment::kTpcdRedirectHeuristicRequireCurrentInteractionName,
          "true"}};
const FeatureEntry::FeatureParam
    kTpcdHeuristicsGrants_CurrentInteraction_LongRedirect_AllFrameInitiator[] =
        {{content_settings::features::kTpcdReadHeuristicsGrantsName, "true"},
         {tpcd::experiment::
              kTpcdWritePopupCurrentInteractionHeuristicsGrantsName,
          "30d"},
         {tpcd::experiment::kTpcdBackfillPopupHeuristicsGrantsName, "30d"},
         {tpcd::experiment::kTpcdPopupHeuristicEnableForIframeInitiatorName,
          "all"},
         {tpcd::experiment::kTpcdWriteRedirectHeuristicGrantsName, "30d"},
         {tpcd::experiment::kTpcdRedirectHeuristicRequireABAFlowName, "true"},
         {tpcd::experiment::kTpcdRedirectHeuristicRequireCurrentInteractionName,
          "true"}};

const FeatureEntry::FeatureVariation kTpcdHeuristicsGrantsVariations[] = {
    {"CurrentInteraction_ShortRedirect_MainFrameInitiator",
     kTpcdHeuristicsGrants_CurrentInteraction_ShortRedirect_MainFrameInitiator,
     std::size(
         kTpcdHeuristicsGrants_CurrentInteraction_ShortRedirect_MainFrameInitiator),
     nullptr},
    {"CurrentInteraction_LongRedirect_MainFrameInitiator",
     kTpcdHeuristicsGrants_CurrentInteraction_LongRedirect_MainFrameInitiator,
     std::size(
         kTpcdHeuristicsGrants_CurrentInteraction_LongRedirect_MainFrameInitiator),
     nullptr},
    {"CurrentInteraction_ShortRedirect_AllFrameInitiator",
     kTpcdHeuristicsGrants_CurrentInteraction_ShortRedirect_AllFrameInitiator,
     std::size(
         kTpcdHeuristicsGrants_CurrentInteraction_ShortRedirect_AllFrameInitiator),
     nullptr},
    {"CurrentInteraction_LongRedirect_AllFrameInitiator",
     kTpcdHeuristicsGrants_CurrentInteraction_LongRedirect_AllFrameInitiator,
     std::size(
         kTpcdHeuristicsGrants_CurrentInteraction_LongRedirect_AllFrameInitiator),
     nullptr}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam kVcInferenceBackendAuto[] = {
    {"inference_backend", "AUTO"},
};

const FeatureEntry::FeatureParam kVcInferenceBackendGpu[] = {
    {"inference_backend", "GPU"},
};

const FeatureEntry::FeatureParam kVcInferenceBackendNpu[] = {
    {"inference_backend", "NPU"},
};

const FeatureEntry::FeatureVariation kVcRelightingInferenceBackendVariations[] =
    {{"AUTO", kVcInferenceBackendAuto, std::size(kVcInferenceBackendAuto),
      nullptr},
     {"GPU", kVcInferenceBackendGpu, std::size(kVcInferenceBackendGpu),
      nullptr},
     {"NPU", kVcInferenceBackendNpu, std::size(kVcInferenceBackendNpu),
      nullptr}};

const FeatureEntry::FeatureVariation
    kVcSegmentationInferenceBackendVariations[] = {
        {"AUTO", kVcInferenceBackendAuto, std::size(kVcInferenceBackendAuto),
         nullptr},
        {"GPU", kVcInferenceBackendGpu, std::size(kVcInferenceBackendGpu),
         nullptr},
        {"NPU", kVcInferenceBackendNpu, std::size(kVcInferenceBackendNpu),
         nullptr}};

const FeatureEntry::FeatureParam kVcSegmentationModelHighResolution[] = {
    {"segmentation_model", "high_resolution"},
};

const FeatureEntry::FeatureParam kVcSegmentationModelLowerResolution[] = {
    {"segmentation_model", "lower_resolution"},
};

const FeatureEntry::FeatureVariation kVcSegmentationModelVariations[] = {
    {"High resolution model", kVcSegmentationModelHighResolution,
     std::size(kVcSegmentationModelHighResolution), nullptr},
    {"Lower resolution model", kVcSegmentationModelLowerResolution,
     std::size(kVcSegmentationModelLowerResolution), nullptr},
};

const FeatureEntry::FeatureParam kVcLightIntensity10[] = {
    {"light_intensity", "1.0"},
};

const FeatureEntry::FeatureParam kVcLightIntensity13[] = {
    {"light_intensity", "1.3"},
};

const FeatureEntry::FeatureParam kVcLightIntensity15[] = {
    {"light_intensity", "1.5"},
};

const FeatureEntry::FeatureParam kVcLightIntensity17[] = {
    {"light_intensity", "1.7"},
};

const FeatureEntry::FeatureParam kVcLightIntensity18[] = {
    {"light_intensity", "1.8"},
};

const FeatureEntry::FeatureParam kVcLightIntensity20[] = {
    {"light_intensity", "2.0"},
};

const FeatureEntry::FeatureVariation kVcLightIntensityVariations[] = {
    {"1.0", kVcLightIntensity10, std::size(kVcLightIntensity10), nullptr},
    {"1.3", kVcLightIntensity13, std::size(kVcLightIntensity13), nullptr},
    {"1.5", kVcLightIntensity15, std::size(kVcLightIntensity15), nullptr},
    {"1.7", kVcLightIntensity17, std::size(kVcLightIntensity17), nullptr},
    {"1.8", kVcLightIntensity18, std::size(kVcLightIntensity18), nullptr},
    {"2.0", kVcLightIntensity20, std::size(kVcLightIntensity20), nullptr},
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const FeatureEntry::FeatureParam
    kCrOSLateBootMissiveDisableStorageDegradation[] = {
        {"controlled_degradation", "false"}};
const FeatureEntry::FeatureParam
    kCrOSLateBootMissiveEnableStorageDegradation[] = {
        {"controlled_degradation", "true"}};
const FeatureEntry::FeatureParam kCrOSLateBootMissiveDisableLegacyStorage[] = {
    {"legacy_storage_enabled",
     "UNDEFINED_PRIORITY"}};  // All others are multi-generation action state.
const FeatureEntry::FeatureParam kCrOSLateBootMissiveEnableLegacyStorage[] = {
    {"legacy_storage_enabled",
     "SECURITY,"
     "IMMEDIATE,"
     "FAST_BATCH,"
     "SLOW_BATCH,"
     "BACKGROUND_BATCH,"
     "MANUAL_BATCH,"
     "MANUAL_BATCH_LACROS,"}};
const FeatureEntry::FeatureParam kCrOSLateBootMissivePartialLegacyStorage[] = {
    {"legacy_storage_enabled",
     "SECURITY,"
     "IMMEDIATE,"}};
const FeatureEntry::FeatureParam kCrOSLateBootMissiveSecurityLegacyStorage[] = {
    {"legacy_storage_enabled", "SECURITY,"}};

const FeatureEntry::FeatureVariation
    kCrOSLateBootMissiveStorageDefaultVariations[] = {
        {"Enable storage degradation",
         kCrOSLateBootMissiveEnableStorageDegradation,
         std::size(kCrOSLateBootMissiveEnableStorageDegradation), nullptr},
        {"Disable storage degradation",
         kCrOSLateBootMissiveDisableStorageDegradation,
         std::size(kCrOSLateBootMissiveDisableStorageDegradation), nullptr},
        {"Enable all queues legacy", kCrOSLateBootMissiveEnableLegacyStorage,
         std::size(kCrOSLateBootMissiveEnableLegacyStorage), nullptr},
        {"Disable all queues legacy", kCrOSLateBootMissiveDisableLegacyStorage,
         std::size(kCrOSLateBootMissiveDisableLegacyStorage), nullptr},
        {"Enable SECURITY and IMMEDIATE queues legacy only",
         kCrOSLateBootMissivePartialLegacyStorage,
         std::size(kCrOSLateBootMissivePartialLegacyStorage), nullptr},
        {"Enable SECURITY queues legacy only",
         kCrOSLateBootMissiveSecurityLegacyStorage,
         std::size(kCrOSLateBootMissiveSecurityLegacyStorage), nullptr},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::Choice kCastMirroringTargetPlayoutDelayChoices[] = {
    {flag_descriptions::kCastMirroringTargetPlayoutDelayDefault, "", ""},
    {flag_descriptions::kCastMirroringTargetPlayoutDelay100ms,
     switches::kCastMirroringTargetPlayoutDelay, "100"},
    {flag_descriptions::kCastMirroringTargetPlayoutDelay150ms,
     switches::kCastMirroringTargetPlayoutDelay, "150"},
    {flag_descriptions::kCastMirroringTargetPlayoutDelay250ms,
     switches::kCastMirroringTargetPlayoutDelay, "250"},
    {flag_descriptions::kCastMirroringTargetPlayoutDelay300ms,
     switches::kCastMirroringTargetPlayoutDelay, "300"},
    {flag_descriptions::kCastMirroringTargetPlayoutDelay350ms,
     switches::kCastMirroringTargetPlayoutDelay, "350"},
    {flag_descriptions::kCastMirroringTargetPlayoutDelay400ms,
     switches::kCastMirroringTargetPlayoutDelay, "400"}};

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
const FeatureEntry::FeatureParam
    kEnableBoundSessionCredentialsWithMultiSessionSupport[] = {
        {"exclusive-registration-path", ""}};

const FeatureEntry::FeatureVariation
    kEnableBoundSessionCredentialsVariations[] = {
        {"with multi-session",
         kEnableBoundSessionCredentialsWithMultiSessionSupport,
         std::size(kEnableBoundSessionCredentialsWithMultiSessionSupport),
         nullptr}};
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kWebAuthnAndroidCredManGpmInCredManParam = {
    device::kWebAuthnAndroidGpmInCredMan.name, "true"};
const FeatureEntry::FeatureParam kWebAuthnAndroidCredManGpmNotInCredManParam = {
    device::kWebAuthnAndroidGpmInCredMan.name, "false"};
const FeatureEntry::FeatureVariation kWebAuthnAndroidCredManVariations[] = {
    {"for Google Password Manager and 3rd party passkeys",
     &kWebAuthnAndroidCredManGpmInCredManParam, 1, nullptr},
    {"for 3rd party passkeys", &kWebAuthnAndroidCredManGpmNotInCredManParam, 1,
     nullptr}};

const FeatureEntry::FeatureParam kHubPhase2WithIcons[] = {
    {"supports_other_tabs", "true"}};
const FeatureEntry::FeatureParam kHubPhase2WithText[] = {
    {"pane_switcher_uses_text", "true"},
    {"supports_other_tabs", "true"}};
const FeatureEntry::FeatureParam kHubPhase3[] = {
    {"pane_switcher_uses_text", "true"},
    {"supports_other_tabs", "true"},
    {"supports_search", "true"}};
const FeatureEntry::FeatureParam kHubPhase4[] = {
    {"pane_switcher_uses_text", "true"},
    {"supports_other_tabs", "true"},
    {"supports_search", "true"},
    {"supports_bookmarks", "true"}};

const FeatureEntry::FeatureVariation kAndroidHubV2Variations[] = {
    {"Phase 2 w/ Icons", kHubPhase2WithIcons, std::size(kHubPhase2WithIcons),
     nullptr},
    {"Phase 2 w/ Text", kHubPhase2WithText, std::size(kHubPhase2WithText),
     nullptr},
    {"Phase 3", kHubPhase3, std::size(kHubPhase3), nullptr},
    {"Phase 4", kHubPhase4, std::size(kHubPhase4), nullptr}};

const FeatureEntry::FeatureParam
    kAndroidHubFloatingActionButtonAlternativeColors[] = {
        {"hub_alternative_fab_color", "true"},
};

const FeatureEntry::FeatureVariation
    kAndroidHubFloatingActionButtonVariations[] = {
        {"Alternative colors", kAndroidHubFloatingActionButtonAlternativeColors,
         std::size(kAndroidHubFloatingActionButtonAlternativeColors), nullptr},
};

const FeatureEntry::FeatureParam kTabGroupCreationDialogAndroidShowSetting[] = {
    {"show_tab_group_creation_dialog_setting", "true"}};

const FeatureEntry::FeatureVariation
    kTabGroupCreationDialogAndroidVariations[] = {
        {"Show tab group creation dialog setting",
         kTabGroupCreationDialogAndroidShowSetting,
         std::size(kTabGroupCreationDialogAndroidShowSetting), nullptr}};

const FeatureEntry::FeatureParam kEdgeToEdgeBottomChinDebugFeatureParams[] = {
    {chrome::android::kEdgeToEdgeBottomChinDebugParam.name, "true"}};
const FeatureEntry::FeatureVariation kEdgeToEdgeBottomChinVariations[] = {
    {"debug", kEdgeToEdgeBottomChinDebugFeatureParams,
     std::size(kEdgeToEdgeBottomChinDebugFeatureParams), nullptr},
};

const FeatureEntry::FeatureParam kBottomBrowserControlsRefactorParams[] = {
    {"disable_bottom_controls_stacker_y_offset", "false"}};
const FeatureEntry::FeatureVariation
    kBottomBrowserControlsRefactorVariations[] = {
        {"Dispatch yOffset", kBottomBrowserControlsRefactorParams,
         std::size(kBottomBrowserControlsRefactorParams), nullptr},
};

#endif  // BUILDFLAG(IS_ANDROID)

const flags_ui::FeatureEntry::FeatureParam kParcelTrackingTestDataDelivered[] =
    {{commerce::kParcelTrackingTestDataParam,
      commerce::kParcelTrackingTestDataParamDelivered}};
const flags_ui::FeatureEntry::FeatureParam kParcelTrackingTestDataInProgress[] =
    {{commerce::kParcelTrackingTestDataParam,
      commerce::kParcelTrackingTestDataParamInProgress}};
const flags_ui::FeatureEntry::FeatureParam
    kParcelTrackingTestDataOutForDelivery[] = {
        {commerce::kParcelTrackingTestDataParam,
         commerce::kParcelTrackingTestDataParamOutForDelivery}};
const flags_ui::FeatureEntry::FeatureVariation
    kParcelTrackingTestDataVariations[] = {
        {"Delivered", kParcelTrackingTestDataDelivered,
         std::size(kParcelTrackingTestDataDelivered), nullptr},
        {"In progress", kParcelTrackingTestDataInProgress,
         std::size(kParcelTrackingTestDataInProgress), nullptr},
        {"Out for delivery", kParcelTrackingTestDataOutForDelivery,
         std::size(kParcelTrackingTestDataOutForDelivery), nullptr},
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
const flags_ui::FeatureEntry::FeatureParam kPwaNavigationCapturingDefaultOn[] =
    {{"link_capturing_state", "on_by_default"}};
const flags_ui::FeatureEntry::FeatureParam kPwaNavigationCapturingDefaultOff[] =
    {{"link_capturing_state", "off_by_default"}};
const flags_ui::FeatureEntry::FeatureParam
    kPwaNavigationCapturingReimplDefaultOn[] = {
        {"link_capturing_state", "reimpl_default_on"}};
const flags_ui::FeatureEntry::FeatureParam
    kPwaNavigationCapturingReimplDefaultOff[] = {
        {"link_capturing_state", "reimpl_default_off"}};
const flags_ui::FeatureEntry::FeatureVariation
    kPwaNavigationCapturingVariations[] = {
        {"On by default", kPwaNavigationCapturingDefaultOn,
         std::size(kPwaNavigationCapturingDefaultOn), nullptr},
        {"Off by default", kPwaNavigationCapturingDefaultOff,
         std::size(kPwaNavigationCapturingDefaultOff), nullptr},
        {"(Reimpl) On by default", kPwaNavigationCapturingReimplDefaultOn,
         std::size(kPwaNavigationCapturingReimplDefaultOn), nullptr},
        {"(Reimpl) Off by default", kPwaNavigationCapturingReimplDefaultOff,
         std::size(kPwaNavigationCapturingReimplDefaultOff), nullptr}};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::Choice kAccountBookmarksAndReadingListBehindOptInChoices[] =
    {
        {"Default", "", ""},
        {"Enabled", switches::kEnableFeatures,
         "SyncEnableBookmarksInTransportMode,"
         "ReadingListEnableSyncTransportModeUponSignIn"},
};

const char kReplaceSyncPromosWithSignInPromosFeatures[] =
    "ReplaceSyncPromosWithSignInPromos,"
    "ReadingListEnableSyncTransportModeUponSignIn,"
    "SyncEnableContactInfoDataTypeInTransportMode,"
    "SyncEnableContactInfoDataTypeForCustomPassphraseUsers,"
    "SyncEnableWalletMetadataInTransportMode,"
    "SyncEnableWalletOfferInTransportMode,"
    "EnablePasswordsAccountStorageForNonSyncingUsers,"
    "HideSettingsSignInPromo,"
    "FeedBottomSyncStringRemoval,"
    "EnableBatchUploadFromSettings";

// The ones above + UnoPhase2FollowUp.
const char kFastFollowFeatures[] =
    "ReplaceSyncPromosWithSignInPromos,"
    "ReadingListEnableSyncTransportModeUponSignIn,"
    "SyncEnableContactInfoDataTypeInTransportMode,"
    "SyncEnableContactInfoDataTypeForCustomPassphraseUsers,"
    "SyncEnableWalletMetadataInTransportMode,"
    "SyncEnableWalletOfferInTransportMode,"
    "EnablePasswordsAccountStorageForNonSyncingUsers,"
    "HideSettingsSignInPromo,"
    "FeedBottomSyncStringRemoval,"
    "EnableBatchUploadFromSettings,"
    "UnoPhase2FollowUp";

const FeatureEntry::Choice kReplaceSyncPromosWithSignInPromosChoices[] = {
    {"Default", "", ""},
    {"Disabled", "disable-features",
     kReplaceSyncPromosWithSignInPromosFeatures},
    {"Enabled", "enable-features", kReplaceSyncPromosWithSignInPromosFeatures},
    {"Enabled with follow-ups", "enable-features", kFastFollowFeatures},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam
    kUserEducationVersion2ShortIdleSessionCooldownDuration[] = {
        {"idle_time_between_sessions", "10m"},
        {"session_start_grace_period", "1m"},
        {"low_priority_cooldown", "5m"}};

const FeatureEntry::FeatureVariation
    kUserEducationExperienceVersion2Variants[] = {
        {"with 10 minutes Idle Session and 5 minutes Cooldown Period",
         kUserEducationVersion2ShortIdleSessionCooldownDuration,
         std::size(kUserEducationVersion2ShortIdleSessionCooldownDuration),
         nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kLinkPreviewTriggerTypeAltClick[] = {
    {"trigger_type", "alt_click"}};
const FeatureEntry::FeatureParam kLinkPreviewTriggerTypeAltHover[] = {
    {"trigger_type", "alt_hover"}};
const FeatureEntry::FeatureParam kLinkPreviewTriggerTypeLongPress[] = {
    {"trigger_type", "long_press"}};

const FeatureEntry::FeatureVariation kLinkPreviewTriggerTypeVariations[] = {
    {"Alt + Click", kLinkPreviewTriggerTypeAltClick,
     std::size(kLinkPreviewTriggerTypeAltClick), nullptr},
    {"Alt + Hover", kLinkPreviewTriggerTypeAltHover,
     std::size(kLinkPreviewTriggerTypeAltHover), nullptr},
    {"Long Press", kLinkPreviewTriggerTypeLongPress,
     std::size(kLinkPreviewTriggerTypeLongPress), nullptr}};
#endif  // !BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam
    kAutofillGranularFillingAvailableVariationWithoutImprovedLabels[] = {
        {"autofill_granular_filling_with_improved_labels", "false"},
        {"autofill_granular_filling_with_fill_everything_in_the_footer",
         "true"},
        {"autofill_granular_filling_with_expand_control_visible_on_selection_"
         "only",
         "false"}};

const FeatureEntry::FeatureParam
    kAutofillGranularFillingAvailableVariationWithFillEverythingAtTheTop[] = {
        {"autofill_granular_filling_with_improved_labels", "true"},
        {"autofill_granular_filling_with_fill_everything_in_the_footer",
         "false"},
        {"autofill_granular_filling_with_expand_control_visible_on_selection_"
         "only",
         "false"}};

const FeatureEntry::FeatureParam
    kAutofillGranularFillingAvailableVariationWithExpandControlVisibleOnSelectionOnly
        [] = {{"autofill_granular_filling_with_improved_labels", "true"},
              {"autofill_granular_filling_with_fill_everything_in_the_footer",
               "true"},
              {"autofill_granular_filling_with_expand_control_visible_on_"
               "selection_only",
               "true"}};

const FeatureEntry::FeatureVariation kAutofillGranularFillingAvailableVariations[] =
    {{"Without improved labels",
      kAutofillGranularFillingAvailableVariationWithoutImprovedLabels,
      std::size(
          kAutofillGranularFillingAvailableVariationWithoutImprovedLabels),
      nullptr},
     {"With \"Fill everything\" at the top",
      kAutofillGranularFillingAvailableVariationWithFillEverythingAtTheTop,
      std::size(
          kAutofillGranularFillingAvailableVariationWithFillEverythingAtTheTop),
      nullptr},
     {"With sub-popup expand control visible for selected/expanded rows only",
      kAutofillGranularFillingAvailableVariationWithExpandControlVisibleOnSelectionOnly,
      std::size(
          kAutofillGranularFillingAvailableVariationWithExpandControlVisibleOnSelectionOnly),
      nullptr}};

#if BUILDFLAG(IS_ANDROID)
inline constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillVirtualViewStructureAndroidSkipCompatibilityCheck = {
        autofill::features::
            kAutofillVirtualViewStructureAndroidSkipsCompatibilityCheck.name,
        "skip_all_checks"};
inline constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillVirtualViewStructureAndroidOnlySkipAwgCheck = {
        autofill::features::
            kAutofillVirtualViewStructureAndroidSkipsCompatibilityCheck.name,
        "only_skip_awg_check"};

inline constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillVirtualViewStructureVariation[] = {
        {" without any compatibility check",
         &kAutofillVirtualViewStructureAndroidSkipCompatibilityCheck, 1,
         nullptr},
        {" without AwG restriction",
         &kAutofillVirtualViewStructureAndroidOnlySkipAwgCheck, 1, nullptr}};

#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam
    kPrerender2WarmUpCompositorTriggerPointDidCommitLoad[] = {
        {"trigger_point", "did_commit_load"}};
const FeatureEntry::FeatureParam
    kPrerender2WarmUpCompositorTriggerPointDidDispatchDOMContentLoadedEvent[] =
        {{"trigger_point", "did_dispatch_dom_content_loaded_event"}};
const FeatureEntry::FeatureParam
    kPrerender2WarmUpCompositorTriggerPointDidFinishLoad[] = {
        {"trigger_point", "did_finish_load"}};
const FeatureEntry::FeatureVariation
    kPrerender2WarmUpCompositorTriggerPointVariations[] = {
        {"(on DidCommitLoad)",
         kPrerender2WarmUpCompositorTriggerPointDidCommitLoad,
         std::size(kPrerender2WarmUpCompositorTriggerPointDidCommitLoad),
         nullptr},
        {"(on DOMContentLoaded)",
         kPrerender2WarmUpCompositorTriggerPointDidDispatchDOMContentLoadedEvent,
         std::size(
             kPrerender2WarmUpCompositorTriggerPointDidDispatchDOMContentLoadedEvent),
         nullptr},
        {"(on DidFinishLoad)",
         kPrerender2WarmUpCompositorTriggerPointDidFinishLoad,
         std::size(kPrerender2WarmUpCompositorTriggerPointDidFinishLoad),
         nullptr},
};

#if BUILDFLAG(ENABLE_COMPOSE)
// Vatiations of the Compose proactive nudge.
const FeatureEntry::FeatureParam kComposeProactiveNudge_CompactUI_50[] = {
    {"proactive_nudge_focus_delay_milliseconds", "1000"},
    {"proactive_nudge_compact_ui", "true"},
    {"proactive_nudge_show_probability", "0.5"}};

const FeatureEntry::FeatureParam kComposeProactiveNudge_NoFocusDelay_10[] = {
    {"proactive_nudge_focus_delay_milliseconds", "0"},
    {"proactive_nudge_show_probability", "1"},
    {"proactive_nudge_text_change_count", "10"}};

const FeatureEntry::FeatureParam kComposeProactiveNudge_NoFocusDelay_50[] = {
    {"proactive_nudge_focus_delay_milliseconds", "0"},
    {"proactive_nudge_show_probability", "1"},
    {"proactive_nudge_text_change_count", "50"}};

const FeatureEntry::FeatureParam kComposeProactiveNudge_LongFocusDelay_10[] = {
    {"proactive_nudge_focus_delay_milliseconds", "60000"},  // one minute
    {"proactive_nudge_show_probability", "1"},
    {"proactive_nudge_text_change_count", "10"}};

const FeatureEntry::FeatureParam kComposeProactiveNudge_LongFocusDelay_50[] = {
    {"proactive_nudge_focus_delay_milliseconds", "60000"},  // one minute
    {"proactive_nudge_show_probability", "1"},
    {"proactive_nudge_text_change_count", "50"}};

const FeatureEntry::FeatureVariation kComposeProactiveNudgeVariations[] = {
    {"Show 50%", kComposeProactiveNudge_CompactUI_50,
     std::size(kComposeProactiveNudge_CompactUI_50), nullptr},
    {"No focus delay - 10 edits ", kComposeProactiveNudge_NoFocusDelay_10,
     std::size(kComposeProactiveNudge_NoFocusDelay_10), nullptr},
    {"No focus delay - 50 edits ", kComposeProactiveNudge_NoFocusDelay_50,
     std::size(kComposeProactiveNudge_NoFocusDelay_50), nullptr},
    {"Long focus delay - 10 edits ", kComposeProactiveNudge_LongFocusDelay_10,
     std::size(kComposeProactiveNudge_LongFocusDelay_10), nullptr},
    {"Long focus delay - 50 edits ", kComposeProactiveNudge_LongFocusDelay_50,
     std::size(kComposeProactiveNudge_LongFocusDelay_50), nullptr}};

// Variations of the Compose selection nudge.
const FeatureEntry::FeatureParam kComposeSelectionNudge_1[] = {
    {"selection_nudge_length", "1"}};

const FeatureEntry::FeatureParam kComposeSelectionNudge_15[] = {
    {"selection_nudge_length", "15"}};

const FeatureEntry::FeatureParam kComposeSelectionNudge_30[] = {
    {"selection_nudge_length", "30"}};

const FeatureEntry::FeatureParam kComposeSelectionNudge_30_1s[] = {
    {"selection_nudge_length", "30"},
    {"selection_nudge_delay_milliseconds", "1000"}};

const FeatureEntry::FeatureParam kComposeSelectionNudge_30_2s[] = {
    {"selection_nudge_length", "30"},
    {"selection_nudge_delay_milliseconds", "2000"}};

const FeatureEntry::FeatureParam kComposeSelectionNudge_50[] = {
    {"selection_nudge_length", "50"}};

const FeatureEntry::FeatureParam kComposeSelectionNudge_100[] = {
    {"selection_nudge_length", "100"}};

const FeatureEntry::FeatureVariation kComposeSelectionNudgeVariations[] = {
    {"1 Char", kComposeSelectionNudge_1, std::size(kComposeSelectionNudge_1),
     nullptr},
    {"15 Char", kComposeSelectionNudge_15, std::size(kComposeSelectionNudge_15),
     nullptr},
    {"30 Char", kComposeSelectionNudge_30, std::size(kComposeSelectionNudge_30),
     nullptr},
    {"50 Char", kComposeSelectionNudge_50, std::size(kComposeSelectionNudge_50),
     nullptr},
    {"100 Char", kComposeSelectionNudge_100,
     std::size(kComposeSelectionNudge_100), nullptr},
    {"30 Char - 1sec", kComposeSelectionNudge_30_1s,
     std::size(kComposeSelectionNudge_30_1s), nullptr},
    {"30 char - 2sec", kComposeSelectionNudge_30_2s,
     std::size(kComposeSelectionNudge_30_2s), nullptr}};
#endif  // ENABLE_COMPOSE

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
const FeatureEntry::FeatureParam kLocationProviderManagerModeNetworkOnly[] = {
    {"LocationProviderManagerMode", "NetworkOnly"}};
const FeatureEntry::FeatureParam kLocationProviderManagerModePlatformOnly[] = {
    {"LocationProviderManagerMode", "PlatformOnly"}};
const FeatureEntry::FeatureParam kLocationProviderManagerModeHybridPlatform[] =
    {{"LocationProviderManagerMode", "HybridPlatform"}};

const FeatureEntry::FeatureVariation kLocationProviderManagerVariations[] = {
    {"NetworkOnly", kLocationProviderManagerModeNetworkOnly,
     std::size(kLocationProviderManagerModeNetworkOnly), nullptr},
    {"PlatformOnly", kLocationProviderManagerModePlatformOnly,
     std::size(kLocationProviderManagerModePlatformOnly), nullptr},
    {"HybridPlatform", kLocationProviderManagerModeHybridPlatform,
     std::size(kLocationProviderManagerModeHybridPlatform), nullptr}};
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kWebAuthnEnclaveAuthenticatorEnabledParam = {
    device::kWebAuthnGpmPinFeatureParameterName, "true"};
const FeatureEntry::FeatureVariation kWebAuthnEnclaveAuthenticatorVariations[] =
    {{"with GPM PIN enabled", &kWebAuthnEnclaveAuthenticatorEnabledParam, 1,
      nullptr}};
#endif

const FeatureEntry::FeatureParam kAutofillUpstreamUpdatedUi_Security[] = {
    {"autofill_upstream_updated_ui_treatment", "1"}};
const FeatureEntry::FeatureParam kAutofillUpstreamUpdatedUi_Convenience[] = {
    {"autofill_upstream_updated_ui_treatment", "2"}};
const FeatureEntry::FeatureParam kAutofillUpstreamUpdatedUi_Education[] = {
    {"autofill_upstream_updated_ui_treatment", "3"}};

const FeatureEntry::FeatureVariation kAutofillUpstreamUpdatedUiOptions[] = {
    {"Security focus", kAutofillUpstreamUpdatedUi_Security,
     std::size(kAutofillUpstreamUpdatedUi_Security), nullptr},
    {"Convenience focus", kAutofillUpstreamUpdatedUi_Convenience,
     std::size(kAutofillUpstreamUpdatedUi_Convenience), nullptr},
    {"Education focus", kAutofillUpstreamUpdatedUi_Education,
     std::size(kAutofillUpstreamUpdatedUi_Education), nullptr}};

const FeatureEntry::FeatureParam
    kDeferRendererTasksAfterInputMinimalTypesPolicyParam[] = {
        {blink::features::kDeferRendererTasksAfterInputPolicyParamName,
         blink::features::kDeferRendererTasksAfterInputMinimalTypesPolicyName}};
const FeatureEntry::FeatureParam
    kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyParam[] = {
        {blink::features::kDeferRendererTasksAfterInputPolicyParamName,
         blink::features::
             kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyName}};
const FeatureEntry::FeatureParam
    kDeferRendererTasksAfterInputAllDeferrableTypesPolicyParam[] = {
        {blink::features::kDeferRendererTasksAfterInputPolicyParamName,
         blink::features::
             kDeferRendererTasksAfterInputAllDeferrableTypesPolicyName}};
const FeatureEntry::FeatureParam
    kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyParam[] = {
        {blink::features::kDeferRendererTasksAfterInputPolicyParamName,
         blink::features::
             kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyName}};
const FeatureEntry::FeatureParam
    kDeferRendererTasksAfterInputAllTypesPolicyParam[] = {
        {blink::features::kDeferRendererTasksAfterInputPolicyParamName,
         blink::features::kDeferRendererTasksAfterInputAllTypesPolicyName}};

const FeatureEntry::FeatureVariation kDeferRendererTasksAfterInputVariations[] = {
    {"with a minimal subset of tasks types",
     kDeferRendererTasksAfterInputMinimalTypesPolicyParam,
     std::size(kDeferRendererTasksAfterInputMinimalTypesPolicyParam), nullptr},
    {"with existing non-user-blocking 'deferrable' task types",
     kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyParam,
     std::size(
         kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyParam),
     nullptr},
    {"with non-user-blocking task types",
     kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyParam,
     std::size(kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyParam),
     nullptr},
    {"with all existing 'deferrable' task types",
     kDeferRendererTasksAfterInputAllDeferrableTypesPolicyParam,
     std::size(kDeferRendererTasksAfterInputAllDeferrableTypesPolicyParam),
     nullptr},
    {"with all task types", kDeferRendererTasksAfterInputAllTypesPolicyParam,
     std::size(kDeferRendererTasksAfterInputAllTypesPolicyParam), nullptr}};

const FeatureEntry::FeatureParam
    kThreadedScrollPreventRenderingStarvation_66ms[] = {{"threshold_ms", "66"}};
const FeatureEntry::FeatureParam
    kThreadedScrollPreventRenderingStarvation_100ms[] = {
        {"threshold_ms", "100"}};
const FeatureEntry::FeatureParam
    kThreadedScrollPreventRenderingStarvation_200ms[] = {
        {"threshold_ms", "200"}};
const FeatureEntry::FeatureParam
    kThreadedScrollPreventRenderingStarvation_333ms[] = {
        {"threshold_ms", "333"}};
const FeatureEntry::FeatureVariation
    kThreadedScrollPreventRenderingStarvationVariations[] = {
        {"with a 66ms threshold",
         kThreadedScrollPreventRenderingStarvation_66ms,
         std::size(kThreadedScrollPreventRenderingStarvation_66ms), nullptr},
        {"with a 100ms threshold",
         kThreadedScrollPreventRenderingStarvation_100ms,
         std::size(kThreadedScrollPreventRenderingStarvation_100ms), nullptr},
        {"with a 200ms threshold",
         kThreadedScrollPreventRenderingStarvation_200ms,
         std::size(kThreadedScrollPreventRenderingStarvation_200ms), nullptr},
        {"with a 333ms threshold",
         kThreadedScrollPreventRenderingStarvation_333ms,
         std::size(kThreadedScrollPreventRenderingStarvation_333ms), nullptr}};

// LINT.IfChange(AutofillUploadCardRequestTimeouts)
const FeatureEntry::FeatureParam
    kAutofillUploadCardRequestTimeout_6Point5Seconds[] = {
        {"autofill_upload_card_request_timeout_milliseconds", "6500"}};
const FeatureEntry::FeatureParam kAutofillUploadCardRequestTimeout_7Seconds[] =
    {{"autofill_upload_card_request_timeout_milliseconds", "7000"}};
const FeatureEntry::FeatureParam kAutofillUploadCardRequestTimeout_9Seconds[] =
    {{"autofill_upload_card_request_timeout_milliseconds", "9000"}};
const FeatureEntry::FeatureVariation
    kAutofillUploadCardRequestTimeoutOptions[] = {
        {"6.5 seconds", kAutofillUploadCardRequestTimeout_6Point5Seconds,
         std::size(kAutofillUploadCardRequestTimeout_6Point5Seconds), nullptr},
        {"7 seconds", kAutofillUploadCardRequestTimeout_7Seconds,
         std::size(kAutofillUploadCardRequestTimeout_7Seconds), nullptr},
        {"9 seconds", kAutofillUploadCardRequestTimeout_9Seconds,
         std::size(kAutofillUploadCardRequestTimeout_9Seconds), nullptr}};
// LINT.ThenChange(//ios/chrome/browser/flags/about_flags.mm:AutofillUploadCardRequestTimeouts)

// LINT.IfChange(AutofillVcnEnrollRequestTimeouts)
const FeatureEntry::FeatureParam kAutofillVcnEnrollRequestTimeout_5Seconds[] = {
    {"autofill_vcn_enroll_request_timeout_milliseconds", "5000"}};
const FeatureEntry::FeatureParam
    kAutofillVcnEnrollRequestTimeout_7Point5Seconds[] = {
        {"autofill_vcn_enroll_request_timeout_milliseconds", "7500"}};
const FeatureEntry::FeatureParam kAutofillVcnEnrollRequestTimeout_10Seconds[] =
    {{"autofill_vcn_enroll_request_timeout_milliseconds", "10000"}};
const FeatureEntry::FeatureVariation kAutofillVcnEnrollRequestTimeoutOptions[] =
    {{"5 seconds", kAutofillVcnEnrollRequestTimeout_5Seconds,
      std::size(kAutofillVcnEnrollRequestTimeout_5Seconds), nullptr},
     {"7.5 seconds", kAutofillVcnEnrollRequestTimeout_7Point5Seconds,
      std::size(kAutofillVcnEnrollRequestTimeout_7Point5Seconds), nullptr},
     {"10 seconds", kAutofillVcnEnrollRequestTimeout_10Seconds,
      std::size(kAutofillVcnEnrollRequestTimeout_10Seconds), nullptr}};
// LINT.ThenChange(//ios/chrome/browser/flags/about_flags.mm:AutofillVcnEnrollRequestTimeouts)

#if BUILDFLAG(ENABLE_EXTENSIONS)
const FeatureEntry::FeatureParam
    kExtensionTelemetryEnterpriseReportingIntervalSeconds_20Seconds[] = {
        {"EnterpriseReportingIntervalSeconds", "20"}};
const FeatureEntry::FeatureParam
    kExtensionTelemetryEnterpriseReportingIntervalSeconds_60Seconds[] = {
        {"EnterpriseReportingIntervalSeconds", "60"}};
const FeatureEntry::FeatureParam
    kExtensionTelemetryEnterpriseReportingIntervalSeconds_300Seconds[] = {
        {"EnterpriseReportingIntervalSeconds", "300"}};
const FeatureEntry::FeatureVariation
    kExtensionTelemetryEnterpriseReportingIntervalSecondsVariations[] = {
        {"20 seconds",
         kExtensionTelemetryEnterpriseReportingIntervalSeconds_20Seconds,
         std::size(
             kExtensionTelemetryEnterpriseReportingIntervalSeconds_20Seconds),
         nullptr},
        {"60 seconds",
         kExtensionTelemetryEnterpriseReportingIntervalSeconds_60Seconds,
         std::size(
             kExtensionTelemetryEnterpriseReportingIntervalSeconds_60Seconds),
         nullptr},
        {"300 seconds",
         kExtensionTelemetryEnterpriseReportingIntervalSeconds_300Seconds,
         std::size(
             kExtensionTelemetryEnterpriseReportingIntervalSeconds_300Seconds),
         nullptr}};
constexpr char kExtensionAiDataInternalName[] =
    "enable-extension-ai-data-collection";
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

const FeatureEntry::FeatureParam kDiscountOnShoppyPage[] = {
    {commerce::kDiscountOnShoppyPageParam, "true"}};

#if !BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureVariation kDiscountsVariations[] = {
    {"Discount on Shoppy page", kDiscountOnShoppyPage,
     std::size(kDiscountOnShoppyPage), nullptr}};
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kDiscountIconOnAndroidUseAlternateColor[] = {
    {commerce::kDiscountOnShoppyPageParam, "true"},
    {"action_chip_with_different_color", "true"}};
const FeatureEntry::FeatureVariation kDiscountsVariationsOnAndroid[] = {
    {"Discount on Shoppy page", kDiscountOnShoppyPage,
     std::size(kDiscountOnShoppyPage), nullptr},
    {"action chip different color", kDiscountIconOnAndroidUseAlternateColor,
     std::size(kDiscountIconOnAndroidUseAlternateColor), nullptr}};
#endif  // BUILDFLAG(IS_ANDROID)

const FeatureEntry::FeatureParam
    kSecurePaymentConfirmationNetworkAndIssuerIcons_Inline[] = {
        {"spc_network_and_issuer_icons_option", "inline"}};
const FeatureEntry::FeatureParam
    kSecurePaymentConfirmationNetworkAndIssuerIcons_Rows[] = {
        {"spc_network_and_issuer_icons_option", "rows"}};

const FeatureEntry::FeatureVariation
    kSecurePaymentConfirmationNetworkAndIssuerIconsOptions[] = {
        {"inline with title",
         kSecurePaymentConfirmationNetworkAndIssuerIcons_Inline,
         std::size(kSecurePaymentConfirmationNetworkAndIssuerIcons_Inline),
         nullptr},
        {"as table rows", kSecurePaymentConfirmationNetworkAndIssuerIcons_Rows,
         std::size(kSecurePaymentConfirmationNetworkAndIssuerIcons_Rows),
         nullptr}};

#if BUILDFLAG(IS_ANDROID)
const FeatureEntry::FeatureParam kClayBlockingWithFakeBackend[] = {
    {"use_fake_backend", "true"}};
const FeatureEntry::FeatureParam kClayBlockingVerbose[] = {
    {"enable_verbose_logging", "true"}};
const FeatureEntry::FeatureVariation kClayBlockingVariations[] = {
    {"(with fake backend)", kClayBlockingWithFakeBackend,
     std::size(kClayBlockingWithFakeBackend), nullptr},
    {"(verbose)", kClayBlockingVerbose, std::size(kClayBlockingVerbose),
     nullptr}};

const FeatureEntry::FeatureParam kSensitiveContentUsePwmHeuristics[] = {
    {"sensitive_content_use_pwm_heuristics", "true"}};

const FeatureEntry::FeatureVariation kSensitiveContentVariations[] = {
    {"with password manager heuristics", kSensitiveContentUsePwmHeuristics,
     std::size(kSensitiveContentUsePwmHeuristics), nullptr},
};
#endif  // BUILDFLAG(IS_ANDROID)

// Feature variations for kSubframeProcessReuseThresholds.
const FeatureEntry::FeatureParam kSubframeProcessReuseMemoryThreshold512MB{
    "SubframeProcessReuseMemoryThreshold", "536870912"};
const FeatureEntry::FeatureParam kSubframeProcessReuseMemoryThreshold1GB{
    "SubframeProcessReuseMemoryThreshold", "1073741824"};
const FeatureEntry::FeatureParam kSubframeProcessReuseMemoryThreshold2GB{
    "SubframeProcessReuseMemoryThreshold", "2147483648"};
const FeatureEntry::FeatureParam kSubframeProcessReuseMemoryThreshold4GB{
    "SubframeProcessReuseMemoryThreshold", "4294967296"};
const FeatureEntry::FeatureVariation
    kSubframeProcessReuseThresholdsVariations[] = {
        {"with 512MB memory threshold",
         &kSubframeProcessReuseMemoryThreshold512MB, 1, nullptr},
        {"with 1GB memory threshold", &kSubframeProcessReuseMemoryThreshold1GB,
         1, nullptr},
        {"with 2GB memory threshold", &kSubframeProcessReuseMemoryThreshold2GB,
         1, nullptr},
        {"with 4GB memory threshold", &kSubframeProcessReuseMemoryThreshold4GB,
         1, nullptr},
};

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
    {variations::switches::kEnableBenchmarking,
     flag_descriptions::kEnableBenchmarkingName,
     flag_descriptions::kEnableBenchmarkingDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableBenchmarkingChoices)},
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
    {"webrtc-hw-decoding", flag_descriptions::kWebrtcHwDecodingName,
     flag_descriptions::kWebrtcHwDecodingDescription, kOsAndroid | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebRtcHWDecoding)},
    {"webrtc-hw-encoding", flag_descriptions::kWebrtcHwEncodingName,
     flag_descriptions::kWebrtcHwEncodingDescription, kOsAndroid | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kWebRtcHWEncoding)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-lacros-in-chrome-kiosk",
     flag_descriptions::kChromeKioskEnableLacrosName,
     flag_descriptions::kChromeKioskEnableLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::standalone_browser::features::kChromeKioskEnableLacros)},
    {"enable-lacros-in-web-kiosk", flag_descriptions::kWebKioskEnableLacrosName,
     flag_descriptions::kWebKioskEnableLacrosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::standalone_browser::features::kWebKioskEnableLacros)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
    {"enable-webrtc-remote-event-log",
     flag_descriptions::kWebRtcRemoteEventLogName,
     flag_descriptions::kWebRtcRemoteEventLogDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebRtcRemoteEventLog)},
#endif
    {"enable-webrtc-allow-input-volume-adjustment",
     flag_descriptions::kWebRtcAllowInputVolumeAdjustmentName,
     flag_descriptions::kWebRtcAllowInputVolumeAdjustmentDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebRtcAllowInputVolumeAdjustment)},
    {"enable-webrtc-apm-downmix-capture-audio-method",
     flag_descriptions::kWebRtcApmDownmixCaptureAudioMethodName,
     flag_descriptions::kWebRtcApmDownmixCaptureAudioMethodDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kWebRtcApmDownmixCaptureAudioMethod,
         kWebRtcApmDownmixMethodVariations,
         "WebRtcApmDownmixCaptureAudioMethod")},
    {"enable-webrtc-hide-local-ips-with-mdns",
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsName,
     flag_descriptions::kWebrtcHideLocalIpsWithMdnsDecription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcHideLocalIpsWithMdns)},
    {"enable-webrtc-use-min-max-vea-dimensions",
     flag_descriptions::kWebrtcUseMinMaxVEADimensionsName,
     flag_descriptions::kWebrtcUseMinMaxVEADimensionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kWebRtcUseMinMaxVEADimensions)},
#if defined(WEBRTC_USE_PIPEWIRE)
    {"enable-webrtc-pipewire-camera",
     flag_descriptions::kWebrtcPipeWireCameraName,
     flag_descriptions::kWebrtcPipeWireCameraDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWebRtcPipeWireCamera)},
#endif  // defined(WEBRTC_USE_PIPEWIRE)
#if BUILDFLAG(ENABLE_NACL)
    {"enable-nacl", flag_descriptions::kNaclName,
     flag_descriptions::kNaclDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableNaCl)},
    {"verbose-logging-in-nacl", flag_descriptions::kVerboseLoggingInNaclName,
     flag_descriptions::kVerboseLoggingInNaclDescription, kOsAll,
     MULTI_VALUE_TYPE(kVerboseLoggingInNaclChoices)},
#endif  // ENABLE_NACL
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"web-hid-in-web-view", flag_descriptions::kEnableWebHidInWebViewName,
     flag_descriptions::kEnableWebHidInWebViewDescription, kOsAll,
     FEATURE_VALUE_TYPE(extensions_features::kEnableWebHidInWebView)},
    {"extensions-on-chrome-urls",
     flag_descriptions::kExtensionsOnChromeUrlsName,
     flag_descriptions::kExtensionsOnChromeUrlsDescription, kOsAll,
     SINGLE_VALUE_TYPE(extensions::switches::kExtensionsOnChromeURLs)},
#endif  // ENABLE_EXTENSIONS
#if BUILDFLAG(IS_ANDROID)
    {"contextual-search-suppress-short-view",
     flag_descriptions::kContextualSearchSuppressShortViewName,
     flag_descriptions::kContextualSearchSuppressShortViewDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kContextualSearchSuppressShortView,
         kContextualSearchSuppressShortViewVariations,
         "ContextualSearchSuppressShortView")},
    {"contextual-search-with-credentials-for-debug",
     flag_descriptions::kContextualSearchWithCredentialsForDebugName,
     flag_descriptions::kContextualSearchWithCredentialsForDebugDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(kContextualSearchWithCredentialsForDebug)},
    {"related-searches-all-language",
     flag_descriptions::kRelatedSearchesAllLanguageName,
     flag_descriptions::kRelatedSearchesAllLanguageDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRelatedSearchesAllLanguage)},
    {"related-searches-switch", flag_descriptions::kRelatedSearchesSwitchName,
     flag_descriptions::kRelatedSearchesSwitchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kRelatedSearchesSwitch)},
    {"omnibox-shortcuts-android",
     flag_descriptions::kOmniboxShortcutsAndroidName,
     flag_descriptions::kOmniboxShortcutsAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxShortcutsAndroid)},
    {"omnibox-rich-autocompletion-android",
     flag_descriptions::kRichAutocompletionAndroidName,
     flag_descriptions::kRichAutocompletionAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kRichAutocompletion,
                                    kRichAutocompletionAndroidVariations,
                                    "OmniboxRichAutocompletion")},
    {"safe-browsing-sync-checker-check-allowlist",
     flag_descriptions::kSafeBrowsingSyncCheckerCheckAllowlistName,
     flag_descriptions::kSafeBrowsingSyncCheckerCheckAllowlistDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(safe_browsing::kSafeBrowsingSyncCheckerCheckAllowlist)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::test::kAutofillShowTypePredictions)},
    {"autofill-more-prominent-popup",
     flag_descriptions::kAutofillMoreProminentPopupName,
     flag_descriptions::kAutofillMoreProminentPopupDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillMoreProminentPopup)},
    {"backdrop-filter-mirror-edge",
     flag_descriptions::kBackdropFilterMirrorEdgeName,
     flag_descriptions::kBackdropFilterMirrorEdgeDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBackdropFilterMirrorEdgeMode)},
    {"smooth-scrolling", flag_descriptions::kSmoothScrollingName,
     flag_descriptions::kSmoothScrollingDescription,
     // Mac has a separate implementation with its own setting to disable.
     kOsLinux | kOsLacros | kOsCrOS | kOsWin | kOsAndroid,
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
    {"webtransport-developer-mode",
     flag_descriptions::kWebTransportDeveloperModeName,
     flag_descriptions::kWebTransportDeveloperModeDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kWebTransportDeveloperMode)},
    {"disable-javascript-harmony-shipping",
     flag_descriptions::kJavascriptHarmonyShippingName,
     flag_descriptions::kJavascriptHarmonyShippingDescription, kOsAll,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableJavaScriptHarmonyShipping)},
    {"enable-javascript-harmony", flag_descriptions::kJavascriptHarmonyName,
     flag_descriptions::kJavascriptHarmonyDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kJavaScriptHarmony)},
    {"enable-enterprise-profile-badging-for-avatar",
     flag_descriptions::kEnterpriseProfileBadgingForAvatarName,
     flag_descriptions::kEnterpriseProfileBadgingForAvatarDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kEnterpriseProfileBadgingForAvatar)},
    {"enable-enterprise-profile-badging-for-menu",
     flag_descriptions::kEnterpriseProfileBadgingForMenuName,
     flag_descriptions::kEnterpriseProfileBadgingForMenuDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kEnterpriseProfileBadgingForMenu)},
    {"enable-enterprise-updated-profile-creation-screen",
     flag_descriptions::kEnterpriseUpdatedProfileCreationScreenName,
     flag_descriptions::kEnterpriseUpdatedProfileCreationScreenDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kEnterpriseUpdatedProfileCreationScreen)},
    {"enable-experimental-webassembly-features",
     flag_descriptions::kExperimentalWebAssemblyFeaturesName,
     flag_descriptions::kExperimentalWebAssemblyFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebAssemblyFeatures)},
    {"enable-experimental-webassembly-jspi",
     flag_descriptions::kExperimentalWebAssemblyJSPIName,
     flag_descriptions::kExperimentalWebAssemblyJSPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableExperimentalWebAssemblyJSPI)},
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
    {"enable-webassembly-memory64", flag_descriptions::kEnableWasmMemory64Name,
     flag_descriptions::kEnableWasmMemory64Description, kOsAll,
     FEATURE_VALUE_TYPE(features::kWebAssemblyMemory64)},
    {"enable-future-v8-vm-features", flag_descriptions::kV8VmFutureName,
     flag_descriptions::kV8VmFutureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kV8VmFuture)},
    {"enable-gpu-rasterization", flag_descriptions::kGpuRasterizationName,
     flag_descriptions::kGpuRasterizationDescription, kOsAll,
     MULTI_VALUE_TYPE(kEnableGpuRasterizationChoices)},
    {"enable-fontations-backend", flag_descriptions::kFontationsFontBackendName,
     flag_descriptions::kFontationsFontBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFontationsFontBackend)},
    {"enable-experimental-web-platform-features",
     flag_descriptions::kExperimentalWebPlatformFeaturesName,
     flag_descriptions::kExperimentalWebPlatformFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableExperimentalWebPlatformFeatures)},
    {"top-chrome-touch-ui", flag_descriptions::kTopChromeTouchUiName,
     flag_descriptions::kTopChromeTouchUiDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kTopChromeTouchUiChoices)},

#if !BUILDFLAG(IS_ANDROID)
    {"top-chrome-toasts", flag_descriptions::kTopChromeToastsName,
     flag_descriptions::kTopChromeToastsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(toast_features::kToastFramework,
                                    kToastVariations,
                                    "ToastFramework")},
#endif

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"allow-apn-modification-policy",
     flag_descriptions::kAllowApnModificationPolicyName,
     flag_descriptions::kAllowApnModificationPolicyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAllowApnModificationPolicy)},
    {"alt-click-and-six-pack-customization",
     flag_descriptions::kAltClickAndSixPackCustomizationName,
     flag_descriptions::kAltClickAndSixPackCustomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAltClickAndSixPackCustomization)},
    {"apn-policies", flag_descriptions::kApnPoliciesName,
     flag_descriptions::kApnPoliciesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kApnPolicies)},
    {"apn-revamp", flag_descriptions::kApnRevampName,
     flag_descriptions::kApnRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kApnRevamp)},
    {"audio-a2dp-advanced-codecs",
     flag_descriptions::kAudioA2DPAdvancedCodecsName,
     flag_descriptions::kAudioA2DPAdvancedCodecsDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioA2DPAdvancedCodecs")},
    {"audio-aec-required-for-cras-processor",
     flag_descriptions::kAudioAecRequiredForCrasProcessorName,
     flag_descriptions::kAudioAecRequiredForCrasProcessorDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE(
         "CrOSLateBootAudioAecRequiredForCrasProcessor")},
    {"audio-ap-noise-cancellation",
     flag_descriptions::kAudioAPNoiseCancellationName,
     flag_descriptions::kAudioAPNoiseCancellationDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioAPNoiseCancellation")},
    {"audio-hfp-mic-sr-toggle", flag_descriptions::kAudioHFPMicSRToggleName,
     flag_descriptions::kAudioHFPMicSRToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAudioHFPMicSRToggle)},
    {"audio-hfp-offload", flag_descriptions::kAudioHFPOffloadName,
     flag_descriptions::kAudioHFPOffloadDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioHFPOffload")},
    {"audio-hfp-swb", flag_descriptions::kAudioHFPSwbName,
     flag_descriptions::kAudioHFPSwbDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioHFPSwb")},
    {"audio-offload-cras-dsp-to-sof",
     flag_descriptions::kAudioOffloadCrasDSPToSOFName,
     flag_descriptions::kAudioOffloadCrasDSPToSOFDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioOffloadCrasDSPToSOF")},
    {"audio-selection-improvement",
     flag_descriptions::kAudioSelectionImprovementName,
     flag_descriptions::kAudioSelectionImprovementDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAudioSelectionImprovement)},
    {"reset-audio-selection-improvement-pref",
     flag_descriptions::kResetAudioSelectionImprovementPrefName,
     flag_descriptions::kResetAudioSelectionImprovementPrefDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kResetAudioSelectionImprovementPref)},
    {"audio-style-transfer", flag_descriptions::kAudioStyleTransferName,
     flag_descriptions::kAudioStyleTransferDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioStyleTransfer")},
    {"audio-suppress-set-rtc-audio-active",
     flag_descriptions::kAudioSuppressSetRTCAudioActiveName,
     flag_descriptions::kAudioSuppressSetRTCAudioActiveDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAudioSuppressSetRTCAudioActive")},
    {"cras-processor-dedicated-thread",
     flag_descriptions::kCrasProcessorDedicatedThreadName,
     flag_descriptions::kCrasProcessorDedicatedThreadDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootCrasProcessorDedicatedThread")},
    {"cras-processor-wav-dump", flag_descriptions::kCrasProcessorWavDumpName,
     flag_descriptions::kCrasProcessorWavDumpDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootCrasProcessorWavDump")},
    {"disable-explicit-dma-fences",
     flag_descriptions::kDisableExplicitDmaFencesName,
     flag_descriptions::kDisableExplicitDmaFencesDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDisableExplicitDmaFences)},
    // TODO(crbug.com/40652358): Remove this flag and provision when HDR is
    // fully
    //  supported on ChromeOS.
    {"use-hdr-transfer-function",
     flag_descriptions::kUseHDRTransferFunctionName,
     flag_descriptions::kUseHDRTransferFunctionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kUseHDRTransferFunction)},
    {"enable-external-display-hdr10",
     flag_descriptions::kEnableExternalDisplayHdr10Name,
     flag_descriptions::kEnableExternalDisplayHdr10Description, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kEnableExternalDisplayHDR10Mode)},
    {"adaptive-charging", flag_descriptions::kAdaptiveChargingName,
     flag_descriptions::kAdaptiveChargingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAdaptiveCharging)},
    {"adaptive-charging-for-testing",
     flag_descriptions::kAdaptiveChargingForTestingName,
     flag_descriptions::kAdaptiveChargingForTestingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAdaptiveChargingForTesting)},
    {"ash-capture-mode-education", flag_descriptions::kCaptureModeEducationName,
     flag_descriptions::kCaptureModeEducationDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kCaptureModeEducation,
                                    kCaptureModeEducationVariations,
                                    "CaptureModeEducation")},
    {"ash-capture-mode-education-bypass-limits",
     flag_descriptions::kCaptureModeEducationBypassLimitsName,
     flag_descriptions::kCaptureModeEducationBypassLimitsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCaptureModeEducationBypassLimits)},
    {"ash-capture-mode-gif-recording",
     flag_descriptions::kCaptureModeGifRecordingName,
     flag_descriptions::kCaptureModeGifRecordingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGifRecording)},
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
    {"rounded-windows", flag_descriptions::kRoundedWindows,
     flag_descriptions::kRoundedWindowsDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chromeos::features::kRoundedWindows,
                                    kRoundedWindowsRadiusVariation,
                                    "RoundedWindows")},
    {"bluetooth-audio-le-audio-only",
     flag_descriptions::kBluetoothAudioLEAudioOnlyName,
     flag_descriptions::kBluetoothAudioLEAudioOnlyDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootBluetoothAudioLEAudioOnly")},
    {"bluetooth-btsnoop-internals",
     flag_descriptions::kBluetoothBtsnoopInternalsName,
     flag_descriptions::kBluetoothBtsnoopInternalsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::bluetooth::features::kBluetoothBtsnoopInternals)},
    {"bluetooth-coredump", flag_descriptions::kBluetoothCoredumpName,
     flag_descriptions::kBluetoothCoredumpDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::bluetooth::features::kBluetoothCoredump)},
    {"bluetooth-disconnect-warning",
     flag_descriptions::kBluetoothDisconnectWarningName,
     flag_descriptions::kBluetoothDisconnectWarningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBluetoothDisconnectWarning)},
    {"bluetooth-floss-coredump", flag_descriptions::kBluetoothFlossCoredumpName,
     flag_descriptions::kBluetoothFlossCoredumpDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::bluetooth::features::kBluetoothFlossCoredump)},
    {"bluetooth-floss-telephony",
     flag_descriptions::kBluetoothFlossTelephonyName,
     flag_descriptions::kBluetoothFlossTelephonyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::bluetooth::features::kBluetoothFlossTelephony)},
    {kBluetoothUseFlossInternalName, flag_descriptions::kBluetoothUseFlossName,
     flag_descriptions::kBluetoothUseFlossDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(floss::features::kFlossEnabled)},
    {"bluetooth-floss-availability-check",
     flag_descriptions::kBluetoothFlossIsAvailabilityCheckNeededName,
     flag_descriptions::kBluetoothFlossIsAvailabilityCheckNeededDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(floss::features::kFlossIsAvailabilityCheckNeeded)},
    {kBluetoothUseLLPrivacyInternalName,
     flag_descriptions::kBluetoothUseLLPrivacyName,
     flag_descriptions::kBluetoothUseLLPrivacyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(bluez::features::kLinkLayerPrivacy)},
    {"campbell-glyph", flag_descriptions::kCampbellGlyphName,
     flag_descriptions::kCampbellGlyphDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kCampbellGlyph,
                                    kCampbellGlyphVariations,
                                    "GampbellGlyph")},
    {"campbell-key", flag_descriptions::kCampbellKeyName,
     flag_descriptions::kCampbellKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kCampbellKey, "")},
    {"cellular-bypass-esim-installation-connectivity-check",
     flag_descriptions::kCellularBypassESimInstallationConnectivityCheckName,
     flag_descriptions::
         kCellularBypassESimInstallationConnectivityCheckDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kCellularBypassESimInstallationConnectivityCheck)},
    {"cellular-use-second-euicc",
     flag_descriptions::kCellularUseSecondEuiccName,
     flag_descriptions::kCellularUseSecondEuiccDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCellularUseSecondEuicc)},
    {"cros-privacy-hub-app-permissions",
     flag_descriptions::kCrosPrivacyHubAppPermissionsName,
     flag_descriptions::kCrosPrivacyHubAppPermissionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrosPrivacyHubAppPermissions)},
    {"cros-privacy-hub-app-permissions-v2",
     flag_descriptions::kCrosPrivacyHubAppPermissionsV2Name,
     flag_descriptions::kCrosPrivacyHubAppPermissionsV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrosPrivacyHubAppPermissionsV2)},
    {"enable-cros-privacy-hub", flag_descriptions::kCrosPrivacyHubName,
     flag_descriptions::kCrosPrivacyHubDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrosPrivacyHub)},
    {"cros-components", flag_descriptions::kCrosComponentsName,
     flag_descriptions::kCrosComponentsDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kCrosComponents)},
    {"os-feedback-dialog", flag_descriptions::kOsFeedbackDialogName,
     flag_descriptions::kOsFeedbackDialogDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOsFeedbackDialog)},
    {"os-settings-revamp-wayfinding",
     flag_descriptions::kOsSettingsRevampWayfindingName,
     flag_descriptions::kOsSettingsRevampWayfindingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOsSettingsRevampWayfinding)},
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
    {
        "enable-brightness-control-in-settings",
        flag_descriptions::kEnableBrightnessControlInSettingsName,
        flag_descriptions::kEnableBrightnessControlInSettingsDescription,
        kOsCrOS,
        FEATURE_VALUE_TYPE(ash::features::kEnableBrightnessControlInSettings),
    },
    // Used to carry the policy value crossing the Chrome process lifetime.
    {ash::standalone_browser::kLacrosAvailabilityPolicyInternalName, "", "",
     kOsCrOS, MULTI_VALUE_TYPE(kLacrosAvailabilityPolicyChoices)},
    // Used to carry the policy value crossing the Chrome process lifetime.
    {crosapi::browser_util::kLacrosDataBackwardMigrationModePolicyInternalName,
     "", "", kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosDataBackwardMigrationModePolicyChoices)},
    {kLacrosStabilityInternalName, flag_descriptions::kLacrosStabilityName,
     flag_descriptions::kLacrosStabilityDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosStabilityChoices)},
    {kLacrosWaylandLoggingInternalName,
     flag_descriptions::kLacrosWaylandLoggingName,
     flag_descriptions::kLacrosWaylandLoggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosWaylandLogging)},
    {kPreferDcheckInternalName, flag_descriptions::kPreferDcheckName,
     flag_descriptions::kPreferDcheckDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kPreferDcheckChoices)},
    {"lacros-profile-migration-force-off",
     flag_descriptions::kLacrosProfileMigrationForceOffName,
     flag_descriptions::kLacrosProfileMigrationForceOffDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::standalone_browser::features::kLacrosProfileMigrationForceOff)},
    {"lacros-trigger-profile-backward-migration",
     flag_descriptions::kLacrosProfileBackwardMigrationName,
     flag_descriptions::kLacrosProfileBackwardMigrationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosProfileBackwardMigration)},
    {kLacrosSelectionInternalName, flag_descriptions::kLacrosSelectionName,
     flag_descriptions::kLacrosSelectionDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kLacrosSelectionChoices)},
    {kLacrosSelectionPolicyIgnoreInternalName,
     flag_descriptions::kLacrosSelectionPolicyIgnoreName,
     flag_descriptions::kLacrosSelectionPolicyIgnoreDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kLacrosSelectionPolicyIgnore)},
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
    {"enable-edid-based-display-ids",
     flag_descriptions::kEnableEdidBasedDisplayIdsName,
     flag_descriptions::kEnableEdidBasedDisplayIdsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kEnableEdidBasedDisplayIds)},
    {"enable-wifi-qos", flag_descriptions::kEnableWifiQosName,
     flag_descriptions::kEnableWifiQosDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableWifiQos)},
    {"enable-wifi-qos-enterprise",
     flag_descriptions::kEnableWifiQosEnterpriseName,
     flag_descriptions::kEnableWifiQosEnterpriseDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableWifiQosEnterprise)},
    {"enforce-ash-extension-keeplist",
     flag_descriptions::kEnforceAshExtensionKeeplistName,
     flag_descriptions::kEnforceAshExtensionKeeplistDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnforceAshExtensionKeeplist)},
    {"instant-hotspot-on-nearby",
     flag_descriptions::kInstantHotspotOnNearbyName,
     flag_descriptions::kInstantHotspotOnNearbyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kInstantHotspotOnNearby)},
    {"instant-hotspot-rebrand", flag_descriptions::kInstantHotspotRebrandName,
     flag_descriptions::kInstantHotspotRebrandDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kInstantHotspotRebrand)},
    {"instant-tethering", flag_descriptions::kTetherName,
     flag_descriptions::kTetherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kInstantTethering)},
    {"improved-keyboard-shortcuts",
     flag_descriptions::kImprovedKeyboardShortcutsName,
     flag_descriptions::kImprovedKeyboardShortcutsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kImprovedKeyboardShortcuts)},
    {"deprecate-alt-click", flag_descriptions::kDeprecateAltClickName,
     flag_descriptions::kDeprecateAltClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kDeprecateAltClick)},
    {"show-bluetooth-debug-log-toggle",
     flag_descriptions::kShowBluetoothDebugLogToggleName,
     flag_descriptions::kShowBluetoothDebugLogToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShowBluetoothDebugLogToggle)},
    {"show-taps", flag_descriptions::kShowTapsName,
     flag_descriptions::kShowTapsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kShowTaps)},
    {"show-touch-hud", flag_descriptions::kShowTouchHudName,
     flag_descriptions::kShowTouchHudDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kAshTouchHud)},
    {"tiled-display-support", flag_descriptions::kTiledDisplaySupportName,
     flag_descriptions::kTiledDisplaySupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kTiledDisplaySupport)},
    {"wake-on-wifi-allowed", flag_descriptions::kWakeOnWifiAllowedName,
     flag_descriptions::kWakeOnWifiAllowedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWakeOnWifiAllowed)},
    {"microphone-mute-switch-device",
     flag_descriptions::kMicrophoneMuteSwitchDeviceName,
     flag_descriptions::kMicrophoneMuteSwitchDeviceDescription, kOsCrOS,
     SINGLE_VALUE_TYPE("enable-microphone-mute-switch-device")},
    {"wifi-connect-mac-address-randomization",
     flag_descriptions::kWifiConnectMacAddressRandomizationName,
     flag_descriptions::kWifiConnectMacAddressRandomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWifiConnectMacAddressRandomization)},
    {"wifi-concurrency", flag_descriptions::kWifiConcurrencyName,
     flag_descriptions::kWifiConcurrencyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWifiConcurrency)},
    {"wifi-direct", flag_descriptions::kWifiDirectName,
     flag_descriptions::kWifiDirectDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWifiDirect)},
    {"disable-lacros-tts-support",
     flag_descriptions::kDisableLacrosTtsSupportName,
     flag_descriptions::kDisableLacrosTtsSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisableLacrosTtsSupport)},
    {"disable-dns-proxy", flag_descriptions::kDisableDnsProxyName,
     flag_descriptions::kDisableDnsProxyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisableDnsProxy)},
    {"firmware-update-ui-v2", flag_descriptions::kFirmwareUpdateUIV2Name,
     flag_descriptions::kFirmwareUpdateUIV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFirmwareUpdateUIV2)},
    {"multi-zone-rgb-keyboard", flag_descriptions::kMultiZoneRgbKeyboardName,
     flag_descriptions::kMultiZoneRgbKeyboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMultiZoneRgbKeyboard)},
    {"enable-rfc-8925", flag_descriptions::kEnableRFC8925Name,
     flag_descriptions::kEnableRFC8925Description, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableRFC8925)},
    {"support-f11-and-f12-shortcuts",
     flag_descriptions::kSupportF11AndF12ShortcutsName,
     flag_descriptions::kSupportF11AndF12ShortcutsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kSupportF11AndF12KeyShortcuts)},
    {"disconnect-wifi-on-ethernet-connected",
     flag_descriptions::kDisconnectWiFiOnEthernetConnectedName,
     flag_descriptions::kDisconnectWiFiOnEthernetConnectedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisconnectWiFiOnEthernetConnected)},
    {"lacros-extension-printing",
     flag_descriptions::kLacrosExtensionPrintingName,
     flag_descriptions::kLacrosExtensionPrintingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLacrosExtensionPrinting)},
    {"use-legacy-dhcpcd", flag_descriptions::kUseLegacyDHCPCDName,
     flag_descriptions::kUseLegacyDHCPCDDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUseLegacyDHCPCD)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"cros-apps-background-event-handling",
     flag_descriptions::kCrosAppsBackgroundEventHandlingName,
     flag_descriptions::kCrosAppsBackgroundEventHandlingDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kCrosAppsBackgroundEventHandling)},
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
    {
        "disable-accelerated-video-decode",
        flag_descriptions::kAcceleratedVideoDecodeName,
        flag_descriptions::kAcceleratedVideoDecodeDescription,
        kOsMac | kOsWin | kOsCrOS | kOsAndroid | kOsLacros | kOsLinux,
        SINGLE_DISABLE_VALUE_TYPE(switches::kDisableAcceleratedVideoDecode),
    },
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
    {"enable-media-foundation-clear-rendering-strategy",
     flag_descriptions::kMediaFoundationClearStrategyName,
     flag_descriptions::kMediaFoundationClearStrategyDescription, kOsWin,
     FEATURE_WITH_PARAMS_VALUE_TYPE(media::kMediaFoundationClearRendering,
                                    kMediaFoundationClearStrategyVariations,
                                    "MediaFoundationClearRendering")},
    {
        "enable-waitable-swap-chain",
        flag_descriptions::kUseWaitableSwapChainName,
        flag_descriptions::kUseWaitableSwapChainDescription,
        kOsWin,
        FEATURE_WITH_PARAMS_VALUE_TYPE(features::kDXGIWaitableSwapChain,
                                       kDXGIWaitableSwapChainVariations,
                                       "DXGIWaitableSwapChain"),
    },
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    {
        "fluent-overlay-scrollbars",
        flag_descriptions::kFluentOverlayScrollbarsName,
        flag_descriptions::kFluentOverlayScrollbarsDescription,
        kOsWin | kOsLinux,
        FEATURE_VALUE_TYPE(features::kFluentOverlayScrollbar),
    },
    {
        "fluent-scrollbars",
        flag_descriptions::kFluentScrollbarsName,
        flag_descriptions::kFluentScrollbarsDescription,
        kOsWin | kOsLinux,
        FEATURE_VALUE_TYPE(features::kFluentScrollbar),
    },
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
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
    {"username-first-flow-with-intermediate-values-predictions",
     flag_descriptions::kUsernameFirstFlowWithIntermediateValuesPredictionsName,
     flag_descriptions::
         kUsernameFirstFlowWithIntermediateValuesPredictionsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::
             kUsernameFirstFlowWithIntermediateValuesPredictions)},
    {"username-first-flow-with-intermediate-values-voting",
     flag_descriptions::kUsernameFirstFlowWithIntermediateValuesVotingName,
     flag_descriptions::
         kUsernameFirstFlowWithIntermediateValuesVotingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kUsernameFirstFlowWithIntermediateValuesVoting)},
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
     flag_descriptions::kWebBluetoothDescription, kOsLinux,
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
     kOsWin | kOsLinux | kOsAndroid | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kVulkan)},
    {"default-angle-vulkan", flag_descriptions::kDefaultAngleVulkanName,
     flag_descriptions::kDefaultAngleVulkanDescription,
     kOsLinux | kOsAndroid | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kDefaultANGLEVulkan)},
    {"vulkan-from-angle", flag_descriptions::kVulkanFromAngleName,
     flag_descriptions::kVulkanFromAngleDescription,
     kOsLinux | kOsAndroid | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kVulkanFromANGLE)},

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-system-notifications",
     flag_descriptions::kNotificationsSystemFlagName,
     flag_descriptions::kNotificationsSystemFlagDescription,
     kOsMac | kOsLinux | kOsWin,
     FEATURE_VALUE_TYPE(features::kSystemNotifications)},
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-ongoing-processes", flag_descriptions::kEnableOngoingProcessesName,
     flag_descriptions::kEnableOngoingProcessesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOngoingProcesses)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_ANDROID)
    {"adaptive-button-in-top-toolbar-page-summary",
     flag_descriptions::kAdaptiveButtonInTopToolbarPageSummaryName,
     flag_descriptions::kAdaptiveButtonInTopToolbarPageSummaryDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kAdaptiveButtonInTopToolbarPageSummary)},
    {"contextual-page-actions-share-model",
     flag_descriptions::kContextualPageActionsShareModelName,
     flag_descriptions::kContextualPageActionsShareModelDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kContextualPageActionShareModel)},
    {"reader-mode-heuristics", flag_descriptions::kReaderModeHeuristicsName,
     flag_descriptions::kReaderModeHeuristicsDescription, kOsAndroid,
     MULTI_VALUE_TYPE(kReaderModeHeuristicsChoices)},
    {"default-viewport-is-device-width",
     flag_descriptions::kDefaultViewportIsDeviceWidthName,
     flag_descriptions::kDefaultViewportIsDeviceWidthDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kDefaultViewportIsDeviceWidth)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeChoiceName,
     flag_descriptions::kInProductHelpDemoModeChoiceDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
#if !BUILDFLAG(IS_ANDROID)
    {"user-education-experience-v2",
     flag_descriptions::kUserEducationExperienceVersion2Name,
     flag_descriptions::kUserEducationExperienceVersion2Description, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         user_education::features::kUserEducationExperienceVersion2,
         kUserEducationExperienceVersion2Variants,
         "UserEducationExperienceVersion2")},
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-lock-screen-notification",
     flag_descriptions::kLockScreenNotificationName,
     flag_descriptions::kLockScreenNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLockScreenNotifications)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-service-workers-for-chrome-untrusted",
     flag_descriptions::kEnableServiceWorkersForChromeUntrustedName,
     flag_descriptions::kEnableServiceWorkersForChromeUntrustedDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableServiceWorkersForChromeUntrusted)},
    {"enterprise-reporting-ui", flag_descriptions::kEnterpriseReportingUIName,
     flag_descriptions::kEnterpriseReportingUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnterpriseReportingUI)},
    {"chromebox-usb-passthrough-restrictions",
     flag_descriptions::kChromeboxUsbPassthroughRestrictionsName,
     flag_descriptions::kChromeboxUsbPassthroughRestrictionsDescription,
     kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE(
         "CrOSLateBootChromeboxUsbPassthroughRestrictions")},
    {"disable-bruschetta-install-checks",
     flag_descriptions::kDisableBruschettaInstallChecksName,
     flag_descriptions::kDisableBruschettaInstallChecksDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisableBruschettaInstallChecks)},
    {"crostini-reset-lxd-db", flag_descriptions::kCrostiniResetLxdDbName,
     flag_descriptions::kCrostiniResetLxdDbDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrostiniResetLxdDb)},
    {"terminal-dev", flag_descriptions::kTerminalDevName,
     flag_descriptions::kTerminalDevDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTerminalDev)},
    {"permissive-usb-passthrough",
     flag_descriptions::kPermissiveUsbPassthroughName,
     flag_descriptions::kPermissiveUsbPassthroughDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootPermissiveUsbPassthrough")},
    {"camera-angle-backend", flag_descriptions::kCameraAngleBackendName,
     flag_descriptions::kCameraAngleBackendDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootCameraAngleBackend")},
    {"crostini-multi-container", flag_descriptions::kCrostiniMultiContainerName,
     flag_descriptions::kCrostiniMultiContainerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrostiniMultiContainer)},
    {"crostini-qt-ime-support", flag_descriptions::kCrostiniQtImeSupportName,
     flag_descriptions::kCrostiniQtImeSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrostiniQtImeSupport)},
    {"crostini-virtual-keyboard-support",
     flag_descriptions::kCrostiniVirtualKeyboardSupportName,
     flag_descriptions::kCrostiniVirtualKeyboardSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrostiniVirtualKeyboardSupport)},
    {"notifications-ignore-require-interaction",
     flag_descriptions::kNotificationsIgnoreRequireInteractionName,
     flag_descriptions::kNotificationsIgnoreRequireInteractionDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNotificationsIgnoreRequireInteraction)},
    {"sys-ui-holdback-drive-integration",
     flag_descriptions::kSysUiShouldHoldbackDriveIntegrationName,
     flag_descriptions::kSysUiShouldHoldbackDriveIntegrationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSysUiShouldHoldbackDriveIntegration)},
    {"sys-ui-holdback-focus-mode",
     flag_descriptions::kSysUiShouldHoldbackFocusModeName,
     flag_descriptions::kSysUiShouldHoldbackFocusModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSysUiShouldHoldbackFocusMode)},
    {"sys-ui-holdback-forest",
     flag_descriptions::kSysUiShouldHoldbackForestName,
     flag_descriptions::kSysUiShouldHoldbackForestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSysUiShouldHoldbackForest)},
    {"sys-ui-holdback-gif-recording",
     flag_descriptions::kSysUiShouldHoldbackGifRecordingName,
     flag_descriptions::kSysUiShouldHoldbackGifRecordingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSysUiShouldHoldbackGifRecording)},
    {"sys-ui-holdback-task-management",
     flag_descriptions::kSysUiShouldHoldbackTaskManagementName,
     flag_descriptions::kSysUiShouldHoldbackTaskManagementDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSysUiShouldHoldbackTaskManagement)},

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
     flag_descriptions::kEnableIsolatedWebAppsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIsolatedWebApps)},
#if BUILDFLAG(IS_CHROMEOS)
    {"enable-isolated-web-app-automatic-updates",
     flag_descriptions::kEnableIsolatedWebAppAutomaticUpdatesName,
     flag_descriptions::kEnableIsolatedWebAppAutomaticUpdatesDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kIsolatedWebAppAutomaticUpdates)},
    {"enable-isolated-web-app-managed-guest-session-install",
     flag_descriptions::kEnableIsolatedWebAppManagedGuestSessionInstallName,
     flag_descriptions::
         kEnableIsolatedWebAppManagedGuestSessionInstallDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kIsolatedWebAppManagedGuestSessionInstall)},
    {"enable-isolated-web-app-unmanaged-install",
     flag_descriptions::kEnableIsolatedWebAppUnmanagedInstallName,
     flag_descriptions::kEnableIsolatedWebAppUnmanagedInstallDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kIsolatedWebAppUnmanagedInstall)},
#endif
    {"enable-isolated-web-app-dev-mode",
     flag_descriptions::kEnableIsolatedWebAppDevModeName,
     flag_descriptions::kEnableIsolatedWebAppDevModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIsolatedWebAppDevMode)},
#if BUILDFLAG(IS_CHROMEOS)
    {"install-isolated-web-app-from-url",
     flag_descriptions::kInstallIsolatedWebAppFromUrl,
     flag_descriptions::kInstallIsolatedWebAppFromUrlDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(switches::kInstallIsolatedWebAppFromUrl, "")},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"enable-controlled-frame", flag_descriptions::kEnableControlledFrameName,
     flag_descriptions::kEnableControlledFrameDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kControlledFrame)},
#endif  // !BUILDFLAG(IS_ANDROID)

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
    {"allow-insecure-localhost", flag_descriptions::kAllowInsecureLocalhostName,
     flag_descriptions::kAllowInsecureLocalhostDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kAllowInsecureLocalhost)},
    {"text-based-audio-descriptions",
     flag_descriptions::kTextBasedAudioDescriptionName,
     flag_descriptions::kTextBasedAudioDescriptionDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kTextBasedAudioDescription)},
    {"bypass-app-banner-engagement-checks",
     flag_descriptions::kBypassAppBannerEngagementChecksName,
     flag_descriptions::kBypassAppBannerEngagementChecksDescription, kOsAll,
     FEATURE_VALUE_TYPE(webapps::features::kBypassAppBannerEngagementChecks)},
    {"enable-desktop-pwas-app-title",
     flag_descriptions::kDesktopPWAsAppTitleName,
     flag_descriptions::kDesktopPWAsAppTitleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableAppTitle)},
    {"enable-desktop-pwas-elided-extensions-menu",
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuName,
     flag_descriptions::kDesktopPWAsElidedExtensionsMenuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsElidedExtensionsMenu)},
    {"enable-desktop-pwas-tab-strip",
     flag_descriptions::kDesktopPWAsTabStripName,
     flag_descriptions::kDesktopPWAsTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kDesktopPWAsTabStrip)},
    {"enable-desktop-pwas-tab-strip-settings",
     flag_descriptions::kDesktopPWAsTabStripSettingsName,
     flag_descriptions::kDesktopPWAsTabStripSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsTabStripSettings)},
    {"enable-desktop-pwas-tab-strip-customizations",
     flag_descriptions::kDesktopPWAsTabStripCustomizationsName,
     flag_descriptions::kDesktopPWAsTabStripCustomizationsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kDesktopPWAsTabStripCustomizations)},
    {"enable-desktop-pwas-sub-apps", flag_descriptions::kDesktopPWAsSubAppsName,
     flag_descriptions::kDesktopPWAsSubAppsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kDesktopPWAsSubApps)},
    {"enable-desktop-pwas-scope-extensions",
     flag_descriptions::kDesktopPWAsScopeExtensionsName,
     flag_descriptions::kDesktopPWAsScopeExtensionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppEnableScopeExtensions)},
    {"enable-desktop-pwas-borderless",
     flag_descriptions::kDesktopPWAsBorderlessName,
     flag_descriptions::kDesktopPWAsBorderlessDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kWebAppBorderless)},
    {"enable-desktop-pwas-additional-windowing-controls",
     flag_descriptions::kDesktopPWAsAdditionalWindowingControlsName,
     flag_descriptions::kDesktopPWAsAdditionalWindowingControlsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         blink::features::kDesktopPWAsAdditionalWindowingControls)},
    {"record-web-app-debug-info", flag_descriptions::kRecordWebAppDebugInfoName,
     flag_descriptions::kRecordWebAppDebugInfoDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kRecordWebAppDebugInfo)},
#if !BUILDFLAG(IS_ANDROID)
    {"web-app-sync-generated-icon-background-fix",
     flag_descriptions::kWebAppSyncGeneratedIconBackgroundFixName,
     flag_descriptions::kWebAppSyncGeneratedIconBackgroundFixDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebAppSyncGeneratedIconBackgroundFix)},
    {"web-app-sync-generated-icon-retroactive-fix",
     flag_descriptions::kWebAppSyncGeneratedIconRetroactiveFixName,
     flag_descriptions::kWebAppSyncGeneratedIconRetroactiveFixDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebAppSyncGeneratedIconRetroactiveFix)},
    {"web-app-sync-generated-icon-update-fix",
     flag_descriptions::kWebAppSyncGeneratedIconUpdateFixName,
     flag_descriptions::kWebAppSyncGeneratedIconUpdateFixDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebAppSyncGeneratedIconUpdateFix)},
    {"web-app-universal-install",
     flag_descriptions::kWebAppUniversalInstallName,
     flag_descriptions::kWebAppUniversalInstallDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kWebAppUniversalInstall)},
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    {"shortcuts-not-apps", flag_descriptions::kShortcutsNotAppsName,
     flag_descriptions::kShortcutsNotAppsDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(features::kShortcutsNotApps)},
#endif
#if BUILDFLAG(IS_CHROMEOS)
    {"web-app-user-display-mode-sync-browser-mitigation",
     flag_descriptions::kUserDisplayModeSyncBrowserMitigationName,
     flag_descriptions::kUserDisplayModeSyncBrowserMitigationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(web_app::kUserDisplayModeSyncBrowserMitigation)},
    {"web-app-user-display-mode-sync-standalone-mitigation",
     flag_descriptions::kUserDisplayModeSyncStandaloneMitigationName,
     flag_descriptions::kUserDisplayModeSyncStandaloneMitigationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(web_app::kUserDisplayModeSyncStandaloneMitigation)},
#endif  // BUILDFLAG(IS_CHROMEOS)
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, kOsAll,
     SINGLE_VALUE_TYPE_AND_VALUE(
         syncer::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
#if !BUILDFLAG(IS_ANDROID)
    {"media-router-cast-allow-all-ips",
     flag_descriptions::kMediaRouterCastAllowAllIPsName,
     flag_descriptions::kMediaRouterCastAllowAllIPsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kCastAllowAllIPsFeature)},
    {"global-media-controls-cast-start-stop",
     flag_descriptions::kGlobalMediaControlsCastStartStopName,
     flag_descriptions::kGlobalMediaControlsCastStartStopDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kGlobalMediaControlsCastStartStop)},
    {"media-remoting-without-fullscreen",
     flag_descriptions::kMediaRemotingWithoutFullscreenName,
     flag_descriptions::kMediaRemotingWithoutFullscreenDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kMediaRemotingWithoutFullscreen)},
    {"remote-playback-backend", flag_descriptions::kRemotePlaybackBackendName,
     flag_descriptions::kRemotePlaybackBackendDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kRemotePlaybackBackend)},
    {"allow-all-sites-to-initiate-mirroring",
     flag_descriptions::kAllowAllSitesToInitiateMirroringName,
     flag_descriptions::kAllowAllSitesToInitiateMirroringDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kAllowAllSitesToInitiateMirroring)},
    {"media-route-dial-provider",
     flag_descriptions::kDialMediaRouteProviderName,
     flag_descriptions::kDialMediaRouteProviderDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kDialMediaRouteProvider)},
    {"delay-media-sink-discovery",
     flag_descriptions::kDelayMediaSinkDiscoveryName,
     flag_descriptions::kDelayMediaSinkDiscoveryDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kDelayMediaSinkDiscovery)},
    {"show-cast-permission-rejected-error",
     flag_descriptions::kShowCastPermissionRejectedErrorName,
     flag_descriptions::kShowCastPermissionRejectedErrorDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media_router::kShowCastPermissionRejectedError)},
    {"cast-streaming-hardware-h264",
     flag_descriptions::kCastStreamingHardwareH264Name,
     flag_descriptions::kCastStreamingHardwareH264Description, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(
         switches::kCastStreamingForceEnableHardwareH264,
         switches::kCastStreamingForceDisableHardwareH264)},

    {"cast-streaming-hardware-vp8",
     flag_descriptions::kCastStreamingHardwareVp8Name,
     flag_descriptions::kCastStreamingHardwareVp8Description, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(
         switches::kCastStreamingForceEnableHardwareVp8,
         switches::kCastStreamingForceDisableHardwareVp8)},

    {"cast-streaming-hardware-vp9",
     flag_descriptions::kCastStreamingHardwareVp9Name,
     flag_descriptions::kCastStreamingHardwareVp9Description, kOsDesktop,
     ENABLE_DISABLE_VALUE_TYPE(
         switches::kCastStreamingForceEnableHardwareVp9,
         switches::kCastStreamingForceDisableHardwareVp9)},

    {"cast-streaming-performance-overlay",
     flag_descriptions::kCastStreamingPerformanceOverlayName,
     flag_descriptions::kCastStreamingPerformanceOverlayDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kCastStreamingPerformanceOverlay)},

    {"enable-cast-streaming-av1", flag_descriptions::kCastStreamingAv1Name,
     flag_descriptions::kCastStreamingAv1Description, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kCastStreamingAv1)},

#if BUILDFLAG(IS_MAC)
    {"enable-cast-streaming-mac-hardware-h264",
     flag_descriptions::kCastStreamingMacHardwareH264Name,
     flag_descriptions::kCastStreamingMacHardwareH264Description, kOsMac,
     FEATURE_VALUE_TYPE(media::kCastStreamingMacHardwareH264)},
    {"use-network-framework-for-local-discovery",
     flag_descriptions::kUseNetworkFrameworkForLocalDiscoveryName,
     flag_descriptions::kUseNetworkFrameworkForLocalDiscoveryDescription,
     kOsMac,
     FEATURE_VALUE_TYPE(media_router::kUseNetworkFrameworkForLocalDiscovery)},
#endif

#if BUILDFLAG(IS_WIN)
    {"enable-cast-streaming-win-hardware-h264",
     flag_descriptions::kCastStreamingWinHardwareH264Name,
     flag_descriptions::kCastStreamingWinHardwareH264Description, kOsWin,
     FEATURE_VALUE_TYPE(media::kCastStreamingWinHardwareH264)},
#endif

    {"enable-cast-streaming-vp8", flag_descriptions::kCastStreamingVp8Name,
     flag_descriptions::kCastStreamingVp8Description, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kCastStreamingVp8)},

    {"enable-cast-streaming-vp9", flag_descriptions::kCastStreamingVp9Name,
     flag_descriptions::kCastStreamingVp9Description, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kCastStreamingVp9)},

    {"enable-cast-streaming-with-hidpi",
     flag_descriptions::kCastEnableStreamingWithHiDPIName,
     flag_descriptions::kCastEnableStreamingWithHiDPIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(mirroring::features::kCastEnableStreamingWithHiDPI)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"clay-blocking-dialog", flag_descriptions::kClayBlockingDialogName,
     flag_descriptions::kClayBlockingDialogDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(switches::kClayBlocking,
                                    kClayBlockingVariations,
                                    "ClayBlocking")},

    {"template-url-reconciliation",
     flag_descriptions::kTemplateUrlReconciliationName,
     flag_descriptions::kTemplateUrlReconciliationDialogDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kTemplateUrlReconciliation)},

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
    {"mac-syscall-sandbox", flag_descriptions::kMacSyscallSandboxName,
     flag_descriptions::kMacSyscallSandboxDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacSyscallSandbox)},

    {"mac-loopback-audio-for-screen-share",
     flag_descriptions::kMacLoopbackAudioForScreenShareName,
     flag_descriptions::kMacLoopbackAudioForScreenShareDescription, kOsMac,
     FEATURE_VALUE_TYPE(media::kMacLoopbackAudioForScreenShare)},

    {"use-sc-content-sharing-picker",
     flag_descriptions::kUseSCContentSharingPickerName,
     flag_descriptions::kUseSCContentSharingPickerDescription, kOsMac,
     FEATURE_VALUE_TYPE(media::kUseSCContentSharingPicker)},
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    {"web-share", flag_descriptions::kWebShareName,
     flag_descriptions::kWebShareDescription, kOsWin | kOsCrOS | kOsMac,
     FEATURE_VALUE_TYPE(features::kWebShare)},
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
    {"pulseaudio-loopback-for-cast",
     flag_descriptions::kPulseaudioLoopbackForCastName,
     flag_descriptions::kPulseaudioLoopbackForCastDescription, kOsLinux,
     FEATURE_VALUE_TYPE(media::kPulseaudioLoopbackForCast)},

    {"pulseaudio-loopback-for-screen-share",
     flag_descriptions::kPulseaudioLoopbackForScreenShareName,
     flag_descriptions::kPulseaudioLoopbackForScreenShareDescription, kOsLinux,
     FEATURE_VALUE_TYPE(media::kPulseaudioLoopbackForScreenShare)},

    {"ozone-platform-hint", flag_descriptions::kOzonePlatformHintName,
     flag_descriptions::kOzonePlatformHintDescription, kOsLinux,
     MULTI_VALUE_TYPE(kOzonePlatformHintRuntimeChoices)},

    {"simplified-tab-drag-ui", flag_descriptions::kSimplifiedTabDragUIName,
     flag_descriptions::kSimplifiedTabDragUIDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kAllowWindowDragUsingSystemDragDrop)},

    {"wayland-per-window-scaling",
     flag_descriptions::kWaylandPerWindowScalingName,
     flag_descriptions::kWaylandPerWindowScalingDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWaylandPerSurfaceScale)},

    {"wayland-text-input-v3", flag_descriptions::kWaylandTextInputV3Name,
     flag_descriptions::kWaylandTextInputV3Description, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWaylandTextInputV3)},

    {"wayland-ui-scaling", flag_descriptions::kWaylandUiScalingName,
     flag_descriptions::kWaylandUiScalingDescription, kOsLinux,
     FEATURE_VALUE_TYPE(features::kWaylandUiScale)},
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
    {"webxr-hand-input", flag_descriptions::kWebXrHandInputName,
     flag_descriptions::kWebXrHandInputDescription, kOsAll,
     FEATURE_VALUE_TYPE(device::features::kWebXrHandInput)},
    {"webxr-incubations", flag_descriptions::kWebXrIncubationsName,
     flag_descriptions::kWebXrIncubationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(device::features::kWebXrIncubations)},
    {"webxr-internals", flag_descriptions::kWebXrInternalsName,
     flag_descriptions::kWebXrInternalsDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(device::features::kWebXrInternals)},
    {"webxr-runtime", flag_descriptions::kWebXrForceRuntimeName,
     flag_descriptions::kWebXrForceRuntimeDescription, kOsDesktop | kOsAndroid,
     MULTI_VALUE_TYPE(kWebXrForceRuntimeChoices)},
#if BUILDFLAG(IS_ANDROID)
    {"webxr-shared-buffers", flag_descriptions::kWebXrSharedBuffersName,
     flag_descriptions::kWebXrSharedBuffersDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(device::features::kWebXrSharedBuffers)},
#if BUILDFLAG(ENABLE_OPENXR)
    {"enable-openxr-android", flag_descriptions::kOpenXRName,
     flag_descriptions::kOpenXRDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(device::features::kOpenXR)},
    {"enable-openxr-extended", flag_descriptions::kOpenXRExtendedFeaturesName,
     flag_descriptions::kOpenXRExtendedFeaturesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(device::features::kOpenXrExtendedFeatureSupport)},
#endif
#endif  // BUILDFLAG(IS_ANDROID)
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
    {"notification-permission-rationale-dialog",
     flag_descriptions::kNotificationPermissionRationaleName,
     flag_descriptions::kNotificationPermissionRationaleDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kNotificationPermissionVariant,
         kNotificationPermissionRationaleVariations,
         "NotificationPermissionVariant")},
    {"notification-permission-rationale-bottom-sheet",
     flag_descriptions::kNotificationPermissionRationaleBottomSheetName,
     flag_descriptions::kNotificationPermissionRationaleBottomSheetDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNotificationPermissionBottomSheet)},
    {"reengagement-notification",
     flag_descriptions::kReengagementNotificationName,
     flag_descriptions::kReengagementNotificationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReengagementNotification)},
    {"back-gesture-activity-tab-provider",
     flag_descriptions::kBackGestureActivityTabProviderName,
     flag_descriptions::kBackGestureActivityTabProviderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackGestureRefactorAndroid)},
    {"back-gesture-refactor-android",
     flag_descriptions::kBackGestureRefactorAndroidName,
     flag_descriptions::kBackGestureRefactorAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackGestureRefactorAndroid)},
    {"back-to-home-animation", flag_descriptions::kBackToHomeAnimationName,
     flag_descriptions::kBackToHomeAnimationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kBackToHomeAnimation)},

    // Android Edge to edge
    {"draw-cutout-edge-to-edge", flag_descriptions::kDrawCutoutEdgeToEdgeName,
     flag_descriptions::kDrawCutoutEdgeToEdgeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kDrawCutoutEdgeToEdge)},
    {"draw-edge-to-edge", flag_descriptions::kDrawEdgeToEdgeName,
     flag_descriptions::kDrawEdgeToEdgeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDrawEdgeToEdge)},
    {"draw-key-native-edge-to-edge",
     flag_descriptions::kDrawKeyNativeEdgeToEdgeName,
     flag_descriptions::kDrawKeyNativeEdgeToEdgeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDrawKeyNativeEdgeToEdge)},
    {"draw-native-edge-to-edge", flag_descriptions::kDrawNativeEdgeToEdgeName,
     flag_descriptions::kDrawNativeEdgeToEdgeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDrawNativeEdgeToEdge)},
    {"edge-to-edge-bottom-chin", flag_descriptions::kEdgeToEdgeBottomChinName,
     flag_descriptions::kEdgeToEdgeBottomChinDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kEdgeToEdgeBottomChin,
                                    kEdgeToEdgeBottomChinVariations,
                                    "EdgeToEdgeBottomChin")},
    {"edge-to-edge-everywhere", flag_descriptions::kEdgeToEdgeEverywhereName,
     flag_descriptions::kEdgeToEdgeEverywhereDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEdgeToEdgeEverywhere)},
    {"edge-to-edge-web-opt-in", flag_descriptions::kEdgeToEdgeWebOptInName,
     flag_descriptions::kEdgeToEdgeWebOptInDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kEdgeToEdgeWebOptIn)},
    {"dynamic-safe-area-insets", flag_descriptions::kDynamicSafeAreaInsetsName,
     flag_descriptions::kDynamicSafeAreaInsetsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kDynamicSafeAreaInsets)},
    {"dynamic-safe-area-insets-on-scroll",
     flag_descriptions::kDynamicSafeAreaInsetsOnScrollName,
     flag_descriptions::kDynamicSafeAreaInsetsOnScrollDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kDynamicSafeAreaInsetsOnScroll)},
    {"bottom-browser-controls-refactor",
     flag_descriptions::kBottomBrowserControlsRefactorName,
     flag_descriptions::kBottomBrowserControlsRefactorDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kBottomBrowserControlsRefactor,
         kBottomBrowserControlsRefactorVariations,
         "BottomBrowserControlsRefactor")},

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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    {"disallow-managed-profile-signout",
     flag_descriptions::kDisallowManagedProfileSignoutName,
     flag_descriptions::kDisallowManagedProfileSignoutDescription,
     kOsMac | kOsWin | kOsLinux | kOsLacros,
     FEATURE_VALUE_TYPE(kDisallowManagedProfileSignout)},
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
    {"view-transition-on-navigation",
     flag_descriptions::kViewTransitionOnNavigationName,
     flag_descriptions::kViewTransitionOnNavigationDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kViewTransitionOnNavigation)},
    {"view-transition-on-navigation-iframe",
     flag_descriptions::kViewTransitionOnNavigationIframeName,
     flag_descriptions::kViewTransitionOnNavigationIframeDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kViewTransitionOnNavigationForIframes)},
#if BUILDFLAG(IS_WIN)
    {"use-winrt-midi-api", flag_descriptions::kUseWinrtMidiApiName,
     flag_descriptions::kUseWinrtMidiApiDescription, kOsWin,
     FEATURE_VALUE_TYPE(midi::features::kMidiManagerWinrt)},
    {"webrtc-allow-wgc-screen-capturer",
     flag_descriptions::kWebRtcAllowWgcScreenCapturerName,
     flag_descriptions::kWebRtcAllowWgcScreenCapturerDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWebRtcAllowWgcScreenCapturer)},
    {"webrtc-allow-wgc-window-capturer",
     flag_descriptions::kWebRtcAllowWgcWindowCapturerName,
     flag_descriptions::kWebRtcAllowWgcWindowCapturerDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kWebRtcAllowWgcWindowCapturer)},
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
    {"omaha-min-sdk-version-android",
     flag_descriptions::kOmahaMinSdkVersionAndroidName,
     flag_descriptions::kOmahaMinSdkVersionAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kOmahaMinSdkVersionAndroid,
                                    kOmahaMinSdkVersionAndroidVariations,
                                    "OmahaMinSdkVersionAndroidStudy")},
#endif  // BUILDFLAG(IS_ANDROID)
    {"enable-tls13-early-data", flag_descriptions::kEnableTLS13EarlyDataName,
     flag_descriptions::kEnableTLS13EarlyDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableTLS13EarlyData)},
    {"enable-tls13-kyber", flag_descriptions::kEnableTLS13KyberName,
     flag_descriptions::kEnableTLS13KyberDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kPostQuantumKyber)},
    {"use-ml-kem", flag_descriptions::kUseMLKEMName,
     flag_descriptions::kUseMLKEMDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kUseMLKEM)},
#if BUILDFLAG(IS_ANDROID)
    {"feed-loading-placeholder", flag_descriptions::kFeedLoadingPlaceholderName,
     flag_descriptions::kFeedLoadingPlaceholderDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedLoadingPlaceholder)},
    {"feed-signed-out-view-demotion",
     flag_descriptions::kFeedSignedOutViewDemotionName,
     flag_descriptions::kFeedSignedOutViewDemotionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedSignedOutViewDemotion)},
    {"feed-v2-hearts", flag_descriptions::kInterestFeedV2HeartsName,
     flag_descriptions::kInterestFeedV2HeartsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2Hearts)},
    {"info-card-acknowledgement-tracking",
     flag_descriptions::kInfoCardAcknowledgementTrackingName,
     flag_descriptions::kInfoCardAcknowledgementTrackingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kInfoCardAcknowledgementTracking)},
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
    {"feed-containment", flag_descriptions::kFeedContainmentName,
     flag_descriptions::kFeedContainmentDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedContainment)},
    {"feed-discofeed-endpoint", flag_descriptions::kFeedDiscoFeedEndpointName,
     flag_descriptions::kFeedDiscoFeedEndpointDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kDiscoFeedEndpoint)},
    {"feed-dynamic-colors", flag_descriptions::kFeedDynamicColorsName,
     flag_descriptions::kFeedDynamicColorsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedDynamicColors)},
    {"feed-follow-ui-update", flag_descriptions::kFeedFollowUiUpdateName,
     flag_descriptions::kFeedFollowUiUpdateDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kFeedFollowUiUpdate)},
    {"refresh-feed-on-start", flag_descriptions::kRefreshFeedOnRestartName,
     flag_descriptions::kRefreshFeedOnRestartDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(feed::kRefreshFeedOnRestart)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"enable-force-dark", flag_descriptions::kAutoWebContentsDarkModeName,
     flag_descriptions::kAutoWebContentsDarkModeDescription, kOsAll,
#if BUILDFLAG(IS_CHROMEOS_ASH)
     // TODO(crbug.com/40651782): Investigate crash reports and
     // re-enable variations for ChromeOS.
     FEATURE_VALUE_TYPE(blink::features::kForceWebContentsDarkMode)},
#else
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kForceWebContentsDarkMode,
                                    kForceDarkVariations,
                                    "ForceDarkVariations")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_ANDROID)
    {"enable-accessibility-include-long-click-action",
     flag_descriptions::kAccessibilityIncludeLongClickActionName,
     flag_descriptions::kAccessibilityIncludeLongClickActionDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAccessibilityIncludeLongClickAction)},
    {"enable-accessibility-page-zoom",
     flag_descriptions::kAccessibilityPageZoomName,
     flag_descriptions::kAccessibilityPageZoomDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kAccessibilityPageZoom,
                                    kAccessibilityPageZoomVariations,
                                    "AccessibilityPageZoom")},
    {"enable-accessibility-page-zoom-enhancements",
     flag_descriptions::kAccessibilityPageZoomEnhancementsName,
     flag_descriptions::kAccessibilityPageZoomEnhancementsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAccessibilityPageZoomEnhancements)},
    {"enable-accessibility-snapshot-stress-tests",
     flag_descriptions::kAccessibilitySnapshotStressTestsName,
     flag_descriptions::kAccessibilitySnapshotStressTestsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAccessibilitySnapshotStressTests)},
    {"enable-accessibility-unified-snapshots",
     flag_descriptions::kAccessibilityUnifiedSnapshotsName,
     flag_descriptions::kAccessibilityUnifiedSnapshotsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAccessibilityUnifiedSnapshots)},
    {"enable-accessibility-manage-broadcast-recevier-on-background",
     flag_descriptions::kAccessibilityManageBroadcastReceiverOnBackgroundName,
     flag_descriptions::
         kAccessibilityManageBroadcastReceiverOnBackgroundDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         features::kAccessibilityManageBroadcastReceiverOnBackground)},
    {"enable-smart-zoom", flag_descriptions::kSmartZoomName,
     flag_descriptions::kSmartZoomDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSmartZoom)},
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
     FEATURE_VALUE_TYPE(ash::features::kAutocorrectParamsTuning)},
    {"enable-cros-autocorrect-toggle",
     flag_descriptions::kAutocorrectToggleName,
     flag_descriptions::kAutocorrectToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAutocorrectToggle)},
    {"enable-cros-autocorrect-by-default",
     flag_descriptions::kAutocorrectByDefaultName,
     flag_descriptions::kAutocorrectByDefaultDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAutocorrectByDefault)},
    {"enable-cros-autocorrect-use-replace-surrounding-text",
     flag_descriptions::kAutocorrectUseReplaceSurroundingTextName,
     flag_descriptions::kAutocorrectUseReplaceSurroundingTextDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAutocorrectUseReplaceSurroundingText)},
    {"enable-cros-diacritics-use-replace-surrounding-text",
     flag_descriptions::kDiacriticsUseReplaceSurroundingTextName,
     flag_descriptions::kDiacriticsUseReplaceSurroundingTextDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDiacriticsUseReplaceSurroundingText)},
    {"enable-cros-first-party-vietnamese-input",
     flag_descriptions::kFirstPartyVietnameseInputName,
     flag_descriptions::kFirstPartyVietnameseInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFirstPartyVietnameseInput)},
    {"enable-cros-hindi-inscript-layout",
     flag_descriptions::kHindiInscriptLayoutName,
     flag_descriptions::kHindiInscriptLayoutDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHindiInscriptLayout)},
    {"enable-cros-ime-assist-multi-word",
     flag_descriptions::kImeAssistMultiWordName,
     flag_descriptions::kImeAssistMultiWordDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAssistMultiWord)},
    {"enable-cros-ime-fst-decoder-params-update",
     flag_descriptions::kImeFstDecoderParamsUpdateName,
     flag_descriptions::kImeFstDecoderParamsUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeFstDecoderParamsUpdate)},
    {"enable-cros-ime-system-emoji-picker-gif-support",
     flag_descriptions::kImeSystemEmojiPickerGIFSupportName,
     flag_descriptions::kImeSystemEmojiPickerGIFSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeSystemEmojiPickerGIFSupport)},
    {"enable-cros-ime-system-emoji-picker-jelly-support",
     flag_descriptions::kImeSystemEmojiPickerJellySupportName,
     flag_descriptions::kImeSystemEmojiPickerJellySupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeSystemEmojiPickerJellySupport)},
    {"enable-cros-ime-system-emoji-picker-mojo-search",
     flag_descriptions::kImeSystemEmojiPickerMojoSearchName,
     flag_descriptions::kImeSystemEmojiPickerMojoSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeSystemEmojiPickerMojoSearch)},
    {"enable-cros-ime-system-emoji-picker-variant-grouping",
     flag_descriptions::kImeSystemEmojiPickerVariantGroupingName,
     flag_descriptions::kImeSystemEmojiPickerVariantGroupingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeSystemEmojiPickerVariantGrouping)},
    {"enable-cros-ime-us-english-model-update",
     flag_descriptions::kImeUsEnglishModelUpdateName,
     flag_descriptions::kImeUsEnglishModelUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeUsEnglishModelUpdate)},
    {"enable-cros-ime-korean-mode-switch-debug",
     flag_descriptions::kImeKoreanModeSwitchDebugName,
     flag_descriptions::kImeKoreanModeSwitchDebugDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeKoreanModeSwitchDebug)},
    {"enable-cros-ime-korean-only-mode-switch-on-right-alt",
     flag_descriptions::kImeKoreanOnlyModeSwitchOnRightAltName,
     flag_descriptions::kImeKoreanOnlyModeSwitchOnRightAltDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeKoreanOnlyModeSwitchOnRightAlt)},
    {"enable-cros-ime-switch-check-connection-status",
     flag_descriptions::kImeSwitchCheckConnectionStatusName,
     flag_descriptions::kImeSwitchCheckConnectionStatusDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kImeSwitchCheckConnectionStatus)},
    {"enable-cros-japanese-os-settings",
     flag_descriptions::kJapaneseOSSettingsName,
     flag_descriptions::kJapaneseOSSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kJapaneseOSSettings)},
    {"enable-cros-system-japanese-physical-typing",
     flag_descriptions::kSystemJapanesePhysicalTypingName,
     flag_descriptions::kSystemJapanesePhysicalTypingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSystemJapanesePhysicalTyping)},
    {"enable-cros-virtual-keyboard-global-emoji-preferences",
     flag_descriptions::kVirtualKeyboardGlobalEmojiPreferencesName,
     flag_descriptions::kVirtualKeyboardGlobalEmojiPreferencesDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVirtualKeyboardGlobalEmojiPreferences)},
    {"enable-accessibility-filter-keys",
     flag_descriptions::kAccessibilityFilterKeysName,
     flag_descriptions::kAccessibilityFilterKeysDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityFilterKeys)},
    {"enable-experimental-accessibility-dictation-context-checking",
     flag_descriptions::kExperimentalAccessibilityDictationContextCheckingName,
     flag_descriptions::
         kExperimentalAccessibilityDictationContextCheckingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kExperimentalAccessibilityDictationContextChecking)},
    {"enable-experimental-accessibility-google-tts-high-quality-voices",
     flag_descriptions::
         kExperimentalAccessibilityGoogleTtsHighQualityVoicesName,
     flag_descriptions::
         kExperimentalAccessibilityGoogleTtsHighQualityVoicesDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         features::kExperimentalAccessibilityGoogleTtsHighQualityVoices)},
    {"enable-experimental-accessibility-manifest-v3",
     flag_descriptions::kExperimentalAccessibilityManifestV3Name,
     flag_descriptions::kExperimentalAccessibilityManifestV3Description,
     kOsCrOS,
     SINGLE_VALUE_TYPE(::switches::kEnableExperimentalAccessibilityManifestV3)},
    {"enable-experimental-accessibility-switch-access-text",
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextName,
     flag_descriptions::kExperimentalAccessibilitySwitchAccessTextDescription,
     kOsCrOS,
     SINGLE_VALUE_TYPE(
         ::switches::kEnableExperimentalAccessibilitySwitchAccessText)},
    {"expose-out-of-process-video-decoding-to-lacros",
     flag_descriptions::kExposeOutOfProcessVideoDecodingToLacrosName,
     flag_descriptions::kExposeOutOfProcessVideoDecodingToLacrosDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(media::kExposeOutOfProcessVideoDecodingToLacros)},
    {"enable-system-proxy-for-system-services",
     flag_descriptions::kSystemProxyForSystemServicesName,
     flag_descriptions::kSystemProxyForSystemServicesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSystemProxyForSystemServices)},
    {"system-shortcut-behavior", flag_descriptions::kSystemShortcutBehaviorName,
     flag_descriptions::kSystemShortcutBehaviorDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kSystemShortcutBehavior,
                                    kSystemShortcutBehaviorVariations,
                                    "SystemShortcutBehavior")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-cros-touch-text-editing-redesign",
     flag_descriptions::kTouchTextEditingRedesignName,
     flag_descriptions::kTouchTextEditingRedesignDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kTouchTextEditingRedesign)},
#if BUILDFLAG(IS_CHROMEOS)
    {"quickoffice-force-file-download",
     flag_descriptions::kQuickOfficeForceFileDownloadName,
     flag_descriptions::kQuickOfficeForceFileDownloadDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kQuickOfficeForceFileDownload)},
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_MAC)
    {"enable-extensible-enterprise-sso",
     flag_descriptions::kEnableExtensibleEnterpriseSSOName,
     flag_descriptions::kEnableExtensibleEnterpriseSSODescription, kOsMac,
     FEATURE_VALUE_TYPE(enterprise_auth::kEnableExtensibleEnterpriseSSO)},
    {"enable-retry-capture-device-enumeration-on-crash",
     flag_descriptions::kRetryGetVideoCaptureDeviceInfosName,
     flag_descriptions::kRetryGetVideoCaptureDeviceInfosDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kRetryGetVideoCaptureDeviceInfos)},
    {"enable-immersive-fullscreen-toolbar",
     flag_descriptions::kImmersiveFullscreenName,
     flag_descriptions::kImmersiveFullscreenDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kImmersiveFullscreen)},
    {"enable-fullscreen-animate-tabs",
     flag_descriptions::kMacFullscreenAnimateTabsName,
     flag_descriptions::kMacFullscreenAnimateTabsDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kFullscreenAnimateTabs)},
    {"enable-fullscreen-always-show-traffic-lights",
     flag_descriptions::kFullscreenAlwaysShowTrafficLightsName,
     flag_descriptions::kFullscreenAlwaysShowTrafficLightsDescription, kOsMac,
     FEATURE_VALUE_TYPE(
         remote_cocoa::features::kFullscreenAlwaysShowTrafficLights)},
#endif  // BUILDFLAG(IS_MAC)
    {"enable-web-payments-experimental-features",
     flag_descriptions::kWebPaymentsExperimentalFeaturesName,
     flag_descriptions::kWebPaymentsExperimentalFeaturesDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kWebPaymentsExperimentalFeatures)},
    {"enable-debug-for-store-billing",
     flag_descriptions::kAppStoreBillingDebugName,
     flag_descriptions::kAppStoreBillingDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(payments::features::kAppStoreBillingDebug)},
    {"enable-debug-for-secure-payment-confirmation",
     flag_descriptions::kSecurePaymentConfirmationDebugName,
     flag_descriptions::kSecurePaymentConfirmationDebugDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSecurePaymentConfirmationDebug)},
    {"enable-network-and-issuer-icons-for-secure-payment-confirmation",
     flag_descriptions::kSecurePaymentConfirmationNetworkAndIssuerIconsName,
     flag_descriptions::
         kSecurePaymentConfirmationNetworkAndIssuerIconsDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kSecurePaymentConfirmationNetworkAndIssuerIcons,
         kSecurePaymentConfirmationNetworkAndIssuerIconsOptions,
         "SecurePaymentConfirmationNetworkAndIssuerIcons")},
#if BUILDFLAG(IS_ANDROID)
    {"show-ready-to-pay-debug-info",
     flag_descriptions::kShowReadyToPayDebugInfoName,
     flag_descriptions::kShowReadyToPayDebugInfoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(payments::android::kShowReadyToPayDebugInfo)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"mutation-events", flag_descriptions::kMutationEventsName,
     flag_descriptions::kMutationEventsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kMutationEvents)},
    {"keyboard-focusable-scrollers",
     flag_descriptions::kKeyboardFocusableScrollersName,
     flag_descriptions::kKeyboardFocusableScrollersDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kKeyboardFocusableScrollers)},
    {"fill-on-account-select", flag_descriptions::kFillOnAccountSelectName,
     flag_descriptions::kFillOnAccountSelectDescription, kOsAll,
     FEATURE_VALUE_TYPE(password_manager::features::kFillOnAccountSelect)},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-aaudio-mmap-low-latency",
     flag_descriptions::kArcAAudioMMAPLowLatencyName,
     flag_descriptions::kArcAAudioMMAPLowLatencyDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootArcVmAAudioMMAPLowLatency")},
    {"arc-custom-tabs-experiment",
     flag_descriptions::kArcCustomTabsExperimentName,
     flag_descriptions::kArcCustomTabsExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kCustomTabsExperimentFeature)},
    {"arc-documents-provider-unknown-size",
     flag_descriptions::kArcDocumentsProviderUnknownSizeName,
     flag_descriptions::kArcDocumentsProviderUnknownSizeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kDocumentsProviderUnknownSizeFeature)},
    {"arc-enable-attestation", flag_descriptions::kArcEnableAttestationName,
     flag_descriptions::kArcEnableAttestationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableArcAttestation)},
    {kArcEnableVirtioBlkForDataInternalName,
     flag_descriptions::kArcEnableVirtioBlkForDataName,
     flag_descriptions::kArcEnableVirtioBlkForDataDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableVirtioBlkForData)},
    {"arc-extend-input-anr-timeout",
     flag_descriptions::kArcExtendInputAnrTimeoutName,
     flag_descriptions::kArcExtendInputAnrTimeoutDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kExtendInputAnrTimeout)},
    {"arc-extend-intent-anr-timeout",
     flag_descriptions::kArcExtendIntentAnrTimeoutName,
     flag_descriptions::kArcExtendIntentAnrTimeoutDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kExtendIntentAnrTimeout)},
    {"arc-extend-service-anr-timeout",
     flag_descriptions::kArcExtendServiceAnrTimeoutName,
     flag_descriptions::kArcExtendServiceAnrTimeoutDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kExtendServiceAnrTimeout)},
    {"arc-external-storage-access",
     flag_descriptions::kArcExternalStorageAccessName,
     flag_descriptions::kArcExternalStorageAccessDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kExternalStorageAccess)},
    {"arc-friendlier-error-dialog",
     flag_descriptions::kArcFriendlierErrorDialogName,
     flag_descriptions::kArcFriendlierErrorDialogDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableFriendlierErrorDialog)},
    {"arc-file-picker-experiment",
     flag_descriptions::kArcFilePickerExperimentName,
     flag_descriptions::kArcFilePickerExperimentDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kFilePickerExperimentFeature)},
    {"arc-ignore-hover-event-anr",
     flag_descriptions::kArcIgnoreHoverEventAnrName,
     flag_descriptions::kArcIgnoreHoverEventAnrDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kIgnoreHoverEventAnr)},
    {"arc-instant-response-window-open",
     flag_descriptions::kArcInstantResponseWindowOpenName,
     flag_descriptions::kArcInstantResponseWindowOpenDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kInstantResponseWindowOpen)},
    {"arc-native-bridge-toggle", flag_descriptions::kArcNativeBridgeToggleName,
     flag_descriptions::kArcNativeBridgeToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kNativeBridgeToggleFeature)},
    {"arc-per-app-language", flag_descriptions::kArcPerAppLanguageName,
     flag_descriptions::kArcPerAppLanguageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kPerAppLanguage)},
    {"arc-resize-compat", flag_descriptions::kArcResizeCompatName,
     flag_descriptions::kArcResizeCompatDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kResizeCompat)},
    {"arc-rounded-window-compat",
     flag_descriptions::kArcRoundedWindowCompatName,
     flag_descriptions::kArcRoundedWindowCompatDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(arc::kRoundedWindowCompat,
                                    kArcRoundedWindowCompatVariation,
                                    "ArcRoundedWindowCompat")},
    {"arc-rt-vcpu-dual-core", flag_descriptions::kArcRtVcpuDualCoreName,
     flag_descriptions::kArcRtVcpuDualCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuDualCore)},
    {"arc-rt-vcpu-quad-core", flag_descriptions::kArcRtVcpuQuadCoreName,
     flag_descriptions::kArcRtVcpuQuadCoreDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kRtVcpuQuadCore)},
    {"arc-touchscreen-emulation",
     flag_descriptions::kArcTouchscreenEmulationName,
     flag_descriptions::kArcTouchscreenEmulationDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kTouchscreenEmulation)},
    {"arc-switch-to-keymint-daemon",
     flag_descriptions::kArcSwitchToKeyMintDaemonName,
     flag_descriptions::kArcSwitchToKeyMintDaemonDesc, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootArcSwitchToKeyMintDaemon")},
    {"arc-switch-to-keymint-on-t",
     flag_descriptions::kArcSwitchToKeyMintOnTName,
     flag_descriptions::kArcSwitchToKeyMintOnTDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kSwitchToKeyMintOnT)},
    {"arc-switch-to-keymint-on-t-override",
     flag_descriptions::kArcSwitchToKeyMintOnTOverrideName,
     flag_descriptions::kArcSwitchToKeyMintOnTOverrideDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kSwitchToKeyMintOnTOverride)},
    {"arc-sync-install-priority",
     flag_descriptions::kArcSyncInstallPriorityName,
     flag_descriptions::kArcSyncInstallPriorityDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kSyncInstallPriority)},
    {"arc-unthrottle-on-active-audio-v2",
     flag_descriptions::kArcUnthrottleOnActiveAudioV2Name,
     flag_descriptions::kArcUnthrottleOnActiveAudioV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kUnthrottleOnActiveAudioV2)},
    {"arc-vmm-swap-keyboard-shortcut",
     flag_descriptions::kArcVmmSwapKBShortcutName,
     flag_descriptions::kArcVmmSwapKBShortcutDesc, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kVmmSwapKeyboardShortcut)},
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
    {"enable-autofill-virtual-view-structure",
     flag_descriptions::kAutofillVirtualViewStructureAndroidName,
     flag_descriptions::kAutofillVirtualViewStructureAndroidDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillVirtualViewStructureAndroid,
         kAutofillVirtualViewStructureVariation,
         "Skip AutofillService Check")},

    {"enable-pix-detection", flag_descriptions::kEnablePixDetectionName,
     flag_descriptions::kEnablePixDetectionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(payments::facilitated::kEnablePixDetection)},

    {"enable-pix-detection-on-dom-content-loaded",
     flag_descriptions::kEnablePixDetectionOnDomContentLoadedName,
     flag_descriptions::kEnablePixDetectionOnDomContentLoadedDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         payments::facilitated::kEnablePixDetectionOnDomContentLoaded)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-touchscreen-calibration",
     flag_descriptions::kTouchscreenCalibrationName,
     flag_descriptions::kTouchscreenCalibrationDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kEnableTouchCalibrationSetting)},
    {"enable-touchscreen-mapping", flag_descriptions::kTouchscreenMappingName,
     flag_descriptions::kTouchscreenMappingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableTouchscreenMappingExperience)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"prefer-constant-frame-rate",
     flag_descriptions::kPreferConstantFrameRateName,
     flag_descriptions::kPreferConstantFrameRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPreferConstantFrameRate)},
    {"force-control-face-ae", flag_descriptions::kForceControlFaceAeName,
     flag_descriptions::kForceControlFaceAeDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kForceControlFaceAeChoices)},
    {"auto-framing-override", flag_descriptions::kAutoFramingOverrideName,
     flag_descriptions::kAutoFramingOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAutoFramingOverrideChoices)},
    {"camera-super-res-override",
     flag_descriptions::kCameraSuperResOverrideName,
     flag_descriptions::kCameraSuperResOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCameraSuperResOverrideChoices)},
    {"face-retouch-override", flag_descriptions::kFaceRetouchOverrideName,
     flag_descriptions::kFaceRetouchOverrideDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kFaceRetouchOverrideChoices)},
    {"camera-app-digital-zoom", flag_descriptions::kCameraAppDigitalZoomName,
     flag_descriptions::kCameraAppDigitalZoomDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCameraAppDigitalZoom)},
    {"camera-app-pdf-ocr", flag_descriptions::kCameraAppPdfOcrName,
     flag_descriptions::kCameraAppPdfOcrDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCameraAppPdfOcr)},
    {"camera-app-preview-ocr", flag_descriptions::kCameraAppPreviewOcrName,
     flag_descriptions::kCameraAppPreviewOcrDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCameraAppPreviewOcr)},
    {"crostini-gpu-support", flag_descriptions::kCrostiniGpuSupportName,
     flag_descriptions::kCrostiniGpuSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrostiniGpuSupport)},
    {"disable-camera-frame-rotation-at-source",
     flag_descriptions::kDisableCameraFrameRotationAtSourceName,
     flag_descriptions::kDisableCameraFrameRotationAtSourceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::features::kDisableCameraFrameRotationAtSource)},
    {"file-transfer-enterprise-connector",
     flag_descriptions::kFileTransferEnterpriseConnectorName,
     flag_descriptions::kFileTransferEnterpriseConnectorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kFileTransferEnterpriseConnector)},
    {"file-transfer-enterprise-connector-ui",
     flag_descriptions::kFileTransferEnterpriseConnectorUIName,
     flag_descriptions::kFileTransferEnterpriseConnectorUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kFileTransferEnterpriseConnectorUI)},
    {"files-conflict-dialog", flag_descriptions::kFilesConflictDialogName,
     flag_descriptions::kFilesConflictDialogDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFilesConflictDialog)},
    {"files-kernel-drivers", flag_descriptions::kFilesKernelDriversName,
     flag_descriptions::kFilesKernelDriversDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFilesKernelDrivers)},
    {"files-local-image-search", flag_descriptions::kFilesLocalImageSearchName,
     flag_descriptions::kFilesLocalImageSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFilesLocalImageSearch)},
    {"files-materialized-views", flag_descriptions::kFilesMaterializedViewsName,
     flag_descriptions::kFilesMaterializedViewsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFilesMaterializedViews)},
    {"files-single-partition-format",
     flag_descriptions::kFilesSinglePartitionFormatName,
     flag_descriptions::kFilesSinglePartitionFormatDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFilesSinglePartitionFormat)},
    {"files-trash-drive", flag_descriptions::kFilesTrashDriveName,
     flag_descriptions::kFilesTrashDriveDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFilesTrashDrive)},
    {"file-system-provider-cloud-file-system",
     flag_descriptions::kFileSystemProviderCloudFileSystemName,
     flag_descriptions::kFileSystemProviderCloudFileSystemDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(
         chromeos::features::kFileSystemProviderCloudFileSystem)},
    {"file-system-provider-content-cache",
     flag_descriptions::kFileSystemProviderContentCacheName,
     flag_descriptions::kFileSystemProviderContentCacheDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kFileSystemProviderContentCache)},
    {"force-resync-drive", flag_descriptions::kForceReSyncDriveName,
     flag_descriptions::kForceReSyncDriveDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kForceReSyncDrive)},
    {"fuse-box-debug", flag_descriptions::kFuseBoxDebugName,
     flag_descriptions::kFuseBoxDebugDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFuseBoxDebug)},
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
     FEATURE_VALUE_TYPE(ash::features::kEcheSWA)},
    {"eche-launcher", flag_descriptions::kEcheLauncherName,
     flag_descriptions::kEcheLauncherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheLauncher)},
    {"eche-launcher-app-icon-in-more-apps-button",
     flag_descriptions::kEcheLauncherIconsInMoreAppsButtonName,
     flag_descriptions::kEcheLauncherIconsInMoreAppsButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheLauncherIconsInMoreAppsButton)},
    {"eche-launcher-list-view", flag_descriptions::kEcheLauncherListViewName,
     flag_descriptions::kEcheLauncherListViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheLauncherListView)},
    {"eche-swa-check-android-network-info",
     flag_descriptions::kEcheSWACheckAndroidNetworkInfoName,
     flag_descriptions::kEcheSWACheckAndroidNetworkInfoDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheSWACheckAndroidNetworkInfo)},
    {"eche-swa-process-android-accessibility-tree",
     flag_descriptions::kEcheSWAProcessAndroidAccessibilityTreeName,
     flag_descriptions::kEcheSWAProcessAndroidAccessibilityTreeDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kEcheSWAProcessAndroidAccessibilityTree)},
    {"eche-swa-debug-mode", flag_descriptions::kEcheSWADebugModeName,
     flag_descriptions::kEcheSWADebugModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheSWADebugMode)},
    {"eche-swa-disable-stun-server",
     flag_descriptions::kEcheSWADisableStunServerName,
     flag_descriptions::kEcheSWADisableStunServerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheSWADisableStunServer)},
    {"eche-swa-measure-latency", flag_descriptions::kEcheSWAMeasureLatencyName,
     flag_descriptions::kEcheSWAMeasureLatencyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheSWAMeasureLatency)},
    {"eche-swa-send-start-signaling",
     flag_descriptions::kEcheSWASendStartSignalingName,
     flag_descriptions::kEcheSWASendStartSignalingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEcheSWASendStartSignaling)},
    {"print-preview-cros-app", flag_descriptions::kPrintPreviewCrosAppName,
     flag_descriptions::kPrintPreviewCrosAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPrintPreviewCrosApp)},
    {kGlanceablesTimeManagementClassroomStudentViewInternalName,
     flag_descriptions::kGlanceablesTimeManagementClassroomStudentViewName,
     flag_descriptions::
         kGlanceablesTimeManagementClassroomStudentViewDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kGlanceablesTimeManagementClassroomStudentView)},
    {kGlanceablesTimeManagementTasksViewInternalName,
     flag_descriptions::kGlanceablesTimeManagementTasksViewName,
     flag_descriptions::kGlanceablesTimeManagementTasksViewDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGlanceablesTimeManagementTasksView)},
    {"vc-dlc-ui", flag_descriptions::kVcDlcUiName,
     flag_descriptions::kVcDlcUiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVcDlcUi)},
    {"vc-studio-look", flag_descriptions::kVcStudioLookName,
     flag_descriptions::kVcStudioLookDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVcStudioLook)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"android-app-integration", flag_descriptions::kAndroidAppIntegrationName,
     flag_descriptions::kAndroidAppIntegrationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidAppIntegration)},

    {"android-app-integration-with-favicon",
     flag_descriptions::kAndroidAppIntegrationWithFaviconName,
     flag_descriptions::kAndroidAppIntegrationWithFaviconDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidAppIntegrationWithFavicon)},
    {"android-bottom-toolbar", flag_descriptions::kAndroidBottomToolbarName,
     flag_descriptions::kAndroidBottomToolbarDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidBottomToolbar)},

    {"auxiliary-search-donation",
     flag_descriptions::kAuxiliarySearchDonationName,
     flag_descriptions::kAuxiliarySearchDonationDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAuxiliarySearchDonation,
                                    kAuxiliarySearchDonationVariations,
                                    "AuxiliarySearchDonation")},

    {"disable-instance-limit", flag_descriptions::kDisableInstanceLimitName,
     flag_descriptions::kDisableInstanceLimitDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDisableInstanceLimit)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"most-visited-tiles-new-scoring",
     flag_descriptions::kMostVisitedTilesNewScoringName,
     flag_descriptions::kMostVisitedTilesNewScoringDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history::kMostVisitedTilesNewScoring,
                                    kMostVisitedTilesNewScoringVariations,
                                    "MostVisitedTilesNewScoring")},

    {"omnibox-local-history-zero-suggest-beyond-ntp",
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestBeyondNTPName,
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription,
     kOsAll, FEATURE_VALUE_TYPE(omnibox::kLocalHistoryZeroSuggestBeyondNTP)},

    {"omnibox-suggestion-answer-migration",
     flag_descriptions::kOmniboxSuggestionAnswerMigrationName,
     flag_descriptions::kOmniboxSuggestionAnswerMigrationDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox_feature_configs::SuggestionAnswerMigration::
                            kOmniboxSuggestionAnswerMigration)},

    {"omnibox-zero-suggest-prefetch-debouncing",
     flag_descriptions::kOmniboxZeroSuggestPrefetchDebouncingName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchDebouncingDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kZeroSuggestPrefetchDebouncing,
         kOmniboxZeroSuggestPrefetchDebouncingVariations,
         "OmniboxZeroSuggestPrefetchDebouncing")},

    {"omnibox-zero-suggest-prefetching",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetching)},

    {"omnibox-zero-suggest-prefetching-on-srp",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnSRPName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnSRPDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetchingOnSRP)},

    {"omnibox-zero-suggest-prefetching-on-web",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnWebName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnWebDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetchingOnWeb)},

    {"omnibox-zero-suggest-in-memory-caching",
     flag_descriptions::kOmniboxZeroSuggestInMemoryCachingName,
     flag_descriptions::kOmniboxZeroSuggestInMemoryCachingDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestInMemoryCaching)},

    {"omnibox-ml-log-url-scoring-signals",
     flag_descriptions::kOmniboxMlLogUrlScoringSignalsName,
     flag_descriptions::kOmniboxMlLogUrlScoringSignalsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kLogUrlScoringSignals)},
    {"omnibox-ml-url-piecewise-mapped-search-blending",
     flag_descriptions::kOmniboxMlUrlPiecewiseMappedSearchBlendingName,
     flag_descriptions::kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kMlUrlPiecewiseMappedSearchBlending,
         kMlUrlPiecewiseMappedSearchBlendingVariations,
         "MlUrlPiecewiseMappedSearchBlending")},
    {"omnibox-ml-url-score-caching",
     flag_descriptions::kOmniboxMlUrlScoreCachingName,
     flag_descriptions::kOmniboxMlUrlScoreCachingDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kMlUrlScoreCaching)},
    {"omnibox-ml-url-scoring", flag_descriptions::kOmniboxMlUrlScoringName,
     flag_descriptions::kOmniboxMlUrlScoringDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMlUrlScoring,
                                    kOmniboxMlUrlScoringVariations,
                                    "MlUrlScoring")},
    {"omnibox-ml-url-search-blending",
     flag_descriptions::kOmniboxMlUrlSearchBlendingName,
     flag_descriptions::kOmniboxMlUrlSearchBlendingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMlUrlSearchBlending,
                                    kMlUrlSearchBlendingVariations,
                                    "MlUrlScoring")},
    {"omnibox-ml-url-scoring-model",
     flag_descriptions::kOmniboxMlUrlScoringModelName,
     flag_descriptions::kOmniboxMlUrlScoringModelDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kUrlScoringModel,
                                    kUrlScoringModelVariations,
                                    "MlUrlScoring")},

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    {"omnibox-domain-suggestions",
     flag_descriptions::kOmniboxDomainSuggestionsName,
     flag_descriptions::kOmniboxDomainSuggestionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDomainSuggestions)},

    {"omnibox-drive-suggestions",
     flag_descriptions::kOmniboxDriveSuggestionsName,
     flag_descriptions::kOmniboxDriveSuggestionsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kDocumentProvider,
                                    kOmniboxDriveSuggestionsVariations,
                                    "OmniboxDocumentProvider")},
    {"omnibox-drive-suggestions-no-setting",
     flag_descriptions::kOmniboxDriveSuggestionsNoSettingName,
     flag_descriptions::kOmniboxDriveSuggestionsNoSettingDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(omnibox::kDocumentProviderNoSetting)},
    {"omnibox-drive-suggestions-no-sync-requirement",
     flag_descriptions::kOmniboxDriveSuggestionsNoSyncRequirementName,
     flag_descriptions::kOmniboxDriveSuggestionsNoSyncRequirementDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kDocumentProviderNoSyncRequirement)},
    {"omnibox-force-allowed-to-be-default",
     flag_descriptions::kOmniboxForceAllowedToBeDefaultName,
     flag_descriptions::kOmniboxForceAllowedToBeDefaultDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox_feature_configs::ForceAllowedToBeDefault::
                            kForceAllowedToBeDefault)},
    {"omnibox-pref-based-data-collection-consent-helper",
     flag_descriptions::kOmniboxPrefBasedDataCollectionConsentHelperName,
     flag_descriptions::kOmniboxPrefBasedDataCollectionConsentHelperDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kPrefBasedDataCollectionConsentHelper)},
    {"omnibox-shortcut-boost", flag_descriptions::kOmniboxShortcutBoostName,
     flag_descriptions::kOmniboxShortcutBoostDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox_feature_configs::ShortcutBoosting::kShortcutBoost,
         kOmniboxShortcutBoostVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-rich-autocompletion-promising",
     flag_descriptions::kOmniboxRichAutocompletionPromisingName,
     flag_descriptions::kOmniboxRichAutocompletionPromisingDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kOmniboxRichAutocompletionPromisingVariations,
         "OmniboxBundledExperimentV1")},
    {"omnibox-starter-pack-expansion",
     flag_descriptions::kOmniboxStarterPackExpansionName,
     flag_descriptions::kOmniboxStarterPackExpansionDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kStarterPackExpansion,
                                    kOmniboxStarterPackExpansionVariations,
                                    "StarterPackExpansion")},

    {"omnibox-starter-pack-iph", flag_descriptions::kOmniboxStarterPackIPHName,
     flag_descriptions::kOmniboxStarterPackIPHDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kStarterPackIPH)},

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
    {"animate-suggestions-list-appearance",
     flag_descriptions::kAnimateSuggestionsListAppearanceName,
     flag_descriptions::kAnimateSuggestionsListAppearanceDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kAnimateSuggestionsListAppearance)},
    {"omnibox-actions-in-suggest",
     flag_descriptions::kOmniboxActionsInSuggestName,
     flag_descriptions::kOmniboxActionsInSuggestDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kActionsInSuggest,
                                    kOmniboxActionsInSuggestVariants,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-answer-actions", flag_descriptions::kOmniboxAnswerActionsName,
     flag_descriptions::kOmniboxAnswerActionsDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxAnswerActions,
                                    kOmniboxAnswerActionsVariants,
                                    "OmniboxBundledExperimentV1")},

    {"omnibox-asynchronous-view-inflation",
     flag_descriptions::kOmniboxAsyncViewInflationName,
     flag_descriptions::kOmniboxAsyncViewInflationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxAsyncViewInflation)},

#endif  // BUILDFLAG(IS_ANDROID)
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

    {"omnibox-on-device-tail-suggestions",
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kOnDeviceTailModel)},

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

    {"omnibox-grouping-framework-non-zps",
     flag_descriptions::kOmniboxGroupingFrameworkNonZPSName,
     flag_descriptions::kOmniboxGroupingFrameworkDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kGroupingFrameworkForNonZPS)},

    {"omnibox-calc-provider", flag_descriptions::kOmniboxCalcProviderName,
     flag_descriptions::kOmniboxCalcProviderDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox_feature_configs::CalcProvider::kCalcProvider)},

    {"optimization-guide-debug-logs",
     flag_descriptions::kOptimizationGuideDebugLogsName,
     flag_descriptions::kOptimizationGuideDebugLogsDescription, kOsAll,
     SINGLE_VALUE_TYPE(optimization_guide::switches::kDebugLoggingEnabled)},

    {"optimization-guide-model-execution",
     flag_descriptions::kOptimizationGuideModelExecutionName,
     flag_descriptions::kOptimizationGuideModelExecutionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         optimization_guide::features::kOptimizationGuideModelExecution)},

    {"optimization-guide-on-device-model",
     flag_descriptions::kOptimizationGuideOnDeviceModelName,
     flag_descriptions::kOptimizationGuideOnDeviceModelDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         optimization_guide::features::kOptimizationGuideOnDeviceModel,
         kOptimizationGuideOnDeviceModelVariations,
         "OptimizationGuideOnDeviceModel")},

    {"organic-repeatable-queries",
     flag_descriptions::kOrganicRepeatableQueriesName,
     flag_descriptions::kOrganicRepeatableQueriesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history::kOrganicRepeatableQueries,
                                    kOrganicRepeatableQueriesVariations,
                                    "OrganicRepeatableQueries")},

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    {"history-embeddings", flag_descriptions::kHistoryEmbeddingsName,
     flag_descriptions::kHistoryEmbeddingsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history_embeddings::kHistoryEmbeddings,
                                    kHistoryEmbeddingsVariations,
                                    "HistoryEmbeddings")},
    {"history-embeddings-answers",
     flag_descriptions::kHistoryEmbeddingsAnswersName,
     flag_descriptions::kHistoryEmbeddingsAnswersDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(history_embeddings::kHistoryEmbeddingsAnswers)},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

    {"history-journeys", flag_descriptions::kJourneysName,
     flag_descriptions::kJourneysDescription, kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(history_clusters::internal::kJourneys,
                                    kJourneysVariations,
                                    "HistoryJourneys")},

    {"history-journeys-show-all-clusters",
     flag_descriptions::kJourneysShowAllClustersName,
     flag_descriptions::kJourneysShowAllClustersDescription,
     kOsDesktop | kOsAndroid,
     SINGLE_VALUE_TYPE(history_clusters::switches::
                           kShouldShowAllClustersOnProminentUiSurfaces)},

    {"history-journeys-zero-state-filtering",
     flag_descriptions::kJourneysZeroStateFilteringName,
     flag_descriptions::kJourneysZeroStateFilteringDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         history_clusters::internal::kJourneysZeroStateFiltering)},

    {"extract-related-searches-from-prefetched-zps-response",
     flag_descriptions::kExtractRelatedSearchesFromPrefetchedZPSResponseName,
     flag_descriptions::
         kExtractRelatedSearchesFromPrefetchedZPSResponseDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(page_content_annotations::features::
                            kExtractRelatedSearchesFromPrefetchedZPSResponse)},

    {"page-image-service-optimization-guide-salient-images",
     flag_descriptions::kPageImageServiceOptimizationGuideSalientImagesName,
     flag_descriptions::
         kPageImageServiceOptimizationGuideSalientImagesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         page_image_service::kImageServiceOptimizationGuideSalientImages,
         kImageServiceOptimizationGuideSalientImagesVariations,
         "PageImageService")},

    {"page-image-service-suggest-powered-images",
     flag_descriptions::kPageImageServiceSuggestPoweredImagesName,
     flag_descriptions::kPageImageServiceSuggestPoweredImagesDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(page_image_service::kImageServiceSuggestPoweredImages)},

    {"page-content-annotations", flag_descriptions::kPageContentAnnotationsName,
     flag_descriptions::kPageContentAnnotationsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         page_content_annotations::features::kPageContentAnnotations,
         kPageContentAnnotationsVariations,
         "PageContentAnnotations")},

    {"page-content-annotations-persist-salient-image-metadata",
     flag_descriptions::kPageContentAnnotationsPersistSalientImageMetadataName,
     flag_descriptions::
         kPageContentAnnotationsPersistSalientImageMetadataDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::
             kPageContentAnnotationsPersistSalientImageMetadata)},

    {"page-content-annotations-remote-page-metadata",
     flag_descriptions::kPageContentAnnotationsRemotePageMetadataName,
     flag_descriptions::kPageContentAnnotationsRemotePageMetadataDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::kRemotePageMetadata)},

    {"page-visibility-page-content-annotations",
     flag_descriptions::kPageVisibilityPageContentAnnotationsName,
     flag_descriptions::kPageVisibilityPageContentAnnotationsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(page_content_annotations::features::
                            kPageVisibilityPageContentAnnotations)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-remove-stale-policy-pinned-apps-from-shelf",
     flag_descriptions::kEnableRemoveStalePolicyPinnedAppsFromShelfName,
     flag_descriptions::kEnableRemoveStalePolicyPinnedAppsFromShelfDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kRemoveStalePolicyPinnedAppsFromShelf)},
    {"language-packs-in-settings",
     flag_descriptions::kLanguagePacksInSettingsName,
     flag_descriptions::kLanguagePacksInSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLanguagePacksInSettings)},
    {"use-ml-service-for-non-longform-handwriting-on-all-boards",
     flag_descriptions::kUseMlServiceForNonLongformHandwritingOnAllBoardsName,
     flag_descriptions::
         kUseMlServiceForNonLongformHandwritingOnAllBoardsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kUseMlServiceForNonLongformHandwritingOnAllBoards)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"block-insecure-private-network-requests",
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsName,
     flag_descriptions::kBlockInsecurePrivateNetworkRequestsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kBlockInsecurePrivateNetworkRequests)},

    {"private-network-access-respect-preflight-results",
     flag_descriptions::kPrivateNetworkAccessRespectPreflightResultsName,
     flag_descriptions::kPrivateNetworkAccessRespectPreflightResultsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kPrivateNetworkAccessRespectPreflightResults)},

    {"private-network-access-preflight-short-timeout",
     flag_descriptions::kPrivateNetworkAccessPreflightShortTimeoutName,
     flag_descriptions::kPrivateNetworkAccessPreflightShortTimeoutDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         network::features::kPrivateNetworkAccessPreflightShortTimeout)},

    {"private-network-access-permission-prompt",
     flag_descriptions::kPrivateNetworkAccessPermissionPromptName,
     flag_descriptions::kPrivateNetworkAccessPermissionPromptDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         network::features::kPrivateNetworkAccessPermissionPrompt)},

    {"private-network-access-ignore-worker-errors",
     flag_descriptions::kPrivateNetworkAccessIgnoreWorkerErrorsName,
     flag_descriptions::kPrivateNetworkAccessIgnoreWorkerErrorsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kPrivateNetworkAccessForWorkersWarningOnly)},

    {"private-network-access-ignore-navigation-errors",
     flag_descriptions::kPrivateNetworkAccessIgnoreNavigationErrorsName,
     flag_descriptions::kPrivateNetworkAccessIgnoreNavigationErrorsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kPrivateNetworkAccessForNavigationsWarningOnly)},

    {"main-thread-compositing-priority",
     flag_descriptions::kMainThreadCompositingPriorityName,
     flag_descriptions::kMainThreadCompositingPriorityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kMainThreadCompositingPriority)},

    {"mbi-mode", flag_descriptions::kMBIModeName,
     flag_descriptions::kMBIModeDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kMBIMode,
                                    kMBIModeVariations,
                                    "MBIMode")},

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

    {flag_descriptions::kTabGroupsSaveV2Id,
     flag_descriptions::kTabGroupsSaveV2Name,
     flag_descriptions::kTabGroupsSaveV2Description, kOsDesktop,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupsSaveV2)},

    {flag_descriptions::kTabGroupSyncServiceDesktopMigrationId,
     flag_descriptions::kTabGroupSyncServiceDesktopMigrationName,
     flag_descriptions::kTabGroupSyncServiceDesktopMigrationDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupSyncServiceDesktopMigration)},

    {flag_descriptions::kTabGroupsSaveUIUpdateId,
     flag_descriptions::kTabGroupsSaveUIUpdateName,
     flag_descriptions::kTabGroupsSaveUIUpdateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupsSaveUIUpdate)},
#if !BUILDFLAG(IS_ANDROID)
    {flag_descriptions::kScrollableTabStripFlagId,
     flag_descriptions::kScrollableTabStripName,
     flag_descriptions::kScrollableTabStripDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(tabs::kScrollableTabStrip,
                                    kTabScrollingVariations,
                                    "TabScrolling")},
#endif
    {flag_descriptions::kTabScrollingButtonPositionFlagId,
     flag_descriptions::kTabScrollingButtonPositionName,
     flag_descriptions::kTabScrollingButtonPositionDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kTabScrollingButtonPosition,
                                    kTabScrollingButtonPositionVariations,
                                    "TabScrollingButtonPosition")},

#if !BUILDFLAG(IS_ANDROID)
    {flag_descriptions::kScrollableTabStripWithDraggingFlagId,
     flag_descriptions::kScrollableTabStripWithDraggingName,
     flag_descriptions::kScrollableTabStripWithDraggingDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(tabs::kScrollableTabStripWithDragging,
                                    kTabScrollingWithDraggingVariations,
                                    "TabScrollingWithDragging")},

    {flag_descriptions::kTabStripCollectionStorageFlagId,
     flag_descriptions::kTabStripCollectionStorageName,
     flag_descriptions::kTabStripCollectionStorageDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(tabs::kTabStripCollectionStorage)},

    {flag_descriptions::kScrollableTabStripOverflowFlagId,
     flag_descriptions::kScrollableTabStripOverflowName,
     flag_descriptions::kScrollableTabStripOverflowDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(tabs::kScrollableTabStripOverflow,
                                    kScrollableTabStripOverflowVariations,
                                    "ScrollableTabStripOverflow")},

    {"split-tabstrip", flag_descriptions::kSplitTabStripName,
     flag_descriptions::kSplitTabStripDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(tabs::kSplitTabStrip)},
#endif

    {flag_descriptions::kSidePanelJourneysFlagId,
     flag_descriptions::kSidePanelJourneysName,
     flag_descriptions::kSidePanelJourneysDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         history_clusters::kSidePanelJourneys,
         kSidePanelJourneysOpensFromOmniboxVariations,
         "SidePanelJourneys")},

    {flag_descriptions::kSidePanelJourneysQuerylessFlagId,
     flag_descriptions::kSidePanelJourneysQuerylessName,
     flag_descriptions::kSidePanelJourneysQuerylessDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanelJourneysQueryless)},

#if !BUILDFLAG(IS_ANDROID)
    {flag_descriptions::kToolbarPinningFlagId,
     flag_descriptions::kToolbarPinningName,
     flag_descriptions::kToolbarPinningDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kToolbarPinning)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {flag_descriptions::kSidePanelResizingFlagId,
     flag_descriptions::kSidePanelResizingName,
     flag_descriptions::kSidePanelResizingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSidePanelResizing)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"enable-reader-mode-in-cct", flag_descriptions::kReaderModeInCCTName,
     flag_descriptions::kReaderModeInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReaderModeInCCT)},

    {"enable-share-custom-actions-in-cct",
     flag_descriptions::kShareCustomActionsInCCTName,
     flag_descriptions::kShareCustomActionsInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kShareCustomActionsInCCT)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"product-specifications",
     commerce::flag_descriptions::kProductSpecificationsName,
     commerce::flag_descriptions::kProductSpecificationsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kProductSpecifications)},

    {"product-specifications-multi-specifics",
     commerce::flag_descriptions::kProductSpecificationsMultiSpecificsName,
     commerce::flag_descriptions::
         kProductSpecificationsMultiSpecificsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kProductSpecificationsMultiSpecifics)},

    {"compare-confirmation-toast",
     commerce::flag_descriptions::kCompareConfirmationToastName,
     commerce::flag_descriptions::kCompareConfirmationToastDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(commerce::kCompareConfirmationToast)},

    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription,
     kOsAndroid | kOsDesktop, FEATURE_VALUE_TYPE(commerce::kShoppingList)},

    {"local-pdp-detection",
     commerce::flag_descriptions::kCommerceLocalPDPDetectionName,
     commerce::flag_descriptions::kCommerceLocalPDPDetectionDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kCommerceLocalPDPDetection)},

    {"parcel-tracking-test-data",
     commerce::flag_descriptions::kParcelTrackingTestDataName,
     commerce::flag_descriptions::kParcelTrackingTestDataDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kParcelTrackingTestData,
                                    kParcelTrackingTestDataVariations,
                                    "ParcelTrackingTestData")},

    {"price-tracking-subscription-service-locale-key",
     commerce::flag_descriptions::
         kPriceTrackingSubscriptionServiceLocaleKeyName,
     commerce::flag_descriptions::
         kPriceTrackingSubscriptionServiceLocaleKeyDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kPriceTrackingSubscriptionServiceLocaleKey)},

    {"price-tracking-subscription-service-product-version",
     commerce::flag_descriptions::
         kPriceTrackingSubscriptionServiceProductVersionName,
     commerce::flag_descriptions::
         kPriceTrackingSubscriptionServiceProductVersionDescription,
     kOsAndroid | kOsDesktop,
     FEATURE_VALUE_TYPE(
         commerce::kPriceTrackingSubscriptionServiceProductVersion)},

#if BUILDFLAG(IS_ANDROID)
    {"price-change-module", flag_descriptions::kPriceChangeModuleName,
     flag_descriptions::kPriceChangeModuleDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPriceChangeModule)},

    {"track-by-default-mobile",
     commerce::flag_descriptions::kTrackByDefaultOnMobileName,
     commerce::flag_descriptions::kTrackByDefaultOnMobileDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(commerce::kTrackByDefaultOnMobile)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"shopping-icon-color-variant",
     commerce::flag_descriptions::kShoppingIconColorVariantName,
     commerce::flag_descriptions::kShoppingIconColorVariantDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(commerce::kShoppingIconColorVariant)},

    {"price-tracking-icon-colors",
     commerce::flag_descriptions::kPriceTrackingIconColorsDescription,
     commerce::flag_descriptions::kPriceTrackingIconColorsDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(commerce::kPriceTrackingIconColors)},

    {"enable-retail-coupons", flag_descriptions::kRetailCouponsName,
     flag_descriptions::kRetailCouponsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kRetailCoupons)},

    {"ntp-alpha-background-collections",
     flag_descriptions::kNtpAlphaBackgroundCollectionsName,
     flag_descriptions::kNtpAlphaBackgroundCollectionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpAlphaBackgroundCollections)},

    {"ntp-background-image-error-detection",
     flag_descriptions::kNtpBackgroundImageErrorDetectionName,
     flag_descriptions::kNtpBackgroundImageErrorDetectionDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpBackgroundImageErrorDetection)},

    {"ntp-calendar-module", flag_descriptions::kNtpCalendarModuleName,
     flag_descriptions::kNtpCalendarModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpCalendarModule,
                                    kNtpCalendarModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-chrome-cart-module", flag_descriptions::kNtpChromeCartModuleName,
     flag_descriptions::kNtpChromeCartModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpChromeCartModule,
                                    kNtpChromeCartModuleVariations,
                                    "DesktopNtpModules")},

    {"chrome-cart-dom-based-heuristics",
     commerce::flag_descriptions::kChromeCartDomBasedHeuristicsName,
     commerce::flag_descriptions::kChromeCartDomBasedHeuristicsDescription,
     kOsDesktop, FEATURE_VALUE_TYPE(commerce::kChromeCartDomBasedHeuristics)},

    {"ntp-drive-module", flag_descriptions::kNtpDriveModuleName,
     flag_descriptions::kNtpDriveModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpDriveModule,
                                    kNtpDriveModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-drive-module-segmentation",
     flag_descriptions::kNtpDriveModuleSegmentationName,
     flag_descriptions::kNtpDriveModuleSegmentationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpDriveModuleSegmentation)},

    {"ntp-drive-module-show-six-files",
     flag_descriptions::kNtpDriveModuleShowSixFilesName,
     flag_descriptions::kNtpDriveModuleShowSixFilesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpDriveModuleShowSixFiles)},

#if !defined(OFFICIAL_BUILD)
    {"ntp-dummy-modules", flag_descriptions::kNtpDummyModulesName,
     flag_descriptions::kNtpDummyModulesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpDummyModules)},
#endif

    {"ntp-modules-header-icon", flag_descriptions::kNtpModulesHeaderIconName,
     flag_descriptions::kNtpModulesHeaderIconDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesHeaderIcon)},

    {"ntp-wide-modules", flag_descriptions::kNtpWideModulesName,
     flag_descriptions::kNtpWideModulesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpWideModules)},

    {"ntp-middle-slot-promo-dismissal",
     flag_descriptions::kNtpMiddleSlotPromoDismissalName,
     flag_descriptions::kNtpMiddleSlotPromoDismissalDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpMiddleSlotPromoDismissal,
                                    kNtpMiddleSlotPromoDismissalVariations,
                                    "DesktopNtpModules")},

    {"ntp-mobile-promo", flag_descriptions::kNtpMobilePromoName,
     flag_descriptions::kNtpMobilePromoName, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpMobilePromo)},

    {"ntp-modules-drag-and-drop", flag_descriptions::kNtpModulesDragAndDropName,
     flag_descriptions::kNtpModulesDragAndDropDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesDragAndDrop)},

    {"ntp-modules-redesigned", flag_descriptions::kNtpModulesRedesignedName,
     flag_descriptions::kNtpModulesRedesignedDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpModulesRedesigned)},

    {"ntp-most-relevant-tab-resumption-module",
     flag_descriptions::kNtpMostRelevantTabResumptionModuleName,
     flag_descriptions::kNtpMostRelevantTabResumptionModuleDescription,
     kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_features::kNtpMostRelevantTabResumptionModule,
         kNtpMostRelevantTabResumptionModuleVariations,
         "NtpMostRelevantTabResumptionModules")},

    {"ntp-most-relevant-tab-resumption-module-device-icon",
     flag_descriptions::kNtpMostRelevantTabResumptionModuleDeviceIconName,
     flag_descriptions::
         kNtpMostRelevantTabResumptionModuleDeviceIconDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         ntp_features::kNtpMostRelevantTabResumptionModuleDeviceIcon)},

    {"ntp-outlook-calendar-module",
     flag_descriptions::kNtpOutlookCalendarModuleName,
     flag_descriptions::kNtpOutlookCalendarModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpOutlookCalendarModule,
                                    kNtpOutlookCalendarModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-realbox-contextual-and-trending-suggestions",
     flag_descriptions::kNtpRealboxContextualAndTrendingSuggestionsName,
     flag_descriptions::kNtpRealboxContextualAndTrendingSuggestionsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         omnibox_feature_configs::RealboxContextualAndTrendingSuggestions::
             kRealboxContextualAndTrendingSuggestions)},

    {"ntp-realbox-cr23-theming", flag_descriptions::kNtpRealboxCr23ThemingName,
     flag_descriptions::kNtpRealboxCr23ThemingDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kRealboxCr23Theming,
                                    kNtpRealboxCr23ThemingVariations,
                                    "NtpRealboxCr23Theming")},

    {"ntp-realbox-match-searchbox-theme",
     flag_descriptions::kNtpRealboxMatchSearchboxThemeName,
     flag_descriptions::kNtpRealboxMatchSearchboxThemeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealboxMatchSearchboxTheme)},

    {"ntp-realbox-pedals", flag_descriptions::kNtpRealboxPedalsName,
     flag_descriptions::kNtpRealboxPedalsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kNtpRealboxPedals)},

    {"ntp-realbox-use-google-g-icon",
     flag_descriptions::kNtpRealboxUseGoogleGIconName,
     flag_descriptions::kNtpRealboxUseGoogleGIconDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kRealboxUseGoogleGIcon)},

    {"ntp-safe-browsing-module", flag_descriptions::kNtpSafeBrowsingModuleName,
     flag_descriptions::kNtpSafeBrowsingModuleDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_features::kNtpSafeBrowsingModule,
                                    kNtpSafeBrowsingModuleVariations,
                                    "DesktopNtpModules")},

    {"ntp-sharepoint-module", flag_descriptions::kNtpSharepointModuleName,
     flag_descriptions::kNtpSharepointModuleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpSharepointModule)},

    {"ntp-wallpaper-search-button",
     flag_descriptions::kNtpWallpaperSearchButtonName,
     flag_descriptions::kNtpWallpaperSearchButtonDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpWallpaperSearchButton)},

    {"ntp-wallpaper-search-button-animation",
     flag_descriptions::kNtpWallpaperSearchButtonAnimationName,
     flag_descriptions::kNtpWallpaperSearchButtonAnimationDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kNtpWallpaperSearchButtonAnimation)},

    {"shopping-page-types", commerce::flag_descriptions::kShoppingPageTypesName,
     commerce::flag_descriptions::kShoppingPageTypesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(commerce::kShoppingPageTypes)},

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
    {"chrome-wide-echo-cancellation",
     flag_descriptions::kChromeWideEchoCancellationName,
     flag_descriptions::kChromeWideEchoCancellationDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(media::kChromeWideEchoCancellation)},
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, kOsWin,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

    {"enable-pixel-canvas-recording",
     flag_descriptions::kEnablePixelCanvasRecordingName,
     flag_descriptions::kEnablePixelCanvasRecordingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kEnablePixelCanvasRecording)},

    {"enable-parallel-downloading", flag_descriptions::kParallelDownloadingName,
     flag_descriptions::kParallelDownloadingDescription, kOsAll,
     FEATURE_VALUE_TYPE(download::features::kParallelDownloading)},

    {"downloads-migrate-to-jobs-api",
     flag_descriptions::kDownloadsMigrateToJobsAPIName,
     flag_descriptions::kDownloadsMigrateToJobsAPIDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kDownloadsMigrateToJobsAPI)},

    {"download-notification-service-unified-api",
     flag_descriptions::kDownloadNotificationServiceUnifiedAPIName,
     flag_descriptions::kDownloadNotificationServiceUnifiedAPIDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         download::features::kDownloadNotificationServiceUnifiedAPI)},

    {"tab-hover-card-images", flag_descriptions::kTabHoverCardImagesName,
     flag_descriptions::kTabHoverCardImagesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabHoverCardImages)},

#if !BUILDFLAG(IS_ANDROID)
    {flag_descriptions::kTabSearchPositionSettingId,
     flag_descriptions::kTabSearchPositionSettingName,
     flag_descriptions::kTabSearchPositionSettingDescription,
     kOsCrOS | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(tabs::kTabSearchPositionSetting)},
#endif

    {"enable-network-logging-to-file",
     flag_descriptions::kEnableNetworkLoggingToFileName,
     flag_descriptions::kEnableNetworkLoggingToFileDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kLogNetLog)},

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

    {"legacy-tech-report-enable-cookie-issue-reports",
     flag_descriptions::kLegacyTechReportEnableCookieIssueReportsName,
     flag_descriptions::kLegacyTechReportEnableCookieIssueReportsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kLegacyTechReportEnableCookieIssueReports)},

    {
        "zero-copy-tab-capture",
        flag_descriptions::kEnableZeroCopyTabCaptureName,
        flag_descriptions::kEnableZeroCopyTabCaptureDescription,
        kOsMac | kOsWin | kOsCrOS,
        FEATURE_VALUE_TYPE(blink::features::kZeroCopyTabCapture),
    },

#if BUILDFLAG(ENABLE_PDF)
    {"accessible-pdf-form", flag_descriptions::kAccessiblePDFFormName,
     flag_descriptions::kAccessiblePDFFormDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kAccessiblePDFForm)},

    {"pdf-cr23", flag_descriptions::kPdfCr23Name,
     flag_descriptions::kPdfCr23Description, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfCr23)},

    {"pdf-oopif", flag_descriptions::kPdfOopifName,
     flag_descriptions::kPdfOopifDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfOopif)},

    {"pdf-portfolio", flag_descriptions::kPdfPortfolioName,
     flag_descriptions::kPdfPortfolioDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfPortfolio)},

    {"pdf-use-skia-renderer", flag_descriptions::kPdfUseSkiaRendererName,
     flag_descriptions::kPdfUseSkiaRendererDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfUseSkiaRenderer)},

#if BUILDFLAG(ENABLE_PDF_INK2)
    {"pdf-ink2", flag_descriptions::kPdfInk2Name,
     flag_descriptions::kPdfInk2Description, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfInk2)},
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINTING)
#if BUILDFLAG(IS_CHROMEOS)
    {"add-printer-via-printscanmgr",
     flag_descriptions::kAddPrinterViaPrintscanmgrName,
     flag_descriptions::kAddPrinterViaPrintscanmgrDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(printing::features::kAddPrinterViaPrintscanmgr)},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    {"cups-ipp-printing-backend",
     flag_descriptions::kCupsIppPrintingBackendName,
     flag_descriptions::kCupsIppPrintingBackendDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(printing::features::kCupsIppPrintingBackend)},
#endif  // BUILDFLAG(IS_LINUX) ||BUILDFLAG(IS_MAC)

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

    {"windows11-mica-titlebar", flag_descriptions::kWindows11MicaTitlebarName,
     flag_descriptions::kWindows11MicaTitlebarDescription, kOsWin,
     FEATURE_VALUE_TYPE(kWindows11MicaTitlebar)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"enable-nav-bar-matches-tab-android",
     flag_descriptions::kNavBarColorMatchesTabBackgroundName,
     flag_descriptions::kNavBarColorMatchesTabBackgroundDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kNavBarColorMatchesTabBackground)},

    {"enable-feed-position-on-ntp", flag_descriptions::kFeedPositionAndroidName,
     flag_descriptions::kFeedPositionAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kFeedPositionAndroid,
                                    kFeedPositionAndroidVariations,
                                    "FeedPositionAndroid")},

    {"enable-magic-stack-android", flag_descriptions::kMagicStackAndroidName,
     flag_descriptions::kMagicStackAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kMagicStackAndroid,
                                    kMagicStackAndroidVariations,
                                    "MagicStackAndroid")},

    {"enable-educational-tip-module",
     flag_descriptions::kEducationalTipModuleName,
     flag_descriptions::kEducationalTipModuleDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kEducationalTipModule,
                                    kEducationalTipModuleVariations,
                                    "EducationalTipModule")},

    {"maylaunchurl-uses-separate-storage-partition",
     flag_descriptions::kMayLaunchUrlUsesSeparateStoragePartitionName,
     flag_descriptions::kMayLaunchUrlUsesSeparateStoragePartitionDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kMayLaunchUrlUsesSeparateStoragePartition)},

    {"enable-segmentation-platform-android-home-module-ranker-v2",
     flag_descriptions::kSegmentationPlatformAndroidHomeModuleRankerV2Name,
     flag_descriptions::
         kSegmentationPlatformAndroidHomeModuleRankerV2Description,
     kOsAndroid,
     FEATURE_VALUE_TYPE(segmentation_platform::features::
                            kSegmentationPlatformAndroidHomeModuleRankerV2)},

    {"enable-logo-polish", flag_descriptions::kLogoPolishName,
     flag_descriptions::kLogoPolishDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kLogoPolish,
                                    kLogoPolishVariations,
                                    "LogoPolish")},

    {"enable-logo-polish-animation-kill-switch",
     flag_descriptions::kLogoPolishAnimationKillSwitchName,
     flag_descriptions::kLogoPolishAnimationKillSwitchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kLogoPolishAnimationKillSwitch)},

    {"search-in-cct", flag_descriptions::kSearchInCCTName,
     flag_descriptions::kSearchInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSearchInCCT)},

    {"search-in-cct-alternate-tap-handling",
     flag_descriptions::kSearchInCCTAlternateTapHandlingName,
     flag_descriptions::kSearchInCCTAlternateTapHandlingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSearchInCCTAlternateTapHandling)},

    {"enable-search-resumption-module",
     flag_descriptions::kSearchResumptionModuleAndroidName,
     flag_descriptions::kSearchResumptionModuleAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kSearchResumptionModuleAndroid,
         kSearchResumptionModuleAndroidVariations,
         "kSearchResumptionModuleAndroid")},

    {"enable-tab-resumption-module",
     flag_descriptions::kTabResumptionModuleAndroidName,
     flag_descriptions::kTabResumptionModuleAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kTabResumptionModuleAndroid,
         kTabResumptionModuleAndroidVariations,
         "kTabResumptionModuleAndroid")},

    {"enable-tabstate-flatbuffer", flag_descriptions::kTabStateFlatBufferName,
     flag_descriptions::kTabStateFlatBufferDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kTabStateFlatBuffer,
                                    kTabStateFlatBufferVariations,
                                    "TabStateFlatBuffer")},

    {"suppress-toolbar-captures",
     flag_descriptions::kSuppressToolbarCapturesName,
     flag_descriptions::kSuppressToolbarCapturesDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kSuppressToolbarCaptures)},

    {"enable-commerce-price-tracking",
     commerce::flag_descriptions::kCommercePriceTrackingName,
     commerce::flag_descriptions::kCommercePriceTrackingDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         commerce::kCommercePriceTracking,
         commerce::kCommercePriceTrackingAndroidVariations,
         "CommercePriceTracking")},

    {"price-insights", commerce::flag_descriptions::kPriceInsightsName,
     commerce::flag_descriptions::kPriceInsightsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kPriceInsights)},

    {"enable-start-surface-return-time",
     flag_descriptions::kStartSurfaceReturnTimeName,
     flag_descriptions::kStartSurfaceReturnTimeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kStartSurfaceReturnTime,
                                    kStartSurfaceReturnTimeVariations,
                                    "StartSurfaceReturnTime")},
    {"account-reauthentication-recent-time-window",
     flag_descriptions::kAccountReauthenticationRecentTimeWindowName,
     flag_descriptions::kAccountReauthenticationRecentTimeWindowDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kAccountReauthenticationRecentTimeWindow,
         kAccountReauthenticationRecentTimeWindowVariations,
         "AccountReauthenticationRecentTimeWindow")},

    {"tab-drag-drop", flag_descriptions::kTabDragDropName,
     flag_descriptions::kTabDragDropDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabDragDropAndroid)},

    {"enable-tablet-toolbar-reordering",
     flag_descriptions::kTabletToolbarReorderingAndroidName,
     flag_descriptions::kTabletToolbarReorderingAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabletToolbarReordering)},

    {"enable-tab-strip-startup-refactoring",
     flag_descriptions::kTabStripStartupRefactoringName,
     flag_descriptions::kTabStripStartupRefactoringDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabStripStartupRefactoring)},

    {"enable-delay-temp-strip-removal",
     flag_descriptions::kDelayTempStripRemovalName,
     flag_descriptions::kDelayTempStripRemovalDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDelayTempStripRemoval)},

    {"enable-most-visited-tiles-reselect",
     flag_descriptions::kMostVisitedTilesReselectName,
     flag_descriptions::kMostVisitedTilesReselectDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kMostVisitedTilesReselect,
                                    kMostVisitedTilesReselectVariations,
                                    "kMostVisitedTilesReselect")},

    {"toolbar-phone-cleanup", flag_descriptions::kToolbarPhoneCleanupName,
     flag_descriptions::kToolbarPhoneCleanupDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kToolbarPhoneCleanup)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"unsafely-treat-insecure-origin-as-secure",
     flag_descriptions::kTreatInsecureOriginAsSecureName,
     flag_descriptions::kTreatInsecureOriginAsSecureDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(
         network::switches::kUnsafelyTreatInsecureOriginAsSecure,
         "")},

    {"disable-process-reuse", flag_descriptions::kDisableProcessReuse,
     flag_descriptions::kDisableProcessReuseDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kDisableProcessReuse)},

    {"subframe-process-reuse-thresholds",
     flag_descriptions::kSubframeProcessReuseThresholds,
     flag_descriptions::kSubframeProcessReuseThresholdsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kSubframeProcessReuseThresholds,
         kSubframeProcessReuseThresholdsVariations,
         "SubframeProcessReuseThresholds" /* trial name */)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-live-caption-multilang",
     flag_descriptions::kEnableLiveCaptionMultilangName,
     flag_descriptions::kEnableLiveCaptionMultilangDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kLiveCaptionMultiLanguage)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-chromeos-live-translate",
     flag_descriptions::kEnableCrOSLiveTranslateName,
     flag_descriptions::kEnableCrOSLiveTranslateDescription,
     kOsCrOS | kOsLacros, FEATURE_VALUE_TYPE(media::kLiveTranslate)},
    {"enable-chromeos-soda-languages",
     flag_descriptions::kEnableCrOSSodaLanguagesName,
     flag_descriptions::kEnableCrOSSodaLanguagesDescription,
     kOsCrOS | kOsLacros, FEATURE_VALUE_TYPE(speech::kCrosExpandSodaLanguages)},
    {"enable-chromeos-soda-conch",
     flag_descriptions::kEnableCrOSSodaConchLanguagesName,
     flag_descriptions::kEnableCrOSSodaLanguagesDescription,
     kOsCrOS | kOsLacros, FEATURE_VALUE_TYPE(speech::kCrosSodaConchLanguages)},
#endif

    {"read-anything-read-aloud", flag_descriptions::kReadAnythingReadAloudName,
     flag_descriptions::kReadAnythingReadAloudDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingReadAloud)},

    {"read-anything-read-aloud-automatic-word-highlighting",
     flag_descriptions::kReadAnythingReadAloudAutomaticWordHighlightingName,
     flag_descriptions::
         kReadAnythingReadAloudAutomaticWordHighlightingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         features::kReadAnythingReadAloudAutomaticWordHighlighting)},

    {"read-anything-read-aloud-auto-voice-switching",
     flag_descriptions::kReadAloudAutoVoiceSwitchingName,
     flag_descriptions::kReadAloudAutoVoiceSwitchingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAloudAutoVoiceSwitching)},

    {"read-anything-read-aloud-language-pack-downloading",
     flag_descriptions::kReadAloudLanguagePackDownloadingName,
     flag_descriptions::kReadAloudLanguagePackDownloadingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAloudLanguagePackDownloading)},

    {"read-anything-read-aloud-phrase-highlighting",
     flag_descriptions::kReadAnythingReadAloudPhraseHighlightingName,
     flag_descriptions::kReadAnythingReadAloudPhraseHighlightingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingReadAloudPhraseHighlighting)},

    {"read-anything-with-screen2x",
     flag_descriptions::kReadAnythingWithScreen2xName,
     flag_descriptions::kReadAnythingWithScreen2xDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingWithScreen2x)},

    {"read-anything-with-algorithm",
     flag_descriptions::kReadAnythingWithAlgorithmName,
     flag_descriptions::kReadAnythingWithAlgorithmDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingWithAlgorithm)},

    {"read-anything-images-via-algorithm",
     flag_descriptions::kReadAnythingImagesViaAlgorithmName,
     flag_descriptions::kReadAnythingImagesViaAlgorithmDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingImagesViaAlgorithm)},

    {"read-anything-docs-integration",
     flag_descriptions::kReadAnythingDocsIntegrationName,
     flag_descriptions::kReadAnythingDocsIntegrationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingDocsIntegration)},

    {"read-anything-docs-load-more-button",
     flag_descriptions::kReadAnythingDocsLoadMoreButtonName,
     flag_descriptions::kReadAnythingDocsLoadMoreButtonDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kReadAnythingDocsLoadMoreButton)},

    {"support-tool", flag_descriptions::kSupportTool,
     flag_descriptions::kSupportToolDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSupportTool)},

    {"support-tool-screenshot", flag_descriptions::kSupportToolScreenshot,
     flag_descriptions::kSupportToolScreenshotDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSupportToolScreenshot)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"enable-auto-disable-accessibility",
     flag_descriptions::kEnableAutoDisableAccessibilityName,
     flag_descriptions::kEnableAutoDisableAccessibilityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kAutoDisableAccessibility)},

    {"image-descriptions-alternative-routing",
     flag_descriptions::kImageDescriptionsAlternateRoutingName,
     flag_descriptions::kImageDescriptionsAlternateRoutingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kImageDescriptionsAlternateRouting)},

#if BUILDFLAG(IS_ANDROID)
    {"app-specific-history", flag_descriptions::kAppSpecificHistoryName,
     flag_descriptions::kAppSpecificHistoryDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAppSpecificHistory)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-auth-tab", flag_descriptions::kCCTAuthTabName,
     flag_descriptions::kCCTAuthTabDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTAuthTab)},
    {"cct-auth-tab-disable-all-external-intents",
     flag_descriptions::kCCTAuthTabDisableAllExternalIntentsName,
     flag_descriptions::kCCTAuthTabDisableAllExternalIntentsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTAuthTabDisableAllExternalIntents)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-before-unload", flag_descriptions::kCCTBeforeUnloadName,
     flag_descriptions::kCCTBeforeUnloadDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTBeforeUnload)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-ephemeral-mode", flag_descriptions::kCCTEphemeralModeName,
     flag_descriptions::kCCTEphemeralModeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTEphemeralMode)},

#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-incognito-available-to-third-party",
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyName,
     flag_descriptions::kCCTIncognitoAvailableToThirdPartyDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTIncognitoAvailableToThirdParty)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-minimized", flag_descriptions::kCCTMinimizedName,
     flag_descriptions::kCCTMinimizedDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCCTMinimized,
                                    kCCTMinimizedIconVariations,
                                    "CCTMinimizedIconVariations")},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"cct-resizable-for-third-parties",
     flag_descriptions::kCCTResizableForThirdPartiesName,
     flag_descriptions::kCCTResizableForThirdPartiesDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kCCTResizableForThirdParties,
         kCCTResizableThirdPartiesDefaultPolicyVariations,
         "CCTResizableThirdPartiesDefaultPolicy")},
    {"cct-google-bottom-bar", flag_descriptions::kCCTGoogleBottomBarName,
     flag_descriptions::kCCTGoogleBottomBarDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kCCTGoogleBottomBar,
                                    kCCTGoogleBottomBarVariations,
                                    "CCTGoogleBottomBarVariations")},
    {"cct-google-bottom-bar-variant-layouts",
     flag_descriptions::kCCTGoogleBottomBarVariantLayoutsName,
     flag_descriptions::kCCTGoogleBottomBarVariantLayoutsDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kCCTGoogleBottomBarVariantLayouts,
         kCCTGoogleBottomBarVariantLayoutsVariations,
         "CCTGoogleBottomBarVariantLayoutsVariations")},
    {"cct-revamped-branding", flag_descriptions::kCCTRevampedBrandingName,
     flag_descriptions::kCCTRevampedBrandingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTRevampedBranding)},
    {"cct-nested-security-icon", flag_descriptions::kCCTNestedSecurityIconName,
     flag_descriptions::kCCTNestedSecurityIconDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kCCTNestedSecurityIcon)},
#endif

#if BUILDFLAG(IS_CHROMEOS)
    {"allow-dsp-based-aec", flag_descriptions::kCrOSDspBasedAecAllowedName,
     flag_descriptions::kCrOSDspBasedAecAllowedDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSDspBasedAecAllowed)},
    {"allow-dsp-based-ns", flag_descriptions::kCrOSDspBasedNsAllowedName,
     flag_descriptions::kCrOSDspBasedNsAllowedDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSDspBasedNsAllowed)},
    {"allow-dsp-based-agc", flag_descriptions::kCrOSDspBasedAgcAllowedName,
     flag_descriptions::kCrOSDspBasedAgcAllowedDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSDspBasedAgcAllowed)},
    {"enforce-system-aec", flag_descriptions::kCrOSEnforceSystemAecName,
     flag_descriptions::kCrOSEnforceSystemAecDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSEnforceSystemAec)},
    {"enforce-system-aec-agc", flag_descriptions::kCrOSEnforceSystemAecAgcName,
     flag_descriptions::kCrOSEnforceSystemAecAgcDescription,
     kOsCrOS | kOsLacros, FEATURE_VALUE_TYPE(media::kCrOSEnforceSystemAecAgc)},
    {"enforce-system-aec-ns-agc",
     flag_descriptions::kCrOSEnforceSystemAecNsAgcName,
     flag_descriptions::kCrOSEnforceSystemAecNsAgcDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSEnforceSystemAecNsAgc)},
    {"enforce-system-aec-ns", flag_descriptions::kCrOSEnforceSystemAecNsName,
     flag_descriptions::kCrOSEnforceSystemAecNsDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSEnforceSystemAecNs)},
    {"system-voice-isolation-option",
     flag_descriptions::kCrOSSystemVoiceIsolationOptionName,
     flag_descriptions::kCrOSSystemVoiceIsolationOptionDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kCrOSSystemVoiceIsolationOption)},
    {"ignore-ui-gains", flag_descriptions::kIgnoreUiGainsName,
     flag_descriptions::kIgnoreUiGainsDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kIgnoreUiGains)},
    {"show-force-respect-ui-gains-toggle",
     flag_descriptions::kShowForceRespectUiGainsToggleName,
     flag_descriptions::kShowForceRespectUiGainsToggleDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kShowForceRespectUiGainsToggle)},
    {"audio-flexible-loopback-for-system-loopback",
     flag_descriptions::kAudioFlexibleLoopbackForSystemLoopbackName,
     flag_descriptions::kAudioFlexibleLoopbackForSystemLoopbackDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kAudioFlexibleLoopbackForSystemLoopback)},
#endif

    {"drop-input-events-before-first-paint",
     flag_descriptions::kDropInputEventsBeforeFirstPaintName,
     flag_descriptions::kDropInputEventsBeforeFirstPaintDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDropInputEventsBeforeFirstPaint)},

    {"boundary-event-dispatch-tracks-node-removal",
     flag_descriptions::kBoundaryEventDispatchTracksNodeRemovalName,
     flag_descriptions::kBoundaryEventDispatchTracksNodeRemovalDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kBoundaryEventDispatchTracksNodeRemoval)},

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

#if !BUILDFLAG(IS_ANDROID)
    {"hats-webui", flag_descriptions::kHatsWebUIName,
     flag_descriptions::kHatsWebUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHaTSWebUI)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"android-elegant-text-height",
     flag_descriptions::kAndroidElegantTextHeightName,
     flag_descriptions::kAndroidElegantTextHeightDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidElegantTextHeight)},

    {"android-google-sans-text", flag_descriptions::kAndroidGoogleSansTextName,
     flag_descriptions::kAndroidGoogleSansTextDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAndroidGoogleSansText,
                                    kAndroidDefaultFontFamilyVariations,
                                    "AndroidDefaultFontFamilyVariations")},

#endif  // BUILDFLAG(IS_ANDROID)

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
     FEATURE_VALUE_TYPE(ash::assistant::features::kEnableDspHotword)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"disable-quick-answers-v2-translation",
     flag_descriptions::kDisableQuickAnswersV2TranslationName,
     flag_descriptions::kDisableQuickAnswersV2TranslationDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kDisableQuickAnswersV2Translation)},
    {"quick-answers-rich-card", flag_descriptions::kQuickAnswersRichCardName,
     flag_descriptions::kQuickAnswersRichCardDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersRichCard)},
    {"quick-answers-material-next-ui",
     flag_descriptions::kQuickAnswersMaterialNextUIName,
     flag_descriptions::kQuickAnswersMaterialNextUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickAnswersMaterialNextUI)},
#endif  // BUILDFLAG(IS_CHROMEOS)

    {"enable-gamepad-button-axis-events",
     flag_descriptions::kEnableGamepadButtonAxisEventsName,
     flag_descriptions::kEnableGamepadButtonAxisEventsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableGamepadButtonAxisEvents)},

    {"enable-gamepad-multitouch",
     flag_descriptions::kEnableGamepadMultitouchName,
     flag_descriptions::kEnableGamepadMultitouchDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kEnableGamepadMultitouch)},

    {"restrict-gamepad-access", flag_descriptions::kRestrictGamepadAccessName,
     flag_descriptions::kRestrictGamepadAccessDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRestrictGamepadAccess)},

#if !BUILDFLAG(IS_ANDROID)
    {"sharing-desktop-screenshots",
     flag_descriptions::kSharingDesktopScreenshotsName,
     flag_descriptions::kSharingDesktopScreenshotsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(sharing_hub::kDesktopScreenshots)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-labs-overview-desk-navigation",
     flag_descriptions::kOverviewDeskNavigationName,
     flag_descriptions::kOverviewDeskNavigationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOverviewDeskNavigation)},
    {"cros-labs-enable-overview-from-wallpaper",
     flag_descriptions::kEnterOverviewFromWallpaperName,
     flag_descriptions::kEnterOverviewFromWallpaperDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnterOverviewFromWallpaper)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-assistant-stereo-input",
     flag_descriptions::kEnableGoogleAssistantStereoInputName,
     flag_descriptions::kEnableGoogleAssistantStereoInputDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::assistant::features::kEnableStereoAudioInput)},
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
    {"arc-window-predictor", flag_descriptions::kArcWindowPredictorName,
     flag_descriptions::kArcWindowPredictorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(full_restore::features::kArcWindowPredictor)},

    {"use-annotated-account-id", flag_descriptions::kUseAnnotatedAccountIdName,
     flag_descriptions::kUseAnnotatedAccountIdDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUseAnnotatedAccountId)},

    {"use-fake-device-for-media-stream",
     flag_descriptions::kUseFakeDeviceForMediaStreamName,
     flag_descriptions::kUseFakeDeviceForMediaStreamDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kUseFakeDeviceForMediaStream)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

    {"enable-vbr-encode-acceleration",
     flag_descriptions::kChromeOSHWVBREncodingName,
     flag_descriptions::kChromeOSHWVBREncodingDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kChromeOSHWVBREncoding)},
#if defined(ARCH_CPU_ARM_FAMILY)
    {"use-gl-scaling", flag_descriptions::kUseGLForScalingName,
     flag_descriptions::kUseGLForScalingDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kUseGLForScaling)},
    {"prefer-gl-image-processor",
     flag_descriptions::kPreferGLImageProcessorName,
     flag_descriptions::kPreferGLImageProcessorDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kPreferGLImageProcessor)},
    {"prefer-software-mt21", flag_descriptions::kPreferSoftwareMT21Name,
     flag_descriptions::kPreferSoftwareMT21Description, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kPreferSoftwareMT21)},
    {"enable-protected-vulkan-detiling",
     flag_descriptions::kEnableProtectedVulkanDetilingName,
     flag_descriptions::kEnableProtectedVulkanDetilingDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kEnableProtectedVulkanDetiling)},
    {"enable-arm-hwdrm-10bit-overlays",
     flag_descriptions::kEnableArmHwdrm10bitOverlaysName,
     flag_descriptions::kEnableArmHwdrm10bitOverlaysDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kEnableArmHwdrm10bitOverlays)},
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
    {"enable-arm-hwdrm", flag_descriptions::kEnableArmHwdrmName,
     flag_descriptions::kEnableArmHwdrmDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kEnableArmHwdrm)},
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_ANDROID)
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"avif-gainmap-hdr-images", flag_descriptions::kAvifGainmapHdrImagesName,
     flag_descriptions::kAvifGainmapHdrImagesDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAvifGainmapHdrImages)},

    {"crabbyavif", flag_descriptions::kCrabbyAvifName,
     flag_descriptions::kCrabbyAvifDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCrabbyAvif)},

    {"file-handling-icons", flag_descriptions::kFileHandlingIconsName,
     flag_descriptions::kFileHandlingIconsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kFileHandlingIcons)},

    {"file-system-observer", flag_descriptions::kFileSystemObserverName,
     flag_descriptions::kFileSystemObserverDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFileSystemObserver)},

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
    {"allow-cross-device-feature-suite",
     flag_descriptions::kAllowCrossDeviceFeatureSuiteName,
     flag_descriptions::kAllowCrossDeviceFeatureSuiteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAllowCrossDeviceFeatureSuite)},

    {"link-cross-device-internals",
     flag_descriptions::kLinkCrossDeviceInternalsName,
     flag_descriptions::kLinkCrossDeviceInternalsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLinkCrossDeviceInternals)},

    {"allow-scroll-settings", flag_descriptions::kAllowScrollSettingsName,
     flag_descriptions::kAllowScrollSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAllowScrollSettings)},

    {"block-telephony-device-phone-mute",
     flag_descriptions::kBlockTelephonyDevicePhoneMuteName,
     flag_descriptions::kBlockTelephonyDevicePhoneMuteDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kBlockTelephonyDevicePhoneMute)},

    {"enable-fast-ink-for-software-cursor",
     flag_descriptions::kEnableFastInkForSoftwareCursorName,
     flag_descriptions::kEnableFastInkForSoftwareCursorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableFastInkForSoftwareCursor)},

    {"enable-neural-palm-adaptive-hold",
     flag_descriptions::kEnableNeuralPalmAdaptiveHoldName,
     flag_descriptions::kEnableNeuralPalmAdaptiveHoldDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableNeuralPalmAdaptiveHold)},

    {"enable-heatmap-palm-detection",
     flag_descriptions::kEnableHeatmapPalmDetectionName,
     flag_descriptions::kEnableHeatmapPalmDetectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableHeatmapPalmDetection)},

    {"enable-neural-stylus-palm-rejection",
     flag_descriptions::kEnableNeuralStylusPalmRejectionName,
     flag_descriptions::kEnableNeuralStylusPalmRejectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableNeuralPalmDetectionFilter)},

    {"enable-edge-detection", flag_descriptions::kEnableEdgeDetectionName,
     flag_descriptions::kEnableEdgeDetectionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableEdgeDetection)},

    {"enable-fast-touchpad-click",
     flag_descriptions::kEnableFastTouchpadClickName,
     flag_descriptions::kEnableFastTouchpadClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableFastTouchpadClick)},

    {"fast-pair-debug-metadata", flag_descriptions::kFastPairDebugMetadataName,
     flag_descriptions::kFastPairDebugMetadataDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairDebugMetadata)},

    {"fast-pair-devices-bluetooth-settings",
     flag_descriptions::kFastPairDevicesBluetoothSettingsName,
     flag_descriptions::kFastPairDevicesBluetoothSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairDevicesBluetoothSettings)},

    {"fast-pair-handshake-long-term-refactor",
     flag_descriptions::kFastPairHandshakeLongTermRefactorName,
     flag_descriptions::kFastPairHandshakeLongTermRefactorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairHandshakeLongTermRefactor)},

    {"fast-pair-hid", flag_descriptions::kFastPairHIDName,
     flag_descriptions::kFastPairHIDDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairHID)},

    {"fast-pair-keyboards", flag_descriptions::kFastPairKeyboardsName,
     flag_descriptions::kFastPairKeyboardsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairKeyboards)},

    {"fast-pair-pwa-companion", flag_descriptions::kFastPairPwaCompanionName,
     flag_descriptions::kFastPairPwaCompanionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFastPairPwaCompanion)},

    {"nearby-ble-v2", flag_descriptions::kEnableNearbyBleV2Name,
     flag_descriptions::kEnableNearbyBleV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableNearbyBleV2)},

    {"nearby-ble-v2-extended-adv",
     flag_descriptions::kEnableNearbyBleV2ExtendedAdvertisingName,
     flag_descriptions::kEnableNearbyBleV2ExtendedAdvertisingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableNearbyBleV2ExtendedAdvertising)},

    {"nearby-ble-v2-gatt-server",
     flag_descriptions::kEnableNearbyBleV2GattServerName,
     flag_descriptions::kEnableNearbyBleV2GattServerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableNearbyBleV2GattServer)},

    {"nearby-bluetooth-classic-adv",
     flag_descriptions::kEnableNearbyBluetoothClassicAdvertisingName,
     flag_descriptions::kEnableNearbyBluetoothClassicAdvertisingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableNearbyBluetoothClassicAdvertising)},

    {"nearby-mdns", flag_descriptions::kEnableNearbyMdnsName,
     flag_descriptions::kEnableNearbyMdnsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableNearbyMdns)},

    {"nearby-presence", flag_descriptions::kNearbyPresenceName,
     flag_descriptions::kNearbyPresenceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNearbyPresence)},

    {"nearby-webrtc", flag_descriptions::kEnableNearbyWebRtcName,
     flag_descriptions::kEnableNearbyWebRtcDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingWebRtc)},

    {"nearby-wifi-direct", flag_descriptions::kEnableNearbyWifiDirectName,
     flag_descriptions::kEnableNearbyWifiDirectDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingWifiDirect)},

    {"nearby-wifi-lan", flag_descriptions::kEnableNearbyWifiLanName,
     flag_descriptions::kEnableNearbyWifiLanDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kNearbySharingWifiLan)},

    {"pcie-billboard-notification",
     flag_descriptions::kPcieBillboardNotificationName,
     flag_descriptions::kPcieBillboardNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPcieBillboardNotification)},

    {"use-search-click-for-right-click",
     flag_descriptions::kUseSearchClickForRightClickName,
     flag_descriptions::kUseSearchClickForRightClickDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUseSearchClickForRightClick)},

    {"show-metered-toggle", flag_descriptions::kMeteredShowToggleName,
     flag_descriptions::kMeteredShowToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kMeteredShowToggle)},

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
     FEATURE_VALUE_TYPE(ash::features::kEnableHostnameSetting)},

    {"enable-oauth-ipp", flag_descriptions::kEnableOAuthIppName,
     flag_descriptions::kEnableOAuthIppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableOAuthIpp)},

    {"enable-shortcut-customization",
     flag_descriptions::kEnableShortcutCustomizationName,
     flag_descriptions::kEnableShortcutCustomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kShortcutCustomization)},

    {"enable-search-customizable-shortcuts-in-launcher",
     flag_descriptions::kEnableSearchCustomizableShortcutsInLauncherName,
     flag_descriptions::kEnableSearchCustomizableShortcutsInLauncherDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSearchCustomizableShortcutsInLauncher)},

    {"enable-suspend-state-machine",
     flag_descriptions::kEnableSuspendStateMachineName,
     flag_descriptions::kEnableSuspendStateMachineDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSuspendStateMachine)},

    {"enable-input-device-settings-split",
     flag_descriptions::kEnableInputDeviceSettingsSplitName,
     flag_descriptions::kEnableInputDeviceSettingsSplitDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kInputDeviceSettingsSplit)},

    {"enable-peripheral-customization",
     flag_descriptions::kEnablePeripheralCustomizationName,
     flag_descriptions::kEnablePeripheralCustomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPeripheralCustomization)},

    {"enable-peripherals-logging",
     flag_descriptions::kEnablePeripheralsLoggingName,
     flag_descriptions::kEnablePeripheralsLoggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnablePeripheralsLogging)},

    {"enable-peripheral-notification",
     flag_descriptions::kEnablePeripheralNotificationName,
     flag_descriptions::kEnablePeripheralNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPeripheralNotification)},

    {"enable-accessibility-accelerator",
     flag_descriptions::kAccessibilityAcceleratorName,
     flag_descriptions::kAccessibilityAcceleratorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityAccelerator)},

    {"enable-accessibility-caret-blink-interval-setting",
     flag_descriptions::kAccessibilityCaretBlinkIntervalSettingName,
     flag_descriptions::kAccessibilityCaretBlinkIntervalSettingDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityCaretBlinkIntervalSetting)},

    {"enable-accessibility-disable-trackpad",
     flag_descriptions::kAccessibilityDisableTrackpadName,
     flag_descriptions::kAccessibilityDisableTrackpadDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityDisableTrackpad)},

    {"enable-accessibility-flash-screen-feature",
     flag_descriptions::kAccessibilityFlashScreenFeatureName,
     flag_descriptions::kAccessibilityFlashScreenFeatureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityFlashScreenFeature)},

    {"enable-accessibility-magnify-accelerator-dialog",
     flag_descriptions::kAccessibilityMagnifyAcceleratorDialogName,
     flag_descriptions::kAccessibilityMagnifyAcceleratorDialogDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityMagnifyAcceleratorDialog)},

    {"enable-accessibility-overscroll-setting",
     flag_descriptions::kAccessibilityOverscrollSettingFeatureName,
     flag_descriptions::kAccessibilityOverscrollSettingFeatureDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityOverscrollSettingFeature)},

    {"enable-accessibility-shake-to-locate",
     flag_descriptions::kAccessibilityShakeToLocateName,
     flag_descriptions::kAccessibilityShakeToLocateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityShakeToLocate)},

    {"enable-accessibility-service",
     flag_descriptions::kAccessibilityServiceName,
     flag_descriptions::kAccessibilityServiceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityService)},

    {"enable-accessibility-reduced-animations",
     flag_descriptions::kAccessibilityReducedAnimationsName,
     flag_descriptions::kAccessibilityReducedAnimationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityReducedAnimations)},

    {"enable-accessibility-reduced-animations-in-kiosk",
     flag_descriptions::kAccessibilityReducedAnimationsInKioskName,
     flag_descriptions::kAccessibilityReducedAnimationsInKioskDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityReducedAnimationsInKiosk)},

    {"enable-accessibility-facegaze",
     flag_descriptions::kAccessibilityFaceGazeName,
     flag_descriptions::kAccessibilityFaceGazeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityFaceGaze)},

    {"enable-accessibility-facegaze-gravity-wells",
     flag_descriptions::kAccessibilityFaceGazeGravityWellsName,
     flag_descriptions::kAccessibilityFaceGazeGravityWellsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityFaceGazeGravityWells)},

    {"enable-accessibility-magnifier-follows-chromevox",
     flag_descriptions::kAccessibilityMagnifierFollowsChromeVoxName,
     flag_descriptions::kAccessibilityMagnifierFollowsChromeVoxDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityMagnifierFollowsChromeVox)},

    {"enable-accessibility-magnifier-follows-sts",
     flag_descriptions::kAccessibilityMagnifierFollowsStsName,
     flag_descriptions::kAccessibilityMagnifierFollowsStsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityMagnifierFollowsSts)},

    {"enable-accessibility-mousekeys",
     flag_descriptions::kAccessibilityMouseKeysName,
     flag_descriptions::kAccessibilityMouseKeysDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilityMouseKeys)},

    {"enable-accessibility-select-to-speak-shortcut",
     flag_descriptions::kAccessibilitySelectToSpeakShortcutName,
     flag_descriptions::kAccessibilitySelectToSpeakShortcutDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kAccessibilitySelectToSpeakShortcut)},

    {"enable-pip-double-tap-to-resize",
     flag_descriptions::kPipDoubleTapToResizeName,
     flag_descriptions::kPipDoubleTapToResizeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipDoubleTapToResize)},

    {"enable-pip-tuck", flag_descriptions::kPipTuckName,
     flag_descriptions::kPipTuckDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPipTuck)},

    {"event-based-log-upload", flag_descriptions::kEventBasedLogUpload,
     flag_descriptions::kEventBasedLogUploadDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEventBasedLogUpload)},

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-fenced-frames-developer-mode",
     flag_descriptions::kEnableFencedFramesDeveloperModeName,
     flag_descriptions::kEnableFencedFramesDeveloperModeDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFencedFramesDefaultMode)},

    {"enable-unsafe-webgpu", flag_descriptions::kUnsafeWebGPUName,
     flag_descriptions::kUnsafeWebGPUDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableUnsafeWebGPU)},

    {"enable-webgpu-developer-features",
     flag_descriptions::kWebGpuDeveloperFeaturesName,
     flag_descriptions::kWebGpuDeveloperFeaturesDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableWebGPUDeveloperFeatures)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"game-dashboard", flag_descriptions::kGameDashboard,
     flag_descriptions::kGameDashboardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGameDashboard)},

    {"game-dashboard-game-pwas", flag_descriptions::kGameDashboardGamePWAs,
     flag_descriptions::kGameDashboardGamePWAsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGameDashboardGamePWAs)},

    {"game-dashboard-gamepad-support",
     flag_descriptions::kGameDashboardGamepadSupport,
     flag_descriptions::kGameDashboardGamepadSupport, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGameDashboardGamepadSupport)},

    {"game-dashboard-games-in-test",
     flag_descriptions::kGameDashboardGamesInTest,
     flag_descriptions::kGameDashboardGamesInTestDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGameDashboardGamesInTest)},

    {"game-dashboard-utilities", flag_descriptions::kGameDashboardUtilities,
     flag_descriptions::kGameDashboardUtilitiesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGameDashboardUtilities)},

    {"gesture-properties-dbus-service",
     flag_descriptions::kEnableGesturePropertiesDBusServiceName,
     flag_descriptions::kEnableGesturePropertiesDBusServiceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGesturePropertiesDBusService)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS)
    {"global-media-controls-updated-ui",
     flag_descriptions::kGlobalMediaControlsUpdatedUIName,
     flag_descriptions::kGlobalMediaControlsUpdatedUIDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(media::kGlobalMediaControlsUpdatedUI)},
#endif  // !BUILDFLAG(IS_CHROMEOS)

    {"enable-cooperative-scheduling",
     flag_descriptions::kCooperativeSchedulingName,
     flag_descriptions::kCooperativeSchedulingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kCooperativeScheduling)},

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    {"enable-network-service-sandbox",
     flag_descriptions::kEnableNetworkServiceSandboxName,
     flag_descriptions::kEnableNetworkServiceSandboxDescription,
     kOsLinux | kOsLacros | kOsCrOS,
     FEATURE_VALUE_TYPE(sandbox::policy::features::kNetworkServiceSandbox)},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
    {"use-out-of-process-video-decoding",
     flag_descriptions::kUseOutOfProcessVideoDecodingName,
     flag_descriptions::kUseOutOfProcessVideoDecodingDescription,
     kOsLinux | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kUseOutOfProcessVideoDecoding)},
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(USE_V4L2_CODEC)
    {"use-v4l2-flat-stateful-video-decoder",
     flag_descriptions::kV4L2FlatStatefulVideoDecoderName,
     flag_descriptions::kV4L2FlatStatefulVideoDecoderDescription,
     kOsLinux | kOsLacros | kOsCrOS,
     FEATURE_VALUE_TYPE(media::kV4L2FlatStatefulVideoDecoder)},
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    {"enable-family-link-extensions-permissions",
     flag_descriptions::
         kEnableExtensionsPermissionsForSupervisedUsersOnDesktopName,
     flag_descriptions::
         kEnableExtensionsPermissionsForSupervisedUsersOnDesktopDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::
             kEnableExtensionsPermissionsForSupervisedUsersOnDesktop)},

    {"enable-updated-supervised-user-extension-approval-strings",
     flag_descriptions::kUpdatedSupervisedUserExtensionApprovalStringsName,
     flag_descriptions::
         kUpdatedSupervisedUserExtensionApprovalStringsDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kUpdatedSupervisedUserExtensionApprovalStrings)},

    {"enable-supervised-user-new-profile-sign-in-iph",
     flag_descriptions::kEnableSupervisedUserProfileSignInIphName,
     flag_descriptions::kEnableSupervisedUserProfileSignInIphDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHSupervisedUserProfileSigninFeature)},
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"enable-skip-parent-approval-to-install-extensions",
     flag_descriptions::
         kEnableSupervisedUserSkipParentApprovalToInstallExtensionsName,
     flag_descriptions::
         EnableSupervisedUserSkipParentApprovalToInstallExtensionsDescription,
     kOsLinux | kOsMac | kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(
         supervised_user::
             kEnableSupervisedUserSkipParentApprovalToInstallExtensions)},
#endif

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
    {"scalable-iph-debug", flag_descriptions::kScalableIphDebugName,
     flag_descriptions::kScalableIphDebugDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kScalableIphDebug)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"settings-app-notification-settings",
     flag_descriptions::kSettingsAppNotificationSettingsName,
     flag_descriptions::kSettingsAppNotificationSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSettingsAppNotificationSettings)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"use-dns-https-svcb-alpn", flag_descriptions::kUseDnsHttpsSvcbAlpnName,
     flag_descriptions::kUseDnsHttpsSvcbAlpnDescription,
     kOsLinux | kOsMac | kOsWin | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kUseDnsHttpsSvcbAlpn)},

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

    {"back-forward-cache", flag_descriptions::kBackForwardCacheName,
     flag_descriptions::kBackForwardCacheDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kBackForwardCache,
                                    kBackForwardCacheVariations,
                                    "BackForwardCache")},
#if BUILDFLAG(IS_ANDROID)
    {"back-forward-transitions", flag_descriptions::kBackForwardTransitionsName,
     flag_descriptions::kBackForwardTransitionsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kBackForwardTransitions)},
    {"mirror-back-forward-gestures-in-rtl",
     flag_descriptions::kMirrorBackForwardGesturesInRTLName,
     flag_descriptions::kMirrorBackForwardGesturesInRTLDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(ui::kMirrorBackForwardGesturesInRTL)},
#endif

    {"windows-scrolling-personality",
     flag_descriptions::kWindowsScrollingPersonalityName,
     flag_descriptions::kWindowsScrollingPersonalityDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWindowsScrollingPersonality)},

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
    {"elastic-overscroll", flag_descriptions::kElasticOverscrollName,
     flag_descriptions::kElasticOverscrollDescription, kOsWin | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kElasticOverscroll)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"element-capture", flag_descriptions::kElementCaptureName,
     flag_descriptions::kElementCaptureDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kElementCapture)},
#endif

    {"device-posture", flag_descriptions::kDevicePostureName,
     flag_descriptions::kDevicePostureDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kDevicePosture)},

    {"viewport-segments", flag_descriptions::kViewportSegmentsName,
     flag_descriptions::kViewportSegmentsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kViewportSegments)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"device-force-scheduled-reboot",
     flag_descriptions::kDeviceForceScheduledRebootName,
     flag_descriptions::kDeviceForceScheduledRebootDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDeviceForceScheduledReboot)},
    {"enable-assistant-aec", flag_descriptions::kEnableGoogleAssistantAecName,
     flag_descriptions::kEnableGoogleAssistantAecDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::assistant::features::kAssistantAudioEraser)},
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {"enable-location-provider-manager",
     flag_descriptions::kLocationProviderManagerName,
     flag_descriptions::kLocationProviderManagerDescription, kOsMac | kOsWin,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kLocationProviderManager,
                                    kLocationProviderManagerVariations,
                                    "LocationProviderManager")},
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
    {"mute-notification-snooze-action",
     flag_descriptions::kMuteNotificationSnoozeActionName,
     flag_descriptions::kMuteNotificationSnoozeActionDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMuteNotificationSnoozeAction)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"notification-one-tap-unsubscribe",
     flag_descriptions::kNotificationOneTapUnsubscribeName,
     flag_descriptions::kNotificationOneTapUnsubscribeDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kNotificationOneTapUnsubscribe)},
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
     FEATURE_VALUE_TYPE(ash::features::kGamepadVibration)},
    {"exo-ordinal-motion", flag_descriptions::kExoOrdinalMotionName,
     flag_descriptions::kExoOrdinalMotionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kExoOrdinalMotion)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"heavy-ad-privacy-mitigations",
     flag_descriptions::kHeavyAdPrivacyMitigationsName,
     flag_descriptions::kHeavyAdPrivacyMitigationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         heavy_ad_intervention::features::kHeavyAdPrivacyMitigations)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"crostini-container-install",
     flag_descriptions::kCrostiniContainerInstallName,
     flag_descriptions::kCrostiniContainerInstallDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kCrostiniContainerChoices)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"os-settings-app-notifications-page",
     flag_descriptions::kOsSettingsAppNotificationsPageName,
     flag_descriptions::kOsSettingsAppNotificationsPageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kOsSettingsAppNotificationsPage)},
    {"help-app-app-detail-page", flag_descriptions::kHelpAppAppDetailPageName,
     flag_descriptions::kHelpAppAppDetailPageDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHelpAppAppDetailPage)},
    {"help-app-apps-list", flag_descriptions::kHelpAppAppsListName,
     flag_descriptions::kHelpAppAppsListDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHelpAppAppsList)},
    {"help-app-auto-trigger-install-dialog",
     flag_descriptions::kHelpAppAutoTriggerInstallDialogName,
     flag_descriptions::kHelpAppAutoTriggerInstallDialogDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHelpAppAutoTriggerInstallDialog)},
    {"help-app-home-page-app-articles",
     flag_descriptions::kHelpAppHomePageAppArticlesName,
     flag_descriptions::kHelpAppHomePageAppArticlesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHelpAppHomePageAppArticles)},
    {"help-app-launcher-search", flag_descriptions::kHelpAppLauncherSearchName,
     flag_descriptions::kHelpAppLauncherSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHelpAppLauncherSearch)},
    {"help-app-onboarding-revamp",
     flag_descriptions::kHelpAppOnboardingRevampName,
     flag_descriptions::kHelpAppOnboardingRevampDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHelpAppOnboardingRevamp)},
    {"help-app-opens-instead-of-release-notes-notification",
     flag_descriptions::kHelpAppOpensInsteadOfReleaseNotesNotificationName,
     flag_descriptions::
         kHelpAppOpensInsteadOfReleaseNotesNotificationDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kHelpAppOpensInsteadOfReleaseNotesNotification)},
    {"media-app-pdf-mahi", flag_descriptions::kMediaAppPdfMahiName,
     flag_descriptions::kMediaAppPdfMahiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMediaAppPdfMahi)},
    {"on-device-app-controls", flag_descriptions::kOnDeviceAppControlsName,
     flag_descriptions::kOnDeviceAppControlsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kForceOnDeviceAppControlsForAllRegions)},
    {"release-notes-notification-all-channels",
     flag_descriptions::kReleaseNotesNotificationAllChannelsName,
     flag_descriptions::kReleaseNotesNotificationAllChannelsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kReleaseNotesNotificationAllChannels)},
    {"release-notes-notification-always-eligible",
     flag_descriptions::kReleaseNotesNotificationAlwaysEligibleName,
     flag_descriptions::kReleaseNotesNotificationAlwaysEligibleDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kReleaseNotesNotificationAlwaysEligible)},
    {"use-android-staging-smds", flag_descriptions::kUseAndroidStagingSmdsName,
     flag_descriptions::kUseAndroidStagingSmdsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUseAndroidStagingSmds)},
    {"use-stork-smds-server-address",
     flag_descriptions::kUseStorkSmdsServerAddressName,
     flag_descriptions::kUseStorkSmdsServerAddressDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUseStorkSmdsServerAddress)},
    {"use-wallpaper-staging-url",
     flag_descriptions::kUseWallpaperStagingUrlName,
     flag_descriptions::kUseWallpaperStagingUrlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUseWallpaperStagingUrl)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-card-product-name",
     flag_descriptions::kAutofillEnableCardProductNameName,
     flag_descriptions::kAutofillEnableCardProductNameDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardProductName)},

    {"autofill-granular-filling-available",
     flag_descriptions::kAutofillGranularFillingAvailableName,
     flag_descriptions::kAutofillGranularFillingAvailableDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillGranularFillingAvailable,
         kAutofillGranularFillingAvailableVariations,
         "AutofillGranularFillingAndManualFallbackForUnclassifiedFieldsAvailabl"
         "e")},

    {"autofill-for-unclassified-fields-available",
     flag_descriptions::kAutofillForUnclassifiedFieldsAvailableName,
     flag_descriptions::kAutofillForUnclassifiedFieldsAvailableDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillForUnclassifiedFieldsAvailable)},

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)
    {"paint-preview-demo", flag_descriptions::kPaintPreviewDemoName,
     flag_descriptions::kPaintPreviewDemoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(paint_preview::kPaintPreviewDemo)},
#endif  // BUILDFLAG(ENABLE_PAINT_PREVIEW) && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"panel-self-refresh-2", flag_descriptions::kPanelSelfRefresh2Name,
     flag_descriptions::kPanelSelfRefresh2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kPanelSelfRefresh2)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"automatic-fullscreen-content-setting",
     flag_descriptions::kAutomaticFullscreenContentSettingName,
     flag_descriptions::kAutomaticFullscreenContentSettingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kAutomaticFullscreenContentSetting)},

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
    {"run-video-capture-service-in-browser",
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessName,
     flag_descriptions::kRunVideoCaptureServiceInBrowserProcessDescription,
     kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kRunVideoCaptureServiceInBrowserProcess)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
    {"disruptive-notification-permission-revocation",
     flag_descriptions::kDisruptiveNotificationPermissionRevocationName,
     flag_descriptions::kDisruptiveNotificationPermissionRevocationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kDisruptiveNotificationPermissionRevocation)},
    {"double-buffer-compositing",
     flag_descriptions::kDoubleBufferCompositingName,
     flag_descriptions::kDoubleBufferCompositingDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(switches::kDoubleBufferCompositing)},

#if !BUILDFLAG(IS_ANDROID)
    {"page-info-hide-site-settings",
     flag_descriptions::kPageInfoHideSiteSettingsName,
     flag_descriptions::kPageInfoHideSiteSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHideSiteSettings)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"page-info-history-desktop",
     flag_descriptions::kPageInfoHistoryDesktopName,
     flag_descriptions::kPageInfoHistoryDesktopDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kPageInfoHistoryDesktop)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"page-info-about-this-page-persistent-side-panel-entry",
     flag_descriptions::kPageInfoAboutThisPagePersistentEntryName,
     flag_descriptions::kPageInfoAboutThisPagePersistentEntryDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(page_info::kAboutThisSitePersistentSidePanelEntry)},
#endif

    {"tracking-protection-3pcd", flag_descriptions::kTrackingProtection3pcdName,
     flag_descriptions::kTrackingProtection3pcdDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(content_settings::features::kTrackingProtection3pcd)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kClipboardHistoryLongpressInternalName,
     flag_descriptions::kClipboardHistoryLongpressName,
     flag_descriptions::kClipboardHistoryLongpressDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kClipboardHistoryLongpress)},
    {kClipboardHistoryRefreshInternalName,
     flag_descriptions::kClipboardHistoryRefreshName,
     flag_descriptions::kClipboardHistoryRefreshDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kClipboardHistoryRefresh)},
    {kClipboardHistoryUrlTitlesInternalName,
     flag_descriptions::kClipboardHistoryUrlTitlesName,
     flag_descriptions::kClipboardHistoryUrlTitlesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kClipboardHistoryUrlTitles)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
    {"enable-media-foundation-video-capture",
     flag_descriptions::kEnableMediaFoundationVideoCaptureName,
     flag_descriptions::kEnableMediaFoundationVideoCaptureDescription, kOsWin,
     FEATURE_VALUE_TYPE(media::kMediaFoundationVideoCapture)},
#endif  // BUILDFLAG(IS_WIN)
    {"shared-highlighting-manager",
     flag_descriptions::kSharedHighlightingManagerName,
     flag_descriptions::kSharedHighlightingManagerDescription, kOsAll,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingManager)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"reset-shortcut-customizations",
     flag_descriptions::kResetShortcutCustomizationsName,
     flag_descriptions::kResetShortcutCustomizationsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kResetShortcutCustomizations)},
    {"shimless-rma-apro-update-rootfs",
     flag_descriptions::kShimlessRMAAproUpdateRootfsName,
     flag_descriptions::kShimlessRMAAproUpdateRootfsDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootShimlessRMAAproUpdateRootfs")},
    {"shimless-rma-os-update", flag_descriptions::kShimlessRMAOsUpdateName,
     flag_descriptions::kShimlessRMAOsUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShimlessRMAOsUpdate)},
    {"quick-share-v2", flag_descriptions::kQuickShareV2Name,
     flag_descriptions::kQuickShareV2Description, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kQuickShareV2)},
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

    {"canvas-2d-layers", flag_descriptions::kCanvas2DLayersName,
     flag_descriptions::kCanvas2DLayersDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnableCanvas2DLayers)},

    {"web-machine-learning-neural-network",
     flag_descriptions::kWebMachineLearningNeuralNetworkName,
     flag_descriptions::kWebMachineLearningNeuralNetworkDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         webnn::mojom::features::kWebMachineLearningNeuralNetwork)},

    {"experimental-web-machine-learning-neural-network",
     flag_descriptions::kExperimentalWebMachineLearningNeuralNetworkName,
     flag_descriptions::kExperimentalWebMachineLearningNeuralNetworkDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         webnn::mojom::features::kExperimentalWebMachineLearningNeuralNetwork)},

    {"one-time-permission", flag_descriptions::kOneTimePermissionName,
     flag_descriptions::kOneTimePermissionDescription, kOsAll,
     FEATURE_VALUE_TYPE(permissions::features::kOneTimePermission)},

    {"improved-semantics-activity-indicators",
     flag_descriptions::kImprovedSemanticsActivityIndicatorsName,
     flag_descriptions::kImprovedSemanticsActivityIndicatorsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         content_settings::features::kImprovedSemanticsActivityIndicators)},

    {"left-hand-side-activity-indicators",
     flag_descriptions::kLeftHandSideActivityIndicatorsName,
     flag_descriptions::kLeftHandSideActivityIndicatorsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         content_settings::features::kLeftHandSideActivityIndicators)},

#if BUILDFLAG(IS_CHROMEOS)
    {"cros-block-warnings",
     flag_descriptions::kCrosSystemLevelPermissionBlockedWarningsName,
     flag_descriptions::kCrosSystemLevelPermissionBlockedWarningsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(content_settings::features::
                            kCrosSystemLevelPermissionBlockedWarnings)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"productivity-launcher", flag_descriptions::kProductivityLauncherName,
     flag_descriptions::kProductivityLauncherDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kProductivityLauncher,
                                    kProductivityLauncherVariations,
                                    "ProductivityLauncher")},
    {"launcher-continue-section-with-recents",
     flag_descriptions::kLauncherContinueSectionWithRecentsName,
     flag_descriptions::kLauncherContinueSectionWithRecentsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLauncherContinueSectionWithRecents)},
    {"launcher-item-suggest", flag_descriptions::kLauncherItemSuggestName,
     flag_descriptions::kLauncherItemSuggestDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::kLauncherItemSuggest,
                                    kLauncherItemSuggestVariations,
                                    "LauncherItemSuggest")},
    {"eol-incentive", flag_descriptions::kEolIncentiveName,
     flag_descriptions::kEolIncentiveDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kEolIncentive,
                                    kEolIncentiveVariations,
                                    "EolIncentive")},
    {"productivity-launcher-image-search",
     flag_descriptions::kProductivityLauncherImageSearchName,
     flag_descriptions::kProductivityLauncherImageSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProductivityLauncherImageSearch)},
    {kSeaPenInternalName, flag_descriptions::kSeaPenName,
     flag_descriptions::kSeaPenDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSeaPen)},
    {"sea-pen-enterprise", flag_descriptions::kSeaPenEnterpriseName,
     flag_descriptions::kSeaPenEnterpriseDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSeaPenEnterprise)},
    {"shelf-auto-hide-separation",
     flag_descriptions::kShelfAutoHideSeparationName,
     flag_descriptions::kShelfAutoHideSeparationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kShelfAutoHideSeparation)},
    {"launcher-game-search", flag_descriptions::kLauncherGameSearchName,
     flag_descriptions::kLauncherGameSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherGameSearch)},
    {"launcher-keyword-extraction-scoring",
     flag_descriptions::kLauncherKeywordExtractionScoring,
     flag_descriptions::kLauncherKeywordExtractionScoringDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherKeywordExtractionScoring)},
    {"launcher-fuzzy-match-for-omnibox",
     flag_descriptions::kLauncherFuzzyMatchForOmniboxName,
     flag_descriptions::kLauncherFuzzyMatchForOmniboxDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherFuzzyMatchForOmnibox)},
    {"launcher-search-control", flag_descriptions::kLauncherSearchControlName,
     flag_descriptions::kLauncherSearchControlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLauncherSearchControl)},
    {"launcher-nudge-session-reset",
     flag_descriptions::kLauncherNudgeSessionResetName,
     flag_descriptions::kLauncherNudgeSessionResetDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLauncherNudgeSessionReset)},
    {"launcher-system-info-answer-cards",
     flag_descriptions::kLauncherSystemInfoAnswerCardsName,
     flag_descriptions::kLauncherSystemInfoAnswerCardsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherSystemInfoAnswerCards)},
    {"text-in-shelf", flag_descriptions::kTextInShelfName,
     flag_descriptions::kTextInShelfDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHomeButtonWithText)},
    {"launcher-local-image-search",
     flag_descriptions::kLauncherLocalImageSearchName,
     flag_descriptions::kLauncherLocalImageSearchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherImageSearch)},
    {"launcher-local-image-search-on-core",
     flag_descriptions::kLauncherLocalImageSearchOnCoreName,
     flag_descriptions::kLauncherLocalImageSearchOnCoreDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kLocalImageSearchOnCore)},
    {"launcher-local-image-search-confidence",
     flag_descriptions::kLauncherLocalImageSearchConfidenceName,
     flag_descriptions::kLauncherLocalImageSearchConfidenceDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         search_features::kLauncherLocalImageSearchConfidence,
         kLauncherLocalImageSearchConfidenceVariations,
         "LauncherLocalImageSearchConfidence")},
    {"launcher-local-image-search-relevance",
     flag_descriptions::kLauncherLocalImageSearchRelevanceName,
     flag_descriptions::kLauncherLocalImageSearchRelevanceDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         search_features::kLauncherLocalImageSearchRelevance,
         kLauncherLocalImageSearchRelevanceVariations,
         "LauncherLocalImageSearchRelevance")},
    {"launcher-local-image-search-ocr",
     flag_descriptions::kLauncherLocalImageSearchOcrName,
     flag_descriptions::kLauncherLocalImageSearchOcrDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherImageSearchOcr)},
    {"launcher-local-image-search-ica",
     flag_descriptions::kLauncherLocalImageSearchIcaName,
     flag_descriptions::kLauncherLocalImageSearchIcaDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherImageSearchIca)},
    {"launcher-key-shortcut-in-best-match",
     flag_descriptions::kLauncherKeyShortcutInBestMatchName,
     flag_descriptions::kLauncherKeyShortcutInBestMatchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(search_features::kLauncherKeyShortcutInBestMatch)},
    {"quick-app-access-test-ui", flag_descriptions::kQuickAppAccessTestUIName,
     flag_descriptions::kQuickAppAccessTestUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kQuickAppAccessTestUI)},

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"mac-address-randomization",
     flag_descriptions::kMacAddressRandomizationName,
     flag_descriptions::kMacAddressRandomizationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMacAddressRandomization)},

    {"tethering-experimental-functionality",
     flag_descriptions::kTetheringExperimentalFunctionalityName,
     flag_descriptions::kTetheringExperimentalFunctionalityDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTetheringExperimentalFunctionality)},

    {"dynamic-search-update-animation",
     flag_descriptions::kDynamicSearchUpdateAnimationName,
     flag_descriptions::kDynamicSearchUpdateAnimationDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         app_list_features::kDynamicSearchUpdateAnimation,
         kDynamicSearchUpdateAnimationVariations,
         "LauncherDynamicAnimations")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"incognito-reauthentication-for-android",
     flag_descriptions::kIncognitoReauthenticationForAndroidName,
     flag_descriptions::kIncognitoReauthenticationForAndroidDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kIncognitoReauthenticationForAndroid)},
    {"enable-surface-control", flag_descriptions::kAndroidSurfaceControlName,
     flag_descriptions::kAndroidSurfaceControlDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidSurfaceControl)},
    {"smart-suggestion-for-large-downloads",
     flag_descriptions::kSmartSuggestionForLargeDownloadsName,
     flag_descriptions::kSmartSuggestionForLargeDownloadsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(download::features::kSmartSuggestionForLargeDownloads)},
    {"messages-for-android-ads-blocked",
     flag_descriptions::kMessagesForAndroidAdsBlockedName,
     flag_descriptions::kMessagesForAndroidAdsBlockedDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidAdsBlocked)},
    {"messages-for-android-save-card",
     flag_descriptions::kMessagesForAndroidSaveCardName,
     flag_descriptions::kMessagesForAndroidSaveCardDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(messages::kMessagesForAndroidSaveCard)},
    {"quick-delete-for-android", flag_descriptions::kQuickDeleteForAndroidName,
     flag_descriptions::kQuickDeleteForAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kQuickDeleteForAndroid)},

    {"quick-delete-android-followup",
     flag_descriptions::kQuickDeleteAndroidFollowupName,
     flag_descriptions::kQuickDeleteAndroidFollowupDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kQuickDeleteAndroidFollowup)},

    {"quick-delete-android-survey",
     flag_descriptions::kQuickDeleteAndroidSurveyName,
     flag_descriptions::kQuickDeleteAndroidSurveyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kQuickDeleteAndroidSurvey)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"pwa-update-dialog-for-icon",
     flag_descriptions::kPwaUpdateDialogForAppIconName,
     flag_descriptions::kPwaUpdateDialogForAppIconDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kPwaUpdateDialogForIcon)},

#if !BUILDFLAG(IS_ANDROID)
    {"keyboard-and-pointer-lock-prompt",
     flag_descriptions::kKeyboardAndPointerLockPromptName,
     flag_descriptions::kKeyboardAndPointerLockPromptDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(permissions::features::kKeyboardAndPointerLockPrompt)},

    {"press-and-hold-esc-to-exit-browser-fullscreen",
     flag_descriptions::kPressAndHoldEscToExitBrowserFullscreenName,
     flag_descriptions::kPressAndHoldEscToExitBrowserFullscreenDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(features::kPressAndHoldEscToExitBrowserFullscreen)},
#endif

    {"responsive-toolbar", flag_descriptions::kResponsiveToolbarName,
     flag_descriptions::kResponsiveToolbarDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kResponsiveToolbar)},

#if BUILDFLAG(ENABLE_OOP_PRINTING)
    {"enable-oop-print-drivers", flag_descriptions::kEnableOopPrintDriversName,
     flag_descriptions::kEnableOopPrintDriversDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(printing::features::kEnableOopPrintDrivers)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"media-picker-adoption", flag_descriptions::kMediaPickerAdoptionStudyName,
     flag_descriptions::kMediaPickerAdoptionStudyDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         photo_picker::features::kAndroidMediaPickerAdoption,
         kPhotoPickerAdoptionStudyFeatureVariations,
         "MediaPickerAdoption")},
#endif  // BUILDFLAG(IS_ANDROID)

    {"privacy-sandbox-ads-apis",
     flag_descriptions::kPrivacySandboxAdsAPIsOverrideName,
     flag_descriptions::kPrivacySandboxAdsAPIsOverrideDescription, kOsAll,
     SINGLE_VALUE_TYPE(switches::kEnablePrivacySandboxAdsApis)},

    {"privacy-sandbox-internals",
     flag_descriptions::kPrivacySandboxInternalsName,
     flag_descriptions::kPrivacySandboxInternalsDescription, kOsAll,
     FEATURE_VALUE_TYPE(privacy_sandbox::kPrivacySandboxInternalsDevUI)},

    {"privacy-sandbox-privacy-guide-ad-topics",
     flag_descriptions::kPrivacySandboxPrivacyGuideAdTopicsName,
     flag_descriptions::kPrivacySandboxPrivacyGuideAdTopicsDescription, kOsAll,
     FEATURE_VALUE_TYPE(privacy_sandbox::kPrivacySandboxPrivacyGuideAdTopics)},

    {"private-state-tokens-dev-ui",
     flag_descriptions::kPrivateStateTokensDevUIName,
     flag_descriptions::kPrivateStateTokensDevUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(privacy_sandbox::kPrivateStateTokensDevUI)},

    {"related-website-sets-dev-ui",
     flag_descriptions::kRelatedWebsiteSetsDevUIName,
     flag_descriptions::kRelatedWebsiteSetsDevUIDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(privacy_sandbox::kRelatedWebsiteSetsDevUI)},

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
#endif

    {"increment-local-surface-id-for-mainframe-same-doc-navigation",
     flag_descriptions::
         kIncrementLocalSurfaceIdForMainframeSameDocNavigationName,
     flag_descriptions::
         kIncrementLocalSurfaceIdForMainframeSameDocNavigationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         blink::features::
             kIncrementLocalSurfaceIdForMainframeSameDocNavigation)},

    {"enable-speculation-rules-prerendering-target-hint",
     flag_descriptions::kSpeculationRulesPrerenderingTargetHintName,
     flag_descriptions::kSpeculationRulesPrerenderingTargetHintDescription,
     kOsAll, FEATURE_VALUE_TYPE(blink::features::kPrerender2InNewTab)},

    {"search-suggestion-for-prerender2",
     flag_descriptions::kSupportSearchSuggestionForPrerender2Name,
     flag_descriptions::kSupportSearchSuggestionForPrerender2Description,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kSupportSearchSuggestionForPrerender2)},

    {"prerender-early-document-lifecycle-update",
     flag_descriptions::kPrerender2EarlyDocumentLifecycleUpdateName,
     flag_descriptions::kPrerender2EarlyDocumentLifecycleUpdateDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kPrerender2EarlyDocumentLifecycleUpdate)},

    {"warm-up-compositor", flag_descriptions::kWarmUpCompositorName,
     flag_descriptions::kWarmUpCompositorDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kWarmUpCompositor)},

    {"prerender2-warm-up-compositor",
     flag_descriptions::kPrerender2WarmUpCompositorName,
     flag_descriptions::kPrerender2WarmUpCompositorDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kPrerender2WarmUpCompositor,
         kPrerender2WarmUpCompositorTriggerPointVariations,
         "Prerender2WarmUpCompositor")},

#if BUILDFLAG(IS_ANDROID)
    {"prerender2-new-tab-page-android",
     flag_descriptions::kPrerender2ForNewTabPageAndroidName,
     flag_descriptions::kPrerender2ForNewTabPageAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kNewTabPageAndroidTriggerForPrerender2)},
#endif

    {"omnibox-search-prefetch",
     flag_descriptions::kEnableOmniboxSearchPrefetchName,
     flag_descriptions::kEnableOmniboxSearchPrefetchDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kSearchPrefetchServicePrefetching,
                                    kSearchPrefetchServicePrefetchingVariations,
                                    "SearchSuggestionPrefetch")},
    {"omnibox-search-client-prefetch",
     flag_descriptions::kEnableOmniboxClientSearchPrefetchName,
     flag_descriptions::kEnableOmniboxClientSearchPrefetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(kSearchNavigationPrefetch)},

    {"chrome-labs", flag_descriptions::kChromeLabsName,
     flag_descriptions::kChromeLabsDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kChromeLabs,
                                    kChromeLabsVariations,
                                    "ChromeLabs")},

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
    {"pdf-ocr", flag_descriptions::kPdfOcrName,
     flag_descriptions::kPdfOcrDescription,
     kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kPdfOcr)},

    {"pdf-searchify", flag_descriptions::kPdfSearchifyName,
     flag_descriptions::kPdfSearchifyDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfSearchify)},

    {"pdf-xfa-forms", flag_descriptions::kPdfXfaFormsName,
     flag_descriptions::kPdfXfaFormsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(chrome_pdf::features::kPdfXfaSupport)},
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_ANDROID)
    {"send-tab-to-self-v2", flag_descriptions::kSendTabToSelfV2Name,
     flag_descriptions::kSendTabToSelfV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfV2)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"enable-managed-configuration-web-api",
     flag_descriptions::kEnableManagedConfigurationWebApiName,
     flag_descriptions::kEnableManagedConfigurationWebApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(blink::features::kManagedConfiguration)},

    {"enable-system-entropy",
     flag_descriptions::kEnableSystemEntropyOnPerformanceNavigationTimingName,
     flag_descriptions::
         kEnableSystemEntropyOnPerformanceNavigationTimingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPerformanceNavigateSystemEntropy)},

    {"clear-cross-site-cross-browsing-context-group-window-name",
     flag_descriptions::kClearCrossSiteCrossBrowsingContextGroupWindowNameName,
     flag_descriptions::
         kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         features::kClearCrossSiteCrossBrowsingContextGroupWindowName)},

#if BUILDFLAG(IS_CHROMEOS)
    {kTaskManagerEndProcessDisabledForExtensionInternalName,
     flag_descriptions::kTaskManagerEndProcessDisabledForExtensionName,
     flag_descriptions::kTaskManagerEndProcessDisabledForExtensionDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsRunOnOsLogin)},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kWallpaperFastRefreshInternalName,
     flag_descriptions::kWallpaperFastRefreshName,
     flag_descriptions::kWallpaperFastRefreshDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperFastRefresh)},
    {kWallpaperGooglePhotosSharedAlbumsInternalName,
     flag_descriptions::kWallpaperGooglePhotosSharedAlbumsName,
     flag_descriptions::kWallpaperGooglePhotosSharedAlbumsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWallpaperGooglePhotosSharedAlbums)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    {"enable-get-all-screens-media", flag_descriptions::kGetAllScreensMediaName,
     flag_descriptions::kGetAllScreensMediaDescription,
     kOsCrOS | kOsLacros | kOsLinux,
     FEATURE_VALUE_TYPE(blink::features::kGetAllScreensMedia)},
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
    {"enable-run-on-os-login", flag_descriptions::kRunOnOsLoginName,
     flag_descriptions::kRunOnOsLoginDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsRunOnOsLogin)},
    {"enable-prevent-close", flag_descriptions::kPreventCloseName,
     flag_descriptions::kPreventCloseDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kDesktopPWAsPreventClose)},

    {"enable-cloud-identifiers",
     flag_descriptions::kFileSystemAccessGetCloudIdentifiersName,
     flag_descriptions::kFileSystemAccessGetCloudIdentifiersDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(blink::features::kFileSystemAccessGetCloudIdentifiers)},

    {"lacros-color-management", flag_descriptions::kLacrosColorManagementName,
     flag_descriptions::kLacrosColorManagementDescription, kOsLacros,
     FEATURE_VALUE_TYPE(features::kLacrosColorManagement)},
#endif  // BUILDFLAG(IS_CHROMEOS)

    {"enable-global-vaapi-lock", flag_descriptions::kGlobalVaapiLockName,
     flag_descriptions::kGlobalVaapiLockDescription,
     kOsCrOS | kOsLinux | kOsLacros,
     FEATURE_VALUE_TYPE(media::kGlobalVaapiLock)},

#if BUILDFLAG(IS_WIN) ||                                      \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_MAC)
    {
        "ui-debug-tools",
        flag_descriptions::kUIDebugToolsName,
        flag_descriptions::kUIDebugToolsDescription,
        kOsWin | kOsLinux | kOsLacros | kOsMac,
        FEATURE_VALUE_TYPE(features::kUIDebugTools),
    },

#endif
    {"http-cache-partitioning",
     flag_descriptions::kSplitCacheByNetworkIsolationKeyName,
     flag_descriptions::kSplitCacheByNetworkIsolationKeyDescription,
     kOsWin | kOsLinux | kOsLacros | kOsMac | kOsCrOS | kOsAndroid,
     FEATURE_VALUE_TYPE(net::features::kSplitCacheByNetworkIsolationKey)},

#if BUILDFLAG(IS_ANDROID)
    {"optimization-guide-personalized-fetching",
     flag_descriptions::kOptimizationGuidePersonalizedFetchingName,
     flag_descriptions::kOptimizationGuidePersonalizedFetchingDescription,
     kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         optimization_guide::features::kOptimizationGuidePersonalizedFetching,
         kOptimizationGuidePersonalizedFetchingAllowPageInsightsVariations,
         "OptimizationGuidePersonalizedFetchingAllowPageInsights")},
    {"optimization-guide-push-notifications",
     flag_descriptions::kOptimizationGuidePushNotificationName,
     flag_descriptions::kOptimizationGuidePushNotificationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(optimization_guide::features::kPushNotifications)},
#endif

    {"fedcm-authz", flag_descriptions::kFedCmAuthzName,
     flag_descriptions::kFedCmAuthzDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFedCmAuthz)},

    {"fedcm-button-mode", flag_descriptions::kFedCmButtonModeName,
     flag_descriptions::kFedCmButtonModeDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFedCmButtonMode)},

    {"fedcm-idp-registration", flag_descriptions::kFedCmIdPRegistrationName,
     flag_descriptions::kFedCmIdPRegistrationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kFedCmIdPRegistration)},

    {"fedcm-metrics-endpoint", flag_descriptions::kFedCmMetricsEndpointName,
     flag_descriptions::kFedCmMetricsEndpointDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFedCmMetricsEndpoint)},

    {"fedcm-multi-idp", flag_descriptions::kFedCmMultiIdpName,
     flag_descriptions::kFedCmMultiIdpDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kFedCmMultipleIdentityProviders)},

    {"fedcm-selective-disclosure",
     flag_descriptions::kFedCmSelectiveDisclosureName,
     flag_descriptions::kFedCmSelectiveDisclosureDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFedCmSelectiveDisclosure)},

    {"fedcm-use-other-account", flag_descriptions::kFedCmUseOtherAccountName,
     flag_descriptions::kFedCmUseOtherAccountDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFedCmUseOtherAccount)},

    {"fedcm-with-storage-access-api",
     flag_descriptions::kFedCmWithStorageAccessAPIName,
     flag_descriptions::kFedCmWithStorageAccessAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kFedCmWithStorageAccessAPI)},

    {"fedcm-without-well-known-enforcement",
     flag_descriptions::kFedCmWithoutWellKnownEnforcementName,
     flag_descriptions::kFedCmWithoutWellKnownEnforcementDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kFedCmWithoutWellKnownEnforcement)},

    {"web-identity-digital-credentials",
     flag_descriptions::kWebIdentityDigitalCredentialsName,
     flag_descriptions::kWebIdentityDigitalCredentialsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kWebIdentityDigitalCredentials,
         kWebIdentityDigitalIdentityCredentialVariations,
         "WebIdentityDigitalCredentials")},

    {"sanitizer-api", flag_descriptions::kSanitizerApiName,
     flag_descriptions::kSanitizerApiDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kSanitizerAPI)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-input-event-logging",
     flag_descriptions::kEnableInputEventLoggingName,
     flag_descriptions::kEnableInputEventLoggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableInputEventLogging)},
#endif

    {"autofill-enable-manual-fallback-iph",
     flag_descriptions::kAutofillEnableManualFallbackIPHName,
     flag_descriptions::kAutofillEnableManualFallbackIPHDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableManualFallbackIPH)},

    {flag_descriptions::kEnableLensStandaloneFlagId,
     flag_descriptions::kEnableLensStandaloneName,
     flag_descriptions::kEnableLensStandaloneDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(lens::features::kLensStandalone)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-lens-overlay", flag_descriptions::kLensOverlayName,
     flag_descriptions::kLensOverlayDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(lens::features::kLensOverlay,
                                    kLensOverlayVariations,
                                    "LensOverlay")},
#endif

    {"enable-lens-image-translate", flag_descriptions::kLensImageTranslateName,
     flag_descriptions::kLensImageTranslateDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(lens::features::kEnableImageTranslate)},

#if BUILDFLAG(IS_ANDROID)
    {"biometric-reauth-password-filling",
     flag_descriptions::kBiometricReauthForPasswordFillingName,
     flag_descriptions::kBiometricReauthForPasswordFillingDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kBiometricTouchToFill)},
#endif

    {"bind-cookies-to-port", flag_descriptions::kBindCookiesToPortName,
     flag_descriptions::kBindCookiesToPortDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnablePortBoundCookies)},

    {"bind-cookies-to-scheme", flag_descriptions::kBindCookiesToSchemeName,
     flag_descriptions::kBindCookiesToSchemeDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kEnableSchemeBoundCookies)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-keyboard-backlight-control-in-settings",
     flag_descriptions::kEnableKeyboardBacklightControlInSettingsName,
     flag_descriptions::kEnableKeyboardBacklightControlInSettingsDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kEnableKeyboardBacklightControlInSettings)},
    {"enable-keyboard-backlight-toggle",
     flag_descriptions::kEnableKeyboardBacklightToggleName,
     flag_descriptions::kEnableKeyboardBacklightToggleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableKeyboardBacklightToggle)},
    {"enable-keyboard-rewriter-fix",
     flag_descriptions::kEnableKeyboardRewriterFixName,
     flag_descriptions::kEnableKeyboardRewriterFixDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableKeyboardRewriterFix)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"align-wakeups", flag_descriptions::kAlignWakeUpsName,
     flag_descriptions::kAlignWakeUpsDescription, kOsAll,
     FEATURE_VALUE_TYPE(base::kAlignWakeUps)},

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
    {"use-passthrough-command-decoder",
     flag_descriptions::kUsePassthroughCommandDecoderName,
     flag_descriptions::kUsePassthroughCommandDecoderDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDefaultPassthroughCommandDecoder)},
#endif  // BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"focus-follows-cursor", flag_descriptions::kFocusFollowsCursorName,
     flag_descriptions::kFocusFollowsCursorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(::features::kFocusFollowsCursor)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"print-preview-cros-primary",
     flag_descriptions::kPrintPreviewCrosPrimaryName,
     flag_descriptions::kPrintPreviewCrosPrimaryDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(features::kPrintPreviewCrosPrimary)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"cbd-timeframe-required", flag_descriptions::kCbdTimeframeRequiredName,
     flag_descriptions::kCbdTimeframeRequiredDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCbdTimeframeRequired)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"privacy-guide-android-3", flag_descriptions::kPrivacyGuideAndroid3Name,
     flag_descriptions::kPrivacyGuideAndroid3Description, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacyGuideAndroid3)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"privacy-guide-preload-android",
     flag_descriptions::kPrivacyGuidePreloadAndroidName,
     flag_descriptions::kPrivacyGuidePreloadAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kPrivacyGuidePreloadAndroid)},
#endif

    {"prerender2", flag_descriptions::kPrerender2Name,
     flag_descriptions::kPrerender2Description, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kPrerender2)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-phone-hub-call-notification",
     flag_descriptions::kPhoneHubCallNotificationName,
     flag_descriptions::kPhoneHubCallNotificationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPhoneHubCallNotification)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"test-third-party-cookie-phaseout",
     flag_descriptions::kTestThirdPartyCookiePhaseoutName,
     flag_descriptions::kTestThirdPartyCookiePhaseoutDescription, kOsAll,
     SINGLE_VALUE_TYPE(network::switches::kTestThirdPartyCookiePhaseout)},

    {"tpc-phase-out-facilitated-testing",
     flag_descriptions::kTPCPhaseOutFacilitatedTestingName,
     flag_descriptions::kTPCPhaseOutFacilitatedTestingDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         features::kCookieDeprecationFacilitatedTesting,
         kTPCPhaseOutFacilitatedTestingVariations,
         "TPCPhaseOutFacilitatedTesting")},

    {"tpcd-heuristics-grants", flag_descriptions::kTpcdHeuristicsGrantsName,
     flag_descriptions::kTpcdHeuristicsGrantsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         content_settings::features::kTpcdHeuristicsGrants,
         kTpcdHeuristicsGrantsVariations,
         "TpcdHeuristicsGrants")},

    {"tpcd-metadata-grants", flag_descriptions::kTpcdMetadataGrantsName,
     flag_descriptions::kTpcdMetadataGrantsDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTpcdMetadataGrants)},

    {"third-party-cookie-deprecation-trial",
     flag_descriptions::kTpcdTrialSettingsName,
     flag_descriptions::kTpcdTrialSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTpcdTrialSettings)},

    {"top-level-third-party-cookie-deprecation-trial",
     flag_descriptions::kTopLevelTpcdTrialSettingsName,
     flag_descriptions::kTopLevelTpcdTrialSettingsDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kTopLevelTpcdTrialSettings)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kBackgroundListeningName, flag_descriptions::kBackgroundListeningName,
     flag_descriptions::kBackgroundListeningDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media::kBackgroundListening)},
    {kBorealisBigGlInternalName, flag_descriptions::kBorealisBigGlName,
     flag_descriptions::kBorealisBigGlDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisBigGl)},
    {kBorealisDGPUInternalName, flag_descriptions::kBorealisDGPUName,
     flag_descriptions::kBorealisDGPUDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisDGPU)},
    {kBorealisEnableUnsupportedHardwareInternalName,
     flag_descriptions::kBorealisEnableUnsupportedHardwareName,
     flag_descriptions::kBorealisEnableUnsupportedHardwareDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisEnableUnsupportedHardware)},
    {kBorealisForceBetaClientInternalName,
     flag_descriptions::kBorealisForceBetaClientName,
     flag_descriptions::kBorealisForceBetaClientDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisForceBetaClient)},
    {kBorealisForceDoubleScaleInternalName,
     flag_descriptions::kBorealisForceDoubleScaleName,
     flag_descriptions::kBorealisForceDoubleScaleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisForceDoubleScale)},
    {kBorealisLinuxModeInternalName, flag_descriptions::kBorealisLinuxModeName,
     flag_descriptions::kBorealisLinuxModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisLinuxMode)},
    {kBorealisPermittedInternalName, flag_descriptions::kBorealisPermittedName,
     flag_descriptions::kBorealisPermittedDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisPermitted)},
    {kBorealisProvisionInternalName, flag_descriptions::kBorealisProvisionName,
     flag_descriptions::kBorealisProvisionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisProvision)},
    {kBorealisScaleClientByDPIInternalName,
     flag_descriptions::kBorealisScaleClientByDPIName,
     flag_descriptions::kBorealisScaleClientByDPIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBorealisScaleClientByDPI)},
    {kBorealisZinkGlDriverInternalName,
     flag_descriptions::kBorealisZinkGlDriverName,
     flag_descriptions::kBorealisZinkGlDriverDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kBorealisZinkGlDriver,
                                    kBorealisZinkGlDriverVariations,
                                    "BorealisZinkGlDriver")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"https-first-balanced-mode",
     flag_descriptions::kHttpsFirstBalancedModeName,
     flag_descriptions::kHttpsFirstBalancedModeDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsFirstBalancedMode)},

    {"https-first-dialog-ui", flag_descriptions::kHttpsFirstDialogUiName,
     flag_descriptions::kHttpsFirstDialogUiDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kHttpsFirstDialogUi)},

    {"https-first-mode-interstitial-august2024-refresh",
     flag_descriptions::kHttpsFirstModeInterstitialAugust2024RefreshName,
     flag_descriptions::kHttpsFirstModeInterstitialAugust2024RefreshDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         features::kHttpsFirstModeInterstitialAugust2024Refresh)},

    {"https-first-mode-v2-for-engaged-sites",
     flag_descriptions::kHttpsFirstModeV2ForEngagedSitesName,
     flag_descriptions::kHttpsFirstModeV2ForEngagedSitesDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsFirstModeV2ForEngagedSites)},

    {"https-upgrades", flag_descriptions::kHttpsUpgradesName,
     flag_descriptions::kHttpsUpgradesDescription, kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsUpgrades)},

    {"https-first-mode-incognito",
     flag_descriptions::kHttpsFirstModeIncognitoName,
     flag_descriptions::kHttpsFirstModeIncognitoDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsFirstModeIncognito)},

    {"https-first-mode-incognito-new-settings",
     flag_descriptions::kHttpsFirstModeIncognitoNewSettingsName,
     flag_descriptions::kHttpsFirstModeIncognitoNewSettingsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsFirstModeIncognitoNewSettings)},

    {"https-first-mode-for-typically-secure-users",
     flag_descriptions::kHttpsFirstModeForTypicallySecureUsersName,
     flag_descriptions::kHttpsFirstModeForTypicallySecureUsersDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kHttpsFirstModeV2ForTypicallySecureUsers)},

#if BUILDFLAG(IS_ANDROID)
    {"omnibox-2023-refresh-connection-security-indicators",
     flag_descriptions::kOmnibox2023RefreshConnectionSecurityIndicatorsName,
     flag_descriptions::
         kOmnibox2023RefreshConnectionSecurityIndicatorsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kUpdatedConnectionSecurityIndicators)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"enable-drdc", flag_descriptions::kEnableDrDcName,
     flag_descriptions::kEnableDrDcDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kEnableDrDc)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"traffic-counters", flag_descriptions::kTrafficCountersEnabledName,
     flag_descriptions::kTrafficCountersEnabledDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTrafficCountersEnabled)},

    {"traffic-counters-for-wifi-testing",
     flag_descriptions::kTrafficCountersForWiFiTestingName,
     flag_descriptions::kTrafficCountersForWiFiTestingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTrafficCountersForWiFiTesting)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {kExtensionAiDataInternalName,
     flag_descriptions::kExtensionAiDataCollectionName,
     flag_descriptions::kExtensionAiDataCollectionDescription, kOsDesktop,
     SINGLE_VALUE_TYPE(switches::kExtensionAiDataCollection)},

    {"extensions-menu-access-control",
     flag_descriptions::kExtensionsMenuAccessControlName,
     flag_descriptions::kExtensionsMenuAccessControlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(extensions_features::kExtensionsMenuAccessControl)},

    {"extensions-toolbar-zero-state-variation",
     flag_descriptions::kExtensionsToolbarZeroStateName,
     flag_descriptions::kExtensionsToolbarZeroStateDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kExtensionsToolbarZeroStateChoices)},

    {"iph-extensions-menu-feature",
     flag_descriptions::kIPHExtensionsMenuFeatureName,
     flag_descriptions::kIPHExtensionsMenuFeatureDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHExtensionsMenuFeature)},

    {"iph-extensions-request-access-button-feature",
     flag_descriptions::kIPHExtensionsRequestAccessButtonFeatureName,
     flag_descriptions::kIPHExtensionsRequestAccessButtonFeatureDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHExtensionsRequestAccessButtonFeature)},

    {"extension-manifest-v2-deprecation-warning",
     flag_descriptions::kExtensionManifestV2DeprecationWarningName,
     flag_descriptions::kExtensionManifestV2DeprecationWarningDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         extensions_features::kExtensionManifestV2DeprecationWarning)},

    {"extension-manifest-v2-deprecation-disabled",
     flag_descriptions::kExtensionManifestV2DeprecationDisabledName,
     flag_descriptions::kExtensionManifestV2DeprecationDisabledDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(extensions_features::kExtensionManifestV2Disabled)},

    {"extension-manifest-v2-deprecation-unsupported",
     flag_descriptions::kExtensionManifestV2DeprecationUnsupportedName,
     flag_descriptions::kExtensionManifestV2DeprecationUnsupportedDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(extensions_features::kExtensionManifestV2Unsupported)},

#if BUILDFLAG(IS_CHROMEOS)
    {"extension-web-file-handlers",
     flag_descriptions::kExtensionWebFileHandlersName,
     flag_descriptions::kExtensionWebFileHandlersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(extensions_features::kExtensionWebFileHandlers)},
#endif  // IS_CHROMEOS
#if BUILDFLAG(IS_WIN)
    {"launch-windows-native-hosts-directly",
     flag_descriptions::kLaunchWindowsNativeHostsDirectlyName,
     flag_descriptions::kLaunchWindowsNativeHostsDirectlyDescription, kOsWin,
     FEATURE_VALUE_TYPE(
         extensions_features::kLaunchWindowsNativeHostsDirectly)},
#endif  // IS_WIN
#endif  // ENABLE_EXTENSIONS

#if !BUILDFLAG(IS_ANDROID)
    {"canvas-oop-rasterization", flag_descriptions::kCanvasOopRasterizationName,
     flag_descriptions::kCanvasOopRasterizationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCanvasOopRasterization)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"captured-surface-control", flag_descriptions::kCapturedSurfaceControlName,
     flag_descriptions::kCapturedSurfaceControlDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kCapturedSurfaceControl)},
#endif

    {"skia-graphite", flag_descriptions::kSkiaGraphiteName,
     flag_descriptions::kSkiaGraphiteDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSkiaGraphite)},

    {"enable-tab-audio-muting", flag_descriptions::kTabAudioMutingName,
     flag_descriptions::kTabAudioMutingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(media::kEnableTabMuting)},

#if defined(TOOLKIT_VIEWS)
    {"side-search", flag_descriptions::kSideSearchName,
     flag_descriptions::kSideSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSideSearch)},

    {"search-web-in-side-panel", flag_descriptions::kSearchWebInSidePanelName,
     flag_descriptions::kSearchWebInSidePanelDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSearchWebInSidePanel)},
#endif  // defined(TOOLKIT_VIEWS)

#if !BUILDFLAG(IS_ANDROID)
    {"customize-chrome-side-panel-extensions-card",
     flag_descriptions::kCustomizeChromeSidePanelExtensionsCardName,
     flag_descriptions::kCustomizeChromeSidePanelExtensionsCardDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCustomizeChromeSidePanelExtensionsCard)},

    {"customize-chrome-wallpaper-search",
     flag_descriptions::kCustomizeChromeWallpaperSearchName,
     flag_descriptions::kCustomizeChromeWallpaperSearchDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCustomizeChromeWallpaperSearch)},

    {"customize-chrome-wallpaper-search-button",
     flag_descriptions::kCustomizeChromeWallpaperSearchButtonName,
     flag_descriptions::kCustomizeChromeWallpaperSearchButtonDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(ntp_features::kCustomizeChromeWallpaperSearchButton)},

    {"customize-chrome-wallpaper-search-inspiration-card",
     flag_descriptions::kCustomizeChromeWallpaperSearchInspirationCardName,
     flag_descriptions::
         kCustomizeChromeWallpaperSearchInspirationCardDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         ntp_features::kCustomizeChromeWallpaperSearchInspirationCard)},

    {"wallpaper-search-settings-visibility",
     flag_descriptions::kWallpaperSearchSettingsVisibilityName,
     flag_descriptions::kWallpaperSearchSettingsVisibilityDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(optimization_guide::features::internal::
                            kWallpaperSearchSettingsVisibility)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-component-updater-test-request",
     flag_descriptions::kComponentUpdaterTestRequestName,
     flag_descriptions::kComponentUpdaterTestRequestDescription, kOsCrOS,
     SINGLE_VALUE_TYPE_AND_VALUE(switches::kComponentUpdater,
                                 component_updater::kSwitchTestRequestParam)},

    {kGrowthCampaignsTestTag,
     flag_descriptions::kCampaignsComponentUpdaterTestTagName,
     flag_descriptions::kCampaignsComponentUpdaterTestTagDescription, kOsCrOS,
     STRING_VALUE_TYPE(switches::kCampaignsTestTag, "")},

    {kGrowthCampaigns, flag_descriptions::kCampaignsOverrideName,
     flag_descriptions::kCampaignsOverrideDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kGrowthCampaigns, "")},

    {"demo-mode-test-tag",
     flag_descriptions::kDemoModeComponentUpdaterTestTagName,
     flag_descriptions::kDemoModeComponentUpdaterTestTagDescription, kOsCrOS,
     STRING_VALUE_TYPE(switches::kDemoModeTestTag, "")},
#endif

    {"enable-raw-draw", flag_descriptions::kEnableRawDrawName,
     flag_descriptions::kEnableRawDrawDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRawDraw)},

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
    {"enable-delegated-compositing",
     flag_descriptions::kEnableDelegatedCompositingName,
     flag_descriptions::kEnableDelegatedCompositingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDelegatedCompositing)},
#endif

    {"enable-render-pass-drawn-rect",
     flag_descriptions::kEnableRenderPassDrawnRectName,
     flag_descriptions::kEnableRenderPassDrawnRectDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kRenderPassDrawnRect)},

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
    {"media-session-enter-picture-in-picture",
     flag_descriptions::kMediaSessionEnterPictureInPictureName,
     flag_descriptions::kMediaSessionEnterPictureInPictureDescription,
     kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(blink::features::kMediaSessionEnterPictureInPicture)},

    {"auto-picture-in-picture-video-heuristics",
     flag_descriptions::kAutoPictureInPictureVideoHeuristicsName,
     flag_descriptions::kAutoPictureInPictureVideoHeuristicsDescription,
     kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(blink::features::kAutoPictureInPictureVideoHeuristics)},

    {"auto-picture-in-picture-for-video-playback",
     flag_descriptions::kAutoPictureInPictureForVideoPlaybackName,
     flag_descriptions::kAutoPictureInPictureForVideoPlaybackDescription,
     kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kAutoPictureInPictureForVideoPlayback)},

    {"video-picture-in-picture-controls-update-2024",
     flag_descriptions::kVideoPictureInPictureControlsUpdate2024Name,
     flag_descriptions::kVideoPictureInPictureControlsUpdate2024Description,
     kOsMac | kOsWin | kOsLinux | kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(media::kVideoPictureInPictureControlsUpdate2024)},

    {"document-picture-in-picture-animate-resize",
     flag_descriptions::kDocumentPictureInPictureAnimateResizeName,
     flag_descriptions::kDocumentPictureInPictureAnimateResizeDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(media::kDocumentPictureInPictureAnimateResize)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

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
    {"enable-desks-templates", flag_descriptions::kDesksTemplatesName,
     flag_descriptions::kDesksTemplatesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDesksTemplates)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"vc-background-replace", flag_descriptions::kVcBackgroundReplaceName,
     flag_descriptions::kVcBackgroundReplaceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVcBackgroundReplace)},

    {"vc-relighting-inference-backend",
     flag_descriptions::kVcRelightingInferenceBackendName,
     flag_descriptions::kVcRelightingInferenceBackendDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ash::features::kVcRelightingInferenceBackend,
         kVcRelightingInferenceBackendVariations,
         "VcRelightingInferenceBackend")},
    {"vc-segmentation-model", flag_descriptions::kVcSegmentationModelName,
     flag_descriptions::kVcSegmentationModelDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kVcSegmentationModel,
                                    kVcSegmentationModelVariations,
                                    "VCSegmentationModel")},
    {"vc-segmentation-inference-backend",
     flag_descriptions::kVcSegmentationInferenceBackendName,
     flag_descriptions::kVcSegmentationInferenceBackendDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ash::features::kVcSegmentationInferenceBackend,
         kVcSegmentationInferenceBackendVariations,
         "VcSegmentationInferenceBackend")},
    {"vc-light-intensity", flag_descriptions::kVcLightIntensityName,
     flag_descriptions::kVcLightIntensityDescription, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ash::features::kVcLightIntensity,
                                    kVcLightIntensityVariations,
                                    "VCLightIntensity")},
    {"vc-web-api", flag_descriptions::kVcWebApiName,
     flag_descriptions::kVcWebApiDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVcWebApi)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kVcTrayMicIndicatorInternalName,
     flag_descriptions::kVcTrayMicIndicatorName,
     flag_descriptions::kVcTrayMicIndicatorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVcTrayMicIndicator)},
    {kVcTrayTitleHeaderInternalName, flag_descriptions::kVcTrayTitleHeaderName,
     flag_descriptions::kVcTrayTitleHeaderDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kVcTrayTitleHeader)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"touch-drag-and-context-menu",
     flag_descriptions::kTouchDragAndContextMenuName,
     flag_descriptions::kTouchDragAndContextMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kTouchDragAndContextMenu)},
    {"animated-image-drag-shadow",
     flag_descriptions::kAnimatedImageDragShadowName,
     flag_descriptions::kAnimatedImageDragShadowDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAnimatedImageDragShadow)},
    {"drag-drop-into-omnibox", flag_descriptions::kDragDropIntoOmniboxName,
     flag_descriptions::kDragDropIntoOmniboxDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDragDropIntoOmnibox)},
    {"drag-drop-tab-tearing", flag_descriptions::kDragDropTabTearingName,
     flag_descriptions::kDragDropTabTearingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDragDropTabTearing)},
    {"drag-drop-tab-tearing-enable-oem",
     flag_descriptions::kDragDropTabTearingEnableOEMName,
     flag_descriptions::kDragDropTabTearingEnableOEMDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDragDropTabTearingEnableOEM)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"main-node-annotations", flag_descriptions::kMainNodeAnnotationsName,
     flag_descriptions::kMainNodeAnnotationsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMainNodeAnnotations)},
#endif

    {"origin-agent-cluster-default",
     flag_descriptions::kOriginAgentClusterDefaultName,
     flag_descriptions::kOriginAgentClusterDefaultDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kOriginAgentClusterDefaultEnabled)},

    {"origin-keyed-processes-by-default",
     flag_descriptions::kOriginKeyedProcessesByDefaultName,
     flag_descriptions::kOriginKeyedProcessesByDefaultDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kOriginKeyedProcessesByDefault)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-code-based-rbd", flag_descriptions::kCodeBasedRBDName,
     flag_descriptions::kCodeBasedRBDDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kCodeBasedRBD,
                                    kCodeBasedRBDVariations,
                                    "CodeBasedRBD")},

    {"enable-discount-consent-v2", flag_descriptions::kDiscountConsentV2Name,
     flag_descriptions::kDiscountConsentV2Description, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kDiscountConsentV2,
                                    kDiscountConsentV2Variations,
                                    "DiscountConsentV2")},

#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-fake-keyboard-heuristic",
     flag_descriptions::kEnableFakeKeyboardHeuristicName,
     flag_descriptions::kEnableFakeKeyboardHeuristicDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableFakeKeyboardHeuristic)},
    {"enable-fake-mouse-heuristic",
     flag_descriptions::kEnableFakeMouseHeuristicName,
     flag_descriptions::kEnableFakeMouseHeuristicDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ui::kEnableFakeMouseHeuristic)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
    {"enable-isolated-sandboxed-iframes",
     flag_descriptions::kIsolatedSandboxedIframesName,
     flag_descriptions::kIsolatedSandboxedIframesDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kIsolateSandboxedIframes,
         kIsolateSandboxedIframesGroupingVariations,
         "IsolateSandboxedIframes" /* trial name */)},
#endif

    {"download-warning-improvements",
     flag_descriptions::kDownloadWarningImprovementsName,
     flag_descriptions::kDownloadWarningImprovementsDescription,
     kOsLinux | kOsLacros | kOsMac | kOsWin | kOsCrOS,
     MULTI_VALUE_TYPE(kDownloadWarningImprovementsChoices)},

    {"reduce-accept-language", flag_descriptions::kReduceAcceptLanguageName,
     flag_descriptions::kReduceAcceptLanguageDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kReduceAcceptLanguage)},

    {"reduce-transfer-size-updated-ipc",
     flag_descriptions::kReduceTransferSizeUpdatedIPCName,
     flag_descriptions::kReduceTransferSizeUpdatedIPCDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kReduceTransferSizeUpdatedIPC)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-variable-refresh-rate",
     flag_descriptions::kEnableVariableRefreshRateName,
     flag_descriptions::kEnableVariableRefreshRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableVariableRefreshRate)},

    {"enable-projector-app-debug", flag_descriptions::kProjectorAppDebugName,
     flag_descriptions::kProjectorAppDebugDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProjectorAppDebug)},

    {kProjectorServerSideSpeechRecognition,
     flag_descriptions::kProjectorServerSideSpeechRecognitionName,
     flag_descriptions::kProjectorServerSideSpeechRecognitionDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kInternalServerSideSpeechRecognition)},

    {"enable-projector-server-side-usm",
     flag_descriptions::kProjectorServerSideUsmName,
     flag_descriptions::kProjectorServerSideUsmDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProjectorUseUSMForS3)},

    {"enable-projector-gm3", flag_descriptions::kProjectorGm3Name,
     flag_descriptions::kProjectorGm3Description, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kProjectorGm3)},

    {"enable-annotator-mode", flag_descriptions::kAnnotatorModeName,
     flag_descriptions::kAnnotatorModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAnnotatorMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"omit-cors-client-cert", flag_descriptions::kOmitCorsClientCertName,
     flag_descriptions::kOmitCorsClientCertDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kOmitCorsClientCert)},

    {"use-idna2008-non-transitional",
     flag_descriptions::kUseIDNA2008NonTransitionalName,
     flag_descriptions::kUseIDNA2008NonTransitionalDescription, kOsAll,
     FEATURE_VALUE_TYPE(url::kUseIDNA2008NonTransitional)},

#if BUILDFLAG(IS_CHROMEOS)
    {"sync-chromeos-explicit-passphrase-sharing",
     flag_descriptions::kSyncChromeOSExplicitPassphraseSharingName,
     flag_descriptions::kSyncChromeOSExplicitPassphraseSharingDescription,
     kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(syncer::kSyncChromeOSExplicitPassphraseSharing)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"always-enable-hdcp", flag_descriptions::kAlwaysEnableHdcpName,
     flag_descriptions::kAlwaysEnableHdcpDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kAlwaysEnableHdcpChoices)},
    {"enable-touchpads-in-diagnostics-app",
     flag_descriptions::kEnableTouchpadsInDiagnosticsAppName,
     flag_descriptions::kEnableTouchpadsInDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableTouchpadsInDiagnosticsApp)},
    {"enable-touchscreens-in-diagnostics-app",
     flag_descriptions::kEnableTouchscreensInDiagnosticsAppName,
     flag_descriptions::kEnableTouchscreensInDiagnosticsAppDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableTouchscreensInDiagnosticsApp)},
    {"enable-external-keyboards-in-diagnostics-app",
     flag_descriptions::kEnableExternalKeyboardsInDiagnosticsAppName,
     flag_descriptions::kEnableExternalKeyboardsInDiagnosticsAppDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableExternalKeyboardsInDiagnostics)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-virtual-card-metadata",
     flag_descriptions::kAutofillEnableVirtualCardMetadataName,
     flag_descriptions::kAutofillEnableVirtualCardMetadataDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVirtualCardMetadata)},

#if BUILDFLAG(IS_ANDROID)
    {"password-suggestion-bottom-sheet-v2",
     flag_descriptions::kPasswordSuggestionBottomSheetV2Name,
     flag_descriptions::kPasswordSuggestionBottomSheetV2Description, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordSuggestionBottomSheetV2)},

    {"pwa-restore-backend", flag_descriptions::kPwaRestoreBackendName,
     flag_descriptions::kPwaRestoreBackendDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(syncer::kWebApkBackupAndRestoreBackend)},

    {"pwa-restore-ui", flag_descriptions::kPwaRestoreUiName,
     flag_descriptions::kPwaRestoreUiDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPwaRestoreUi)},

    {"pwa-restore-ui-at-startup", flag_descriptions::kPwaRestoreUiAtStartupName,
     flag_descriptions::kPwaRestoreUiAtStartupDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kPwaRestoreUiAtStartup)},
#endif  // BUILDFLAG(IS_ANDROID)
    {"autofill-enable-ranking-formula-address-profiles",
     flag_descriptions::kAutofillEnableRankingFormulaAddressProfilesName,
     flag_descriptions::kAutofillEnableRankingFormulaAddressProfilesDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableRankingFormulaAddressProfiles)},

    {"autofill-enable-ranking-formula-credit-cards",
     flag_descriptions::kAutofillEnableRankingFormulaCreditCardsName,
     flag_descriptions::kAutofillEnableRankingFormulaCreditCardsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableRankingFormulaCreditCards)},

    {"safe-browsing-async-real-time-check",
     flag_descriptions::kSafeBrowsingAsyncRealTimeCheckName,
     flag_descriptions::kSafeBrowsingAsyncRealTimeCheckDescription, kOsAll,
     FEATURE_VALUE_TYPE(safe_browsing::kSafeBrowsingAsyncRealTimeCheck)},

    {"safe-browsing-hash-prefix",
     flag_descriptions::kSafeBrowsingHashPrefixRealTimeLookupsName,
     flag_descriptions::kSafeBrowsingHashPrefixRealTimeLookupsDescription,
     kOsAll, FEATURE_VALUE_TYPE(safe_browsing::kHashPrefixRealTimeLookups)},

    {"safe-browsing-local-lists-use-sbv5",
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Name,
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Description, kOsAll,
     FEATURE_VALUE_TYPE(safe_browsing::kLocalListsUseSBv5)},

    {"safety-check-unused-site-permissions",
     flag_descriptions::kSafetyCheckUnusedSitePermissionsName,
     flag_descriptions::kSafetyCheckUnusedSitePermissionsDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         content_settings::features::kSafetyCheckUnusedSitePermissions,
         kSafetyCheckUnusedSitePermissionsVariations,
         "SafetyCheckUnusedSitePermissions")},

    {"safety-hub", flag_descriptions::kSafetyHubName,
     flag_descriptions::kSafetyHubDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kSafetyHub,
                                    kSafetyHubVariations,
                                    "SafetyHub")},

#if BUILDFLAG(IS_ANDROID)
    {"record-permissions-expiration-timestamp",
     flag_descriptions::kRecordPermissionExpirationTimestampsName,
     flag_descriptions::kRecordPermissionExpirationTimestampsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         permissions::features::kRecordPermissionExpirationTimestamps)},

    {"safety-hub-magic-stack", flag_descriptions::kSafetyHubMagicStackName,
     flag_descriptions::kSafetyHubMagicStackDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSafetyHubMagicStack)},

    {"safety-hub-followup", flag_descriptions::kSafetyHubFollowupName,
     flag_descriptions::kSafetyHubFollowupDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSafetyHubFollowup)},

    {"safety-hub-android-survey",
     flag_descriptions::kSafetyHubAndroidSurveyName,
     flag_descriptions::kSafetyHubAndroidSurveyDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kSafetyHubAndroidSurvey)},
#else
    {"safety-hub-one-off-survey",
     flag_descriptions::kSafetyHubHaTSOneOffSurveyName,
     flag_descriptions::kSafetyHubHaTSOneOffSurveyDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSafetyHubHaTSOneOffSurvey)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"safety-hub-abusive-notification-revocation",
     flag_descriptions::kSafetyHubAbusiveNotificationRevocationName,
     flag_descriptions::kSafetyHubAbusiveNotificationRevocationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         safe_browsing::kSafetyHubAbusiveNotificationRevocation)},

#if BUILDFLAG(IS_ANDROID)
    {"enable-commerce-hint-android",
     flag_descriptions::kCommerceHintAndroidName,
     flag_descriptions::kCommerceHintAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(commerce::kCommerceHintAndroid)},
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    {"enable-web-bluetooth-confirm-pairing-support",
     flag_descriptions::kWebBluetoothConfirmPairingSupportName,
     flag_descriptions::kWebBluetoothConfirmPairingSupportDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(device::features::kWebBluetoothConfirmPairingSupport)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

    {"enable-perfetto-system-tracing",
     flag_descriptions::kEnablePerfettoSystemTracingName,
     flag_descriptions::kEnablePerfettoSystemTracingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kEnablePerfettoSystemTracing)},

#if BUILDFLAG(IS_ANDROID)
    {"browsing-data-model-clank", flag_descriptions::kBrowsingDataModelName,
     flag_descriptions::kBrowsingDataModelDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(browsing_data::features::kBrowsingDataModel)},
    {"enable-android-gamepad-vibration",
     flag_descriptions::kEnableAndroidGamepadVibrationName,
     flag_descriptions::kEnableAndroidGamepadVibrationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kEnableAndroidGamepadVibration)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-seamless-refresh-rate-switching",
     flag_descriptions::kEnableSeamlessRefreshRateSwitchingName,
     flag_descriptions::kEnableSeamlessRefreshRateSwitchingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSeamlessRefreshRateSwitching)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"click-to-call", flag_descriptions::kClickToCallName,
     flag_descriptions::kClickToCallDescription, kOsAll,
     FEATURE_VALUE_TYPE(kClickToCall)},

    {"css-gamut-mapping", flag_descriptions::kCssGamutMappingName,
     flag_descriptions::kCssGamutMappingDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kBakedGamutMapping)},

    {"clipboard-maximum-age", flag_descriptions::kClipboardMaximumAgeName,
     flag_descriptions::kClipboardMaximumAgeDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kClipboardMaximumAge,
                                    kClipboardMaximumAgeVariations,
                                    "ClipboardMaximumAge")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-media-dynamic-cgroup", flag_descriptions::kMediaDynamicCgroupName,
     flag_descriptions::kMediaDynamicCgroupDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootMediaDynamicCgroup")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-parse-vcn-card-on-file-standalone-cvc-fields",
     flag_descriptions::kAutofillParseVcnCardOnFileStandaloneCvcFieldsName,
     flag_descriptions::
         kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillParseVcnCardOnFileStandaloneCvcFields)},

    {"background-resource-fetch",
     flag_descriptions::kBackgroundResourceFetchName,
     flag_descriptions::kBackgroundResourceFetchDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kBackgroundResourceFetch)},

#if BUILDFLAG(IS_ANDROID)
    {"external-navigation-debug-logs",
     flag_descriptions::kExternalNavigationDebugLogsName,
     flag_descriptions::kExternalNavigationDebugLogsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(external_intents::kExternalNavigationDebugLogs)},
#endif

    {"webui-omnibox-popup", flag_descriptions::kWebUIOmniboxPopupName,
     flag_descriptions::kWebUIOmniboxPopupDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(omnibox::kWebUIOmniboxPopup)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-vm-memory-size", flag_descriptions::kArcVmMemorySizeName,
     flag_descriptions::kArcVmMemorySizeDesc, kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(arc::kVmMemorySize,
                                    kArcVmMemorySizeVariations,
                                    "VmMemorySize")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-card-art-image",
     flag_descriptions::kAutofillEnableCardArtImageName,
     flag_descriptions::kAutofillEnableCardArtImageDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardArtImage)},

#if BUILDFLAG(IS_ANDROID)
    {"tab-group-creation-dialog-android",
     flag_descriptions::kTabGroupCreationDialogAndroidName,
     flag_descriptions::kTabGroupCreationDialogAndroidDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kTabGroupCreationDialogAndroid,
         kTabGroupCreationDialogAndroidVariations,
         "TabGroupCreationDialogVariations")},

    {"tab-group-pane-android", flag_descriptions::kTabGroupPaneAndroidName,
     flag_descriptions::kTabGroupPaneAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupPaneAndroid)},

    {"tab-strip-group-collapse-android",
     flag_descriptions::kTabStripGroupCollapseAndroidName,
     flag_descriptions::kTabStripGroupCollapseAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabStripGroupCollapseAndroid)},

    {"tab-strip-group-context-menu-android",
     flag_descriptions::kTabStripGroupContextMenuAndroidName,
     flag_descriptions::kTabStripGroupContextMenuAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabStripGroupContextMenuAndroid)},

    {"tab-strip-incognito-migration",
     flag_descriptions::kTabStripIncognitoMigrationName,
     flag_descriptions::kTabStripIncognitoMigrationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabStripIncognitoMigration)},

    {"tab-strip-layout-optimization",
     flag_descriptions::kTabStripLayoutOptimizationName,
     flag_descriptions::kTabStripLayoutOptimizationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabStripLayoutOptimization)},

    {"tab-strip-transition-in-desktop-window",
     flag_descriptions::kTabStripTransitionInDesktopWindowName,
     flag_descriptions::kTabStripTransitionInDesktopWindowDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kTabStripTransitionInDesktopWindow)},
#endif

    {"use-dmsaa-for-tiles", flag_descriptions::kUseDMSAAForTilesName,
     flag_descriptions::kUseDMSAAForTilesDescription, kOsAll,
     FEATURE_VALUE_TYPE(::features::kUseDMSAAForTiles)},

#if BUILDFLAG(IS_ANDROID)
    {"use-dmsaa-for-tiles-android-gl",
     flag_descriptions::kUseDMSAAForTilesAndroidGLName,
     flag_descriptions::kUseDMSAAForTilesAndroidGLDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(::features::kUseDMSAAForTilesAndroidGL)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-holding-space-suggestions",
     flag_descriptions::kHoldingSpaceSuggestionsName,
     flag_descriptions::kHoldingSpaceSuggestionsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kHoldingSpaceSuggestions)},
    {"enable-welcome-experience", flag_descriptions::kWelcomeExperienceName,
     flag_descriptions::kWelcomeExperienceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWelcomeExperience)},
    {"enable-welcome-experience-test-unsupported-devices",
     flag_descriptions::kWelcomeExperienceTestUnsupportedDevicesName,
     flag_descriptions::kWelcomeExperienceTestUnsupportedDevicesDescription,
     kOsCrOS,
     FEATURE_VALUE_TYPE(
         ash::features::kWelcomeExperienceTestUnsupportedDevices)},
    {"enable-welcome-tour", flag_descriptions::kWelcomeTourName,
     flag_descriptions::kWelcomeTourDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWelcomeTour)},
    {"enable-welcome-tour-force-user-eligibility",
     flag_descriptions::kWelcomeTourForceUserEligibilityName,
     flag_descriptions::kWelcomeTourForceUserEligibilityDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWelcomeTourForceUserEligibility)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"google-one-offer-files-banner",
     flag_descriptions::kGoogleOneOfferFilesBannerName,
     flag_descriptions::kGoogleOneOfferFilesBannerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kGoogleOneOfferFilesBanner)},
#endif

    {"sync-autofill-wallet-credential-data",
     flag_descriptions::kSyncAutofillWalletCredentialDataName,
     flag_descriptions::kSyncAutofillWalletCredentialDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillWalletCredentialData)},

    {"sync-autofill-wallet-usage-data",
     flag_descriptions::kSyncAutofillWalletUsageDataName,
     flag_descriptions::kSyncAutofillWalletUsageDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillWalletUsageData)},

#if !BUILDFLAG(IS_ANDROID)
    {"ui-enable-shared-image-cache-for-gpu",
     flag_descriptions::kUIEnableSharedImageCacheForGpuName,
     flag_descriptions::kUIEnableSharedImageCacheForGpuDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(::features::kUIEnableSharedImageCacheForGpu)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-labs-window-cycle-shortcut",
     flag_descriptions::kSameAppWindowCycleName,
     flag_descriptions::kSameAppWindowCycleDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSameAppWindowCycle)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"promise-icons", flag_descriptions::kPromiseIconsName,
     flag_descriptions::kPromiseIconsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPromiseIcons)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"printing-ppd-channel", flag_descriptions::kPrintingPpdChannelName,
     flag_descriptions::kPrintingPpdChannelDescription, kOsCrOS,
     MULTI_VALUE_TYPE(kPrintingPpdChannelChoices)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"arc-idle-manager", flag_descriptions::kArcIdleManagerName,
     flag_descriptions::kArcIdleManagerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(arc::kEnableArcIdleManager)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-bubble-corner-radius-update",
     flag_descriptions::kEnableBubbleCornerRadiusUpdateName,
     flag_descriptions::kEnableBubbleCornerRadiusUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kEnableBubbleCornerRadiusUpdate)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-notification-image-drag",
     flag_descriptions::kEnableNotificationImageDragName,
     flag_descriptions::kEnableNotificationImageDragDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kNotificationImageDrag)},
#endif

    {"autofill-enable-new-card-art-and-network-images",
     flag_descriptions::kAutofillEnableNewCardArtAndNetworkImagesName,
     flag_descriptions::kAutofillEnableNewCardArtAndNetworkImagesDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableNewCardArtAndNetworkImages)},

    {"autofill-enable-card-art-server-side-stretching",
     flag_descriptions::kAutofillEnableCardArtServerSideStretchingName,
     flag_descriptions::kAutofillEnableCardArtServerSideStretchingDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardArtServerSideStretching)},

    {"power-bookmark-backend", flag_descriptions::kPowerBookmarkBackendName,
     flag_descriptions::kPowerBookmarkBackendDescription, kOsAll,
     FEATURE_VALUE_TYPE(power_bookmarks::kPowerBookmarkBackend)},

#if !BUILDFLAG(IS_ANDROID)
    {"user-notes-side-panel", flag_descriptions::kUserNotesSidePanelName,
     flag_descriptions::kUserNotesSidePanelDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(user_notes::kUserNotes)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-eol-notification-reset-dismissed-prefs",
     flag_descriptions::kEolResetDismissedPrefsName,
     flag_descriptions::kEolResetDismissedPrefsDescription, kOsCrOS,
     SINGLE_VALUE_TYPE(ash::switches::kEolResetDismissedPrefs)},
#endif

    {"enable-preferences-account-storage",
     flag_descriptions::kEnablePreferencesAccountStorageName,
     flag_descriptions::kEnablePreferencesAccountStorageDescription, kOsAll,
     FEATURE_VALUE_TYPE(syncer::kEnablePreferencesAccountStorage)},

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {"experimental-web-app-storage-partition-isolation",
     flag_descriptions::kExperimentalWebAppStoragePartitionIsolationName,
     flag_descriptions::kExperimentalWebAppStoragePartitionIsolationDescription,
     kOsLacros,
     FEATURE_VALUE_TYPE(
         chromeos::features::kExperimentalWebAppStoragePartitionIsolation)},

    {"blink-extension", flag_descriptions::kBlinkExtensionName,
     flag_descriptions::kBlinkExtensionDescription, kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kBlinkExtension)},
    {"blink-extension-diagnostics",
     flag_descriptions::kBlinkExtensionDiagnosticsName,
     flag_descriptions::kBlinkExtensionDiagnosticsDescription, kOsLacros,
     FEATURE_VALUE_TYPE(chromeos::features::kBlinkExtensionDiagnostics)},
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-desk-button", flag_descriptions::kDeskButtonName,
     flag_descriptions::kDeskButtonDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDeskButton)},
    {"cros-focus-mode", flag_descriptions::kFocusModeName,
     flag_descriptions::kFocusModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFocusMode)},
    {"cros-focus-mode-ytm", flag_descriptions::kFocusModeYTMName,
     flag_descriptions::kFocusModeYTMDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFocusModeYTM)},
    {"render-arc-notifications-by-chrome",
     flag_descriptions::kRenderArcNotificationsByChromeName,
     flag_descriptions::kRenderArcNotificationsByChromeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kRenderArcNotificationsByChrome)},
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"ios-promo-refreshed-password-bubble",
     flag_descriptions::kIOSPromoRefreshedPasswordBubbleName,
     flag_descriptions::kIOSPromoRefreshedPasswordBubbleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIOSPromoRefreshedPasswordBubble)},

    {"ios-promo-address-bubble", flag_descriptions::kIOSPromoAddressBubbleName,
     flag_descriptions::kIOSPromoAddressBubbleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIOSPromoAddressBubble)},

    {"ios-promo-payment-bubble", flag_descriptions::kIOSPromoPaymentBubbleName,
     flag_descriptions::kIOSPromoPaymentBubbleDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kIOSPromoPaymentBubble)},
#endif

    {"enable-compression-dictionary-transport",
     flag_descriptions::kCompressionDictionaryTransportName,
     flag_descriptions::kCompressionDictionaryTransportDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kCompressionDictionaryTransport)},

    {"enable-compression-dictionary-transport-backend",
     flag_descriptions::kCompressionDictionaryTransportBackendName,
     flag_descriptions::kCompressionDictionaryTransportBackendDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         network::features::kCompressionDictionaryTransportBackend)},

    {"enable-compression-dictionary-transport-allow-http1",
     flag_descriptions::kCompressionDictionaryTransportOverHttp1Name,
     flag_descriptions::kCompressionDictionaryTransportOverHttp1Description,
     kOsAll,
     FEATURE_VALUE_TYPE(
         net::features::kCompressionDictionaryTransportOverHttp1)},

    {"enable-compression-dictionary-transport-allow-http2",
     flag_descriptions::kCompressionDictionaryTransportOverHttp2Name,
     flag_descriptions::kCompressionDictionaryTransportOverHttp2Description,
     kOsAll,
     FEATURE_VALUE_TYPE(
         net::features::kCompressionDictionaryTransportOverHttp2)},

    {"enable-compression-dictionary-transport-require-known-root-cert",
     flag_descriptions::kCompressionDictionaryTransportRequireKnownRootCertName,
     flag_descriptions::
         kCompressionDictionaryTransportRequireKnownRootCertDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         net::features::kCompressionDictionaryTransportRequireKnownRootCert)},

    {"enable-compute-pressure-rate-obfuscation-mitigation",
     flag_descriptions::kComputePressureRateObfuscationMitigationName,
     flag_descriptions::kComputePressureRateObfuscationMitigationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::kComputePressureRateObfuscationMitigation)},

    {"enable-container-type-no-layout-containment",
     flag_descriptions::kContainerTypeNoLayoutContainmentName,
     flag_descriptions::kContainerTypeNoLayoutContainmentDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kContainerTypeNoLayoutContainment)},

    {"enable-compute-pressure-break-calibration-mitigation",
     flag_descriptions::kComputePressureBreakCalibrationMitigationName,
     flag_descriptions::kComputePressureBreakCalibrationMitigationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kComputePressureBreakCalibrationMitigation)},

    {"enable-zstd-content-encoding",
     flag_descriptions::kZstdContentEncodingName,
     flag_descriptions::kZstdContentEncodingDescription, kOsAll,
     FEATURE_VALUE_TYPE(net::features::kZstdContentEncoding)},

    {"enable-shared-zstd", flag_descriptions::kSharedZstdName,
     flag_descriptions::kSharedZstdDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kSharedZstd)},

#if BUILDFLAG(IS_ANDROID)
    {"web-authentication-android-credential-management",
     flag_descriptions::kWebAuthnAndroidCredManName,
     flag_descriptions::kWebAuthnAndroidCredManDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(device::kWebAuthnAndroidCredMan,
                                    kWebAuthnAndroidCredManVariations,
                                    "WebAuthenticationAndroidCredMan")},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"android-extended-keyboard-shortcuts",
     flag_descriptions::kAndroidExtendedKeyboardShortcutsName,
     flag_descriptions::kAndroidExtendedKeyboardShortcutsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kAndroidExtendedKeyboardShortcuts)},
    {"convert-trackpad-events-to-mouse",
     flag_descriptions::kConvertTrackpadEventsToMouseName,
     flag_descriptions::kConvertTrackpadEventsToMouseDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(ui::kConvertTrackpadEventsToMouse)},
    {"deprecated-external-picker-function",
     flag_descriptions::kDeprecatedExternalPickerFunctionName,
     flag_descriptions::kDeprecatedExternalPickerFunctionDescription,
     kOsAndroid, FEATURE_VALUE_TYPE(ui::kDeprecatedExternalPickerFunction)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-missive-storage-config", flag_descriptions::kMissiveStorageName,
     flag_descriptions::kMissiveStorageDescription, kOsCrOS,
     PLATFORM_FEATURE_WITH_PARAMS_VALUE_TYPE(
         "CrOSLateBootMissiveStorage",
         kCrOSLateBootMissiveStorageDefaultVariations,
         "CrOSLateBootMissiveStorage")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
    {"cast-mirroring-target-playout-delay",
     flag_descriptions::kCastMirroringTargetPlayoutDelayName,
     flag_descriptions::kCastMirroringTargetPlayoutDelayDescription, kOsDesktop,
     MULTI_VALUE_TYPE(kCastMirroringTargetPlayoutDelayChoices)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"allow-devtools-in-system-ui",
     flag_descriptions::kAllowDevtoolsInSystemUIName,
     flag_descriptions::kAllowDevtoolsInSystemUIDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kAllowDevtoolsInSystemUI)},
    {"separate-web-app-shortcut-badge-icon",
     flag_descriptions::kSeparateWebAppShortcutBadgeIconName,
     flag_descriptions::kSeparateWebAppShortcutBadgeIconDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSeparateWebAppShortcutBadgeIcon)},
    {"enable-audio-focus-enforcement",
     flag_descriptions::kEnableAudioFocusEnforcementName,
     flag_descriptions::kEnableAudioFocusEnforcementDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(media_session::features::kAudioFocusEnforcement)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"enable-process-per-site-up-to-main-frame-threshold",
     flag_descriptions::kEnableProcessPerSiteUpToMainFrameThresholdName,
     flag_descriptions::kEnableProcessPerSiteUpToMainFrameThresholdDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kProcessPerSiteUpToMainFrameThreshold)},

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    {"camera-mic-effects", flag_descriptions::kCameraMicEffectsName,
     flag_descriptions::kCameraMicEffectsDescription,
     static_cast<unsigned short>(kOsMac | kOsWin | kOsLinux),
     FEATURE_VALUE_TYPE(media::kCameraMicEffects)},

    {"camera-mic-preview", flag_descriptions::kCameraMicPreviewName,
     flag_descriptions::kCameraMicPreviewDescription,
     static_cast<unsigned short>(kOsMac | kOsWin | kOsLinux),
     FEATURE_VALUE_TYPE(blink::features::kCameraMicPreview)},

    {"get-user-media-deferred-device-settings-selection",
     flag_descriptions::kGetUserMediaDeferredDeviceSettingsSelectionName,
     flag_descriptions::kGetUserMediaDeferredDeviceSettingsSelectionDescription,
     static_cast<unsigned short>(kOsMac | kOsWin | kOsLinux),
     FEATURE_VALUE_TYPE(
         blink::features::kGetUserMediaDeferredDeviceSettingsSelection)},
#endif

    {"render-document", flag_descriptions::kRenderDocumentName,
     flag_descriptions::kRenderDocumentDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kRenderDocument,
                                    kRenderDocumentVariations,
                                    "RenderDocument")},

    {"site-instance-groups-for-data-urls",
     flag_descriptions::kSiteInstanceGroupsForDataUrlsName,
     flag_descriptions::kSiteInstanceGroupsForDataUrlsDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kSiteInstanceGroupsForDataUrls)},

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    {"site-search-settings-policy",
     flag_descriptions::kSiteSearchSettingsPolicyName,
     flag_descriptions::kSiteSearchSettingsPolicyDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kSiteSearchSettingsPolicy)},

    {"show-featured-enterprise-site-search",
     flag_descriptions::kShowFeaturedEnterpriseSiteSearchName,
     flag_descriptions::kShowFeaturedEnterpriseSiteSearchDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kShowFeaturedEnterpriseSiteSearch)},

    {"show-featured-enterprise-site-search-iph",
     flag_descriptions::kShowFeaturedEnterpriseSiteSearchIPHName,
     flag_descriptions::kShowFeaturedEnterpriseSiteSearchIPHDescription, kOsAll,
     FEATURE_VALUE_TYPE(omnibox::kShowFeaturedEnterpriseSiteSearchIPH)},
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {"cws-info-fast-check", flag_descriptions::kCWSInfoFastCheckName,
     flag_descriptions::kCWSInfoFastCheckDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(extensions::kCWSInfoFastCheck)},

    {"extension-telemetry-for-enterprise",
     flag_descriptions::kExtensionTelemetryForEnterpriseName,
     flag_descriptions::kExtensionTelemetryForEnterpriseDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         safe_browsing::kExtensionTelemetryForEnterprise,
         kExtensionTelemetryEnterpriseReportingIntervalSecondsVariations,
         "EnterpriseReportingIntervalSeconds")},

    {"safety-check-extensions", flag_descriptions::kSafetyCheckExtensionsName,
     flag_descriptions::kSafetyCheckExtensionsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kSafetyCheckExtensions)},
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    {"autofill-enable-cvc-storage-and-filling",
     flag_descriptions::kAutofillEnableCvcStorageAndFillingName,
     flag_descriptions::kAutofillEnableCvcStorageAndFillingDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCvcStorageAndFilling)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"drive-fs-show-cse-files", flag_descriptions::kDriveFsShowCSEFilesName,
     flag_descriptions::kDriveFsShowCSEFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDriveFsShowCSEFiles)},
    {"drive-fs-mirroring", flag_descriptions::kDriveFsMirroringName,
     flag_descriptions::kDriveFsShowCSEFilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDriveFsMirroring)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-labs-continuous-overview-animation",
     flag_descriptions::kContinuousOverviewScrollAnimationName,
     flag_descriptions::kContinuousOverviewScrollAnimationDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kContinuousOverviewScrollAnimation)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-labs-window-splitting", flag_descriptions::kWindowSplittingName,
     flag_descriptions::kWindowSplittingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kWindowSplitting)},

    {"cros-labs-tiling-window-resize",
     flag_descriptions::kTilingWindowResizeName,
     flag_descriptions::kTilingWindowResizeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kTilingWindowResize)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"privacy-sandbox-enrollment-overrides",
     flag_descriptions::kPrivacySandboxEnrollmentOverridesName,
     flag_descriptions::kPrivacySandboxEnrollmentOverridesDescription, kOsAll,
     ORIGIN_LIST_VALUE_TYPE(privacy_sandbox::kPrivacySandboxEnrollmentOverrides,
                            "")},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-smart-card-web-api", flag_descriptions::kSmartCardWebApiName,
     flag_descriptions::kSmartCardWebApiDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(blink::features::kSmartCard)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
    {"enable-web-printing-api", flag_descriptions::kWebPrintingApiName,
     flag_descriptions::kWebPrintingApiDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(blink::features::kWebPrinting)},
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
    {"mouse-and-trackpad-dropdown-menu",
     flag_descriptions::kMouseAndTrackpadDropdownMenuName,
     flag_descriptions::kMouseAndTrackpadDropdownMenuDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kMouseAndTrackpadDropdownMenu)},
#endif

    {"autofill-enable-prefetching-risk-data-for-retrieval",
     flag_descriptions::kAutofillEnablePrefetchingRiskDataForRetrievalName,
     flag_descriptions::
         kAutofillEnablePrefetchingRiskDataForRetrievalDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnablePrefetchingRiskDataForRetrieval)},

#if BUILDFLAG(IS_ANDROID)
    {"read-aloud", flag_descriptions::kReadAloudName,
     flag_descriptions::kReadAloudDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReadAloud)},

    {"read-aloud-background-playback",
     flag_descriptions::kReadAloudBackgroundPlaybackName,
     flag_descriptions::kReadAloudBackgroundPlaybackDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReadAloudBackgroundPlayback)},

    {"read-aloud-in-cct", flag_descriptions::kReadAloudInCCTName,
     flag_descriptions::kReadAloudInCCTDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReadAloudInOverflowMenuInCCT)},

    {"read-aloud-tap-to-seek", flag_descriptions::kReadAloudTapToSeekName,
     flag_descriptions::kReadAloudTapToSeekDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kReadAloudTapToSeek)},
#endif

    {"hide-incognito-media-metadata",
     flag_descriptions::kHideIncognitoMediaMetadataName,
     flag_descriptions::kHideIncognitoMediaMetadataDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kHideIncognitoMediaMetadata)},

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {"third-party-profile-management",
     flag_descriptions::kThirdPartyProfileManagementName,
     flag_descriptions::kThirdPartyProfileManagementDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         profile_management::features::kThirdPartyProfileManagement)},

    {"oidc-auth-profile-management",
     flag_descriptions::kOidcAuthProfileManagementName,
     flag_descriptions::kOidcAuthProfileManagementDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         profile_management::features::kOidcAuthProfileManagement)},

    {"explicit-browser-signin-ui-on-desktop",
     flag_descriptions::kExplicitBrowserSigninUIOnDesktopName,
     flag_descriptions::kExplicitBrowserSigninUIOnDesktopDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(switches::kExplicitBrowserSigninUIOnDesktop)},

    {"enable-generic-oidc-auth-profile-management",
     flag_descriptions::kEnableGenericOidcAuthProfileManagementName,
     flag_descriptions::kEnableGenericOidcAuthProfileManagementDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(profile_management::features::
                            kEnableGenericOidcAuthProfileManagement)},
    {"enable-user-link-capturing-scope-extensions-pwa",
     flag_descriptions::kDesktopPWAsUserLinkCapturingScopeExtensionsName,
     flag_descriptions::kDesktopPWAsUserLinkCapturingScopeExtensionsDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(features::kPwaNavigationCapturingWithScopeExtensions)},

    {"sync-enable-contact-info-data-type-in-transport-mode",
     flag_descriptions::kSyncEnableContactInfoDataTypeInTransportModeName,
     flag_descriptions::
         kSyncEnableContactInfoDataTypeInTransportModeDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(syncer::kSyncEnableContactInfoDataTypeInTransportMode)},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
    {"enable-user-navigation-capturing-pwa",
     flag_descriptions::kPwaNavigationCapturingName,
     flag_descriptions::kPwaNavigationCapturingDescription,
     kOsLinux | kOsMac | kOsWin | kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kPwaNavigationCapturing,
                                    kPwaNavigationCapturingVariations,
                                    "PwaNavigationCapturing")},
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS)

    {"ip-protection-proxy-opt-out",
     flag_descriptions::kIpProtectionProxyOptOutName,
     flag_descriptions::kIpProtectionProxyOptOutDescription, kOsAll,
     MULTI_VALUE_TYPE(kIpProtectionProxyOptOutChoices)},

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-android-n-key-for-fido-authentication",
     flag_descriptions::kAutofillEnableAndroidNKeyForFidoAuthenticationName,
     flag_descriptions::
         kAutofillEnableAndroidNKeyForFidoAuthenticationDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableAndroidNKeyForFidoAuthentication)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"protected-audience-debug-token",
     flag_descriptions::kProtectedAudiencesConsentedDebugTokenName,
     flag_descriptions::kProtectedAudiencesConsentedDebugTokenDescription,
     kOsAll,
     STRING_VALUE_TYPE(switches::kProtectedAudiencesConsentedDebugToken, "")},

    {"deprecate-unload", flag_descriptions::kDeprecateUnloadName,
     flag_descriptions::kDeprecateUnloadDescription, kOsAll | kDeprecated,
     FEATURE_VALUE_TYPE(blink::features::kDeprecateUnload)},

    {"autofill-enable-fpan-risk-based-authentication",
     flag_descriptions::kAutofillEnableFpanRiskBasedAuthenticationName,
     flag_descriptions::kAutofillEnableFpanRiskBasedAuthenticationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableFpanRiskBasedAuthentication)},

    {"draw-immediately-when-interactive",
     flag_descriptions::kDrawImmediatelyWhenInteractiveName,
     flag_descriptions::kDrawImmediatelyWhenInteractiveDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDrawImmediatelyWhenInteractive)},

    {"ack-on-surface-activation-when-interactive",
     flag_descriptions::kAckOnSurfaceActivationWhenInteractiveName,
     flag_descriptions::kAckOnSurfaceActivationWhenInteractiveDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(features::kAckOnSurfaceActivationWhenInteractive)},

#if BUILDFLAG(IS_MAC)
    {"enable-mac-pwas-notification-attribution",
     flag_descriptions::kMacPWAsNotificationAttributionName,
     flag_descriptions::kMacPWAsNotificationAttributionDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kAppShimNotificationAttribution)},

    {"use-adhoc-signing-for-web-app-shims",
     flag_descriptions::kUseAdHocSigningForWebAppShimsName,
     flag_descriptions::kUseAdHocSigningForWebAppShimsDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kUseAdHocSigningForWebAppShims)},
#endif  // BUILDFLAG(IS_MAC)

    {"autofill-enable-server-iban",
     flag_descriptions::kAutofillEnableServerIbanName,
     flag_descriptions::kAutofillEnableServerIbanDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableServerIban)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"seal-key", flag_descriptions::kSealKeyName,
     flag_descriptions::kSealKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kSealKey, "")},
#endif

    {"enable-manta-service", flag_descriptions::kEnableMantaServiceName,
     flag_descriptions::kEnableMantaServiceDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(manta::features::kMantaService)},

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"esb-download-row-promo",
     flag_descriptions::kEsbDownloadRowPromoFeatureName,
     flag_descriptions::kEsbDownloadRowPromoFeatureDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(feature_engagement::kEsbDownloadRowPromoFeature)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
    {"enable-builtin-hls", flag_descriptions::kEnableBuiltinHlsName,
     flag_descriptions::kEnableBuiltinHlsDescription, kOsAll,
     FEATURE_VALUE_TYPE(media::kBuiltInHlsPlayer)},
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {"profiles-reordering", flag_descriptions::kProfilesReorderingName,
     flag_descriptions::kProfilesReorderingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(kProfilesReordering)},
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    {"batch-upload-desktop", flag_descriptions::kBatchUploadDesktopName,
     flag_descriptions::kBatchUploadDesktopDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(switches::kBatchUploadDesktop)},
#endif

    {"autofill-require-valid-local-cards-in-settings",
     flag_descriptions::kAutofillRequireValidLocalCardsInSettingsName,
     flag_descriptions::kAutofillRequireValidLocalCardsInSettingsDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRequireValidLocalCardsInSettings)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"flex-firmware-update", flag_descriptions::kFlexFirmwareUpdateName,
     flag_descriptions::kFlexFirmwareUpdateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kFlexFirmwareUpdate)},
    {"upstream-trusted-reports-firmware",
     flag_descriptions::kUpstreamTrustedReportsFirmwareName,
     flag_descriptions::kUpstreamTrustedReportsFirmwareDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kUpstreamTrustedReportsFirmware)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ipp-first-setup-for-usb-printers",
     flag_descriptions::kIppFirstSetupForUsbPrintersName,
     flag_descriptions::kIppFirstSetupForUsbPrintersDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kIppFirstSetupForUsbPrinters)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    {"enable-bound-session-credentials",
     flag_descriptions::kEnableBoundSessionCredentialsName,
     flag_descriptions::kEnableBoundSessionCredentialsDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_WITH_PARAMS_VALUE_TYPE(switches::kEnableBoundSessionCredentials,
                                    kEnableBoundSessionCredentialsVariations,
                                    "EnableBoundSessionCredentials")},
    {"enable-bound-session-credentials-software-keys-for-manual-testing",
     flag_descriptions::
         kEnableBoundSessionCredentialsSoftwareKeysForManualTestingName,
     flag_descriptions::
         kEnableBoundSessionCredentialsSoftwareKeysForManualTestingDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(
         unexportable_keys::
             kEnableBoundSessionCredentialsSoftwareKeysForManualTesting)},
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-soul", flag_descriptions::kCrosSoulName,
     flag_descriptions::kCrosSoulDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootCrOSSOUL")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"observable-api", flag_descriptions::kObservableAPIName,
     flag_descriptions::kObservableAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kObservableAPI)},

    {"atomic-move", flag_descriptions::kAtomicMoveAPIName,
     flag_descriptions::kAtomicMoveAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kAtomicMoveAPI)},

#if BUILDFLAG(IS_ANDROID)
    {"android-hub-floating-action-button",
     flag_descriptions::kAndroidHubFloatingActionButtonName,
     flag_descriptions::kAndroidHubFloatingActionButtonDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         chrome::android::kAndroidHubFloatingActionButton,
         kAndroidHubFloatingActionButtonVariations,
         "AndroidHubFloatingActionButton")},

    {"android-hub-search", flag_descriptions::kAndroidHubSearchName,
     flag_descriptions::kAndroidHubSearchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidHubSearch)},

    {"android-hub-v2", flag_descriptions::kAndroidHubV2Name,
     flag_descriptions::kAndroidHubV2Description, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(chrome::android::kAndroidHubV2,
                                    kAndroidHubV2Variations,
                                    "AndroidHubV2")},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    {"enable-web-app-system-media-controls",
     flag_descriptions::kWebAppSystemMediaControlsName,
     flag_descriptions::kWebAppSystemMediaControlsDescription, kOsWin | kOsMac,
     FEATURE_VALUE_TYPE(features::kWebAppSystemMediaControls)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_COMPOSE)
    {flag_descriptions::kComposeId, flag_descriptions::kComposeName,
     flag_descriptions::kComposeDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(compose::features::kEnableCompose)},

    {"compose-text-selection", flag_descriptions::kComposeTextSelectionName,
     flag_descriptions::kComposeTextSelectionDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(compose::features::kComposeTextSelection)},

    {"compose-proactive-nudge", flag_descriptions::kComposeProactiveNudgeName,
     flag_descriptions::kComposeProactiveNudgeDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         compose::features::kEnableComposeProactiveNudge,
         kComposeProactiveNudgeVariations,
         "ComposeProactiveNudge")},

    {"compose-polite-nudge", flag_descriptions::kComposePoliteNudgeName,
     flag_descriptions::kComposePoliteNudgeDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(autofill::features::kComposePopupAnnouncePolitely)},

    {"compose-segmentation-promotion",
     flag_descriptions::kComposeSegmentationPromotionName,
     flag_descriptions::kComposeSegmentationPromotionDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(segmentation_platform::features::
                            kSegmentationPlatformComposePromotion)},

    {"compose-selection-nudge", flag_descriptions::kComposeSelectionNudgeName,
     flag_descriptions::kComposeSelectionNudgeDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         compose::features::kEnableComposeSelectionNudge,
         kComposeSelectionNudgeVariations,
         "ComposeSelectionNudge")},

    {"compose-upfront-input-modes",
     flag_descriptions::kComposeUpfrontInputModesName,
     flag_descriptions::kComposeUpfrontInputModesDescription,
     kOsWin | kOsLinux | kOsMac | kOsCrOS,
     FEATURE_VALUE_TYPE(compose::features::kComposeUpfrontInputModes)},
#endif

    {"related-website-sets-permission-grants",
     flag_descriptions::kShowRelatedWebsiteSetsPermissionGrantsName,
     flag_descriptions::kShowRelatedWebsiteSetsPermissionGrantsDescription,
     kOsDesktop | kOsAndroid,
     FEATURE_VALUE_TYPE(
         permissions::features::kShowRelatedWebsiteSetsPermissionGrants)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"coral-feature-key", flag_descriptions::kCoralFeatureKeyName,
     flag_descriptions::kCoralFeatureKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kCoralFeatureKey, "")},

    {"cros-switcher", flag_descriptions::kCrosSwitcherName,
     flag_descriptions::kCrosSwitcherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kCrosSwitcher)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"platform-keys-aes-encryption",
     flag_descriptions::kPlatformKeysAesEncryptionName,
     flag_descriptions::kPlatformKeysAesEncryptionDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kPlatformKeysAesEncryption)},
#endif  // BUILDFLAG(IS_CHROMEOS)

    {"autofill-enable-save-card-loading-and-confirmation",
     flag_descriptions::kAutofillEnableSaveCardLoadingAndConfirmationName,
     flag_descriptions::
         kAutofillEnableSaveCardLoadingAndConfirmationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation)},

    {"autofill-enable-vcn-enroll-loading-and-confirmation",
     flag_descriptions::kAutofillEnableVcnEnrollLoadingAndConfirmationName,
     flag_descriptions::
         kAutofillEnableVcnEnrollLoadingAndConfirmationDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation)},

#if BUILDFLAG(IS_ANDROID)
    {"boarding-pass-detector", flag_descriptions::kBoardingPassDetectorName,
     flag_descriptions::kBoardingPassDetectorDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(features::kBoardingPassDetector,
                                    kBoardingPassDetectorVariations,
                                    "Allowed Urls")},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cloud-gaming-device", flag_descriptions::kCloudGamingDeviceName,
     flag_descriptions::kCloudGamingDeviceDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCloudGamingDevice)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {flag_descriptions::kCompactModeId, flag_descriptions::kCompactModeName,
     flag_descriptions::kCompactModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kCompactMode)},

    {"content-settings-partitioning",
     flag_descriptions::kContentSettingsPartitioningName,
     flag_descriptions::kContentSettingsPartitioningDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         content_settings::features::kContentSettingsPartitioning)},

#if BUILDFLAG(IS_ANDROID)
    {"use-fullscreen-insets-api",
     flag_descriptions::kFullscreenInsetsApiMigrationName,
     flag_descriptions::kFullscreenInsetsApiMigrationDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kFullscreenInsetsApiMigration)},

    {"use-fullscreen-insets-api-on-automotive",
     flag_descriptions::kFullscreenInsetsApiMigrationOnAutomotiveName,
     flag_descriptions::kFullscreenInsetsApiMigrationOnAutomotiveDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kFullscreenInsetsApiMigrationOnAutomotive)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
    {"enable-mac-ime-live-conversion-fix",
     flag_descriptions::kMacImeLiveConversionFixName,
     flag_descriptions::kMacImeLiveConversionFixDescription, kOsMac,
     FEATURE_VALUE_TYPE(features::kMacImeLiveConversionFix)},
#endif

    {"tear-off-web-app-tab-opens-web-app-window",
     flag_descriptions::kTearOffWebAppAppTabOpensWebAppWindowName,
     flag_descriptions::kTearOffWebAppAppTabOpensWebAppWindowDescription,
     kOsAll, FEATURE_VALUE_TYPE(features::kTearOffWebAppTabOpensWebAppWindow)},

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-local-iban",
     flag_descriptions::kAutofillEnableLocalIbanName,
     flag_descriptions::kAutofillEnableLocalIbanDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableLocalIban)},
#endif

#if BUILDFLAG(IS_ANDROID)
    {"offline-auto-fetch", flag_descriptions::kOfflineAutoFetchName,
     flag_descriptions::kOfflineAutoFetchDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kOfflineAutoFetch)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"offline-content-on-net-error",
     flag_descriptions::kOfflineContentOnNetErrorName,
     flag_descriptions::kOfflineContentOnNetErrorDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kOfflineContentOnNetError)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kAssistantIphInternalName, flag_descriptions::kAssistantIphName,
     flag_descriptions::kAssistantIphDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHLauncherSearchHelpUiFeature)},

    {"battery-badge-icon", flag_descriptions::kBatteryBadgeIconName,
     flag_descriptions::kBatteryBadgeIconDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBatteryBadgeIcon)},

    {"bluetooth-wifi-qs-pod-refresh",
     flag_descriptions::kBluetoothWifiQSPodRefreshName,
     flag_descriptions::kBluetoothWifiQSPodRefreshDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kBluetoothWifiQSPodRefresh)},

    {"container", flag_descriptions::kContainerName,
     flag_descriptions::kContainerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kContainerAppPreinstall)},

    {"container-debug", flag_descriptions::kContainerDebugName,
     flag_descriptions::kContainerDebugDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kContainerAppPreinstallDebug)},

    {"mahi", flag_descriptions::kMahiName, flag_descriptions::kMahiDescription,
     kOsCrOS, FEATURE_VALUE_TYPE(chromeos::features::kMahi)},

    {"mahi-debugging", flag_descriptions::kMahiDebuggingName,
     flag_descriptions::kMahiDebuggingDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kMahiDebugging)},

    {"notification-width-increase",
     flag_descriptions::kNotificationWidthIncreaseName,
     flag_descriptions::kNotificationWidthIncreaseDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kNotificationWidthIncrease)},

    {"sparky", flag_descriptions::kSparkyName,
     flag_descriptions::kSparkyDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kSparky)},

    {"sparky-feature-key", flag_descriptions::kSparkyFeatureKeyName,
     flag_descriptions::kSparkyFeatureKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kSparkyFeatureKey, "")},

    {"sparky-server-url", flag_descriptions::kSparkyServerUrlName,
     flag_descriptions::kSparkyServerUrlDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kSparkyServerUrl, "")},

    {"mahi-feature-key", flag_descriptions::kMahiFeatureKeyName,
     flag_descriptions::kMahiFeatureKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kMahiFeatureKey, "")},

    {"ash-picker", flag_descriptions::kAshPickerName,
     flag_descriptions::kAshPickerDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPicker)},

    {"ash-picker-always-show-feature-tour",
     flag_descriptions::kAshPickerAlwaysShowFeatureTourName,
     flag_descriptions::kAshPickerAlwaysShowFeatureTourDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPickerAlwaysShowFeatureTour)},

    {"ash-picker-gifs", flag_descriptions::kAshPickerGifsName,
     flag_descriptions::kAshPickerGifsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPickerGifs)},

    {"ash-picker-grid", flag_descriptions::kAshPickerGridName,
     flag_descriptions::kAshPickerGridDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kPickerGrid)},

    {"ash-modifier-split", flag_descriptions::kAshModifierSplitName,
     flag_descriptions::kAshModifierSplitDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kModifierSplit)},

    {"ash-modifier-split-feature-key",
     flag_descriptions::kAshModifierSplitFeatureKeyName,
     flag_descriptions::kAshModifierSplitFeatureKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kModifierSplitFeatureKey, "")},

    {"ash-split-keyboard-refactor",
     flag_descriptions::kAshSplitKeyboardRefactorName,
     flag_descriptions::kAshSplitKeyboardRefactorDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kSplitKeyboardRefactor)},

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
    {"password-manual-fallback-available",
     flag_descriptions::kPasswordManualFallbackAvailableName,
     flag_descriptions::kPasswordManualFallbackAvailableDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordManualFallbackAvailable)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"force-signin-flows-profile-picker",
     flag_descriptions::kForceSigninFlowInProfilePickerName,
     flag_descriptions::kForceSigninFlowInProfilePickerDescription,
     kOsMac | kOsWin, FEATURE_VALUE_TYPE(kForceSigninFlowInProfilePicker)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"enable-unrestricted-usb", flag_descriptions::kEnableUnrestrictedUsbName,
     flag_descriptions::kEnableUnrestrictedUsbDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kUnrestrictedUsb)},

    {"autofill-enable-vcn-3ds-authentication",
     flag_descriptions::kAutofillEnableVcn3dsAuthenticationName,
     flag_descriptions::kAutofillEnableVcn3dsAuthenticationDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVcn3dsAuthentication)},

#if BUILDFLAG(IS_CHROMEOS)
    {"cros-locked-mode", flag_descriptions::kLockedModeName,
     flag_descriptions::kLockedModeDescription, kOsCrOS | kOsLacros,
     FEATURE_VALUE_TYPE(blink::features::kLockedMode)},
#endif  // BUILDFLAG_(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
    {"link-preview", flag_descriptions::kLinkPreviewName,
     flag_descriptions::kLinkPreviewDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(blink::features::kLinkPreview,
                                    kLinkPreviewTriggerTypeVariations,
                                    "LinkPreview")},
#endif  // !BUILDFLAG_(IS_ANDROID)

    {"send-tab-ios-push-notifications",
     flag_descriptions::kSendTabToSelfIOSPushNotificationsName,
     flag_descriptions::kSendTabToSelfIOSPushNotificationsDescription, kOsAll,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfIOSPushNotifications)},

#if BUILDFLAG(IS_ANDROID)
    {"bookmarks-and-reading-list-behind-opt-in",
     flag_descriptions::kAccountBookmarksAndReadingListBehindOptInName,
     flag_descriptions::kAccountBookmarksAndReadingListBehindOptInDescription,
     kOsAndroid,
     MULTI_VALUE_TYPE(kAccountBookmarksAndReadingListBehindOptInChoices)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"desk-profiles", flag_descriptions::kDeskProfilesName,
     flag_descriptions::kDeskProfilesDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kDeskProfiles)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    {"captive-portal-popup-window",
     flag_descriptions::kCaptivePortalPopupWindowName,
     flag_descriptions::kCaptivePortalPopupWindowDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCaptivePortalPopupWindow)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"enable-display-performance-mode",
     flag_descriptions::kEnableDisplayPerformanceModeName,
     flag_descriptions::kEnableDisplayPerformanceModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kDisplayPerformanceMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"android-tab-declutter", flag_descriptions::kAndroidTabDeclutterName,
     flag_descriptions::kAndroidTabDeclutterDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kAndroidTabDeclutter)},

    {"android-tab-declutter-archive-all-but-active-tab",
     flag_descriptions::kAndroidTabDeclutterArchiveAllButActiveTabName,
     flag_descriptions::kAndroidTabDeclutterArchiveAllButActiveTabDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kAndroidTabDeclutterArchiveAllButActiveTab)},

    {"force-list-tab-switcher", flag_descriptions::kForceListTabSwitcherName,
     flag_descriptions::kForceListTabSwitcherDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kForceListTabSwitcher)},

    {"tab-group-sync-android", flag_descriptions::kTabGroupSyncAndroidName,
     flag_descriptions::kTabGroupSyncAndroidDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupSyncAndroid)},

    {"tab-group-sync-disable-network-layer",
     flag_descriptions::kTabGroupSyncDisableNetworkLayerName,
     flag_descriptions::kTabGroupSyncDisableNetworkLayerDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupSyncDisableNetworkLayer)},

    {"tab-group-sync-force-off", flag_descriptions::kTabGroupSyncForceOffName,
     flag_descriptions::kTabGroupSyncForceOffDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(tab_groups::kTabGroupSyncForceOff)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-shared-storage-server-card-data",
     flag_descriptions::kAutofillSharedStorageServerCardDataName,
     flag_descriptions::kAutofillSharedStorageServerCardDataDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSharedStorageServerCardData)},

#if BUILDFLAG(IS_ANDROID)
    {"android-open-pdf-inline", flag_descriptions::kAndroidOpenPdfInlineName,
     flag_descriptions::kAndroidOpenPdfInlineDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidOpenPdfInline)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"page-info-sharing", flag_descriptions::kChromePageInfoSharingName,
     flag_descriptions::kChromePageInfoSharingDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kChromeSharePageInfo)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"multi-calendar-in-quick-settings",
     flag_descriptions::kMultiCalendarSupportName,
     flag_descriptions::kMultiCalendarSupportDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kMultiCalendarSupport)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-syncing-of-pix-bank-accounts",
     flag_descriptions::kAutofillEnableSyncingOfPixBankAccountsName,
     flag_descriptions::kAutofillEnableSyncingOfPixBankAccountsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSyncingOfPixBankAccounts)},

    {"enable-pix-payments", flag_descriptions::kEnablePixPaymentsName,
     flag_descriptions::kEnablePixPaymentsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(payments::facilitated::kEnablePixPayments)},

    {"enable-pix-payments-in-landscape-mode",
     flag_descriptions::kEnablePixPaymentsInLandscapeModeName,
     flag_descriptions::kEnablePixPaymentsInLandscapeModeDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         payments::facilitated::kEnablePixPaymentsInLandscapeMode)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-card-benefits-for-american-express",
     flag_descriptions::kAutofillEnableCardBenefitsForAmericanExpressName,
     flag_descriptions::
         kAutofillEnableCardBenefitsForAmericanExpressDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardBenefitsForAmericanExpress)},

    {"autofill-enable-card-benefits-for-capital-one",
     flag_descriptions::kAutofillEnableCardBenefitsForCapitalOneName,
     flag_descriptions::kAutofillEnableCardBenefitsForCapitalOneDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardBenefitsForCapitalOne)},

    {"autofill-enable-card-benefits-sync",
     flag_descriptions::kAutofillEnableCardBenefitsSyncName,
     flag_descriptions::kAutofillEnableCardBenefitsSyncDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsSync)},

    {"linked-services-setting", flag_descriptions::kLinkedServicesSettingName,
     flag_descriptions::kLinkedServicesSettingDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kLinkedServicesSetting)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-mall", flag_descriptions::kCrosMallName,
     flag_descriptions::kCrosMallDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrosMall)},
    {"cros-mall-swa", flag_descriptions::kCrosMallSwaName,
     flag_descriptions::kCrosMallSwaDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(chromeos::features::kCrosMallSwa)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-vcn-gray-out-for-merchant-opt-out",
     flag_descriptions::kAutofillEnableVcnGrayOutForMerchantOptOutName,
     flag_descriptions::kAutofillEnableVcnGrayOutForMerchantOptOutDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVcnGrayOutForMerchantOptOut)},

#if BUILDFLAG(IS_MAC)
    {"reduce-ip-address-change-notification",
     flag_descriptions::kReduceIPAddressChangeNotificationName,
     flag_descriptions::kReduceIPAddressChangeNotificationDescription, kOsMac,
     FEATURE_VALUE_TYPE(net::features::kReduceIPAddressChangeNotification)},
#endif  // BUILDFLAG(IS_MAC)

    {"autofill-enable-save-card-local-save-fallback",
     flag_descriptions::kAutofillEnableSaveCardLocalSaveFallbackName,
     flag_descriptions::kAutofillEnableSaveCardLocalSaveFallbackDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSaveCardLocalSaveFallback)},

    {"enable-fingerprinting-protection-blocklist",
     flag_descriptions::kEnableFingerprintingProtectionBlocklistName,
     flag_descriptions::kEnableFingerprintingProtectionBlocklistDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(fingerprinting_protection_filter::features::
                            kEnableFingerprintingProtectionFilter)},

#if BUILDFLAG(IS_WIN)
    {"authenticate-using-user-consent-verifier-api",
     flag_descriptions::kAuthenticateUsingUserConsentVerifierApiName,
     flag_descriptions::kAuthenticateUsingUserConsentVerifierApiDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(
         password_manager::features::kAuthenticateUsingUserConsentVerifierApi)},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"ash-forest-feature", flag_descriptions::kForestFeatureName,
     flag_descriptions::kForestFeatureDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kForestFeature)},
    {"birch-weather", flag_descriptions::kBirchWeatherName,
     flag_descriptions::kBirchWeatherDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kBirchWeather)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
    {"enable-standard-device-bound-session-credentials",
     flag_descriptions::kEnableStandardBoundSessionCredentialsName,
     flag_descriptions::kEnableStandardBoundSessionCredentialsDescription,
     kOsWin, FEATURE_VALUE_TYPE(net::features::kDeviceBoundSessions)},
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

#if BUILDFLAG(IS_ANDROID)
    {"android-no-surface-sync-for-browser-controls",
     flag_descriptions::kAndroidNoSurfaceSyncForBrowserControlsName,
     flag_descriptions::kAndroidNoSurfaceSyncForBrowserControlsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidNoSurfaceSyncForBrowserControls)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-soul-gd", flag_descriptions::kCrosSoulGravediggerName,
     flag_descriptions::kCrosSoulGravediggerDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootGravedigger")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"clear-login-database-for-all-migrated-upm-users",
     flag_descriptions::kClearLoginDatabaseForAllMigratedUPMUsersName,
     flag_descriptions::kClearLoginDatabaseForAllMigratedUPMUsersDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kClearLoginDatabaseForAllMigratedUPMUsers)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"clear-undecryptable-passwords",
     flag_descriptions::kClearUndecryptablePasswordsName,
     flag_descriptions::kClearUndecryptablePasswordsDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kClearUndecryptablePasswords)},

#if BUILDFLAG(IS_ANDROID)
    {"replace-sync-promos-with-sign-in-promos",
     flag_descriptions::kReplaceSyncPromosWithSignInPromosName,
     flag_descriptions::kReplaceSyncPromosWithSignInPromosDescription,
     kOsAndroid, MULTI_VALUE_TYPE(kReplaceSyncPromosWithSignInPromosChoices)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-verve-card-support",
     flag_descriptions::kAutofillEnableVerveCardSupportName,
     flag_descriptions::kAutofillEnableVerveCardSupportDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVerveCardSupport)},

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
    {"cert-management-v2-ui", flag_descriptions::kEnableCertManagementV2UIName,
     flag_descriptions::kEnableCertManagementV2UIDescription,
     kOsLinux | kOsMac | kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(features::kEnableCertManagementUIV2)},
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_ANDROID)
    {"autofill-skip-android-bottom-sheet-for-iban",
     flag_descriptions::kAutofillSkipAndroidBottomSheetForIbanName,
     flag_descriptions::kAutofillSkipAndroidBottomSheetForIbanDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSkipAndroidBottomSheetForIban)},
#endif

    {"password-leak-toggle-move",
     flag_descriptions::kPasswordLeakToggleMoveName,
     flag_descriptions::kPasswordLeakToggleMoveDescription, kOsAll,
     FEATURE_VALUE_TYPE(safe_browsing::kPasswordLeakToggleMove)},

    {"pwm-show-suggestions-on-autofocus",
     flag_descriptions::kPasswordManagerShowSuggestionsOnAutofocusName,
     flag_descriptions::kPasswordManagerShowSuggestionsOnAutofocusDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         password_manager::features::kShowSuggestionsOnAutofocus)},

#if BUILDFLAG(IS_ANDROID)
    {"fetch-gaia-hash-on-sign-in",
     flag_descriptions::kFetchGaiaHashOnSignInName,
     flag_descriptions::kFetchGaiaHashOnSignInDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(password_manager::features::kFetchGaiaHashOnSignIn)},

    {"android-browser-controls-in-viz",
     flag_descriptions::kAndroidBrowserControlsInVizName,
     flag_descriptions::kAndroidBrowserControlsInVizDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kAndroidBrowserControlsInViz)},
#endif

    {"optimization-guide-enable-dogfood-logging",
     flag_descriptions::kOptimizationGuideEnableDogfoodLoggingName,
     flag_descriptions::kOptimizationGuideEnableDogfoodLoggingDescription,
     kOsAll,
     SINGLE_VALUE_TYPE(
         optimization_guide::switches::kEnableModelQualityDogfoodLogging)},

#if !BUILDFLAG(IS_ANDROID)
    {"web-authentication-enclave-authenticator",
     flag_descriptions::kWebAuthnEnclaveAuthenticatorName,
     flag_descriptions::kWebAuthnEnclaveAuthenticatorDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(device::kWebAuthnEnclaveAuthenticator,
                                    kWebAuthnEnclaveAuthenticatorVariations,
                                    "WebAuthenticationEnclaveAuthenticator")},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"conch", flag_descriptions::kConchName,
     flag_descriptions::kConchDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kConch)},

    {"conch-key", flag_descriptions::kConchFeatureKeyName,
     flag_descriptions::kConchFeatureKeyDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kConchKey, "")},

    {"conch-system-audio-from-mic",
     flag_descriptions::kConchSystemAudioFromMicName,
     flag_descriptions::kConchSystemAudioFromMicDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(ash::features::kConchSystemAudioFromMic)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"autofill-sync-ewallet-accounts",
     flag_descriptions::kAutofillSyncEwalletAccountsName,
     flag_descriptions::kAutofillSyncEwalletAccountsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSyncEwalletAccounts)},

    {"ewallet-payments", flag_descriptions::kEwalletPaymentsName,
     flag_descriptions::kEwalletPaymentsDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(payments::facilitated::kEwalletPayments)},

    {"payment-link-detection", flag_descriptions::kPaymentLinkDetectionName,
     flag_descriptions::kPaymentLinkDetectionDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(blink::features::kPaymentLinkDetection)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"screenlock-reauth", flag_descriptions::kScreenlockReauthCardName,
     flag_descriptions::kScreenlockReauthCardDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(password_manager::features::kBiometricsAuthForPwdFill)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
    {"screenlock-reauth-promo-card",
     flag_descriptions::kScreenlockReauthPromoCardName,
     flag_descriptions::kScreenlockReauthPromoCardDescription,
     kOsMac | kOsWin | kOsCrOS,
     FEATURE_VALUE_TYPE(
         password_manager::features::kScreenlockReauthPromoCard)},
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

    {"ruby-short-heuristics", flag_descriptions::kRubyShortHeuristicsName,
     flag_descriptions::kRubyShortHeuristicsDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kRubyShortHeuristics)},

#if BUILDFLAG(IS_ANDROID)
    {"pwm-access-loss-warning",
     flag_descriptions::
         kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarningName,
     flag_descriptions::
         kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarningDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::
             kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"prompt-api-for-gemini-nano",
     flag_descriptions::kPromptAPIForGeminiNanoName,
     flag_descriptions::kPromptAPIForGeminiNanoDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEnableAIPromptAPIForWebPlatform),
     flag_descriptions::kAIAPIsForGeminiNanoLinks},

    {"summarization-api-for-gemini-nano",
     flag_descriptions::kSummarizationAPIForGeminiNanoName,
     flag_descriptions::kSummarizationAPIForGeminiNanoDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEnableAISummarizationAPI),
     flag_descriptions::kAIAPIsForGeminiNanoLinks},

    {"writer-api-for-gemini-nano",
     flag_descriptions::kWriterAPIForGeminiNanoName,
     flag_descriptions::kWriterAPIForGeminiNanoDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEnableAIWriterAPI),
     flag_descriptions::kAIAPIsForGeminiNanoLinks},

    {"rewriter-api-for-gemini-nano",
     flag_descriptions::kRewriterAPIForGeminiNanoName,
     flag_descriptions::kRewriterAPIForGeminiNanoDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kEnableAIRewriterAPI),
     flag_descriptions::kAIAPIsForGeminiNanoLinks},

    {"text-box-trim", flag_descriptions::kCssTextBoxTrimName,
     flag_descriptions::kCssTextBoxTrimDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCSSTextBoxTrim)},

    {"storage-access-headers", flag_descriptions::kStorageAccessHeadersName,
     flag_descriptions::kStorageAccessHeadersDescription, kOsAll,
     FEATURE_VALUE_TYPE(network::features::kStorageAccessHeaders)},

    {"autofill-upstream-updated-ui",
     flag_descriptions::kAutofillUpstreamUpdatedUiName,
     flag_descriptions::kAutofillUpstreamUpdatedUiDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUpstreamUpdatedUi,
         kAutofillUpstreamUpdatedUiOptions,
         "AutofillUpstreamUpdatedUi")},

    {"canvas-2d-hibernation", flag_descriptions::kCanvasHibernationName,
     flag_descriptions::kCanvasHibernationDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kCanvas2DHibernation)},
#if !BUILDFLAG(IS_ANDROID)
    {"performance-intervention-ui",
     flag_descriptions::kPerformanceInterventionUiName,
     flag_descriptions::kPerformanceInterventionUiDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         performance_manager::features::kPerformanceInterventionUI,
         kPerformanceInterventionStringVariations,
         "PerformanceInterventionUI")},
    {"performance-intervention-demo-mode",
     flag_descriptions::kPerformanceInterventionDemoModeName,
     flag_descriptions::kPerformanceInterventionDemoModeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         performance_manager::features::kPerformanceInterventionDemoMode)},
#endif

#if BUILDFLAG(IS_WIN)
    {"authenticate-using-user-consent-verifier-interop-api",
     flag_descriptions::kAuthenticateUsingUserConsentVerifierInteropApiName,
     flag_descriptions::
         kAuthenticateUsingUserConsentVerifierInteropApiDescription,
     kOsWin,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kAuthenticateUsingUserConsentVerifierInteropApi)},
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    {"sync-enable-bookmarks-in-transport-mode",
     flag_descriptions::kSyncEnableBookmarksInTransportModeName,
     flag_descriptions::kSyncEnableBookmarksInTransportModeDescription,
     kOsWin | kOsMac | kOsLinux,
     FEATURE_VALUE_TYPE(syncer::kSyncEnableBookmarksInTransportMode)},
#endif

    {"visited-url-ranking-service-history-visibility-score-filter",
     flag_descriptions::
         kVisitedURLRankingServiceHistoryVisibilityScoreFilterName,
     flag_descriptions::
         kVisitedURLRankingServiceHistoryVisibilityScoreFilterDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(visited_url_ranking::features::
                            kVisitedURLRankingHistoryVisibilityScoreFilter)},

    {"defer-renderer-tasks-after-input",
     flag_descriptions::kDeferRendererTasksAfterInputName,
     flag_descriptions::kDeferRendererTasksAfterInputDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kDeferRendererTasksAfterInput,
         kDeferRendererTasksAfterInputVariations,
         "DeferRendererTasksAfterInput")},

    {"blink-scheduler-discrete-input-matches-responsiveness-metrics",
     flag_descriptions::
         kBlinkSchedulerDiscreteInputMatchesResponsivenessMetricsName,
     flag_descriptions::
         kBlinkSchedulerDiscreteInputMatchesResponsivenessMetricsDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         blink::features::
             kBlinkSchedulerDiscreteInputMatchesResponsivenessMetrics)},

    {"threaded-scroll-prevent-rendering-starvation",
     flag_descriptions::kThreadedScrollPreventRenderingStarvationName,
     flag_descriptions::kThreadedScrollPreventRenderingStarvationDescription,
     kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         blink::features::kThreadedScrollPreventRenderingStarvation,
         kThreadedScrollPreventRenderingStarvationVariations,
         "ThreadedScrollPreventRenderingStarvation")},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"allow-fpmcu-beta-firmware",
     flag_descriptions::kAllowFpmcuBetaFirmwareName,
     flag_descriptions::kAllowFpmcuBetaFirmwareDescription, kOsCrOS,
     PLATFORM_FEATURE_NAME_TYPE("CrOSLateBootAllowFpmcuBetaFirmware")},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-upload-card-request-timeout",
     flag_descriptions::kAutofillUploadCardRequestTimeoutName,
     flag_descriptions::kAutofillUploadCardRequestTimeoutDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUploadCardRequestTimeout,
         kAutofillUploadCardRequestTimeoutOptions,
         "AutofillUploadCardRequestTimeout")},

    {"autofill-vcn-enroll-request-timeout",
     flag_descriptions::kAutofillVcnEnrollRequestTimeoutName,
     flag_descriptions::kAutofillVcnEnrollRequestTimeoutDescription, kOsAll,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillVcnEnrollRequestTimeout,
         kAutofillVcnEnrollRequestTimeoutOptions,
         "AutofillVcnEnrollRequestTimeout")},

    {"autofill-unmask-card-request-timeout",
     flag_descriptions::kAutofillUnmaskCardRequestTimeoutName,
     flag_descriptions::kAutofillUnmaskCardRequestTimeoutDescription, kOsAll,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUnmaskCardRequestTimeout)},

#if !BUILDFLAG(IS_ANDROID)
    {"freezing-on-energy-saver", flag_descriptions::kFreezingOnEnergySaverName,
     flag_descriptions::kFreezingOnEnergySaverDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         performance_manager::features::kFreezingOnBatterySaver)},

    {"freezing-on-energy-saver-testing",
     flag_descriptions::kFreezingOnEnergySaverTestingName,
     flag_descriptions::kFreezingOnEnergySaverTestingDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         performance_manager::features::kFreezingOnBatterySaverForTesting)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    {"hid-get-feature-report-fix",
     flag_descriptions::kHidGetFeatureReportFixName,
     flag_descriptions::kHidGetFeatureReportFixDescription, kOsWin,
     FEATURE_VALUE_TYPE(features::kHidGetFeatureReportFix)},
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
    {"translate-open-settings", flag_descriptions::kTranslateOpenSettingsName,
     flag_descriptions::kTranslateOpenSettingsDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(language::kTranslateOpenSettings)},
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_WIN)
    {"classify-url-on-process-response-event",
     flag_descriptions::kClassifyUrlOnProcessResponseEventName,
     flag_descriptions::kClassifyUrlOnProcessResponseEventDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(::supervised_user::kClassifyUrlOnProcessResponseEvent)},
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    {"tab-organization", flag_descriptions::kTabOrganizationName,
     flag_descriptions::kTabOrganizationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabOrganization)},

    {"tab-organization-settings-visibility",
     flag_descriptions::kTabOrganizationSettingsVisibilityName,
     flag_descriptions::kTabOrganizationSettingsVisibilityDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(optimization_guide::features::internal::
                            kTabOrganizationSettingsVisibility)},

    {"multi-tab-organization", flag_descriptions::kMultiTabOrganizationName,
     flag_descriptions::kMultiTabOrganizationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kMultiTabOrganization)},

    {"tab-reorganization", flag_descriptions::kTabReorganizationName,
     flag_descriptions::kTabReorganizationDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabReorganization)},

    {"tab-reorganization-divider",
     flag_descriptions::kTabReorganizationDividerName,
     flag_descriptions::kTabReorganizationDividerDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTabReorganizationDivider)},
#endif

#if !BUILDFLAG(IS_ANDROID)
    {"autofill-remove-payments-butter-dropdown",
     flag_descriptions::kAutofillRemovePaymentsButterDropdownName,
     flag_descriptions::kAutofillRemovePaymentsButterDropdownDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRemovePaymentsButterDropdown)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"language-detection-api", flag_descriptions::kLanguageDetectionAPIName,
     flag_descriptions::kLanguageDetectionAPIDescription, kOsAll,
     FEATURE_VALUE_TYPE(blink::features::kLanguageDetectionAPI)},

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    {"supervised-profile-hide-guest",
     flag_descriptions::kSupervisedProfileHideGuestName,
     flag_descriptions::kSupervisedProfileHideGuestDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(supervised_user::kHideGuestModeForSupervisedUsers)},

    {"supervised-profile-safe-search",
     flag_descriptions::kSupervisedProfileSafeSearchName,
     flag_descriptions::kSupervisedProfileSafeSearchDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kForceSafeSearchForUnauthenticatedSupervisedUsers)},

    {"supervised-profile-youtube-reauth",
     flag_descriptions::kSupervisedProfileReauthForYouTubeName,
     flag_descriptions::kSupervisedProfileReauthForYouTubeDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kForceSupervisedUserReauthenticationForYouTube)},

    {"supervised-profile-blocked-site-reauth",
     flag_descriptions::kSupervisedProfileReauthForBlockedSiteName,
     flag_descriptions::kSupervisedProfileReauthForBlockedSiteDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kForceSupervisedUserReauthenticationForBlockedSites)},

    {"supervised-profile-subframe-reauth",
     flag_descriptions::kSupervisedProfileSubframeReauthName,
     flag_descriptions::kSupervisedProfileSubframeReauthDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kAllowSupervisedUserReauthenticationForSubframes)},

    {"supervised-profile-filtering-fallback",
     flag_descriptions::kSupervisedProfileFilteringFallbackName,
     flag_descriptions::kSupervisedProfileFilteringFallbackDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kUncredentialedFilteringFallbackForSupervisedUsers)},

    {"supervised-profile-custom-strings",
     flag_descriptions::kSupervisedProfileCustomStringsName,
     flag_descriptions::kSupervisedProfileCustomStringsDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(
         supervised_user::kCustomProfileStringsForSupervisedUsers)},

    {"supervised-profile-sign-in-iph",
     flag_descriptions::kSupervisedProfileSignInIphName,
     flag_descriptions::kSupervisedProfileSignInIphDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(supervised_user::kSupervisedUserProfileSigninIPH)},

    {"supervised-profile-kite-badging",
     flag_descriptions::kSupervisedProfileShowKiteBadgeName,
     flag_descriptions::kSupervisedProfileShowKiteBadgeDescription,
     kOsLinux | kOsMac | kOsWin,
     FEATURE_VALUE_TYPE(supervised_user::kShowKiteForSupervisedUsers)},
#endif

    {"use-frame-interval-decider",
     flag_descriptions::kUseFrameIntervalDeciderName,
     flag_descriptions::kUseFrameIntervalDeciderDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kUseFrameIntervalDecider)},

#if BUILDFLAG(IS_ANDROID)
    {"migrate-syncing-user-to-signed-in",
     flag_descriptions::kMigrateSyncingUserToSignedInName,
     flag_descriptions::kMigrateSyncingUserToSignedInDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kMigrateSyncingUserToSignedIn)},

    {"undo-migration-of-syncing-user-to-signed-in",
     flag_descriptions::kUndoMigrationOfSyncingUserToSignedInName,
     flag_descriptions::kUndoMigrationOfSyncingUserToSignedInDescription,
     flags_ui::kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kUndoMigrationOfSyncingUserToSignedIn)},

    {"sensitive-content", flag_descriptions::kSensitiveContentName,
     flag_descriptions::kSensitiveContentDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         sensitive_content::features::kSensitiveContent,
         kSensitiveContentVariations,
         "SensitiveContent")},

    {"sensitive-content-while-switching-tabs",
     flag_descriptions::kSensitiveContentWhileSwitchingTabsName,
     flag_descriptions::kSensitiveContentWhileSwitchingTabsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         sensitive_content::features::kSensitiveContentWhileSwitchingTabs)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"data-sharing", flag_descriptions::kDataSharingName,
     flag_descriptions::kDataSharingDescription, kOsAll,
     FEATURE_VALUE_TYPE(data_sharing::features::kDataSharingFeature)},

    {"data-sharing-join-only", flag_descriptions::kDataSharingJoinOnlyName,
     flag_descriptions::kDataSharingJoinOnlyDescription, kOsAll,
     FEATURE_VALUE_TYPE(data_sharing::features::kDataSharingJoinOnly)},

    {"history-sync-alternative-illustration",
     flag_descriptions::kHistorySyncAlternativeIllustrationName,
     flag_descriptions::kHistorySyncAlternativeIllustrationDescription, kOsAll,
     FEATURE_VALUE_TYPE(tab_groups::kUseAlternateHistorySyncIllustration)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-content-adjusted-refresh-rate",
     flag_descriptions::kCrosContentAdjustedRefreshRateName,
     flag_descriptions::kCrosContentAdjustedRefreshRateDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kCrosContentAdjustedRefreshRate)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-cvc-storage-and-filling-enhancement",
     flag_descriptions::kAutofillEnableCvcStorageAndFillingEnhancementName,
     flag_descriptions::
         kAutofillEnableCvcStorageAndFillingEnhancementDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCvcStorageAndFillingEnhancement)},

    {"improved-signin-ui-on-desktop",
     flag_descriptions::kImprovedSigninUIOnDesktopName,
     flag_descriptions::kImprovedSigninUIOnDesktopDescription,
     kOsMac | kOsWin | kOsLinux,
     FEATURE_VALUE_TYPE(switches::kImprovedSigninUIOnDesktop)},

    {"outline-silhouette-icon", flag_descriptions::kOutlineSilhouetteIconName,
     flag_descriptions::kOutlineSilhouetteIconDescription,
     kOsMac | kOsWin | kOsLinux, FEATURE_VALUE_TYPE(kOutlineSilhouetteIcon)},

#if BUILDFLAG(IS_ANDROID)
    {"discount-on-navigation",
     commerce::flag_descriptions::kDiscountOnNavigationName,
     commerce::flag_descriptions::kDiscountOnNavigationDescription, kOsAndroid,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kEnableDiscountInfoApi,
                                    kDiscountsVariationsOnAndroid,
                                    "DisocuntOnNavigation")},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"discount-on-navigation",
     commerce::flag_descriptions::kDiscountOnNavigationName,
     commerce::flag_descriptions::kDiscountOnNavigationDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kEnableDiscountInfoApi,
                                    kDiscountsVariations,
                                    "DisocuntOnNavigation")},
#endif  //! BUILDFLAG(IS_ANDROID)

    {"devtools-privacy-ui", flag_descriptions::kDevToolsPrivacyUIName,
     flag_descriptions::kDevToolsPrivacyUIDescription, kOsAll,
     FEATURE_VALUE_TYPE(features::kDevToolsPrivacyUI)},

    {"permission-predictions-v3",
     flag_descriptions::kPermissionPredictionsV3Name,
     flag_descriptions::kPermissionPredictionsV3Description, kOsAll,
     FEATURE_VALUE_TYPE(permissions::features::kPermissionPredictionsV3)},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"exclude-display-in-mirror-mode",
     flag_descriptions::kExcludeDisplayInMirrorModeName,
     flag_descriptions::kExcludeDisplayInMirrorModeDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(display::features::kExcludeDisplayInMirrorMode)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    {"enable-task-manager-clank", flag_descriptions::kTaskManagerClankName,
     flag_descriptions::kTaskManagerClankDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(features::kTaskManagerClank)},
#else
    {"enable-task-manager-desktop-refresh",
     flag_descriptions::kTaskManagerDesktopRefreshName,
     flag_descriptions::kTaskManagerDesktopRefreshDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(features::kTaskManagerDesktopRefresh)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"autofill-enable-payment-settings-card-promo-and-scan-card",
     flag_descriptions::kAutofillEnablePaymentSettingsCardPromoAndScanCardName,
     flag_descriptions::
         kAutofillEnablePaymentSettingsCardPromoAndScanCardDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnablePaymentSettingsCardPromoAndScanCard)},

    {"autofill-enable-payment-settings-server-card-save",
     flag_descriptions::kAutofillEnablePaymentSettingsServerCardSaveName,
     flag_descriptions::kAutofillEnablePaymentSettingsServerCardSaveDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnablePaymentSettingsServerCardSave)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"cert-verification-network-time",
     flag_descriptions::kCertVerificationNetworkTimeName,
     flag_descriptions::kCertVerificationNetworkTimeDescription,
     kOsMac | kOsWin | kOsLinux | kOsAndroid,
     FEATURE_VALUE_TYPE(features::kCertVerificationNetworkTime)},

#if !BUILDFLAG(IS_ANDROID)
    {"enable-lens-overlay-translate-button",
     flag_descriptions::kLensOverlayTranslateButtonName,
     flag_descriptions::kLensOverlayTranslateButtonDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(lens::features::kLensOverlayTranslateButton)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"enable-lens-overlay-contextual-search-box",
     flag_descriptions::kLensOverlayContextualSearchboxName,
     flag_descriptions::kLensOverlayContextualSearchboxDescription, kOsDesktop,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         lens::features::kLensOverlayContextualSearchbox,
         kContextualSearchboxVariations,
         "LensOverlayContextualSearchbox")},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"jump-start-omnibox", flag_descriptions::kJumpStartOmniboxName,
     flag_descriptions::kJumpStartOmniboxDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kJumpStartOmnibox)},
    {"retain-omnibox-on-focus", flag_descriptions::kRetainOmniboxOnFocusName,
     flag_descriptions::kRetainOmniboxOnFocusDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(omnibox::kRetainOmniboxOnFocus)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-log-form-events-to-all-parsed-form-types",
     flag_descriptions::kAutofillEnableLogFormEventsToAllParsedFormTypesName,
     flag_descriptions::
         kAutofillEnableLogFormEventsToAllParsedFormTypesDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableLogFormEventsToAllParsedFormTypes)},

    {"enable-segmentation-internals-survey",
     flag_descriptions::kSegmentationSurveyPageName,
     flag_descriptions::kSegmentationSurveyPageDescription, kOsAll,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kSegmentationSurveyPage)},

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    {"autofill-enable-buy-now-pay-later-for-affirm",
     flag_descriptions::kAutofillEnableBuyNowPayLaterForAffirmName,
     flag_descriptions::kAutofillEnableBuyNowPayLaterForAffirmDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableBuyNowPayLaterForAffirm)},

    {"autofill-enable-buy-now-pay-later-for-zip",
     flag_descriptions::kAutofillEnableBuyNowPayLaterForZipName,
     flag_descriptions::kAutofillEnableBuyNowPayLaterForZipDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableBuyNowPayLaterForZip)},

    {"autofill-enable-buy-now-pay-later-syncing",
     flag_descriptions::kAutofillEnableBuyNowPayLaterSyncingName,
     flag_descriptions::kAutofillEnableBuyNowPayLaterSyncingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableBuyNowPayLaterSyncing)},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
    {"biometric-auth-identity-check",
     flag_descriptions::kBiometricAuthIdentityCheckName,
     flag_descriptions::kBiometricAuthIdentityCheckDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(
         password_manager::features::kBiometricAuthIdentityCheck)},
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    {"password-generation-soft-nudge",
     flag_descriptions::kPasswordGenerationSoftNudgeName,
     flag_descriptions::kPasswordGenerationSoftNudgeDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordGenerationSoftNudge)},
#endif  // !BUILDFLAG(IS_ANDROID)

    {"autofill-enable-cvc-storage-and-filling-standalone-form-enhancement",
     flag_descriptions::
         kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementName,
     flag_descriptions::
         kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancementDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)},
#if BUILDFLAG(IS_ANDROID)
    {"cct-sign-in-prompt", flag_descriptions::kCCTSignInPromptName,
     flag_descriptions::kCCTSignInPromptDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(switches::kCctSignInPrompt)},
#endif

    {"enable-bookmarks-selected-type-on-signin-for-testing",
     flag_descriptions::kEnableBookmarksSelectedTypeOnSigninForTestingName,
     flag_descriptions::
         kEnableBookmarksSelectedTypeOnSigninForTestingDescription,
     kOsDesktop,
     FEATURE_VALUE_TYPE(
         syncer::kEnableBookmarksSelectedTypeOnSigninForTesting)},

#if !BUILDFLAG(IS_ANDROID)
    {"separate-local-and-account-themes",
     flag_descriptions::kSeparateLocalAndAccountThemesName,
     flag_descriptions::kSeparateLocalAndAccountThemesDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(syncer::kSeparateLocalAndAccountThemes)},
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    {"clank-default-browser-promo-role-manager",
     flag_descriptions::kClankDefaultBrowserPromoRoleManagerName,
     flag_descriptions::kClankDefaultBrowserPromoRoleManagerDescription,
     kOsAndroid,
     SINGLE_DISABLE_VALUE_TYPE(switches::kDisableDefaultBrowserPromo)},

    {"clank-default-browser-promo2",
     flag_descriptions::kClankDefaultBrowserPromoName,
     flag_descriptions::kClankDefaultBrowserPromoDescription, kOsAndroid,
     FEATURE_VALUE_TYPE(chrome::android::kDefaultBrowserPromoAndroid2)},
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"cros-mall-url", flag_descriptions::kCrosMallUrlName,
     flag_descriptions::kCrosMallUrlDescription, kOsCrOS,
     STRING_VALUE_TYPE(ash::switches::kMallUrl, "")},

    {"overlay-scrollbars-os-settings",
     flag_descriptions::kOverlayScrollbarsOSSettingsName,
     flag_descriptions::kOverlayScrollbarsOSSettingsDescription, kOsCrOS,
     FEATURE_VALUE_TYPE(features::kOverlayScrollbarsOSSetting)},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    {"autofill-enable-card-benefits-iph",
     flag_descriptions::kAutofillEnableCardBenefitsIphName,
     flag_descriptions::kAutofillEnableCardBenefitsIphDescription, kOsDesktop,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsIph)},

#if BUILDFLAG(IS_ANDROID)
    {"enable-automotive-fullscreen-toolbar-improvements",
     flag_descriptions::kAutomotiveFullscreenToolbarImprovementsName,
     flag_descriptions::kAutomotiveFullscreenToolbarImprovementsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         chrome::android::kAutomotiveFullscreenToolbarImprovements)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"privacy-sandbox-privacy-policy",
     flag_descriptions::kPrivacySandboxPrivacyPolicyName,
     flag_descriptions::kPrivacySandboxPrivacyPolicyDescription, kOsAll,
     FEATURE_VALUE_TYPE(privacy_sandbox::kPrivacySandboxPrivacyPolicy)},

#if BUILDFLAG(IS_ANDROID)
    {"support-multiple-server-requests-for-pix-payments",
     flag_descriptions::kSupportMultipleServerRequestsForPixPaymentsName,
     flag_descriptions::kSupportMultipleServerRequestsForPixPaymentsDescription,
     kOsAndroid,
     FEATURE_VALUE_TYPE(
         payments::facilitated::kSupportMultipleServerRequestsForPixPayments)},
#endif  // BUILDFLAG(IS_ANDROID)

    {"autofill-enable-card-info-runtime-retrieval",
     flag_descriptions::kAutofillEnableCardInfoRuntimeRetrievalName,
     flag_descriptions::kAutofillEnableCardInfoRuntimeRetrievalDescription,
     kOsAll,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardInfoRuntimeRetrieval)}

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This method may be invoked both synchronously or asynchronously. Based on
// whether the current user is the owner of the device, generates the
// appropriate flag storage.
void GetStorageAsync(Profile* profile,
                     GetStorageCallback callback,
                     bool current_user_is_owner) {
  // On ChromeOS the owner can set system wide flags and other users can only
  // set flags for their own session.
  if (current_user_is_owner) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
    std::move(callback).Run(
        std::make_unique<ash::about_flags::OwnerFlagsStorage>(
            profile->GetPrefs(), service),
        flags_ui::kOwnerAccessToFlags);
  } else {
    std::move(callback).Run(std::make_unique<flags_ui::PrefServiceFlagsStorage>(
                                profile->GetPrefs()),
                            flags_ui::kGeneralAccessFlagsOnly);
  }
}
#endif

// ash-chrome uses different storage flag storage logic from other desktop
// platforms.
void GetStorage(Profile* profile, GetStorageCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bypass possible incognito profile.
  // On ChromeOS the owner can set system wide flags and other users can only
  // set flags for their own session.
  Profile* original_profile = profile->GetOriginalProfile();
  if (base::SysInfo::IsRunningOnChromeOS() &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    service->IsOwnerAsync(base::BindOnce(&GetStorageAsync, original_profile,
                                         std::move(callback)));
  } else {
    GetStorageAsync(original_profile, std::move(callback),
                    /*current_user_is_owner=*/false);
  }
#else
  std::move(callback).Run(std::make_unique<flags_ui::PrefServiceFlagsStorage>(
                              g_browser_process->local_state()),
                          flags_ui::kOwnerAccessToFlags);
#endif
}

bool ShouldSkipConditionalFeatureEntry(const flags_ui::FlagsStorage* storage,
                                       const FeatureEntry& entry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  version_info::Channel channel = chrome::GetChannel();
  // enable-projector-server-side-speech-recognition is only available if
  // the InternalServerSideSpeechRecognitionControl flag is enabled as well.
  if (!strcmp(kProjectorServerSideSpeechRecognition, entry.internal_name)) {
    return !ash::features::
        IsInternalServerSideSpeechRecognitionControlEnabled();
  }

  // enable-ui-devtools is only available on for non Stable channels.
  if (!strcmp(ui_devtools::switches::kEnableUiDevTools, entry.internal_name) &&
      channel == version_info::Channel::STABLE) {
    return true;
  }

  // Skip lacros-availability-policy always. This is a pseudo entry
  // and used to carry the policy value crossing the Chrome's lifetime.
  if (!strcmp(ash::standalone_browser::kLacrosAvailabilityPolicyInternalName,
              entry.internal_name)) {
    return true;
  }

  // Skip lacros-backward-data-migration-policy always. This is a pseudo entry
  // and used to carry the policy value crossing the Chrome's lifetime.
  if (!strcmp(crosapi::browser_util::
                  kLacrosDataBackwardMigrationModePolicyInternalName,
              entry.internal_name)) {
    return true;
  }
  // Skip lacros-selection if it is controlled by LacrosSelection policy.
  if (!strcmp(kLacrosSelectionInternalName, entry.internal_name)) {
    return ash::standalone_browser::GetCachedLacrosSelectionPolicy() !=
           ash::standalone_browser::LacrosSelectionPolicy::kUserChoice;
  }

  if (!strcmp(kPreferDcheckInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosAllowedToBeEnabled();
  }

  if (!strcmp(kLacrosStabilityInternalName, entry.internal_name)) {
    return !crosapi::browser_util::IsLacrosAllowedToBeEnabled();
  }

  if (!strcmp(kArcEnableVirtioBlkForDataInternalName, entry.internal_name)) {
    return !arc::IsArcVmEnabled();
  }

  // Only show the Background Listening flag if channel is one of
  // Beta/Dev/Canary/Unknown (non-stable).
  if (!strcmp(kBackgroundListeningName, entry.internal_name)) {
    return channel != version_info::Channel::BETA &&
           channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }

  // Only show Borealis flags on enabled devices.
  if (!strcmp(kBorealisBigGlInternalName, entry.internal_name) ||
      !strcmp(kBorealisDGPUInternalName, entry.internal_name) ||
      !strcmp(kBorealisEnableUnsupportedHardwareInternalName,
              entry.internal_name) ||
      !strcmp(kBorealisForceBetaClientInternalName, entry.internal_name) ||
      !strcmp(kBorealisForceDoubleScaleInternalName, entry.internal_name) ||
      !strcmp(kBorealisLinuxModeInternalName, entry.internal_name) ||
      !strcmp(kBorealisPermittedInternalName, entry.internal_name) ||
      !strcmp(kBorealisProvisionInternalName, entry.internal_name) ||
      !strcmp(kBorealisScaleClientByDPIInternalName, entry.internal_name) ||
      !strcmp(kBorealisZinkGlDriverInternalName, entry.internal_name)) {
    return !base::FeatureList::IsEnabled(features::kBorealis);
  }

  // Only show wallpaper fast refresh flag if channel is one of
  // Dev/Canary/Unknown.
  if (!strcmp(kWallpaperFastRefreshInternalName, entry.internal_name)) {
    return (channel != version_info::Channel::DEV &&
            channel != version_info::Channel::CANARY &&
            channel != version_info::Channel::UNKNOWN);
  }

  // Only show clipboard history longpress flag if channel is one of
  // Beta/Dev/Canary/Unknown.
  if (!strcmp(kClipboardHistoryLongpressInternalName, entry.internal_name)) {
    return channel != version_info::Channel::BETA &&
           channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }

  // Disable and prevent users from enabling Floss on boards that were
  // explicitly built without it (b/228902194 for more info).
  if (!strcmp(kBluetoothUseFlossInternalName, entry.internal_name)) {
    return floss::features::IsFlossAvailabilityCheckNeeded() &&
           base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
               floss::features::kFlossIsAvailable.name,
               base::FeatureList::OVERRIDE_DISABLE_FEATURE);
  }

  // Disable and prevent users from enabling LL privacy on boards that were
  // explicitly built without floss or hardware does not support LL privacy.
  if (!strcmp(kBluetoothUseLLPrivacyInternalName, entry.internal_name)) {
    return (
        base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
            floss::features::kFlossIsAvailable.name,
            base::FeatureList::OVERRIDE_DISABLE_FEATURE) ||
        base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
            floss::features::kLLPrivacyIsAvailable.name,
            base::FeatureList::OVERRIDE_DISABLE_FEATURE));
  }

  // Only show sea-pen flag if channel is one of Dev/Canary/Unknown.
  if (!strcmp(kSeaPenInternalName, entry.internal_name)) {
    return channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }

  // Only show Assistant Launcher search IPH flag if channel is one of
  // Beta/Dev/Canary/Unknown.
  if (!strcmp(kAssistantIphInternalName, entry.internal_name)) {
    return channel != version_info::Channel::BETA &&
           channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }

  // Only show Growth campaigns flag if channel is one of Beta/Dev/Canary/
  // Unknown.
  if (!strcmp(kGrowthCampaigns, entry.internal_name)) {
    return channel != version_info::Channel::BETA &&
           channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }

  // Only show Growth campaigns test tag flag if channel is one of
  // Beta/Dev/Canary/ Unknown.
  if (!strcmp(kGrowthCampaignsTestTag, entry.internal_name)) {
    return channel != version_info::Channel::BETA &&
           channel != version_info::Channel::DEV &&
           channel != version_info::Channel::CANARY &&
           channel != version_info::Channel::UNKNOWN;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_EXTENSIONS)
  version_info::Channel chrome_channel = chrome::GetChannel();
  // Only show extension AI data flag in non-stable channels.
  if (!strcmp(kExtensionAiDataInternalName, entry.internal_name)) {
    return chrome_channel != version_info::Channel::BETA &&
           chrome_channel != version_info::Channel::DEV &&
           chrome_channel != version_info::Channel::CANARY &&
           chrome_channel != version_info::Channel::UNKNOWN;
  }
#endif
  if (flags::IsFlagExpired(storage, entry.internal_name)) {
    return true;
  }

  return false;
}

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            flags_ui::SentinelsMode sentinels) {
  if (command_line->HasSwitch(switches::kNoExperiments)) {
    return;
  }

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

void SetStringFlag(const std::string& internal_name,
                   const std::string& value,
                   flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->SetStringFlag(internal_name, value,
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
  for (const auto& entry : entries) {
    GetEntriesForTesting()->push_back(entry);  // IN-TEST
  }
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
  if (!GetEntriesForTesting()->empty()) {
    return base::span<FeatureEntry>(*GetEntriesForTesting());
  }
  return base::make_span(kFeatureEntries, std::size(kFeatureEntries));
}

}  // namespace testing

}  // namespace about_flags
