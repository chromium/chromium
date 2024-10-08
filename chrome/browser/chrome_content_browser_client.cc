// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/character_encoding.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"  // PLATFORM_CFM
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/child_process_host_flags.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/chrome_content_browser_client_binder_policies.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/device_api/managed_configuration_service.h"
#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/first_party_sets/first_party_sets_navigation_throttle.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/gpu/chrome_browser_main_extra_parts_gpu.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/media/audio_service_util.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/audio_debug_recordings_handler.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/chrome_screen_enumerator.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/browser/memory/chrome_browser_main_extra_parts_memory.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/optimization_guide/chrome_browser_main_extra_parts_optimization_guide.h"
#include "chrome/browser/payments/payment_request_display_manager_factory.h"
#include "chrome/browser/performance_manager/public/chrome_browser_main_extra_parts_performance_manager.h"
#include "chrome/browser/performance_manager/public/chrome_content_browser_client_performance_manager_part.h"
#include "chrome/browser/performance_monitor/chrome_browser_main_extra_parts_performance_monitor.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/navigation_ablation_throttle.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_speculation_host_delegate.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_navigation_throttle.h"
#include "chrome/browser/preloading/prefetch/prefetch_service/chrome_prefetch_service_delegate.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader_interceptor.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/preloading/prerender/prerender_web_contents_delegate.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_delegate.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/profiling_host/chrome_browser_main_extra_parts_profiling.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/request_header_integrity/buildflags.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/delayed_warning_navigation_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/segmentation_platform/chrome_browser_main_extra_parts_segmentation_platform.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"
#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"
#include "chrome/browser/signin/chrome_signin_url_loader_throttle.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ssl/https_defaulted_callbacks.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/task_manager/sampling/task_manager_impl.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_navigation_throttle.h"
#include "chrome/browser/ui/login/http_auth_coordinator.h"
#include "chrome/browser/ui/login/login_navigation_throttle.h"
#include "chrome/browser/ui/passwords/password_manager_navigation_throttle.h"
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/navigation_capturing_redirection_throttle.h"
#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/ui/webui/top_chrome/webui_url_utils.h"
#include "chrome/browser/universal_web_contents_observers.h"
#include "chrome/browser/usb/chrome_usb_delegate.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/webapps/web_app_offline.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/ppapi_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/private_network_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/protocol_handler_throttle.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/embedder_support/content_settings_utils.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/content/clipboard_restriction_service.h"
#include "components/enterprise/content/pref_names.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/google/core/common/google_switches.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/history/content/browser/visited_link_navigation_throttle.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/language/core/browser/pref_names.h"
#include "components/lens/buildflags.h"
#include "components/live_caption/caption_util.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/media_router/browser/presentation/receiver_presentation_service_delegate_impl.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/metrics/client_info.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/no_state_prefetch_url_loader_throttle.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/payments/content/payment_credential_factory.h"
#include "components/payments/content/payment_handler_navigation_throttle.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/pdf/common/pdf_util.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/scenarios/performance_scenarios.h"
#include "components/permissions/permission_context_base.h"
#include "components/policy/content/policy_blocklist_navigation_throttle.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_throttle.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/security_state/core/security_state.h"
#include "components/site_isolation/pref_names.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/supervised_user/core/common/features.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/data_url.h"
#include "net/base/features.h"
#include "net/cookies/site_for_cookies.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/switches.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/platform_auth/platform_auth_navigation_throttle.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "base/files/file_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/install_static/install_util.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/accessibility/accessibility_features.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process_platform_part_mac.h"
#include "chrome/browser/chrome_browser_main_mac.h"
#include "chrome/browser/mac/auth_session_request.h"
#include "chrome/browser/mac/chrome_browser_main_extra_parts_mac.h"
#include "components/soda/constants.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/policy/mac/params.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/print_management/url_constants.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "ash/webui/scanning/url_constants.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_backend_delegate.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_backend_delegate.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/drive/fileapi/drivefs_file_system_backend_delegate.h"
#include "chrome/browser/ash/file_system_provider/fileapi/backend_delegate.h"
#include "chrome/browser/ash/fileapi/external_file_url_loader_factory.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fileapi/mtp_file_system_backend_delegate.h"
#include "chrome/browser/ash/login/signin/merge_session_navigation_throttle.h"
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/main_parts/chrome_browser_main_parts_ash.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/fileapi/smbfs_file_system_backend_delegate.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/speech/tts_chromeos.h"
#include "chrome/browser/speech/tts_controller_delegate_impl.h"
#include "chrome/browser/ui/ash/main_extra_parts/chrome_browser_main_extra_parts_ash.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/services/network_health/public/cpp/network_health_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/service_manager/public/mojom/interface_provider_spec.mojom.h"
#include "storage/browser/file_system/external_mount_points.h"
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chrome_browser_main_linux.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"
#elif BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "chrome/android/features/dev_ui/buildflags.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"
#include "chrome/browser/android/service_tab_launcher.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/chrome_browser_main_android.h"
#include "chrome/browser/digital_credentials/digital_identity_provider_android.h"
#include "chrome/browser/download/android/available_offline_content_provider.h"
#include "chrome/browser/download/android/intercept_oma_download_navigation_throttle.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_descriptors.h"
#include "components/browser_ui/accessibility/android/font_size_prefs_android.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "content/public/browser/android/java_interfaces.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/display/util/display_util.h"
#if BUILDFLAG(DFMIFY_DEV_UI)
#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"
#endif  // BUILDFLAG(DFMIFY_DEV_UI)
#elif BUILDFLAG(IS_POSIX)
#include "chrome/browser/chrome_browser_main_posix.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/digital_credentials/digital_identity_provider_desktop.h"
#include "chrome/browser/preloading/preview/preview_navigation_throttle.h"
#include "chrome/browser/web_applications/isolated_web_apps/chrome_content_browser_client_isolated_web_apps_part.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/debug/leak_annotations.h"
#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/chromeos_disabled_apps_throttle.h"
#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"
#include "chrome/browser/chromeos/enterprise/incognito_navigation_throttle.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "chrome/browser/chromeos/quickoffice/quickoffice_prefs.h"
#include "chrome/browser/chromeos/tablet_mode/chrome_content_browser_client_tablet_mode_part.h"
#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_cros.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/chromeos/multi_capture_service.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/direct_sockets/chrome_direct_sockets_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/headless/chrome_browser_main_extra_parts_headless.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/media_effects/media_effects_manager_binder.h"
#include "chrome/browser/metrics/usage_scenario/chrome_responsiveness_calculator_delegate.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/page_info/about_this_site_side_panel_throttle.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/serial/chrome_serial_delegate.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/search/new_tab_page_navigation_throttle.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_navigation_throttle.h"
#include "chrome/browser/ui/web_applications/tabbed_web_app_navigation_throttle.h"
#include "chrome/browser/ui/web_applications/webui_web_app_navigation_throttle.h"
#include "chrome/browser/ui/webui/chrome_content_browser_client_webui_part.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_error_page.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/grit/chrome_unscaled_resources.h"  // nogncheck crbug.com/1125897
#include "components/commerce/core/commerce_feature_list.h"
#include "components/lens/lens_features.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#endif  //  !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#else
#include "chrome/browser/apps/link_capturing/web_app_link_capturing_delegate.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/chrome_browser_main_extra_parts_enterprise.h"
#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"
#include "chrome/browser/enterprise/profile_management/profile_management_navigation_throttle.h"
#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"
#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/views/lens/lens_side_panel_navigation_helper.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/chrome_browser_main_extra_parts_linux.h"
#elif BUILDFLAG(IS_OZONE)
#include "chrome/browser/chrome_browser_main_extra_parts_ozone.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "components/captive_portal/content/captive_portal_url_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/site_isolation_policy.h"
#include "extensions/browser/api/web_request/web_request_proxying_webtransport.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/browser/apps/platform_apps/platform_app_navigation_redirector.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"
#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"
#include "components/pdf/browser/pdf_navigation_throttle.h"
#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"
#include "components/pdf/common/constants.h"
#endif  // BUILDFLAG(ENABLE_PDF)


#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "chrome/browser/media/cast_remoting_connector.h"
#endif

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
#include "chrome/browser/request_header_integrity/request_header_integrity_url_loader_throttle.h"  // nogncheck crbug.com/1125897
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"  // nogncheck crbug.com/1125897
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"  // nogncheck crbug.com/1125897
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_navigation_throttle.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_url_loader_request_interceptor.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "chrome/browser/vr/chrome_xr_integration_client.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chrome_browser_main_parts_lacros.h"
#include "chrome/browser/lacros/chrome_browser_main_extra_parts_lacros.h"
#include "chrome/browser/speech/tts_lacros.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_lacros.h"
#include "chrome/common/chrome_descriptors.h"
#include "chromeos/crosapi/mojom/kerberos_in_browser.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "chromeos/startup/startup.h"           // nogncheck
#include "chromeos/startup/startup_switches.h"  // nogncheck
#include "mojo/core/embedder/embedder.h"
#include "ui/base/ui_base_switches.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/component_updater/hyphenation_component_installer.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/enterprise/common/files_scan_data.h"
#endif

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_request_throttled_handler_browser_impl.h"
#include "chrome/common/bound_session_request_throttled_handler.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feed/feed_service_factory.h"
#include "components/feed/feed_feature_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

using blink::mojom::EffectiveConnectionType;
using blink::web_pref::WebPreferences;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::ChildProcessSecurityPolicy;
using content::RenderFrameHost;
using content::SiteInstance;
using content::WebContents;

#if BUILDFLAG(IS_POSIX)
using content::PosixFileDescriptorInfo;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
using extensions::APIPermission;
using extensions::ChromeContentBrowserClientExtensionsPart;
using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::APIPermissionID;
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
using plugins::ChromeContentBrowserClientPluginsPart;
#endif

#if !BUILDFLAG(IS_ANDROID)
using web_apps::ChromeContentBrowserClientIsolatedWebAppsPart;
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
// Kill switch that allows falling back to the legacy behavior on Android when
// it comes to site isolation for Gaia's origin (|GaiaUrls::gaia_origin()|).
BASE_FEATURE(kAllowGaiaOriginIsolationOnAndroid,
             "AllowGaiaOriginIsolationOnAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateNetworkAccessRestrictionsForAutomotive,
             "PrivateNetworkAccessRestrictionsForAutomotive",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// A small ChromeBrowserMainExtraParts that invokes a callback when threads are
// ready. Used to initialize ChromeContentBrowserClient data that needs the UI
// thread.
class ChromeBrowserMainExtraPartsThreadNotifier final
    : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserMainExtraPartsThreadNotifier(
      base::OnceClosure threads_ready_closure)
      : threads_ready_closure_(std::move(threads_ready_closure)) {}

  // ChromeBrowserMainExtraParts:
  void PostCreateThreads() final { std::move(threads_ready_closure_).Run(); }

 private:
  base::OnceClosure threads_ready_closure_;
};

// Wrapper for SSLErrorHandler::HandleSSLError() that supplies //chrome-level
// parameters.
void HandleSSLErrorWrapper(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback) {
  DCHECK(request_url.SchemeIsCryptographic());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Profile should always outlive a WebContents
  DCHECK(profile);

  captive_portal::CaptivePortalService* captive_portal_service = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service = CaptivePortalServiceFactory::GetForProfile(profile);
#endif

  const bool is_ssl_error_override_allowed_for_origin =
      policy::IsOriginInAllowlist(request_url, profile->GetPrefs(),
                                  prefs::kSSLErrorOverrideAllowedForOrigins,
                                  prefs::kSSLErrorOverrideAllowed);

  SSLErrorHandler::HandleSSLError(
      web_contents, cert_error, ssl_info, request_url,
      std::move(blocking_page_ready_callback),
      g_browser_process->network_time_tracker(), captive_portal_service,
      std::make_unique<ChromeSecurityBlockingPageFactory>(),
      is_ssl_error_override_allowed_for_origin);
}

// Cached version of the locale so we can return the locale on the I/O
// thread.
std::string& GetIOThreadApplicationLocale() {
  static base::NoDestructor<std::string> s;
  return *s;
}

// Returns a copy of the given url with its host set to given host and path set
// to given path. Other parts of the url will be the same.
GURL ReplaceURLHostAndPath(const GURL& url,
                           const std::string& host,
                           const std::string& path) {
  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

// Handles the rewriting of the new tab page URL based on group policy.
bool HandleNewTabPageLocationOverride(
    GURL* url,
    content::BrowserContext* browser_context) {
  if (!url->SchemeIs(content::kChromeUIScheme) ||
      url->host() != chrome::kChromeUINewTabHost) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Don't change the URL when incognito mode.
  if (profile->IsOffTheRecord())
    return false;

  std::string ntp_location =
      profile->GetPrefs()->GetString(prefs::kNewTabPageLocationOverride);
  if (ntp_location.empty())
    return false;
  url::Component scheme;
  if (!url::ExtractScheme(ntp_location.data(),
                          static_cast<int>(ntp_location.length()), &scheme)) {
    ntp_location = base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, ntp_location});
  }

  *url = GURL(ntp_location);
  return true;
}

#if !BUILDFLAG(IS_ANDROID)
bool IsFileOrDirectoryPickerWithoutGestureAllowed(
    content::WebContents* contents) {
  if (!contents) {
    return true;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!profile) {
    return true;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return true;
  }

  return !policy::IsOriginInAllowlist(
      contents->GetURL(), prefs,
      prefs::kFileOrDirectoryPickerWithoutGestureAllowedForOrigins);
}

// Check if autoplay is allowed by policy configuration.
bool IsAutoplayAllowedByPolicy(content::WebContents* contents,
                               PrefService* prefs) {
  if (!contents) {
    return false;
  }

  return policy::IsOriginInAllowlist(contents->GetURL(), prefs,
                                     prefs::kAutoplayAllowlist,
                                     prefs::kAutoplayAllowed);
}
#endif  // !BUILDFLAG(IS_ANDROID)

blink::mojom::AutoplayPolicy GetAutoplayPolicyForWebContents(
    WebContents* web_contents) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::string autoplay_policy = media::GetEffectiveAutoplayPolicy(command_line);
  auto result = blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;

  if (autoplay_policy == switches::autoplay::kNoUserGestureRequiredPolicy) {
    result = blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kUserGestureRequiredPolicy) {
    result = blink::mojom::AutoplayPolicy::kUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kDocumentUserActivationRequiredPolicy) {
    result = blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

#if !BUILDFLAG(IS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  // Override autoplay policy used in internal switch in case of enabling
  // features such as policy, allowlisting or disabling from settings.
  if (IsAutoplayAllowedByPolicy(web_contents, prefs)) {
    result = blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  } else if (base::FeatureList::IsEnabled(media::kAutoplayDisableSettings) &&
             result == blink::mojom::AutoplayPolicy::
                           kDocumentUserActivationRequired) {
    result = UnifiedAutoplayConfig::ShouldBlockAutoplay(profile)
                 ? blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired
                 : blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  } else if (web_contents->GetPrimaryMainFrame()->IsFeatureEnabled(
                 blink::mojom::PermissionsPolicyFeature::kAutoplay) &&
             IsAutoplayAllowedByPolicy(web_contents->GetOuterWebContents(),
                                       prefs)) {
    // If the domain policy allows autoplay and has delegated that to an iframe,
    // allow autoplay within the iframe. Only allow a nesting of single depth.
    result = blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  }
#else   // !BUILDFLAG(IS_ANDROID)
  // TWAs don't require a user gesture for unmuted autoplay.
  if (base::FeatureList::IsEnabled(features::kAllowUnmutedAutoplayForTWA)) {
    if (auto* delegate = TabAndroid::FromWebContents(web_contents)) {
      if (delegate->IsTrustedWebActivity()) {
        result = blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
      }
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return result;
}

#if BUILDFLAG(IS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  int fd;
  return crash_reporter::GetHandlerSocket(&fd, nullptr) ? fd : -1;
}
#endif  // BUILDFLAG(IS_ANDROID)

void SetApplicationLocaleOnIOThread(const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetIOThreadApplicationLocale() = locale;
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)

// Returns true if there is is an extension matching `url` in
// `render_process_id` with `permission`.
//
// GetExtensionOrAppByURL requires a full URL in order to match with a hosted
// app, even though normal extensions just use the host.
bool URLHasExtensionPermission(extensions::ProcessMap* process_map,
                               extensions::ExtensionRegistry* registry,
                               const GURL& url,
                               int render_process_id,
                               APIPermissionID permission) {
  // Includes web URLs that are part of an extension's web extent.
  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(url);
  return extension &&
         extension->permissions_data()->HasAPIPermission(permission) &&
         process_map->Contains(extension->id(), render_process_id);
}

// Returns true if |extension_id| is allowed to run as an Isolated Context,
// giving it access to additional APIs.
bool IsExtensionIdAllowedToUseIsolatedContext(std::string_view extension_id) {
  constexpr auto kAllowedIsolatedContextExtensionIds =
      base::MakeFixedFlatSet<std::string_view>({
          "algkcnfjnajfhgimadimbjhmpaeohhln",  // Secure Shell Extension (dev)
          "iodihamcpbpeioajjeobimgagajmlibd",  // Secure Shell Extension
                                               // (stable)
          // Extension IDs used in tests.
          "bbobefdodiifgmhhdijgpelmkdaebfpn",  // Controlled Frame Service
                                               // Worker Test
      });
  return base::Contains(kAllowedIsolatedContextExtensionIds, extension_id);
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

mojo::PendingRemote<prerender::mojom::PrerenderCanceler> GetPrerenderCanceler(
    base::OnceCallback<content::WebContents*()> wc_getter) {
  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler;
  prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
      std::move(wc_getter).Run())
      ->AddPrerenderCancelerReceiver(canceler.InitWithNewPipeAndPassReceiver());
  return canceler;
}

bool ShouldHonorPolicies() {
#if BUILDFLAG(IS_WIN)
  return policy::ManagementServiceFactory::GetForPlatform()
             ->GetManagementAuthorityTrustworthiness() >=
         policy::ManagementAuthorityTrustworthiness::TRUSTED;
#else
  return true;
#endif
}

// Used by Enterprise policy. Disable blocking of navigations toward external
// applications from a sandboxed iframe.
// https://chromestatus.com/feature/5680742077038592
const char kDisableSandboxExternalProtocolSwitch[] =
    "disable-sandbox-external-protocols";

void LaunchURL(
    base::WeakPtr<ChromeContentBrowserClient> client,
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    ui::PageTransition page_transition,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // If there is no longer a WebContents, the request may have raced with tab
  // closing. Don't fire the external request. (It may have been a prerender.)
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  // Do not launch external requests attached to unswapped no-state prefetchers.
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(
        prerender::FINAL_STATUS_UNSUPPORTED_SCHEME);
    return;
  }

  // Do not launch external requests for schemes that have a handler registered.
  custom_handlers::ProtocolHandlerRegistry* protocol_handler_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (protocol_handler_registry &&
      protocol_handler_registry->IsHandledProtocol(url.scheme()))
    return;

  // Sandbox flags
  // =============
  //
  // Navigations to external protocol in iframe can be seen as "top-level"
  // navigations somehow, because they cause the user to switch from Chrome's
  // page toward a different application.
  //
  // Internally in Chrome, they are seen as aborted iframe navigation, so the
  // regular sandbox logic do not really apply.
  //
  // This block adds an extra logic, gating external protocol in iframes to have
  // one of:
  // - 'allow-top-navigation'
  // - 'allow-top-navigation-to-custom-protocols'
  // - 'allow-top-navigation-by-user-navigation' + user-activation
  // - 'allow-popups'
  //
  // See https://crbug.com/1148777
  if (!is_primary_main_frame) {
    using SandboxFlags = network::mojom::WebSandboxFlags;
    auto allow = [&](SandboxFlags flag) {
      return (sandbox_flags & flag) == SandboxFlags::kNone;
    };
    bool allowed = (allow(SandboxFlags::kTopNavigationToCustomProtocols)) ||
                   (allow(SandboxFlags::kTopNavigationByUserActivation) &&
                    has_user_gesture);

    if (!allowed) {
      content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
      if (client) {
        client->LogWebFeatureForCurrentPage(
            rfh, blink::mojom::WebFeature::kExternalProtocolBlockedBySandbox);
      }

      if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
              kDisableSandboxExternalProtocolSwitch)) {
        if (base::FeatureList::IsEnabled(
                features::kSandboxExternalProtocolBlocked)) {
          rfh->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError,
              "Navigation to external protocol blocked by sandbox, because it "
              "doesn't contain any of: "
              "'allow-top-navigation-to-custom-protocols', "
              "'allow-top-navigation-by-user-activation', "
              "'allow-top-navigation', or "
              "'allow-popups'. See "
              "https://chromestatus.com/feature/5680742077038592 and "
              "https://chromeenterprise.google/policies/"
              "#SandboxExternalProtocolBlocked");
          return;
        }

        if (base::FeatureList::IsEnabled(
                features::kSandboxExternalProtocolBlockedWarning)) {
          rfh->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError,
              "After Chrome M103, navigation toward external protocol "
              "will be blocked by sandbox, if it doesn't contain any of:"
              "'allow-top-navigation-to-custom-protocols', "
              "'allow-top-navigation-by-user-activation', "
              "'allow-top-navigation', or "
              "'allow-popups'. See "
              "https://chromestatus.com/feature/5680742077038592 and "
              "https://chromeenterprise.google/policies/"
              "#SandboxExternalProtocolBlocked");
        }
      }
    }
  }

  bool is_allowlisted = false;
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (ShouldHonorPolicies() && service) {
    const policy::URLBlocklist::URLBlocklistState url_state =
        service->GetURLBlocklistState(url);
    is_allowlisted =
        url_state == policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Never skip security checks for the intent:// scheme because
  // `ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck` does not handle
  // intent:// URLs correctly (or any URLs that should be opened in ARC).
  // TODO(b/331400224): Fix `LaunchUrlWithoutSecurityCheck` to handle intent://
  // URLs correctly and stop treating them in a special way here.
  if (url.SchemeIs("intent")) {
    is_allowlisted = false;
  }
#endif

  // If the URL is in allowlist, we launch it without asking the user and
  // without any additional security checks. Since the URL is allowlisted,
  // we assume it can be executed.
  if (is_allowlisted) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
        url, web_contents, std::move(initiator_document));
  } else {
    ExternalProtocolHandler::LaunchUrl(
        url, std::move(web_contents_getter), page_transition, has_user_gesture,
        is_in_fenced_frame_tree, initiating_origin,
        std::move(initiator_document)
#if BUILDFLAG(IS_ANDROID)
            ,
        out_factory
#endif
    );
  }
}

void MaybeAppendSecureOriginsAllowlistSwitch(base::CommandLine* cmdline) {
  // |allowlist| combines pref/policy + cmdline switch in the browser process.
  // For renderer and utility (e.g. NetworkService) processes the switch is the
  // only available source, so below the combined (pref/policy + cmdline)
  // allowlist of secure origins is injected into |cmdline| for these other
  // processes.
  std::vector<std::string> allowlist =
      network::SecureOriginAllowlist::GetInstance().GetCurrentAllowlist();
  if (!allowlist.empty()) {
    cmdline->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        base::JoinString(allowlist, ","));
  }
}

#if BUILDFLAG(IS_WIN) && !defined(COMPONENT_BUILD) && \
    !defined(ADDRESS_SANITIZER)
// Returns the full path to |module_name|. Both dev builds (where |module_name|
// is in the current executable's directory) and proper installs (where
// |module_name| is in a versioned sub-directory of the current executable's
// directory) are supported. The identified file is not guaranteed to exist.
base::FilePath GetModulePath(std::wstring_view module_name) {
  base::FilePath exe_dir;
  const bool has_path = base::PathService::Get(base::DIR_EXE, &exe_dir);
  DCHECK(has_path);

  // Look for the module in a versioned sub-directory of the current
  // executable's directory and return the path if it can be read. This is the
  // expected location of modules for proper installs.
  const base::FilePath module_path =
      exe_dir.AppendASCII(chrome::kChromeVersion).Append(module_name);
  if (base::PathExists(module_path))
    return module_path;

  // Otherwise, return the path to the module in the current executable's
  // directory. This is the expected location of modules for dev builds.
  return exe_dir.Append(module_name);
}
#endif  // BUILDFLAG(IS_WIN) && !defined(COMPONENT_BUILD) &&
        // !defined(ADDRESS_SANITIZER)

void MaybeAddThrottle(
    std::unique_ptr<content::NavigationThrottle> maybe_throttle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  if (maybe_throttle)
    throttles->push_back(std::move(maybe_throttle));
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
void MaybeAddCondition(
    std::unique_ptr<content::CommitDeferringCondition> maybe_condition,
    std::vector<std::unique_ptr<content::CommitDeferringCondition>>*
        conditions) {
  if (maybe_condition)
    conditions->push_back(std::move(maybe_condition));
}
#endif

void MaybeAddThrottles(
    std::vector<std::unique_ptr<content::NavigationThrottle>> additional,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* combined) {
  combined->insert(combined->end(), std::make_move_iterator(additional.begin()),
                   std::make_move_iterator(additional.end()));
}

// Returns whether |web_contents| is within a hosted app.
bool IsInHostedApp(WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return web_app::AppBrowserController::IsWebApp(browser);
#else
  return false;
#endif
}

bool IsErrorPageAutoReloadEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableAutomation))
    return false;
  if (command_line.HasSwitch(embedder_support::kEnableAutoReload))
    return true;
  if (command_line.HasSwitch(embedder_support::kDisableAutoReload))
    return false;
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void NotifyMultiCaptureStarted(const std::string& label,
                               content::WebContents* web_contents,
                               const webapps::AppId* app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app_id &&
      video_capture::mojom::MultiCaptureServiceClient::Version_ >=
          video_capture::mojom::MultiCaptureServiceClient::MethodMinVersions::
              kMultiCaptureStartedFromAppMinVersion) {
    content::GetMultiCaptureService().NotifyMultiCaptureStartedFromApp(
        label, *app_id,
        web_app::WebAppProvider::GetForWebContents(web_contents)
            ->registrar_unsafe()
            .GetAppShortName(*app_id));
  } else {
    // TODO(b/319317165): Remove this case once the pivot to web apps is
    // complete.
    content::GetMultiCaptureService().NotifyMultiCaptureStarted(
        label, url::Origin::Create(web_contents->GetLastCommittedURL()));
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService& service =
      CHECK_DEREF(chromeos::LacrosService::Get());
  crosapi::mojom::MultiCaptureService& multi_capture_service = CHECK_DEREF(
      service.GetRemote<crosapi::mojom::MultiCaptureService>().get());
  if (app_id &&
      service.GetInterfaceVersion<crosapi::mojom::MultiCaptureService>() >=
          (int)crosapi::mojom::MultiCaptureService::MethodMinVersions::
              kMultiCaptureStartedFromAppMinVersion) {
    multi_capture_service.MultiCaptureStartedFromApp(
        label, *app_id,
        web_app::WebAppProvider::GetForWebContents(web_contents)
            ->registrar_unsafe()
            .GetAppShortName(*app_id));
  } else {
    // TODO(b/319317165): Remove this case once the pivot to web apps is
    // complete.
    multi_capture_service.MultiCaptureStarted(
        label, web_contents->GetLastCommittedURL().host());
  }
#endif
}

void NotifyMultiCaptureStopped(const std::string& label) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  content::GetMultiCaptureService().NotifyMultiCaptureStopped(label);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::MultiCaptureService& multi_capture_service =
      CHECK_DEREF(chromeos::LacrosService::Get()
                      ->GetRemote<crosapi::mojom::MultiCaptureService>()
                      .get());
  multi_capture_service.MultiCaptureStopped(label);
#endif
}

bool IsSubAppsPermissionGrantedByAdmins(content::WebContents* contents) {
  if (!contents) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!profile) {
    return false;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return false;
  }

  return policy::IsOriginInAllowlist(
      contents->GetURL(), prefs,
      prefs::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins);
}

// Checks if installation and removal of subapps require a user gesture and
// authorization. Both requirements can be overridden via admin policy.
bool SubAppsAPIsRequireUserGestureAndAuthorization(
    content::WebContents* web_contents) {
  return !IsSubAppsPermissionGrantedByAdmins(web_contents);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<blocked_content::PopupNavigationDelegate>
CreatePopupNavigationDelegate(NavigateParams params) {
  return std::make_unique<ChromePopupNavigationDelegate>(std::move(params));
}

// NOTE: MaybeCreateVisitedLinkNavigationThrottleFor is defined here due to
// usage of Profile code which lives in chrome/. The rest of the
// VisitedLinkNavigationThrottle class lives in components/, which cannot access
// chrome/ code due to layering.
std::unique_ptr<VisitedLinkNavigationThrottle>
MaybeCreateVisitedLinkNavigationThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabase) &&
      !base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  // Off-the-record profiles do not record history or visited links.
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (!history_service) {
    return nullptr;
  }
  return std::make_unique<VisitedLinkNavigationThrottle>(
      std::move(navigation_handle), history_service);
}

ChromeContentBrowserClient::PopupNavigationDelegateFactory
    g_popup_navigation_delegate_factory = &CreatePopupNavigationDelegate;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(CHROME_FOR_TESTING)
bool DetermineIfDevToolsUserForProcessPerSite() {
  bool is_devtools_user = false;
  // Only count uses of DevTools from within the last week.
  constexpr base::TimeDelta kDevToolsUserActivityWindow = base::Days(7);
  auto now = base::Time::Now();
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
    for (auto* profile : profiles) {
      base::Time last_open_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Milliseconds(profile->GetPrefs()->GetInt64(
              prefs::kDevToolsLastOpenTimestamp)));
      if (now - last_open_time < kDevToolsUserActivityWindow) {
        is_devtools_user = true;
        break;
      }
    }
  }
  base::UmaHistogramBoolean(
      "SiteIsolation.ProcessPerSiteWithMainFrameThreshold.IsDevToolsUser",
      is_devtools_user);
  return is_devtools_user;
}
#endif

net::handles::NetworkHandle GetBoundNetworkFromRenderFrameHost(
    content::RenderFrameHost* frame) {
  auto* web_contents = WebContents::FromRenderFrameHost(frame);
  if (!web_contents) {
    return net::handles::kInvalidNetworkHandle;
  }
  return web_contents->GetTargetNetwork();
}

}  // namespace

// static
ChromeContentBrowserClient::PopupNavigationDelegateFactory&
ChromeContentBrowserClient::GetPopupNavigationDelegateFactoryForTesting() {
  return g_popup_navigation_delegate_factory;
}

ChromeContentBrowserClient::ChromeContentBrowserClient() {
#if BUILDFLAG(ENABLE_PLUGINS)
  extra_parts_.push_back(
      std::make_unique<ChromeContentBrowserClientPluginsPart>());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  extra_parts_.push_back(
      std::make_unique<ChromeContentBrowserClientTabletModePart>());
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  extra_parts_.push_back(
      std::make_unique<ChromeContentBrowserClientWebUiPart>());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extra_parts_.push_back(
      std::make_unique<ChromeContentBrowserClientExtensionsPart>());
#endif

#if !BUILDFLAG(IS_ANDROID)
  extra_parts_.push_back(
      std::make_unique<ChromeContentBrowserClientIsolatedWebAppsPart>());
#endif

  extra_parts_.push_back(
      std::make_unique<ChromeContentBrowserClientPerformanceManagerPart>());
}

ChromeContentBrowserClient::~ChromeContentBrowserClient() {
  // std::vector<> does not guarantee any specific destruction order, so
  // explicitly destroy elements in the reverse order per header comment.
  while (!extra_parts_.empty()) {
    extra_parts_.pop_back();
  }
}

// static
void ChromeContentBrowserClient::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kDiskCacheDir, base::FilePath());
  registry->RegisterIntegerPref(prefs::kDiskCacheSize, 0);
  registry->RegisterStringPref(prefs::kIsolateOrigins, std::string());
  registry->RegisterBooleanPref(prefs::kSitePerProcess, false);
  registry->RegisterBooleanPref(prefs::kTabFreezingEnabled, true);
  registry->RegisterIntegerPref(prefs::kSCTAuditingHashdanceReportCount, 0);
  registry->RegisterBooleanPref(prefs::kDataURLWhitespacePreservationEnabled,
                                true);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kNativeClientForceAllowed, false);
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kOutOfProcessSystemDnsResolutionEnabled,
                                true);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
}

// static
void ChromeContentBrowserClient::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDisable3DAPIs, false);
  registry->RegisterBooleanPref(prefs::kEnableHyperlinkAuditing, true);
  // Register user prefs for mapping SitePerProcess and IsolateOrigins in
  // user policy in addition to the same named ones in Local State (which are
  // used for mapping the command-line flags).
  registry->RegisterStringPref(prefs::kIsolateOrigins, std::string());
  registry->RegisterBooleanPref(prefs::kSitePerProcess, false);
  registry->RegisterListPref(
      site_isolation::prefs::kUserTriggeredIsolatedOrigins);
  registry->RegisterDictionaryPref(
      site_isolation::prefs::kWebTriggeredIsolatedOrigins);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsBackgroundServicesExpirationDict);
  registry->RegisterBooleanPref(prefs::kSignedHTTPExchangeEnabled, true);
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kAutoplayAllowed, false);
  registry->RegisterListPref(prefs::kAutoplayAllowlist);
  registry->RegisterListPref(
      prefs::kScreenCaptureWithoutGestureAllowedForOrigins);
  registry->RegisterListPref(
      prefs::kFileOrDirectoryPickerWithoutGestureAllowedForOrigins);
  registry->RegisterIntegerPref(prefs::kFetchKeepaliveDurationOnShutdown, 0);
  registry->RegisterBooleanPref(
      prefs::kSharedArrayBufferUnrestrictedAccessAllowed, false);
#endif
  registry->RegisterBooleanPref(prefs::kSandboxExternalProtocolBlocked, true);
  registry->RegisterBooleanPref(prefs::kSSLErrorOverrideAllowed, true);
  registry->RegisterListPref(prefs::kSSLErrorOverrideAllowedForOrigins);
  registry->RegisterBooleanPref(prefs::kCompressionDictionaryTransportEnabled,
                                true);
  registry->RegisterBooleanPref(
      prefs::kSuppressDifferentOriginSubframeJSDialogs, true);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kWebXRImmersiveArEnabled, true);
#endif
  registry->RegisterBooleanPref(prefs::kPromptOnMultipleMatchingCertificates,
                                false);
  registry->RegisterBooleanPref(prefs::kCorsNonWildcardRequestHeadersSupport,
                                true);
  registry->RegisterBooleanPref(prefs::kIPv6ReachabilityOverrideEnabled, false);
  registry->RegisterDictionaryPref(
      enterprise::content::kCopyPreventionSettings);
  registry->RegisterIntegerPref(
      prefs::kUserAgentReduction,
      static_cast<int>(
          embedder_support::UserAgentReductionEnterprisePolicyState::kDefault));
  registry->RegisterBooleanPref(prefs::kOriginAgentClusterDefaultEnabled, true);

  registry->RegisterBooleanPref(
      prefs::kStrictMimetypeCheckForWorkerScriptsEnabled, true);
  registry->RegisterBooleanPref(policy::policy_prefs::kFeedbackSurveysEnabled,
                                true);
  registry->RegisterBooleanPref(
      prefs::kAccessControlAllowMethodsInCORSPreflightSpecConformant, true);
  registry->RegisterBooleanPref(prefs::kDataUrlInSvgUseEnabled, false);

  registry->RegisterBooleanPref(policy::policy_prefs::kMutationEventsEnabled,
                                false);

  registry->RegisterBooleanPref(
      policy::policy_prefs::kCSSCustomStateDeprecatedSyntaxEnabled,
      /*default_value=*/false);

  registry->RegisterBooleanPref(
      policy::policy_prefs::kBeforeunloadEventCancelByPreventDefaultEnabled,
      true);

  registry->RegisterBooleanPref(
      policy::policy_prefs::kKeyboardFocusableScrollersEnabled, true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kStandardizedBrowserZoomEnabled, true);

  registry->RegisterBooleanPref(
      policy::policy_prefs::
          kAllowBackForwardCacheForCacheControlNoStorePageEnabled,
      true);

  registry->RegisterBooleanPref(
      policy::policy_prefs::kForcePermissionPolicyUnloadDefaultEnabled, false);

#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterListPref(prefs::kMandatoryExtensionsForIncognitoNavigation);
  registry->RegisterListPref(
      prefs::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins);
#endif
}

// static
void ChromeContentBrowserClient::SetApplicationLocale(
    const std::string& locale) {
  // The common case is that this function is called early in Chrome startup
  // before any threads are created or registered. When there are no threads,
  // we can just set the string without worrying about threadsafety.
  if (!BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    GetIOThreadApplicationLocale() = locale;
    return;
  }

  // Otherwise we're being called to change the locale. In this case set it on
  // the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SetApplicationLocaleOnIOThread, locale));
}

void ChromeContentBrowserClient::MaybeProxyNetworkBoundRequest(
    content::BrowserContext* browser_context,
    net::handles::NetworkHandle bound_network,
    network::URLLoaderFactoryBuilder& factory_builder,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override,
    const net::IsolationInfo& isolation_info) {
  if (bound_network == net::handles::kInvalidNetworkHandle) {
    return;
  }

  // We support one network-bound NetworkContext at most. If a new one is
  // needed, make sure to clean up the previous one first.
  if (bound_network != target_network_for_network_bound_network_context_) {
    network_bound_network_context_ =
        mojo::Remote<network::mojom::NetworkContext>();
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->bound_network = bound_network;
    context_params->cert_verifier_params = content::GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    ConfigureNetworkContextParams(
        browser_context, true, base::FilePath(), context_params.get(),
        cert_verifier::mojom::CertVerifierCreationParams::New().get());
    content::CreateNetworkContextInNetworkService(
        network_bound_network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
    target_network_for_network_bound_network_context_ = bound_network;
  }

  // TLDR; if `factory_override` != nullptr, this is being called for the
  // creation of a 2-layer URLLoaderFactory (see
  // network.mojom.URLLoaderFactoryOverride documentation). In this case, we
  // want to substitute the internal (defined by
  // factory_override->overriding_factory, with a URLLoaderFactory that targets
  // `bound_network`. If `factory_override` == nullptr, this is a single-layer
  // URLLoaderFactory. In this case, we want the last URLLoaderFactory in the
  // `factory_builder` chain to be a URLLoaderFactory that targets
  // `bound_network`.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> proxied_receiver;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> bypassed_remote;
  if (!factory_override) {
    // Hijack the receiver end returned by network::URLLoaderFactoryBuilder.
    // This will be then redirected to a network-bound URLLoaderFactory.
    std::tie(proxied_receiver, bypassed_remote) = factory_builder.Append();
  } else {
    // Hijack the remote end stored in network::mojom::URLLoaderFactoryOverride.
    // This will be then redirected to a network-bound URLLoaderFactory.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New();
    proxied_receiver =
        (*factory_override)
            ->overriding_factory.InitWithNewPipeAndPassReceiver();
    (*factory_override)->overridden_factory_receiver =
        bypassed_remote.InitWithNewPipeAndPassReceiver();
    (*factory_override)->skip_cors_enabled_scheme_check = true;
  }

  // Create a network-bound URLLoaderFactory and redirect the receiver end of
  // the hijacked remote to this.
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_trusted = true;
  params->isolation_info = isolation_info;
  // Disable CORS wrapping, this is already handled by the caller.
  params->disable_web_security = true;
  network_bound_network_context_->CreateURLLoaderFactory(
      std::move(proxied_receiver), std::move(params));
}

std::unique_ptr<content::BrowserMainParts>
ChromeContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  std::unique_ptr<ChromeBrowserMainParts> main_parts;
  // Construct the Main browser parts based on the OS type.
#if BUILDFLAG(IS_WIN)
  main_parts = std::make_unique<ChromeBrowserMainPartsWin>(is_integration_test,
                                                           &startup_data_);
#elif BUILDFLAG(IS_MAC)
  main_parts = std::make_unique<ChromeBrowserMainPartsMac>(is_integration_test,
                                                           &startup_data_);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  main_parts = std::make_unique<ash::ChromeBrowserMainPartsAsh>(
      is_integration_test, &startup_data_);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts = std::make_unique<ChromeBrowserMainPartsLacros>(
      is_integration_test, &startup_data_);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts = std::make_unique<ChromeBrowserMainPartsLinux>(
      is_integration_test, &startup_data_);
#elif BUILDFLAG(IS_ANDROID)
  main_parts = std::make_unique<ChromeBrowserMainPartsAndroid>(
      is_integration_test, &startup_data_);
#elif BUILDFLAG(IS_POSIX)
  main_parts = std::make_unique<ChromeBrowserMainPartsPosix>(
      is_integration_test, &startup_data_);
#else
#error "Unimplemented platform"
#endif

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsThreadNotifier>(
          base::BindOnce(&ChromeContentBrowserClient::InitOnUIThread,
                         weak_factory_.GetWeakPtr())));

  bool add_profiles_extra_parts = true;
#if BUILDFLAG(IS_ANDROID)
  if (startup_data_.HasBuiltProfilePrefService())
    add_profiles_extra_parts = false;
#endif
  if (add_profiles_extra_parts)
    chrome::AddProfilesExtraParts(main_parts.get());

    // Construct additional browser parts. Stages are called in the order in
    // which they are added.
#if defined(TOOLKIT_VIEWS)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsViewsLacros>());
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsViewsLinux>());
#else
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsViews>());
#endif
#endif

#if BUILDFLAG(IS_MAC)
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsMac>());
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(jamescook): Combine with `ChromeBrowserMainPartsAsh`.
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsAsh>());
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsLacros>());
#endif

#if BUILDFLAG(IS_LINUX)
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsLinux>());
#elif BUILDFLAG(IS_OZONE)
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsOzone>());
#endif

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsPerformanceMonitor>());

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsPerformanceManager>());

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsProfiling>());

  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsMemory>());

  chrome::AddMetricsExtraParts(main_parts.get());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  main_parts->AddParts(
      std::make_unique<
          enterprise_util::ChromeBrowserMainExtraPartsEnterprise>());
#endif

#if !BUILDFLAG(IS_ANDROID)
  main_parts->AddParts(
      std::make_unique<headless::ChromeBrowserMainExtraPartsHeadless>());
#endif

  // Always add ChromeBrowserMainExtraPartsGpu last to make sure
  // GpuDataManager initialization could pick up about:flags settings.
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsGpu>());

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsSegmentationPlatform>());

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsOptimizationGuide>());

  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsNaclDeprecation>());

  return main_parts;
}

void ChromeContentBrowserClient::PostAfterStartupTask(
    const base::Location& from_here,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::OnceClosure task) {
  AfterStartupTaskUtils::PostTask(from_here, task_runner, std::move(task));
}

bool ChromeContentBrowserClient::IsBrowserStartupComplete() {
  return AfterStartupTaskUtils::IsBrowserStartupComplete();
}

void ChromeContentBrowserClient::SetBrowserStartupIsCompleteForTesting() {
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
}

bool ChromeContentBrowserClient::IsShuttingDown() {
  return browser_shutdown::HasShutdownStarted();
}

void ChromeContentBrowserClient::ThreadPoolWillTerminate() {
  sampling_profiler_.reset();
}

content::StoragePartitionConfig
ChromeContentBrowserClient::GetStoragePartitionConfigForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  // Default to the browser-wide storage partition and override based on |site|
  // below.
  content::StoragePartitionConfig default_storage_partition_config =
      content::StoragePartitionConfig::CreateDefault(browser_context);

  // A non-default storage partition is used in the following situations:
  // - To enforce process isolation between a more-trusted content (Chrome Apps,
  // Extensions, and Isolated Web Apps) and regular web content.
  // - For the <webview> tag, which Chrome Apps, Isolated Web Apps and WebUI use
  // to create temporary storage buckets for loading various kinds of web
  // content.
  //
  // In general, those use cases aren't considered part of the user's normal
  // browsing activity.
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (site.SchemeIs(extensions::kExtensionScheme)) {
    // The host in an extension site URL is the extension_id.
    CHECK(site.has_host());
    return extensions::util::GetStoragePartitionConfigForExtensionId(
        site.host(), browser_context);
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (content::SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
          browser_context, site)) {
    CHECK(site.SchemeIs(chrome::kIsolatedAppScheme));
    ASSIGN_OR_RETURN(const auto iwa_url_info,
                     web_app::IsolatedWebAppUrlInfo::Create(site), [&](auto) {
                       LOG(ERROR) << "Invalid isolated-app URL: " << site;
                       return default_storage_partition_config;
                     });
    return iwa_url_info.storage_partition_config(browser_context);
  }
#endif

  return default_storage_partition_config;
}

std::unique_ptr<content::WebContentsViewDelegate>
ChromeContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return CreateWebContentsViewDelegate(web_contents);
}

bool ChromeContentBrowserClient::AllowGpuLaunchRetryOnIOThread() {
#if BUILDFLAG(IS_ANDROID)
  const base::android::ApplicationState app_state =
      base::android::ApplicationStatusListener::GetState();
  return base::android::APPLICATION_STATE_UNKNOWN == app_state ||
         base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES == app_state ||
         base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES == app_state;
#else
  return true;
#endif
}

void ChromeContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  WebRtcLoggingController::AttachToRenderProcessHost(host);

  // The audio manager outlives the host, so it's safe to hand a raw pointer to
  // it to the AudioDebugRecordingsHandler, which is owned by the host.
  AudioDebugRecordingsHandler* audio_debug_recordings_handler =
      new AudioDebugRecordingsHandler(profile);
  host->SetUserData(
      AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey,
      std::make_unique<base::UserDataAdapter<AudioDebugRecordingsHandler>>(
          audio_debug_recordings_handler));

#if BUILDFLAG(ENABLE_NACL)
  if (IsNaclAllowed() && !profile->IsSystemProfile()) {
    host->AddFilter(new nacl::NaClHostMessageFilter(
        host->GetID(), profile->IsOffTheRecord(), profile->GetPath()));
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  // Register CrashMemoryMetricsCollector to report oom related metrics.
  host->SetUserData(
      CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey,
      std::make_unique<CrashMemoryMetricsCollector>(host));
#endif

  IdentifiabilityStudyState* identifiability_study_state =
      g_browser_process->GetMetricsServicesManager()
          ->GetIdentifiabilityStudyState();
  if (identifiability_study_state) {
    identifiability_study_state->InitializeRenderer(host);
  }

  // The RendereUpdater might be null for some irregular profiles, e.g. the
  // System Profile.
  if (RendererUpdater* service = RendererUpdaterFactory::GetForProfile(profile))
    service->InitializeRenderer(host);

  for (auto& part : extra_parts_) {
    part->RenderProcessWillLaunch(host);
  }
}

GURL ChromeContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return url;

#if !BUILDFLAG(IS_ANDROID)
  // If the input |url| should be assigned to the Instant renderer, make its
  // effective URL distinct from other URLs on the search provider's domain.
  // This needs to happen even if |url| corresponds to an isolated origin; see
  // https://crbug.com/755595.
  if (search::ShouldAssignURLToInstantRenderer(url, profile))
    return search::GetEffectiveURLForInstant(url, profile);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          profile))
    return url;

  return ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(profile,
                                                                   url);
#else
  return url;
#endif
}

bool ChromeContentBrowserClient::
    ShouldCompareEffectiveURLsForSiteInstanceSelection(
        content::BrowserContext* browser_context,
        content::SiteInstance* candidate_site_instance,
        bool is_outermost_main_frame,
        const GURL& candidate_url,
        const GURL& destination_url) {
  DCHECK(browser_context);
  DCHECK(candidate_site_instance);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          browser_context)) {
    return true;
  }

  return ChromeContentBrowserClientExtensionsPart::
      ShouldCompareEffectiveURLsForSiteInstanceSelection(
          browser_context, candidate_site_instance, is_outermost_main_frame,
          candidate_url, destination_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  // NTP should use process-per-site.  This is a performance optimization to
  // reduce process count associated with NTP tabs.
  if (site_url == GURL(chrome::kChromeUINewTabURL) ||
      site_url == GURL(chrome::kChromeUINewTabPageURL)) {
    return true;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (search::ShouldUseProcessPerSiteForInstantSiteURL(site_url, profile))
    return true;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::ShouldUseProcessPerSite(
          profile, site_url))
    return true;
#endif

  // Non-extension, non-NTP URLs should generally use process-per-site-instance
  // (rather than process-per-site).
  return false;
}

bool ChromeContentBrowserClient::ShouldAllowProcessPerSiteForMultipleMainFrames(
    content::BrowserContext* browser_context) {
#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(CHROME_FOR_TESTING)
  static bool is_devtools_user = true;
#else
  static bool is_devtools_user =
      DetermineIfDevToolsUserForProcessPerSite() || headless::IsHeadlessMode();
#endif

  if (is_devtools_user && base::FeatureList::IsEnabled(
                              features::kProcessPerSiteSkipDevtoolsUsers)) {
    return false;
  }
#endif
  // Skip enterprise users.
  if (base::FeatureList::IsEnabled(
          features::kProcessPerSiteSkipEnterpriseUsers)) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    if (!profile) {
      return false;
    }
    auto* management_service =
        policy::ManagementServiceFactory::GetForProfile(profile);
    if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged() &&
        management_service && management_service->IsManaged()) {
      return false;
    }
  }
  return true;
}

std::optional<
    content::ContentBrowserClient::SpareProcessRefusedByEmbedderReason>
ChromeContentBrowserClient::ShouldUseSpareRenderProcessHost(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return SpareProcessRefusedByEmbedderReason::NoProfile;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Instant renderers should not use a spare process, because they require
  // passing switches::kInstantProcess to the renderer process when it
  // launches.  A spare process is launched earlier, before it is known which
  // navigation will use it, so it lacks this flag.
  if (search::ShouldAssignURLToInstantRenderer(site_url, profile)) {
    // The NTP page chrome://new-tab-page and chrome://new-tab-page-third-party
    // are using WebUI and will not use instant renderer.
    // The only usecase is chrome-search:// URLs.
    return SpareProcessRefusedByEmbedderReason::InstantRendererForNewTabPage;
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!ChromeContentBrowserClientExtensionsPart::
          ShouldUseSpareRenderProcessHost(profile, site_url)) {
    return SpareProcessRefusedByEmbedderReason::ExtensionProcess;
  }
#endif
  return std::nullopt;
}

bool ChromeContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess(
          browser_context, effective_site_url)) {
    return true;
  }
#endif
  return false;
}

bool ChromeContentBrowserClient::
    ShouldAllowCrossProcessSandboxedFrameForPrecursor(
        content::BrowserContext* browser_context,
        const GURL& precursor,
        const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!ChromeContentBrowserClientExtensionsPart::
          ShouldAllowCrossProcessSandboxedFrameForPrecursor(browser_context,
                                                            precursor, url)) {
    return false;
  }
#endif
  return true;
}

bool ChromeContentBrowserClient::DoesWebUIUrlRequireProcessLock(
    const GURL& url) {
  // Note: This method can be called from multiple threads. It is not safe to
  // assume it runs only on the UI thread.

  // We only allow the most visited tiles on third-party NTPs to not require a
  // process lock. Everything else, including the actual third-party NTP which
  // embeds those tiles, should be locked.  This allows most visited tiles to
  // stay in their parent (i.e., third-party NTP's) process.
  if (url.SchemeIs(chrome::kChromeSearchScheme) &&
      url.host() == chrome::kChromeSearchMostVisitedHost) {
    return false;
  }

  // All other WebUIs must be locked to origin.
  return true;
}

bool ChromeContentBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    std::string_view scheme,
    bool is_embedded_origin_secure) {
  // This is needed to bypass the normal SameSite rules for any chrome:// page
  // embedding a secure origin, regardless of the registrable domains of any
  // intervening frames. For example, this is needed for browser UI to interact
  // with SameSite cookies on accounts.google.com, which is used for displaying
  // a list of available accounts on the NTP (chrome://new-tab-page), etc.
  if (is_embedded_origin_secure && scheme == content::kChromeUIScheme)
    return true;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return scheme == extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::
    ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
        std::string_view scheme,
        bool is_embedded_origin_secure) {
  return is_embedded_origin_secure && scheme == content::kChromeUIScheme;
}

// TODO(crbug.com/40694933): This is based on SubframeTask::GetTitle()
// implementation. Find a general solution to avoid code duplication.
std::string ChromeContentBrowserClient::GetSiteDisplayNameForCdmProcess(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  // By default, use the |site_url| spec as the display name.
  std::string name = site_url.spec();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If |site_url| wraps a chrome extension ID, we can display the extension
  // name instead, which is more human-readable.
  if (site_url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(browser_context)
            ->enabled_extensions()
            .GetExtensionOrAppByURL(site_url);
    if (extension)
      name = extension->name();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return name;
}

void ChromeContentBrowserClient::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
#if BUILDFLAG(IS_ANDROID)
  // Loading state text isn't used on Android, only in desktop UI.
  factory_params->provide_loading_state_updates = false;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          browser_context)) {
    return;
  }

  ChromeContentBrowserClientExtensionsPart::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
#endif
}

// These are treated as WebUI schemes but do not get WebUI bindings. Also,
// view-source is allowed for these schemes.
void ChromeContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->emplace_back(chrome::kChromeSearchScheme);
  additional_schemes->emplace_back(dom_distiller::kDomDistillerScheme);
  additional_schemes->emplace_back(content::kChromeDevToolsScheme);
}

void ChromeContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  additional_schemes->push_back(extensions::kExtensionScheme);
#endif
}

network::mojom::IPAddressSpace
ChromeContentBrowserClient::DetermineAddressSpaceFromURL(const GURL& url) {
  if (url.SchemeIs(chrome::kChromeSearchScheme))
    return network::mojom::IPAddressSpace::kLocal;
  if (url.SchemeIs(dom_distiller::kDomDistillerScheme))
    return network::mojom::IPAddressSpace::kPublic;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (url.SchemeIs(extensions::kExtensionScheme))
    return network::mojom::IPAddressSpace::kLocal;
#endif

  return network::mojom::IPAddressSpace::kUnknown;
}

bool ChromeContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) {
  return webui::LogWebUIUrl(web_ui_url);
}

bool ChromeContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
      origin);
}

bool ChromeContentBrowserClient::IsHandledURL(const GURL& url) {
  return ProfileIOData::IsHandledURL(url);
}

bool ChromeContentBrowserClient::HasCustomSchemeHandler(
    content::BrowserContext* browser_context,
    const std::string& scheme) {
  if (custom_handlers::ProtocolHandlerRegistry* protocol_handler_registry =
          ProtocolHandlerRegistryFactory::GetForBrowserContext(
              browser_context)) {
    return protocol_handler_registry->IsHandledProtocol(scheme);
  }

  return false;
}

bool ChromeContentBrowserClient::HasWebRequestAPIProxy(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  if (!web_request_api) {
    return false;
  }
  return web_request_api && web_request_api->MayHaveProxies();
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return ChromeContentBrowserClientExtensionsPart::CanCommitURL(process_host,
                                                                url);
#else
  return true;
#endif
}

void ChromeContentBrowserClient::OverrideNavigationParams(
    std::optional<GURL> source_process_site_url,
    ui::PageTransition* transition,
    bool* is_renderer_initiated,
    content::Referrer* referrer,
    std::optional<url::Origin>* initiator_origin) {
  DCHECK(transition);
  DCHECK(is_renderer_initiated);
  DCHECK(referrer);
  // IsNTPURL only looks at the origin of the parameter, so it is safe to use
  // the effective site URL for the source process.
  if (source_process_site_url &&
      search::IsNTPURL(source_process_site_url.value()) &&
      ui::PageTransitionCoreTypeIs(*transition, ui::PAGE_TRANSITION_LINK)) {
    // Clicks on tiles of the new tab page should be treated as if a user
    // clicked on a bookmark.  This is consistent with native implementations
    // like Android's.  This also helps ensure that security features (like
    // Sec-Fetch-Site and SameSite-cookies) will treat the navigation as
    // browser-initiated.
    *transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    *is_renderer_initiated = false;
    *referrer = content::Referrer();
    *initiator_origin = std::nullopt;
  }
}

bool ChromeContentBrowserClient::ShouldStayInParentProcessForNTP(
    const GURL& url,
    const GURL& parent_site_url) {
  // Allow most visited iframes to stay in the parent process but only if that
  // process is for NTP.
  //
  // TODO(alexmos): Consider further tightening this exception to just the
  // third-party remote NTP in the parent, rather than any NTP.
  //
  // TODO(crbug.com/40447789): place those iframes into OOPIFs and remove this
  // exception. Relaxing site isolation like this is a bad idea and should be
  // avoided.
  //
  // TODO(crbug.com/41261582): clean up the logic for detecting NTP.
  return url.SchemeIs(chrome::kChromeSearchScheme) &&
         url.host() == chrome::kChromeSearchMostVisitedHost &&
         search::IsNTPURL(parent_site_url);
}

bool ChromeContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  Profile* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());
  // This may be nullptr during tests. In that case, just assume any site can
  // share any host.
  if (!profile)
    return true;

#if !BUILDFLAG(IS_ANDROID)
  // Instant URLs should only be in the instant process and instant process
  // should only have Instant URLs.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (instant_service) {
    bool is_instant_process =
        instant_service->IsInstantProcess(process_host->GetID());
    bool should_be_in_instant_process =
        search::ShouldAssignURLToInstantRenderer(site_url, profile);
    if (is_instant_process || should_be_in_instant_process)
      return is_instant_process && should_be_in_instant_process;
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return ChromeContentBrowserClientExtensionsPart::IsSuitableHost(
      profile, process_host, site_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::MayReuseHost(
    content::RenderProcessHost* process_host) {
  // If there is currently a no-state prefetcher in progress for the host
  // provided, it may not be shared. We require prefetchers to be by themselves
  // in a separate process so that we can monitor their resource usage.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          process_host->GetBrowserContext());
  if (no_state_prefetch_manager &&
      !no_state_prefetch_manager->MayReuseProcessHost(process_host)) {
    return false;
  }

  return true;
}

size_t ChromeContentBrowserClient::GetProcessCountToIgnoreForLimit() {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return ChromeContentBrowserClientExtensionsPart::
      GetProcessCountToIgnoreForLimit();
#else
  return 0;
#endif
}

std::optional<blink::ParsedPermissionsPolicy>
ChromeContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    content::WebContents* web_contents,
    const url::Origin& app_origin) {
#if !BUILDFLAG(IS_ANDROID)
  // Extensions are exempt from manifest policy enforcement and retain the
  // default frame permissions policy.
  if (app_origin.scheme() == extensions::kExtensionScheme) {
    return std::nullopt;
  }

  CHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto& registrar =
      web_app::WebAppProvider::GetForWebApps(profile)->registrar_unsafe();
  std::vector<webapps::AppId> app_ids_for_origin =
      registrar.FindAppsInScope(app_origin.GetURL());
  if (app_ids_for_origin.empty()) {
    return blink::ParsedPermissionsPolicy();
  }

  return registrar.GetPermissionsPolicy(app_ids_for_origin[0]);
#else
  NOTIMPLEMENTED();
  return blink::ParsedPermissionsPolicy();
#endif
}

bool ChromeContentBrowserClient::ShouldTryToUseExistingProcessHost(
    content::BrowserContext* browser_context,
    const GURL& url) {
  // Top Chrome WebUI should try to share a RenderProcessHost with other
  // existing Top Chrome WebUI.
  if (IsTopChromeWebUIURL(url)) {
    return true;
  }

  return false;
}

bool ChromeContentBrowserClient::ShouldEmbeddedFramesTryToReuseExistingProcess(
    content::RenderFrameHost* outermost_main_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldEmbeddedFramesTryToReuseExistingProcess(outermost_main_frame);
#else
  return true;
#endif
}

void ChromeContentBrowserClient::SiteInstanceGotProcessAndSite(
    SiteInstance* site_instance) {
  CHECK(site_instance->HasProcess());

  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  if (!profile)
    return;

#if !BUILDFLAG(IS_ANDROID)
  // Remember the ID of the Instant process to signal the renderer process
  // on startup in |AppendExtraCommandLineSwitches| below.
  if (search::ShouldAssignURLToInstantRenderer(site_instance->GetSiteURL(),
                                               profile)) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile);
    if (instant_service)
      instant_service->AddInstantProcess(site_instance->GetProcess());
  }
#endif

  for (auto& part : extra_parts_) {
    part->SiteInstanceGotProcessAndSite(site_instance);
  }
}

bool ChromeContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_effective_url,
    const GURL& destination_effective_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldSwapBrowsingInstancesForNavigation(
          site_instance, current_effective_url, destination_effective_url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldIsolateErrorPage(bool in_main_frame) {
  // TODO(nasko): Consider supporting error page isolation in subframes if
  // Site Isolation is enabled.
  return in_main_frame;
}

std::vector<url::Origin>
ChromeContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  std::vector<url::Origin> isolated_origin_list;

  if (DoesGaiaOriginRequireDedicatedProcess()) {
    isolated_origin_list.push_back(GaiaUrls::GetInstance()->gaia_origin());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  auto origins_from_extensions = ChromeContentBrowserClientExtensionsPart::
      GetOriginsRequiringDedicatedProcess();
  std::move(std::begin(origins_from_extensions),
            std::end(origins_from_extensions),
            std::back_inserter(isolated_origin_list));
#endif

  // Include additional origins preloaded with specific browser configurations,
  // if any.  For example, this is used on Google Chrome for Android to preload
  // a list of important sites to isolate.
  auto built_in_origins =
      site_isolation::GetBrowserSpecificBuiltInIsolatedOrigins();
  std::move(std::begin(built_in_origins), std::end(built_in_origins),
            std::back_inserter(isolated_origin_list));

  return isolated_origin_list;
}

bool ChromeContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  return base::FeatureList::IsEnabled(features::kSitePerProcess);
}

bool ChromeContentBrowserClient::ShouldDisableSiteIsolation(
    content::SiteIsolationMode site_isolation_mode) {
  return site_isolation::SiteIsolationPolicy::
      ShouldDisableSiteIsolationDueToMemoryThreshold(site_isolation_mode);
}

std::vector<std::string>
ChromeContentBrowserClient::GetAdditionalSiteIsolationModes() {
  std::vector<std::string> modes;
  if (site_isolation::SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled())
    modes.push_back("Password Sites");
  if (site_isolation::SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled())
    modes.push_back("Logged-in Sites");
  return modes;
}

void ChromeContentBrowserClient::PersistIsolatedOrigin(
    content::BrowserContext* context,
    const url::Origin& origin,
    content::ChildProcessSecurityPolicy::IsolatedOriginSource source) {
  site_isolation::SiteIsolationPolicy::PersistIsolatedOrigin(context, origin,
                                                             source);
}

bool ChromeContentBrowserClient::ShouldUrlUseApplicationIsolationLevel(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)

  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
          browser_context)) {
    return false;
  }

  // Convert |url| to an origin to resolve blob: URLs.
  auto origin = url::Origin::Create(url);
  if (origin.scheme() == chrome::kIsolatedAppScheme) {
    return true;
  }
#endif
  return false;
}

bool ChromeContentBrowserClient::IsIsolatedContextAllowedForUrl(
    content::BrowserContext* browser_context,
    const GURL& lock_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          browser_context)) {
    return false;
  }

  // Allow restricted context APIs in Chrome Apps.
  auto* extension = extensions::ExtensionRegistry::Get(browser_context)
                        ->enabled_extensions()
                        .GetExtensionOrAppByURL(lock_url);
  return extension &&
         (extension->is_platform_app() ||
          IsExtensionIdAllowedToUseIsolatedContext(extension->id()));
#else
  return false;
#endif
}

void ChromeContentBrowserClient::CheckGetAllScreensMediaAllowed(
    content::RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  capture_policy::CheckGetAllScreensMediaAllowed(
      render_frame_host->GetBrowserContext(),
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin().GetURL(),
      std::move(callback));
}

bool ChromeContentBrowserClient::IsFileAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
  return ChromeNetworkDelegate::IsAccessAllowed(path, absolute_path,
                                                profile_path);
}

namespace {

void MaybeAppendBlinkSettingsSwitchForFieldTrial(
    const base::CommandLine& browser_command_line,
    base::CommandLine* command_line) {
  // List of field trials that modify the blink-settings command line flag. No
  // two field trials in the list should specify the same keys, otherwise one
  // field trial may overwrite another. See Source/core/frame/Settings.in in
  // Blink for the list of valid keys.
  static const char* const kBlinkSettingsFieldTrials[] = {
      // Keys: disallowFetchForDocWrittenScriptsInMainFrame
      //       disallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections
      //       disallowFetchForDocWrittenScriptsInMainFrameIfEffectively2G
      "DisallowFetchForDocWrittenScriptsInMainFrame",
  };

  std::vector<std::string> blink_settings;
  for (const char* field_trial_name : kBlinkSettingsFieldTrials) {
    // Each blink-settings field trial should include a forcing_flag group,
    // to make sure that clients that specify the blink-settings flag on the
    // command line are excluded from the experiment groups. To make
    // sure we assign clients that specify this flag to the forcing_flag
    // group, we must call GetFieldTrialParams for each field trial first
    // (for example, before checking HasSwitch() and returning), since
    // GetFieldTrialParams has the side-effect of assigning the client to
    // a field trial group.
    std::map<std::string, std::string> params;
    if (base::GetFieldTrialParams(field_trial_name, &params)) {
      for (const auto& param : params) {
        blink_settings.push_back(base::StringPrintf(
            "%s=%s", param.first.c_str(), param.second.c_str()));
      }
    }
  }

  if (blink_settings.empty()) {
    return;
  }

  if (browser_command_line.HasSwitch(blink::switches::kBlinkSettings) ||
      command_line->HasSwitch(blink::switches::kBlinkSettings)) {
    // The field trials should be configured to force users that specify the
    // blink-settings flag into a group with no params, and we return
    // above if no params were specified, so it's an error if we reach
    // this point.
    LOG(WARNING) << "Received field trial params, "
                    "but blink-settings switch already specified.";
    return;
  }

  command_line->AppendSwitchASCII(blink::switches::kBlinkSettings,
                                  base::JoinString(blink_settings, ","));
}

}  // namespace

void ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  crash_keys::AppendStringAnnotationsCommandLineSwitch(command_line);
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info) {
    command_line->AppendSwitchASCII(switches::kMetricsClientID,
                                    client_info->client_id);
  }
#elif BUILDFLAG(IS_POSIX)
#if !BUILDFLAG(IS_ANDROID)
  pid_t pid;
  if (crash_reporter::GetHandlerSocket(nullptr, &pid)) {
    command_line->AppendSwitchASCII(
        crash_reporter::switches::kCrashpadHandlerPid,
        base::NumberToString(pid));
  }
#endif
  std::string switch_value;
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info) {
    switch_value = client_info->client_id;
  }
  switch_value.push_back(',');
  switch_value.append(chrome::GetChannelName(chrome::WithExtendedStable(true)));
  command_line->AppendSwitchASCII(switches::kEnableCrashReporter, switch_value);
#endif

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Pass startup parameter FDs to child processes in Lacros.
  if (process_type != switches::kZygoteProcess) {
    constexpr int kStartupDataFD =
        kCrosStartupDataDescriptor + base::GlobalDescriptors::kBaseDescriptor;
    command_line->AppendSwitchASCII(chromeos::switches::kCrosStartupDataFD,
                                    base::NumberToString(kStartupDataFD));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  static const char* const kCommonSwitchNames[] = {
      embedder_support::kUserAgent,
      switches::kUserDataDir,  // Make logs go to the right file.
  };
  command_line->CopySwitchesFrom(browser_command_line, kCommonSwitchNames);

  static const char* const kDinosaurEasterEggSwitches[] = {
      error_page::switches::kDisableDinosaurEasterEgg,
      error_page::switches::kEnableDinosaurEasterEggAltGameImages,
  };
  command_line->CopySwitchesFrom(browser_command_line,
                                 kDinosaurEasterEggSwitches);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS need to pass primary user homedir (in multi-profiles session).
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  command_line->AppendSwitchASCII(ash::switches::kHomedir, homedir.value());
#endif

  if (process_type == switches::kRendererProcess) {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(child_process_id);
    if (process) {
      for (auto& part : extra_parts_) {
        part->AppendExtraRendererCommandLineSwitches(command_line, *process);
      }
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    const std::string& login_profile =
        browser_command_line.GetSwitchValueASCII(ash::switches::kLoginProfile);
    if (!login_profile.empty()) {
      command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                      login_profile);
    }
#endif

    MaybeCopyDisableWebRtcEncryptionSwitch(command_line, browser_command_line,
                                           chrome::GetChannel());
    if (process) {
      Profile* profile =
          Profile::FromBrowserContext(process->GetBrowserContext());
      PrefService* prefs = profile->GetPrefs();
      // Currently this pref is only registered if applied via a policy.
      if (prefs->HasPrefPath(prefs::kDisable3DAPIs) &&
          prefs->GetBoolean(prefs::kDisable3DAPIs)) {
        // Turn this policy into a command line switch.
        command_line->AppendSwitch(switches::kDisable3DAPIs);
      }

      if (prefs->GetBoolean(prefs::kPrintPreviewDisabled))
        command_line->AppendSwitch(switches::kDisablePrintPreview);

      if (prefs->GetBoolean(prefs::kDataUrlInSvgUseEnabled)) {
        command_line->AppendSwitch(blink::switches::kDataUrlInSvgUseEnabled);
      }

      if (prefs->GetBoolean(policy::policy_prefs::kMutationEventsEnabled)) {
        command_line->AppendSwitch(blink::switches::kMutationEventsEnabled);
      }

      if (!prefs->GetBoolean(
              policy::policy_prefs::kKeyboardFocusableScrollersEnabled)) {
        command_line->AppendSwitch(
            blink::switches::kKeyboardFocusableScrollersOptOut);
      }
      if (!prefs->GetBoolean(
              policy::policy_prefs::kStandardizedBrowserZoomEnabled)) {
        command_line->AppendSwitch(
            blink::switches::kDisableStandardizedBrowserZoom);
      }
      if (prefs->GetBoolean(
              policy::policy_prefs::kCSSCustomStateDeprecatedSyntaxEnabled)) {
        command_line->AppendSwitch(
            blink::switches::kCSSCustomStateDeprecatedSyntaxEnabled);
      }

      if (prefs->GetBoolean(policy::policy_prefs::
                                kForcePermissionPolicyUnloadDefaultEnabled)) {
        command_line->AppendSwitch(
            blink::switches::kForcePermissionPolicyUnloadDefaultEnabled);
      }

#if !BUILDFLAG(IS_ANDROID)
      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile);
      if (instant_service &&
          instant_service->IsInstantProcess(process->GetID())) {
        command_line->AppendSwitch(switches::kInstantProcess);
      }

      // Enable SharedArrayBuffer on desktop if allowed by Enterprise Policy.
      // TODO(crbug.com/40155376) Remove when migration to COOP+COEP is
      // complete.
      if (prefs->GetBoolean(
              prefs::kSharedArrayBufferUnrestrictedAccessAllowed)) {
        command_line->AppendSwitch(
            switches::kSharedArrayBufferUnrestrictedAccessAllowed);
      }
#endif
      if (!prefs->GetBoolean(prefs::kSandboxExternalProtocolBlocked))
        command_line->AppendSwitch(kDisableSandboxExternalProtocolSwitch);

      if (prefs->HasPrefPath(prefs::kAllowDinosaurEasterEgg) &&
          !prefs->GetBoolean(prefs::kAllowDinosaurEasterEgg)) {
        command_line->AppendSwitch(
            error_page::switches::kDisableDinosaurEasterEgg);
      }

      auto* management_service_factory =
          policy::ManagementServiceFactory::GetInstance();
      auto* browser_managment_service =
          management_service_factory->GetForProfile(profile);
      if ((browser_managment_service &&
           browser_managment_service->IsManaged()) ||
          management_service_factory->GetForPlatform()->IsManaged()) {
        command_line->AppendSwitch(
            error_page::switches::kEnableDinosaurEasterEggAltGameImages);
      }

      MaybeAppendSecureOriginsAllowlistSwitch(command_line);

      if (prefs->HasPrefPath(prefs::kScrollToTextFragmentEnabled) &&
          !prefs->GetBoolean(prefs::kScrollToTextFragmentEnabled)) {
        command_line->AppendSwitch(switches::kDisableScrollToTextFragment);
      }

      if (!prefs->GetList(enterprise_reporting::kCloudLegacyTechReportAllowlist)
               .empty()) {
        command_line->AppendSwitch(
            blink::switches::kLegacyTechReportPolicyEnabled);
      }

      // The IntensiveWakeUpThrottling feature is typically managed via a
      // base::Feature, but it has a managed policy override. The override is
      // communicated to blink via a custom command-line flag. See
      // PageSchedulerImpl for the other half of related logic.
      PrefService* local_state = g_browser_process->local_state();
      const PrefService::Preference* pref = local_state->FindPreference(
          policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled);
      if (pref && pref->IsManaged()) {
        command_line->AppendSwitchASCII(
            blink::switches::kIntensiveWakeUpThrottlingPolicy,
            pref->GetValue()->GetBool()
                ? blink::switches::kIntensiveWakeUpThrottlingPolicy_ForceEnable
                : blink::switches::
                      kIntensiveWakeUpThrottlingPolicy_ForceDisable);
      }

#if BUILDFLAG(IS_ANDROID)
      // Communicating to content/ for BackForwardCache.
      if (prefs->HasPrefPath(policy::policy_prefs::kBackForwardCacheEnabled) &&
          !prefs->GetBoolean(policy::policy_prefs::kBackForwardCacheEnabled)) {
        command_line->AppendSwitch(switches::kDisableBackForwardCache);
      }
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
      // Make the WebAuthenticationRemoteProxiedRequestsAllowed policy enable
      // the experimental WebAuthenticationRemoteDesktopSupport Blink runtime
      // feature.
      if (prefs->GetBoolean(
              webauthn::pref_names::kRemoteProxiedRequestsAllowed)) {
        command_line->AppendSwitch(switches::kWebAuthRemoteDesktopSupport);
      }

      if (IsCartModuleEnabled()) {
        command_line->AppendSwitch(commerce::switches::kEnableChromeCart);
      }
#endif
    }

    MaybeAppendBlinkSettingsSwitchForFieldTrial(browser_command_line,
                                                command_line);

#if BUILDFLAG(IS_ANDROID)
    // If the platform is Android, force the distillability service on.
    command_line->AppendSwitch(switches::kEnableDistillabilityService);
#endif

#if BUILDFLAG(ENABLE_NACL)
    AppendDisableNaclSwitchIfNecessary(command_line);
#endif

    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
        autofill::switches::kIgnoreAutocompleteOffForAutofill,
        autofill::switches::kShowAutofillSignatures,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        switches::kShortMergeSessionTimeoutForTest,  // For tests only.
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
        extensions::switches::kAllowHTTPBackgroundPage,
        extensions::switches::kAllowLegacyExtensionManifests,
        extensions::switches::kDisableExtensionsHttpThrottling,
        extensions::switches::kEnableExperimentalExtensionApis,
        extensions::switches::kExtensionsOnChromeURLs,
        extensions::switches::kSetExtensionThrottleTestParams,  // For tests
                                                                // only.
        extensions::switches::kAllowlistedExtensionID,
        extensions::switches::kExtensionTestApiOnWebPages,  // For tests only.
#endif
        switches::kAllowInsecureLocalhost,
        switches::kAppsGalleryURL,
        switches::kDisableJavaScriptHarmonyShipping,
        variations::switches::kEnableBenchmarking,
        switches::kEnableDistillabilityService,
        switches::kEnableNaCl,
#if BUILDFLAG(ENABLE_NACL)
        switches::kEnableNaClDebug,
#endif
        switches::kEnableNetBenchmarking,
        switches::kExtensionAiDataCollection,
#if BUILDFLAG(IS_CHROMEOS)
        chromeos::switches::
            kTelemetryExtensionPwaOriginOverrideForTesting,  // For tests only.
        switches::kForceAppMode,
#endif
#if BUILDFLAG(ENABLE_NACL)
        switches::kForcePNaClSubzero,
#endif
        switches::kForceUIDirection,
        switches::kIgnoreGooglePortNumbers,
        switches::kJavaScriptHarmony,
        switches::kEnableExperimentalWebAssemblyFeatures,
        embedder_support::kOriginTrialDisabledFeatures,
        embedder_support::kOriginTrialPublicKey,
        switches::kReaderModeHeuristics,
        translate::switches::kTranslateSecurityOrigin,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames);
  } else if (process_type == switches::kUtilityProcess) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    static const char* const kSwitchNames[] = {
        extensions::switches::kAllowHTTPBackgroundPage,
        extensions::switches::kEnableExperimentalExtensionApis,
        extensions::switches::kExtensionsOnChromeURLs,
        extensions::switches::kAllowlistedExtensionID,
        switches::kExtensionAiDataCollection,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames);
#endif
    MaybeAppendSecureOriginsAllowlistSwitch(command_line);
  } else if (process_type == switches::kZygoteProcess) {
    // It would be preferable to call AppendDisableNaclSwitchIfNecessary to
    // disable NaCl for the zygote process. Unfortunately that method depends on
    // state (including policy) that is determined after the zygote is forked.
    // Instead we rely on renderers overriding the zygote state.

    // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
#if BUILDFLAG(ENABLE_NACL)
    static const char* const kSwitchNames[] = {
        switches::kEnableNaClDebug,
        switches::kForcePNaClSubzero,
        switches::kVerboseLoggingInNacl,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames);
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Ensure zygote loads the resource bundle for the right locale.
    static const char* const kMoreSwitchNames[] = {switches::kLang};
    command_line->CopySwitchesFrom(browser_command_line, kMoreSwitchNames);
#endif
#if BUILDFLAG(IS_CHROMEOS)
    // This is called before feature flags are parsed, so pass them in their raw
    // form.
    static const char* const kMoreCrOSSwitchNames[] = {
        chromeos::switches::kFeatureFlags};
    command_line->CopySwitchesFrom(browser_command_line, kMoreCrOSSwitchNames);
#endif
  } else if (process_type == switches::kGpuProcess) {
    // If --ignore-gpu-blocklist is passed in, don't send in crash reports
    // because GPU is expected to be unreliable.
    if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlocklist) &&
        !command_line->HasSwitch(switches::kDisableBreakpad))
      command_line->AppendSwitch(switches::kDisableBreakpad);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ChromeCrashReporterClient::ShouldPassCrashLoopBefore(process_type)) {
    static const char* const kSwitchNames[] = {
        crash_reporter::switches::kCrashLoopBefore,
    };
    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames);
  }
#endif

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kNoPreReadMainDll)) {
    command_line->AppendSwitch(switches::kNoPreReadMainDll);
  }
#endif

  ThreadProfilerConfiguration::Get()->AppendCommandLineSwitchForChildProcess(
      command_line);

  if (process_type != switches::kZygoteProcess) {
    // The switch value depends on the "HeapProfilerCentralControl" feature, and
    // the zygote starts before the FeatureList is available.
    if (const auto* heap_profiler_controller =
            heap_profiling::HeapProfilerController::GetInstance()) {
      heap_profiler_controller->AppendCommandLineSwitchForChildProcess(
          command_line, GetProfilerProcessType(*command_line),
          child_process_id);
    }

    // Enterprise policies may set the local state. `g_browser_process` is only
    // available for non-zygote processes.
    if (!g_browser_process->local_state()->GetBoolean(
            prefs::kDataURLWhitespacePreservationEnabled)) {
      command_line->AppendSwitch(net::kRemoveWhitespaceForDataURLs);
    }
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Opt into a hardened stack canary mitigation if it hasn't already been
  // force-disabled.
  if (!browser_command_line.HasSwitch(switches::kChangeStackGuardOnFork)) {
    command_line->AppendSwitchASCII(switches::kChangeStackGuardOnFork,
                                    switches::kChangeStackGuardOnForkEnabled);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
  // Communicating to renderer for starting the reader for web feed.
  if (feed::IsWebFeedEnabledForLocale(feed::FeedServiceFactory::GetCountry())) {
    command_line->AppendSwitch(feed::switches::kEnableRssLinkReader);
  }
#endif
}

std::string
ChromeContentBrowserClient::GetApplicationClientGUIDForQuarantineCheck() {
  return std::string(chrome::kApplicationClientIDStringForAVScanning);
}

download::QuarantineConnectionCallback
ChromeContentBrowserClient::GetQuarantineConnectionCallback() {
  return base::BindRepeating(
      &ChromeDownloadManagerDelegate::ConnectToQuarantineService);
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return GetIOThreadApplicationLocale();
  return g_browser_process->GetApplicationLocale();
}

std::string ChromeContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
}

gfx::ImageSkia ChromeContentBrowserClient::GetDefaultFavicon() {
  return favicon::GetDefaultFavicon().AsImageSkia();
}

bool ChromeContentBrowserClient::IsDataSaverEnabled(
    content::BrowserContext* browser_context) {
  if (!browser_context || browser_context->IsOffTheRecord())
    return false;

  return data_saver::IsDataSaverEnabled();
}

void ChromeContentBrowserClient::UpdateRendererPreferencesForWorker(
    content::BrowserContext* browser_context,
    blink::RendererPreferences* out_prefs) {
  DCHECK(browser_context);
  DCHECK(out_prefs);
  renderer_preferences_util::UpdateFromSystemSettings(
      out_prefs, Profile::FromBrowserContext(browser_context));
}

content::AllowServiceWorkerResult
ChromeContentBrowserClient::AllowServiceWorker(
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    content::BrowserContext* context) {
  DCHECK(context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Check if this is an extension-related service worker, and, if so, if it's
  // allowed (this can return false if, e.g., the extension is disabled).
  // If it's not allowed, return immediately. We deliberately do *not* report
  // to the PageSpecificContentSettings, since the service worker is blocked
  // because of the extension, rather than because of the user's content
  // settings.
  if (!ChromeContentBrowserClientExtensionsPart::AllowServiceWorker(
          scope, first_party_url, script_url, context)) {
    return content::AllowServiceWorkerResult::No();
  }
#endif

  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile);
  return embedder_support::AllowServiceWorker(
      scope, site_for_cookies, top_frame_origin, cookie_settings.get(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

bool ChromeContentBrowserClient::MayDeleteServiceWorkerRegistration(
    const GURL& scope,
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!ChromeContentBrowserClientExtensionsPart::
          MayDeleteServiceWorkerRegistration(scope, browser_context)) {
    return false;
  }
#endif

  return true;
}

bool ChromeContentBrowserClient::ShouldTryToUpdateServiceWorkerRegistration(
    const GURL& scope,
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!ChromeContentBrowserClientExtensionsPart::
          ShouldTryToUpdateServiceWorkerRegistration(scope, browser_context)) {
    return false;
  }
#endif

  return true;
}

bool ChromeContentBrowserClient::AllowSharedWorker(
    const GURL& worker_url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
    content::BrowserContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if cookies are allowed.
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return embedder_support::AllowSharedWorker(
      worker_url, site_for_cookies, top_frame_origin, name, storage_key,
      same_site_cookies, render_process_id, render_frame_id,
      cookie_settings.get());
}

bool ChromeContentBrowserClient::DoesSchemeAllowCrossOriginSharedWorker(
    const std::string& scheme) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Extensions are allowed to start cross-origin shared workers.
  if (scheme == extensions::kExtensionScheme)
    return true;
#endif

  return false;
}

bool ChromeContentBrowserClient::AllowSignedExchange(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetPrefs()->GetBoolean(prefs::kSignedHTTPExchangeEnabled);
}

bool ChromeContentBrowserClient::AllowCompressionDictionaryTransport(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetPrefs()->GetBoolean(
      prefs::kCompressionDictionaryTransportEnabled);
}

void ChromeContentBrowserClient::RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)>
        continuation_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS)
  auto* delegate = policy::DlpScopedFileAccessDelegate::Get();
  if (delegate) {
    delegate->RequestFilesAccess(files, destination_url,
                                 std::move(continuation_callback));
  } else {
    std::move(continuation_callback)
        .Run(file_access::ScopedFileAccess::Allowed());
  }
#else
  std::move(continuation_callback)
      .Run(file_access::ScopedFileAccess::Allowed());
#endif
}

void ChromeContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback) {
  // An empty list is passed for render_frames here since we manually notify
  // PageSpecificContentSettings that the file system was accessed below.
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  bool allow =
      embedder_support::AllowWorkerFileSystem(url, {}, cookie_settings.get());
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  GuestPermissionRequestHelper(url, render_frames, std::move(callback), allow);
#else
  FileSystemAccessed(url, render_frames, std::move(callback), allow);
#endif
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
void ChromeContentBrowserClient::GuestPermissionRequestHelper(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback,
    bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::map<int, int> process_map;
  bool has_web_view_guest = false;
  // Record access to file system for potential display in UI.
  for (const auto& it : render_frames) {
    if (process_map.find(it.child_id) != process_map.end())
      continue;

    process_map.insert(std::pair<int, int>(it.child_id, it.frame_routing_id));

    if (extensions::WebViewRendererState::GetInstance()->IsGuest(it.child_id))
      has_web_view_guest = true;
  }
  if (!has_web_view_guest) {
    FileSystemAccessed(url, render_frames, std::move(callback), allow);
    return;
  }
  DCHECK_EQ(1U, process_map.size());
  std::map<int, int>::const_iterator it = process_map.begin();

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHostId(
          content::GlobalRenderFrameHostId(it->first, it->second));
  web_view_permission_helper->RequestFileSystemPermission(
      url, allow,
      base::BindOnce(&ChromeContentBrowserClient::FileSystemAccessed,
                     weak_factory_.GetWeakPtr(), url, render_frames,
                     std::move(callback)));
}
#endif

void ChromeContentBrowserClient::FileSystemAccessed(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback,
    bool allow) {
  // Record access to file system for potential display in UI.
  for (const auto& it : render_frames) {
    auto* rfh = content::RenderFrameHost::FromID(it);
    if (!rfh) {
      continue;
    }
    content_settings::PageSpecificContentSettings::StorageAccessed(
        content_settings::mojom::ContentSettingsManager::StorageType::
            FILE_SYSTEM,
        it, rfh->GetStorageKey(), !allow);
  }
  std::move(callback).Run(allow);
}

bool ChromeContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  return embedder_support::AllowWorkerIndexedDB(url, render_frames,
                                                cookie_settings.get());
}

bool ChromeContentBrowserClient::AllowWorkerCacheStorage(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  return embedder_support::AllowWorkerCacheStorage(url, render_frames,
                                                   cookie_settings.get());
}

bool ChromeContentBrowserClient::AllowWorkerWebLocks(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  return embedder_support::AllowWorkerWebLocks(url, cookie_settings.get());
}

ChromeContentBrowserClient::AllowWebBluetoothResult
ChromeContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  // TODO(crbug.com/40462828): Don't disable if
  // base::CommandLine::ForCurrentProcess()->
  // HasSwitch(switches::kEnableWebBluetooth) is true.
  if (base::GetFieldTrialParamValue(
          permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
          "Bluetooth") ==
      permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue) {
    // The kill switch is enabled for this permission. Block requests.
    return AllowWebBluetoothResult::BLOCK_GLOBALLY_DISABLED;
  }

  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_GUARD) == CONTENT_SETTING_BLOCK) {
    return AllowWebBluetoothResult::BLOCK_POLICY;
  }
  return AllowWebBluetoothResult::ALLOW;
}

std::string ChromeContentBrowserClient::GetWebBluetoothBlocklist() {
  return base::GetFieldTrialParamValue("WebBluetoothBlocklist",
                                       "blocklist_additions");
}

bool ChromeContentBrowserClient::IsInterestGroupAPIAllowed(
    content::RenderFrameHost* render_frame_host,
    InterestGroupApiOperation operation,
    const url::Origin& top_frame_origin,
    const url::Origin& api_origin) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);

  bool allowed = privacy_sandbox_settings->IsFledgeAllowed(
      top_frame_origin, api_origin, operation, render_frame_host);

  if (operation == InterestGroupApiOperation::kJoin) {
    content_settings::PageSpecificContentSettings::InterestGroupJoined(
        render_frame_host, api_origin, !allowed);
    content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
        render_frame_host,
        content::InterestGroupManager::InterestGroupDataKey{api_origin,
                                                            top_frame_origin},
        BrowsingDataModel::StorageType::kInterestGroup, !allowed);
  }

  return allowed;
}

bool ChromeContentBrowserClient::IsPrivacySandboxReportingDestinationAttested(
    content::BrowserContext* browser_context,
    const url::Origin& destination_origin,
    content::PrivacySandboxInvokingAPI invoking_api) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);

  privacy_sandbox::PrivacySandboxAttestationsGatedAPI gated_api;
  switch (invoking_api) {
    case content::PrivacySandboxInvokingAPI::kProtectedAudience:
      gated_api = privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
          kProtectedAudience;
      break;
    case content::PrivacySandboxInvokingAPI::kSharedStorage:
      gated_api =
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kSharedStorage;
      break;
  }

  return privacy_sandbox_settings->IsEventReportingDestinationAttested(
      destination_origin, gated_api);
}

void ChromeContentBrowserClient::OnAuctionComplete(
    content::RenderFrameHost* render_frame_host,
    std::optional<content::InterestGroupManager::InterestGroupDataKey>
        winner_data_key,
    bool is_server_auction,
    bool is_on_device_auction,
    content::AuctionResult result) {
  if (winner_data_key) {
    content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
        render_frame_host, winner_data_key.value(),
        BrowsingDataModel::StorageType::kInterestGroup,
        /*blocked=*/false);
  }
  if (auto* observer =
          page_load_metrics::MetricsWebContentsObserver::FromWebContents(
              WebContents::FromRenderFrameHost(render_frame_host))) {
    observer->OnAdAuctionComplete(render_frame_host, is_server_auction,
                                  is_on_device_auction, result);
  }
}

bool ChromeContentBrowserClient::IsAttributionReportingOperationAllowed(
    content::BrowserContext* browser_context,
    AttributionReportingOperation operation,
    content::RenderFrameHost* rfh,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin,
    bool* can_bypass) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (!privacy_sandbox_settings)
    return false;

  switch (operation) {
    case AttributionReportingOperation::kSource:
    case AttributionReportingOperation::kOsSource: {
      DCHECK(source_origin);
      DCHECK(reporting_origin);
      bool allowed = privacy_sandbox_settings->IsAttributionReportingAllowed(
          *source_origin, *reporting_origin, rfh);
      if (rfh) {
        content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
            rfh, content::AttributionDataModel::DataKey(*reporting_origin),
            BrowsingDataModel::StorageType::kAttributionReporting,
            /*blocked=*/!allowed);
      }
      return allowed;
    }
    case AttributionReportingOperation::kSourceVerboseDebugReport:
    case AttributionReportingOperation::kSourceAggregatableDebugReport:
    case AttributionReportingOperation::kOsSourceVerboseDebugReport:
      DCHECK(source_origin);
      DCHECK(reporting_origin);
      return privacy_sandbox_settings->IsAttributionReportingAllowed(
          *source_origin, *reporting_origin, rfh);
    case AttributionReportingOperation::kTrigger:
    case AttributionReportingOperation::kOsTrigger: {
      DCHECK(destination_origin);
      DCHECK(reporting_origin);
      bool allowed = privacy_sandbox_settings->IsAttributionReportingAllowed(
          *destination_origin, *reporting_origin, rfh);
      if (rfh) {
        content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
            rfh, content::AttributionDataModel::DataKey(*reporting_origin),
            BrowsingDataModel::StorageType::kAttributionReporting,
            /*blocked=*/!allowed);
      }
      return allowed;
    }
    case AttributionReportingOperation::kTriggerVerboseDebugReport:
    case AttributionReportingOperation::kTriggerAggregatableDebugReport:
    case AttributionReportingOperation::kOsTriggerVerboseDebugReport:
      DCHECK(destination_origin);
      DCHECK(reporting_origin);
      return privacy_sandbox_settings->IsAttributionReportingAllowed(
          *destination_origin, *reporting_origin, rfh);
    case AttributionReportingOperation::kReport:
      DCHECK(source_origin);
      DCHECK(destination_origin);
      DCHECK(reporting_origin);
      return privacy_sandbox_settings->MaySendAttributionReport(
          *source_origin, *destination_origin, *reporting_origin, rfh);
    case AttributionReportingOperation::kSourceTransitionalDebugReporting:
    case AttributionReportingOperation::kOsSourceTransitionalDebugReporting:
      DCHECK(source_origin);
      DCHECK(reporting_origin);
      DCHECK(can_bypass);
      return privacy_sandbox_settings
          ->IsAttributionReportingTransitionalDebuggingAllowed(
              *source_origin, *reporting_origin, *can_bypass);
    case AttributionReportingOperation::kTriggerTransitionalDebugReporting:
    case AttributionReportingOperation::kOsTriggerTransitionalDebugReporting:
      DCHECK(destination_origin);
      DCHECK(reporting_origin);
      DCHECK(can_bypass);
      return privacy_sandbox_settings
          ->IsAttributionReportingTransitionalDebuggingAllowed(
              *destination_origin, *reporting_origin, *can_bypass);
    case AttributionReportingOperation::kAny:
      return privacy_sandbox_settings->IsAttributionReportingEverAllowed();
  }
}

bool ChromeContentBrowserClient::IsAttributionReportingAllowedForContext(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& context_origin,
    const url::Origin& reporting_origin) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (!privacy_sandbox_settings) {
    return false;
  }

  return privacy_sandbox_settings->IsAttributionReportingAllowed(
      context_origin, reporting_origin, rfh);
}

bool ChromeContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);
  bool allowed = privacy_sandbox_settings->IsSharedStorageAllowed(
      top_frame_origin, accessing_origin, out_debug_message, rfh,
      out_block_is_site_setting_specific);
  if (rfh) {
    content_settings::PageSpecificContentSettings::BrowsingDataAccessed(
        rfh, blink::StorageKey::CreateFirstParty(accessing_origin),
        BrowsingDataModel::StorageType::kSharedStorage, !allowed);
  }
  return allowed;
}

bool ChromeContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);
  return privacy_sandbox_settings->IsSharedStorageSelectURLAllowed(
      top_frame_origin, accessing_origin, out_debug_message,
      out_block_is_site_setting_specific);
}

bool ChromeContentBrowserClient::IsPrivateAggregationAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin,
    bool* out_block_is_site_setting_specific) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);

  return privacy_sandbox_settings->IsPrivateAggregationAllowed(
      top_frame_origin, reporting_origin, out_block_is_site_setting_specific);
}

bool ChromeContentBrowserClient::IsPrivateAggregationDebugModeAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);

  return privacy_sandbox_settings->IsPrivateAggregationDebugModeAllowed(
      top_frame_origin, reporting_origin);
}

bool ChromeContentBrowserClient::IsCookieDeprecationLabelAllowed(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (!privacy_sandbox_settings) {
    return false;
  }
  return privacy_sandbox_settings->IsCookieDeprecationLabelAllowed();
}

bool ChromeContentBrowserClient::IsCookieDeprecationLabelAllowedForContext(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& context_origin) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  DCHECK(privacy_sandbox_settings);
  return privacy_sandbox_settings->IsCookieDeprecationLabelAllowedForContext(
      top_frame_origin, context_origin);
}

bool ChromeContentBrowserClient::IsFullCookieAccessAllowed(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& url,
    const blink::StorageKey& storage_key) {
  return dips_move::IsFullCookieAccessAllowed(browser_context, web_contents,
                                              url, storage_key);
}

// TODO: crbug.com/369813097 - Move this implementation into
// ChromeContentBrowserClient::IsFullCookieAccessAllowed() after DIPS migrates
// to //content.
namespace dips_move {
bool IsFullCookieAccessAllowed(content::BrowserContext* browser_context,
                               content::WebContents* web_contents,
                               const GURL& url,
                               const blink::StorageKey& storage_key) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile);
  if (!cookie_settings) {
    return true;
  }
  return cookie_settings->IsFullCookieAccessAllowed(
      url, storage_key.ToNetSiteForCookies(),
      url::Origin::Create(storage_key.top_level_site().GetURL()),
      cookie_settings->SettingOverridesForStorage());
}
}  // namespace dips_move

void ChromeContentBrowserClient::GrantCookieAccessDueToHeuristic(
    content::BrowserContext* browser_context,
    const net::SchemefulSite& top_frame_site,
    const net::SchemefulSite& accessing_site,
    base::TimeDelta ttl,
    bool ignore_schemes) {
  dips_move::GrantCookieAccessDueToHeuristic(
      browser_context, top_frame_site, accessing_site, ttl, ignore_schemes);
}

// TODO: crbug.com/369813097 - Move this implementation into
// ChromeContentBrowserClient::GrantCookieAccessDueToHeuristic() after DIPS
// migrates to //content.
namespace dips_move {
void GrantCookieAccessDueToHeuristic(content::BrowserContext* browser_context,
                                     const net::SchemefulSite& top_frame_site,
                                     const net::SchemefulSite& accessing_site,
                                     base::TimeDelta ttl,
                                     bool ignore_schemes) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!cookie_settings) {
    return;
  }

  cookie_settings->SetTemporaryCookieGrantForHeuristic(
      accessing_site.GetURL(), top_frame_site.GetURL(), ttl,
      /*use_schemeless_patterns=*/ignore_schemes);
}
}  // namespace dips_move

#if BUILDFLAG(IS_CHROMEOS)
void ChromeContentBrowserClient::OnTrustAnchorUsed(
    content::BrowserContext* browser_context) {
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!service) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  service->SetUsedPolicyCertificates();
}
#endif

bool ChromeContentBrowserClient::CanSendSCTAuditingReport(
    content::BrowserContext* browser_context) {
  return SCTReportingService::CanSendSCTAuditingReport();
}

void ChromeContentBrowserClient::OnNewSCTAuditingReportSent(
    content::BrowserContext* browser_context) {
  SCTReportingService::OnNewSCTAuditingReportSent();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  if (!SystemNetworkContextManager::GetInstance())
    return nullptr;

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network::mojom::NetworkContext*
ChromeContentBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_browser_process->system_network_context_manager());
  return g_browser_process->system_network_context_manager()->GetContext();
}

std::string ChromeContentBrowserClient::GetGeolocationApiKey() {
  return google_apis::GetAPIKey();
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
device::GeolocationSystemPermissionManager*
ChromeContentBrowserClient::GetGeolocationSystemPermissionManager() {
  return device::GeolocationSystemPermissionManager::GetInstance();
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool ChromeContentBrowserClient::ShouldUseGmsCoreGeolocationProvider() {
  // Indicate that Chrome uses the GMS core location provider.
  return true;
}
#endif

content::GeneratedCodeCacheSettings
ChromeContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(context->GetPath(), &cache_path);
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  int64_t size_in_bytes = 0;
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    size_in_bytes = local_state->GetInteger(prefs::kDiskCacheSize);
    base::FilePath disk_cache_dir =
        local_state->GetFilePath(prefs::kDiskCacheDir);
    if (!disk_cache_dir.empty())
      cache_path = disk_cache_dir.Append(cache_path.BaseName());
  }
  return content::GeneratedCodeCacheSettings(true, size_in_bytes, cache_path);
}

std::string ChromeContentBrowserClient::GetWebUIHostnameForCodeCacheMetrics(
    const GURL& webui_url) const {
#if !BUILDFLAG(IS_ANDROID)
  return webui::GetWebUIHostnameForCodeCacheMetrics(webui_url);
#else
  return ContentBrowserClient::GetWebUIHostnameForCodeCacheMetrics(webui_url);
#endif
}

void ChromeContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_primary_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  DCHECK(web_contents);
  if (!is_primary_main_frame_request) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    if (!callback.is_null())
      std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return;
  }

  // If the tab is being no-state prefetched, cancel the prefetcher and the
  // request.
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_SSL_ERROR);
    if (!callback.is_null()) {
      std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
    }
    return;
  }

  std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  return;
}

#if !BUILDFLAG(IS_ANDROID)
bool ChromeContentBrowserClient::ShouldDenyRequestOnCertificateError(
    const GURL main_page_url) {
  // Desktop Reader Mode pages should never load resources with certificate
  // errors. Desktop Reader Mode is more strict about security than Reader Mode
  // on Android: the desktop version has its own security indicator and
  // is not downgraded to a WARNING, whereas Android will show "Not secure"
  // in the omnibox (for low-end devices which show the omnibox on Reader Mode
  // pages).
  return main_page_url.SchemeIs(dom_distiller::kDomDistillerScheme);
}
#endif

namespace {

bool ShouldDisableForcedColorsForWebContent(content::WebContents* contents,
                                            bool in_forced_colors) {
  if (!contents || !in_forced_colors) {
    return false;
  }

  PrefService* prefs =
      Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs();
  CHECK(prefs);

  const base::Value::List& forced_colors_blocklist =
      prefs->GetList(prefs::kPageColorsBlockList);

  if (forced_colors_blocklist.empty()) {
    return false;
  }

  GURL url = contents->GetLastCommittedURL();

  // Forced Colors should be disabled for the current URL if it is in the block
  // list.
  for (auto const& value : forced_colors_blocklist) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());

    if (pattern == ContentSettingsPattern::Wildcard() || !pattern.IsValid()) {
      continue;
    }

    if (pattern.Matches(url)) {
      return true;
    }
  }

  return false;
}

bool UpdateForcedColorsForWebContent(WebPreferences* web_prefs,
                                     WebContents* web_contents,
                                     const ui::NativeTheme* native_theme) {
  auto old_in_forced_colors = web_prefs->in_forced_colors;
  auto old_forced_colors_disabled = web_prefs->is_forced_colors_disabled;
  bool in_forced_colors = native_theme->InForcedColorsMode();
  bool should_disable_forced_colors =
      ShouldDisableForcedColorsForWebContent(web_contents, in_forced_colors);

  web_prefs->in_forced_colors =
      in_forced_colors && !should_disable_forced_colors;
  web_prefs->is_forced_colors_disabled = should_disable_forced_colors;

  return old_in_forced_colors != web_prefs->in_forced_colors ||
         old_forced_colors_disabled != web_prefs->is_forced_colors_disabled;
}

#if !BUILDFLAG(IS_ANDROID)
blink::mojom::PreferredColorScheme ToBlinkPreferredColorScheme(
    ui::NativeTheme::PreferredColorScheme native_theme_scheme) {
  switch (native_theme_scheme) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      return blink::mojom::PreferredColorScheme::kDark;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      return blink::mojom::PreferredColorScheme::kLight;
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Returns true if preferred color scheme is modified based on at least one of
// the following -
// |url| - Last committed url.
// |web_contents| - For Android based on IsNightModeEnabled().
// |native_theme| - For other platforms based on native theme scheme.
bool UpdatePreferredColorScheme(WebPreferences* web_prefs,
                                const GURL& url,
                                WebContents* web_contents,
                                const ui::NativeTheme* native_theme) {
  auto old_preferred_color_scheme = web_prefs->preferred_color_scheme;

#if BUILDFLAG(IS_ANDROID)
  auto* delegate = TabAndroid::FromWebContents(web_contents)
                       ? static_cast<android::TabWebContentsDelegateAndroid*>(
                             web_contents->GetDelegate())
                       : nullptr;
  if (delegate) {
    web_prefs->preferred_color_scheme =
        delegate->IsNightModeEnabled()
            ? blink::mojom::PreferredColorScheme::kDark
            : blink::mojom::PreferredColorScheme::kLight;
    web_prefs->preferred_root_scrollbar_color_scheme =
        web_prefs->preferred_color_scheme;
  }
#else
  // Update based on native theme scheme.
  web_prefs->preferred_color_scheme =
      ToBlinkPreferredColorScheme(native_theme->GetPreferredColorScheme());

  bool using_different_colored_frame = false;
  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    if (ThemeService* theme_service =
            ThemeServiceFactory::GetForProfile(profile)) {
      using_different_colored_frame = !theme_service->UsingDefaultTheme() ||
                                      theme_service->GetUserColor().has_value();
    }
  }

  // Update based on the ColorProvider associated with `web_contents`. Depends
  // on the browser color mode settings and whether the user profile has set a
  // custom coloring for the browser ui.
  web_prefs->preferred_root_scrollbar_color_scheme =
      web_contents->GetColorMode() == ui::ColorProviderKey::ColorMode::kLight ||
              using_different_colored_frame
          ? blink::mojom::PreferredColorScheme::kLight
          : blink::mojom::PreferredColorScheme::kDark;
#endif  // BUILDFLAG(IS_ANDROID)

  // Reauth WebUI doesn't support dark mode yet because it shares the dialog
  // with GAIA web contents that is not correctly themed.
  const bool force_light =
      url.SchemeIs(content::kChromeUIScheme) &&
      url.host_piece() == chrome::kChromeUISigninReauthHost;

  if (force_light) {
    web_prefs->preferred_color_scheme =
        blink::mojom::PreferredColorScheme::kLight;
#if !BUILDFLAG(IS_ANDROID)
  } else if (content::HasWebUIScheme(url)) {
    // If color scheme is not forced, WebUI should track the color mode of the
    // ColorProvider associated with `web_contents`.
    web_prefs->preferred_color_scheme =
        web_contents->GetColorMode() == ui::ColorProviderKey::ColorMode::kLight
            ? blink::mojom::PreferredColorScheme::kLight
            : blink::mojom::PreferredColorScheme::kDark;
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  return old_preferred_color_scheme != web_prefs->preferred_color_scheme;
}

// Returns whether the user can be prompted to select a client certificate after
// no certificate got auto-selected.
bool CanPromptWithNonmatchingCertificates(const Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsSigninProfile(profile) ||
      ash::ProfileHelper::IsLockScreenProfile(profile) ||
      ash::ProfileHelper::IsLockScreenAppProfile(profile)) {
    // On non-regular profiles (e.g. sign-in profile or lock-screen profile),
    // never show certificate selection to the user. A client certificate is an
    // identifier that can be stable for a long time, so only the administrator
    // is allowed to decide which endpoints should see it.
    // This also returns false for the lock screen app profile which can
    // not use client certificates anyway - to be on the safe side in case
    // support for client certificates is added later.
    return false;
  }
#endif
  return true;
}

// Returns whether the user should be prompted to select a client certificate
// when multiple certificates got auto-selected.
bool ShouldPromptOnMultipleMatchingCertificates(const Profile* profile) {
  const PrefService* const prefs = profile->GetPrefs();
  DCHECK(prefs);
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kPromptOnMultipleMatchingCertificates);
  if (pref && pref->IsManaged() && pref->GetValue()->is_bool())
    return pref->GetValue()->GetBool();
  return false;
}

}  // namespace

base::OnceClosure ChromeContentBrowserClient::SelectClientCertificate(
    content::BrowserContext* browser_context,
    int process_id,
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      web_contents
          ? prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
                web_contents)
          : nullptr;
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(
        prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED);
    return base::OnceClosure();
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On the sign-in or lock screen profile, only allow client certs in the
  // context of the sign-in frame.
  // Note that this is explicitly not happening for the lock screen app profile
  // which does not support a gaia / SAML IdP sign-in frame.
  if (ash::ProfileHelper::IsSigninProfile(profile) ||
      ash::ProfileHelper::IsLockScreenProfile(profile)) {
    const char* profile_name = ash::ProfileHelper::IsSigninProfile(profile)
                                   ? "sign-in"
                                   : "lock screen";

    // TODO(b/290262513): See also comment below -- if the continuation should
    // be a cancelation, this check is unnecessary and we can just fall-through
    // without treating signin profiles differently for service workers.
    if (!web_contents) {
      LOG(WARNING) << "Client cert requested in " << profile_name
                   << " profile from service worker. This is not supported.";
      // Return without calling anything on `delegate`. This results in the
      // `delegate` being deleted, which implicitly calls to cancel the request.
      return base::OnceClosure();
    }

    content::StoragePartition* storage_partition =
        profile->GetStoragePartition(web_contents->GetSiteInstance());
    auto* signin_partition_manager =
        ash::login::SigninPartitionManager::Factory::GetForBrowserContext(
            profile);
    if (!signin_partition_manager->IsCurrentSigninStoragePartition(
            storage_partition)) {
      LOG(WARNING) << "Client cert requested in " << profile_name
                   << " profile in wrong context.";
      // Continue without client certificate. We do this to mimic the case of no
      // client certificate being present in the profile's certificate store.
      // TODO(b/290262513): Should this be a cancel? Selecting "no certificate"
      // is a sticky decision.
      delegate->ContinueWithCertificate(nullptr, nullptr);
      return base::OnceClosure();
    }
    VLOG(1) << "Client cert requested in " << profile_name << " profile.";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  GURL requesting_url =
      enterprise_util::GetRequestingUrl(cert_request_info->host_and_port);
  DCHECK(requesting_url.is_valid()) << "Invalid URL string: " << requesting_url;

  net::ClientCertIdentityList matching_certificates, nonmatching_certificates;
  enterprise_util::AutoSelectCertificates(
      profile, requesting_url, std::move(client_certs), &matching_certificates,
      &nonmatching_certificates);

  if (matching_certificates.size() == 1 ||
      (matching_certificates.size() > 1 &&
       !ShouldPromptOnMultipleMatchingCertificates(profile))) {
    // Always take the first certificate, even if multiple ones matched -
    // there's no other criteria available for tie-breaking, and user prompts
    // aren't enabled.
    std::unique_ptr<net::ClientCertIdentity> auto_selected_identity =
        std::move(matching_certificates[0]);
    // The callback will own |auto_selected_identity| and |delegate|, keeping
    // them alive until after ContinueWithCertificate is called.
    scoped_refptr<net::X509Certificate> cert =
        auto_selected_identity->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move(auto_selected_identity),
        base::BindOnce(
            &content::ClientCertificateDelegate::ContinueWithCertificate,
            std::move(delegate), std::move(cert)));
    return base::OnceClosure();
  }

  // At this point, we're going to either a) continue without a valid
  // certificate (if we're not allowed to prompt) or b) show the picker for the
  // user to select a valid cert. b) requires an associated WebContents; we
  // don't want to show a picker with no context. In the case of a), we don't
  // need a WebContents to display a picker. However, we don't always know
  // whether a) or b) will happen on all platforms. In particular, on Android,
  // the process to check for a cert will *also* show the picker. Thus, we
  // typically just early-out here unless we're ready to show a cert picker.
  if (!web_contents) {
    // There's one exception to the above. In the case of extensions, we allow
    // the request to continue without a certificate if there are no client
    // certs. This allows extension service workers to behave in the same way
    // as extension offscreen documents and legacy extension background pages.
    // Those cases would lead to the SSLClientCertificateSelector, which would
    // automatically continue if the associated certificate list was empty.
    // See https://crbug.com/333954429.
    // Note: the !IS_ANDROID here is currently moot, but is important in case
    // this ever changes. On Android, `matching_certificates` and
    // `nonmatching_certificates` are always empty at this stage, even when
    // there are matching certificates available in the OS, so this would
    // result in always proceeding with no certificate for any request from an
    // extension service worker. That decision would be remembered across the
    // entire profile, potentially locking the user out of the origin.
#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_ANDROID)
    if (matching_certificates.empty() && nonmatching_certificates.empty()) {
      extensions::ProcessMap* process_map =
          extensions::ProcessMap::Get(profile);
      if (process_map && process_map->Contains(process_id)) {
        delegate->ContinueWithCertificate(nullptr, nullptr);
        return base::OnceClosure();
      }
    }
#endif

    // Return without calling anything on `delegate`. This results in the
    // `delegate` being deleted, which implicitly calls to cancel the request.
    return base::OnceClosure();
  }

  if (matching_certificates.empty() &&
      !CanPromptWithNonmatchingCertificates(profile)) {
    LOG(WARNING) << "No client cert matched by policy and user selection is "
                    "not allowed.";
    // Continue without client certificate. We do this to mimic the case of no
    // client certificate being present in the profile's certificate store.
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  // Note: It can happen that both lists are empty, still the selector needs to
  // be shown - see the comment in SSLClientAuthHandler::DidGetClientCerts()
  // about platforms not having a client cert store.
  net::ClientCertIdentityList client_cert_choices =
      !matching_certificates.empty() ? std::move(matching_certificates)
                                     : std::move(nonmatching_certificates);

  return ShowSSLClientCertificateSelector(web_contents, cert_request_info,
                                          std::move(client_cert_choices),
                                          std::move(delegate));
}

content::MediaObserver* ChromeContentBrowserClient::GetMediaObserver() {
  return MediaCaptureDevicesDispatcher::GetInstance();
}

content::FeatureObserverClient*
ChromeContentBrowserClient::GetFeatureObserverClient() {
  return ChromeBrowserMainExtraPartsPerformanceManager::GetInstance()
      ->GetFeatureObserverClient();
}

bool ChromeContentBrowserClient::CanCreateWindow(
    RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(opener);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  *no_javascript_access = false;

  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (container_type == content::mojom::WindowContainerType::BACKGROUND) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    auto* process_map = extensions::ProcessMap::Get(profile);
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    if (!URLHasExtensionPermission(process_map, registry, opener_url,
                                   opener->GetProcess()->GetID(),
                                   APIPermissionID::kBackground)) {
      return false;
    }

    // Note: this use of GetExtensionOrAppByURL is safe but imperfect.  It may
    // return a recently installed Extension even if this CanCreateWindow call
    // was made by an old copy of the page in a normal web process.  That's ok,
    // because the permission check above would have caused an early return
    // already. We must use the full URL to find hosted apps, though, and not
    // just the origin.
    const Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(opener_url);
    if (extension && !extensions::BackgroundInfo::AllowJSAccess(extension))
      *no_javascript_access = true;
#endif

    return true;
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          opener->GetProcess()->GetID())) {
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  if (target_url.SchemeIs(extensions::kExtensionScheme)) {
    // Intentionally duplicating |registry| code from above because we want to
    // reduce calls to retrieve them as this function is a SYNC IPC handler.
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    const Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(target_url);
    if (extension && extension->is_platform_app()) {
      // window.open() may not be used to load v2 apps in a regular tab.
      return false;
    }
  }
#endif

  DCHECK(!prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
      web_contents));

  BlockedWindowParams blocked_params(
      target_url, source_origin, opener->GetSiteInstance(), referrer,
      frame_name, disposition, features, user_gesture, opener_suppressed);
  NavigateParams nav_params =
      blocked_params.CreateNavigateParams(opener->GetProcess(), web_contents);
  return !blocked_content::ConsiderForPopupBlocking(disposition) ||
         blocked_content::MaybeBlockPopup(
             web_contents, &opener_top_level_frame_url,
             (*g_popup_navigation_delegate_factory)(std::move(nav_params)),
             nullptr /*=open_url_params*/, blocked_params.features(),
             HostContentSettingsMapFactory::GetForProfile(profile)) != nullptr;
}

content::SpeechRecognitionManagerDelegate*
ChromeContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new speech::ChromeSpeechRecognitionManagerDelegate();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
content::TtsControllerDelegate*
ChromeContentBrowserClient::GetTtsControllerDelegate() {
  return TtsControllerDelegateImpl::GetInstance();
}
#endif

void ChromeContentBrowserClient::MaybeOverrideManifest(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::ManifestPtr& manifest) {
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (provider)
    provider->policy_manager().MaybeOverrideManifest(render_frame_host,
                                                     manifest);
#endif
}

content::TtsPlatform* ChromeContentBrowserClient::GetTtsPlatform() {
#if !BUILDFLAG(IS_ANDROID)
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      TtsExtensionEngine::GetInstance());
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return TtsPlatformImplChromeOs::GetInstance();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return TtsPlatformImplLacros::GetInstance();
#else
  return nullptr;
#endif
}

void ChromeContentBrowserClient::OverrideWebkitPrefs(
    WebContents* web_contents,
    WebPreferences* web_prefs) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

// Fill font preferences. These are not registered on Android
// - http://crbug.com/308033, http://crbug.com/696364.
#if !BUILDFLAG(IS_ANDROID)
  // Enabling the FontFamilyCache needs some KeyedService that might not be
  // available for some irregular profiles, like the System Profile.
  if (!AreKeyedServicesDisabledForProfileByDefault(profile)) {
    FontFamilyCache::FillFontFamilyMap(profile,
                                       prefs::kWebKitStandardFontFamilyMap,
                                       &web_prefs->standard_font_family_map);
    FontFamilyCache::FillFontFamilyMap(profile,
                                       prefs::kWebKitFixedFontFamilyMap,
                                       &web_prefs->fixed_font_family_map);
    FontFamilyCache::FillFontFamilyMap(profile,
                                       prefs::kWebKitSerifFontFamilyMap,
                                       &web_prefs->serif_font_family_map);
    FontFamilyCache::FillFontFamilyMap(profile,
                                       prefs::kWebKitSansSerifFontFamilyMap,
                                       &web_prefs->sans_serif_font_family_map);
    FontFamilyCache::FillFontFamilyMap(profile,
                                       prefs::kWebKitCursiveFontFamilyMap,
                                       &web_prefs->cursive_font_family_map);
    FontFamilyCache::FillFontFamilyMap(profile,
                                       prefs::kWebKitFantasyFontFamilyMap,
                                       &web_prefs->fantasy_font_family_map);
    FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitMathFontFamilyMap,
                                       &web_prefs->math_font_family_map);
  }

  web_prefs->default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs->default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs->minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs->minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);
#endif

  web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs->dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs->javascript_can_access_clipboard =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanAccessClipboard);
  web_prefs->tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled))
    web_prefs->javascript_enabled = false;

  if (!prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled))
    web_prefs->web_security_enabled = false;

  if (!prefs->GetBoolean(prefs::kWebKitPluginsEnabled))
    web_prefs->plugins_enabled = false;
  web_prefs->loads_images_automatically =
      prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);

  if (prefs->GetBoolean(prefs::kDisable3DAPIs)) {
    web_prefs->webgl1_enabled = false;
    web_prefs->webgl2_enabled = false;
  }

  web_prefs->allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);
#if BUILDFLAG(IS_ANDROID)
  web_prefs->font_scale_factor = static_cast<float>(
      prefs->GetDouble(browser_ui::prefs::kWebKitFontScaleFactor));
  web_prefs->text_size_contrast_factor =
      prefs->GetInteger(prefs::kAccessibilityTextSizeContrastFactor);
  web_prefs->force_enable_zoom =
      prefs->GetBoolean(browser_ui::prefs::kWebKitForceEnableZoom);
  web_prefs->font_weight_adjustment =
      prefs->GetInteger(prefs::kAccessibilityFontWeightAdjustment);
#endif
  web_prefs->force_dark_mode_enabled =
      prefs->GetBoolean(prefs::kWebKitForceDarkModeEnabled);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_prefs->always_show_focus =
      prefs->GetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled);
#else
  web_prefs->always_show_focus =
      prefs->GetBoolean(prefs::kAccessibilityFocusHighlightEnabled);
#endif

#if BUILDFLAG(IS_ANDROID)
  web_prefs->password_echo_enabled =
      prefs->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
#else
  web_prefs->password_echo_enabled = false;
#endif

  web_prefs->text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);
  web_prefs->hyperlink_auditing_enabled =
      prefs->GetBoolean(prefs::kEnableHyperlinkAuditing);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::string image_animation_policy =
      prefs->GetString(prefs::kAnimationPolicy);
  if (image_animation_policy == kAnimationPolicyOnce) {
    web_prefs->animation_policy =
        blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyAnimateOnce;
  } else if (image_animation_policy == kAnimationPolicyNone) {
    web_prefs->animation_policy =
        blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyNoAnimation;
  } else {
    web_prefs->animation_policy =
        blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyAllowed;
  }
#endif

  // Make sure we will set the default_encoding with canonical encoding name.
  web_prefs->default_encoding =
      base::GetCanonicalEncodingNameByAliasName(web_prefs->default_encoding);
  if (web_prefs->default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);
  }
  DCHECK(!web_prefs->default_encoding.empty());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePotentiallyAnnoyingSecurityFeatures)) {
    web_prefs->disable_reading_from_canvas = true;
    web_prefs->strict_mixed_content_checking = true;
    web_prefs->strict_powerful_feature_restrictions = true;
  }

  // See crbug.com/1238157: the Native Client flag (chrome://flags/#enable-nacl)
  // can be manually re-enabled. In that case, we also need to return the full
  // plugins list, for compat.
  web_prefs->allow_non_empty_navigator_plugins |=
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNaCl);

  web_prefs->data_saver_enabled = IsDataSaverEnabled(profile);

  if (web_contents) {
#if BUILDFLAG(IS_ANDROID)
    auto* delegate = TabAndroid::FromWebContents(web_contents)
                         ? static_cast<android::TabWebContentsDelegateAndroid*>(
                               web_contents->GetDelegate())
                         : nullptr;
    if (delegate) {
      web_prefs->embedded_media_experience_enabled =
          delegate->ShouldEnableEmbeddedMediaExperience();

      web_prefs->picture_in_picture_enabled =
          delegate->IsPictureInPictureEnabled();

      web_prefs->force_dark_mode_enabled =
          delegate->IsForceDarkWebContentEnabled();

      web_prefs->modal_context_menu = delegate->IsModalContextMenu();
    }
#endif  // BUILDFLAG(IS_ANDROID)

    // web_app_scope value is platform specific.
#if BUILDFLAG(IS_ANDROID)
    if (delegate)
      web_prefs->web_app_scope = delegate->GetManifestScope();
#elif BUILDFLAG(ENABLE_EXTENSIONS)
    {
      web_prefs->web_app_scope = GURL();
      // Set |web_app_scope| based on the app associated with the app window if
      // any. Note that the app associated with the window never changes, even
      // if the app navigates off scope. This is not a problem because we still
      // want to use the scope of the app associated with the window, not the
      // WebContents.
      Browser* browser = chrome::FindBrowserWithTab(web_contents);
      if (browser && browser->app_controller()) {
        web_app::WebAppProvider* const web_app_provider =
            web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
        const webapps::AppId& app_id = browser->app_controller()->app_id();
        const web_app::WebAppRegistrar& registrar =
            web_app_provider->registrar_unsafe();
        if (registrar.IsInstallState(
                app_id, {web_app::proto::INSTALLED_WITH_OS_INTEGRATION,
                         web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION})) {
          web_prefs->web_app_scope = registrar.GetAppScope(app_id);
        }

#if BUILDFLAG(IS_CHROMEOS_ASH)
        auto* system_app = browser->app_controller()->system_app();
        if (system_app) {
          web_prefs->allow_scripts_to_close_windows =
              system_app->ShouldAllowScriptsToCloseWindows();
        }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      }
    }
#endif

    web_prefs->immersive_mode_enabled = vr::VrTabHelper::IsInVr(web_contents);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLazyLoading)) {
    web_prefs->lazy_load_enabled = false;
  } else {
    web_prefs->lazy_load_enabled =
        !web_contents || !web_contents->GetDelegate() ||
        web_contents->GetDelegate()->ShouldAllowLazyLoad();
  }

  if (base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    std::string effective_connection_type_param =
        base::GetFieldTrialParamValueByFeature(
            features::kNetworkQualityEstimatorWebHoldback,
            "web_effective_connection_type_override");

    std::optional<net::EffectiveConnectionType> effective_connection_type =
        net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
    DCHECK(effective_connection_type_param.empty() ||
           effective_connection_type);
    if (effective_connection_type) {
      DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                effective_connection_type.value());
      web_prefs->network_quality_estimator_web_holdback =
          static_cast<EffectiveConnectionType>(
              effective_connection_type.value());
    }
  }

  web_prefs->autoplay_policy = GetAutoplayPolicyForWebContents(web_contents);
#if !BUILDFLAG(IS_ANDROID)
  web_prefs->require_transient_activation_for_get_display_media =
      capture_policy::IsTransientActivationRequiredForGetDisplayMedia(
          web_contents);
  web_prefs->require_transient_activation_for_show_file_or_directory_picker =
      IsFileOrDirectoryPickerWithoutGestureAllowed(web_contents);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  web_prefs->subapps_apis_require_user_gesture_and_authorization =
      SubAppsAPIsRequireUserGestureAndAuthorization(web_contents);
#endif  // BUILDFLAG(IS_CHROMEOS)

  switch (GetWebTheme()->GetPreferredContrast()) {
    case ui::NativeTheme::PreferredContrast::kNoPreference:
      web_prefs->preferred_contrast =
          blink::mojom::PreferredContrast::kNoPreference;
      break;
    case ui::NativeTheme::PreferredContrast::kMore:
      web_prefs->preferred_contrast = blink::mojom::PreferredContrast::kMore;
      break;
    case ui::NativeTheme::PreferredContrast::kLess:
      web_prefs->preferred_contrast = blink::mojom::PreferredContrast::kLess;
      break;
    case ui::NativeTheme::PreferredContrast::kCustom:
      web_prefs->preferred_contrast = blink::mojom::PreferredContrast::kCustom;
      break;
  }

  UpdateForcedColorsForWebContent(web_prefs, web_contents, GetWebTheme());

  UpdatePreferredColorScheme(
      web_prefs,
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
      web_contents, GetWebTheme());

  web_prefs->translate_service_available = TranslateService::IsAvailable(prefs);

  std::optional<ui::CaptionStyle> style =
      captions::GetCaptionStyleFromUserSettings(prefs,
                                                true /* record_metrics */);
  if (style) {
    web_prefs->text_track_background_color = style->background_color;
    web_prefs->text_track_text_color = style->text_color;
    web_prefs->text_track_text_size = style->text_size;
    web_prefs->text_track_text_shadow = style->text_shadow;
    web_prefs->text_track_font_family = style->font_family;
    web_prefs->text_track_font_variant = style->font_variant;
    web_prefs->text_track_window_color = style->window_color;
    web_prefs->text_track_window_radius = style->window_radius;
  }

#if BUILDFLAG(IS_ANDROID)
  // If the pref is not set, the default value (true) will be used:
  web_prefs->webxr_immersive_ar_allowed =
      prefs->GetBoolean(prefs::kWebXRImmersiveArEnabled);

  // Only set `databases_enabled` if disabled, otherwise check blink::feature
  // settings.
  web_prefs->databases_enabled =
      !web_prefs->databases_enabled
          ? false
          : base::FeatureList::IsEnabled(blink::features::kWebSQLAccess);
#else
  // TODO(crbug.com/333756088): WebSQL is disabled everywhere except Android
  // WebView.
  web_prefs->databases_enabled = false;
#endif

  for (auto& parts : extra_parts_) {
    parts->OverrideWebkitPrefs(web_contents, web_prefs);
  }

  web_prefs->prefers_default_scrollbar_styles =
      prefs->GetBoolean(prefs::kPrefersDefaultScrollbarStyles);
}

bool ChromeContentBrowserClientParts::OverrideWebPreferencesAfterNavigation(
    WebContents* web_contents,
    WebPreferences* web_prefs) {
  return false;
}

bool ChromeContentBrowserClient::OverrideWebPreferencesAfterNavigation(
    WebContents* web_contents,
    WebPreferences* web_prefs) {
  bool prefs_changed = false;

  const auto autoplay_policy = GetAutoplayPolicyForWebContents(web_contents);
  prefs_changed |= (web_prefs->autoplay_policy != autoplay_policy);
  web_prefs->autoplay_policy = autoplay_policy;

#if !BUILDFLAG(IS_ANDROID)
  const bool require_transient_activation_for_get_display_media =
      capture_policy::IsTransientActivationRequiredForGetDisplayMedia(
          web_contents);
  prefs_changed |=
      (web_prefs->require_transient_activation_for_get_display_media !=
       require_transient_activation_for_get_display_media);
  web_prefs->require_transient_activation_for_get_display_media =
      require_transient_activation_for_get_display_media;

  const bool require_transient_activation_for_show_file_or_directory_picker =
      IsFileOrDirectoryPickerWithoutGestureAllowed(web_contents);
  prefs_changed |=
      (web_prefs
           ->require_transient_activation_for_show_file_or_directory_picker !=
       require_transient_activation_for_show_file_or_directory_picker);
  web_prefs->require_transient_activation_for_show_file_or_directory_picker =
      require_transient_activation_for_show_file_or_directory_picker;
#endif  // !BUILDFLAG(IS_ANDROID)

  for (auto& parts : extra_parts_) {
    prefs_changed |=
        parts->OverrideWebPreferencesAfterNavigation(web_contents, web_prefs);
  }

  prefs_changed |=
      UpdateForcedColorsForWebContent(web_prefs, web_contents, GetWebTheme());

  prefs_changed |=
      UpdatePreferredColorScheme(web_prefs, web_contents->GetLastCommittedURL(),
                                 web_contents, GetWebTheme());

#if BUILDFLAG(IS_ANDROID)
  auto* delegate = TabAndroid::FromWebContents(web_contents)
                       ? static_cast<android::TabWebContentsDelegateAndroid*>(
                             web_contents->GetDelegate())
                       : nullptr;
  if (delegate) {
    bool force_dark_mode_new_state = delegate->IsForceDarkWebContentEnabled();
    prefs_changed |=
        (web_prefs->force_dark_mode_enabled != force_dark_mode_new_state);
    web_prefs->force_dark_mode_enabled = force_dark_mode_new_state;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  const bool subapps_apis_require_user_gesture_and_authorization =
      SubAppsAPIsRequireUserGestureAndAuthorization(web_contents);
  prefs_changed |=
      (web_prefs->subapps_apis_require_user_gesture_and_authorization !=
       subapps_apis_require_user_gesture_and_authorization);
#endif  // BUILDFLAG(IS_CHROMEOS)

  return prefs_changed;
}

void ChromeContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  // The group policy NTP URL handler must be registered before the other NTP
  // URL handlers below. Also register it before the "parts" handlers, so the
  // NTP policy takes precedence over extensions that override the NTP.
  handler->AddHandlerPair(&HandleNewTabPageLocationOverride,
                          BrowserURLHandler::null_handler());

  for (auto& part : extra_parts_) {
    part->BrowserURLHandlerCreated(handler);
  }

  // Handler to rewrite chrome://about and chrome://sync URLs.
  handler->AddHandlerPair(&HandleChromeAboutAndChromeSyncRewrite,
                          BrowserURLHandler::null_handler());

#if BUILDFLAG(IS_ANDROID)
  // Handler to rewrite chrome://newtab on Android.
  handler->AddHandlerPair(&chrome::android::HandleAndroidNativePageURL,
                          BrowserURLHandler::null_handler());
#else   // BUILDFLAG(IS_ANDROID)
  // Handler to rewrite chrome://newtab for InstantExtended.
  handler->AddHandlerPair(&search::HandleNewTabURLRewrite,
                          &search::HandleNewTabURLReverseRewrite);
#endif  // BUILDFLAG(IS_ANDROID)

  // chrome: & friends.
  handler->AddHandlerPair(&ChromeContentBrowserClient::HandleWebUI,
                          &ChromeContentBrowserClient::HandleWebUIReverse);
}

base::FilePath ChromeContentBrowserClient::GetDefaultDownloadDirectory() {
  return DownloadPrefs::GetDefaultDownloadDirectory();
}

std::string ChromeContentBrowserClient::GetDefaultDownloadName() {
  return l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME);
}

base::FilePath ChromeContentBrowserClient::GetShaderDiskCacheDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("ShaderCache"));
}

base::FilePath ChromeContentBrowserClient::GetGrShaderDiskCacheDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("GrShaderCache"));
}

base::FilePath ChromeContentBrowserClient::GetGraphiteDawnDiskCacheDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.Append(FILE_PATH_LITERAL("GraphiteDawnCache"));
}

base::FilePath ChromeContentBrowserClient::GetNetLogDefaultDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir;
}

base::FilePath ChromeContentBrowserClient::GetFirstPartySetsDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir;
}

std::optional<base::FilePath>
ChromeContentBrowserClient::GetLocalTracesDirectory() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_LOCAL_TRACES, &user_data_dir)) {
    return std::nullopt;
  }
  DCHECK(!user_data_dir.empty());
  return user_data_dir;
}

void ChromeContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
#if BUILDFLAG(ENABLE_PLUGINS)
  ChromeContentBrowserClientPluginsPart::DidCreatePpapiPlugin(browser_host);
#endif
}

content::BrowserPpapiHost*
ChromeContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
#if BUILDFLAG(ENABLE_NACL)
  content::BrowserChildProcessHostIterator iter(PROCESS_TYPE_NACL_LOADER);
  while (!iter.Done()) {
    nacl::NaClProcessHost* host =
        static_cast<nacl::NaClProcessHost*>(iter.GetDelegate());
    if (host->process() && host->process()->GetData().id == plugin_process_id) {
      // Found the plugin.
      return host->browser_ppapi_host();
    }
    ++iter;
  }
#endif
  return nullptr;
}

bool ChromeContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::AllowPepperSocketAPI(
      browser_context, url, private_api, params);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::IsPepperVpnProviderAPIAllowed(
      browser_context, url);
#else
  return false;
#endif
}

std::unique_ptr<content::VpnServiceProxy>
ChromeContentBrowserClient::GetVpnServiceProxy(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::GetVpnServiceProxy(
      browser_context);
#else
  return nullptr;
#endif
}

std::unique_ptr<ui::SelectFilePolicy>
ChromeContentBrowserClient::CreateSelectFilePolicy(WebContents* web_contents) {
  return std::make_unique<ChromeSelectFilePolicy>(web_contents);
}

void ChromeContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(content::kChromeDevToolsScheme);
  additional_allowed_schemes->push_back(content::kChromeUIScheme);
  additional_allowed_schemes->push_back(content::kChromeUIUntrustedScheme);
  additional_allowed_schemes->push_back(chrome::kIsolatedAppScheme);
  for (auto& extra_part : extra_parts_) {
    extra_part->GetAdditionalAllowedSchemesForFileSystem(
        additional_allowed_schemes);
  }
}

void ChromeContentBrowserClient::GetSchemesBypassingSecureContextCheckAllowlist(
    std::set<std::string>* schemes) {
  *schemes = secure_origin_allowlist::GetSchemesBypassingSecureContextCheck();
}

void ChromeContentBrowserClient::GetURLRequestAutoMountHandlers(
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
  for (auto& part : extra_parts_) {
    part->GetURLRequestAutoMountHandlers(handlers);
  }
}

void ChromeContentBrowserClient::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>*
        additional_backends) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  storage::ExternalMountPoints* external_mount_points =
      browser_context->GetMountPoints();
  DCHECK(external_mount_points);
  auto backend = std::make_unique<ash::FileSystemBackend>(
      Profile::FromBrowserContext(browser_context),
      ash::file_system_provider::BackendDelegate::MakeUnique(),
      std::make_unique<ash::MTPFileSystemBackendDelegate>(
          storage_partition_path),
      std::make_unique<arc::ArcContentFileSystemBackendDelegate>(),
      std::make_unique<arc::ArcDocumentsProviderBackendDelegate>(),
      std::make_unique<drive::DriveFsFileSystemBackendDelegate>(
          Profile::FromBrowserContext(browser_context)),
      std::make_unique<ash::smb_client::SmbFsFileSystemBackendDelegate>(
          Profile::FromBrowserContext(browser_context)),
      external_mount_points, storage::ExternalMountPoints::GetSystemInstance());
  backend->AddSystemMountPoints();
  DCHECK(backend->CanHandleType(storage::kFileSystemTypeExternal));
  additional_backends->push_back(std::move(backend));
#endif

  for (auto& part : extra_parts_) {
    part->GetAdditionalFileSystemBackends(
        browser_context, storage_partition_path,
        GetQuarantineConnectionCallback(), additional_backends);
  }
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    PosixFileDescriptorInfo* mappings) {
#if BUILDFLAG(IS_ANDROID)
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  mappings->ShareWithRegion(kAndroidUIResourcesPakDescriptor, fd, region);

  // For Android: Native resources for DFMs should only be used by the browser
  // process. Their file descriptors and memory mapped file regions are not
  // passed to child processes.

  fd = ui::GetCommonResourcesPackFd(&region);
  mappings->ShareWithRegion(kAndroidChrome100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  mappings->ShareWithRegion(kAndroidLocalePakDescriptor, fd, region);

  // Optional secondary locale .pak file.
  fd = ui::GetSecondaryLocalePackFd(&region);
  if (fd != -1) {
    mappings->ShareWithRegion(kAndroidSecondaryLocalePakDescriptor, fd, region);
  }

  base::FilePath app_data_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_path);
  DCHECK(!app_data_path.empty());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Map startup parameter files to child processes in Lacros.
  // The FD numbers are passed via command line switches in
  // |AppendExtraCommandLineSwitches|.
  //
  // NOTE: the Zygote process requires special handling.
  // Serializing startup data early in the initialization
  // process requires temporarily initializing Mojo. That's handled in the
  // |LaunchZygoteHelper| function in |content_main_runner_impl.cc|. Here, we
  // deal with all other type of processes.
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type != switches::kZygoteProcess) {
    base::ScopedFD cros_startup_fd =
        chromeos::BrowserInitParams::CreateStartupData();
    if (cros_startup_fd.is_valid()) {
      constexpr int kStartupDataFD =
          kCrosStartupDataDescriptor + base::GlobalDescriptors::kBaseDescriptor;
      mappings->Transfer(kStartupDataFD, std::move(cros_startup_fd));
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForZygote(
    base::CommandLine* command_line,
    PosixFileDescriptorInfo* mappings) {
  // Create the file descriptor for Cros startup data and pass it.
  // This FD will be used to obtain BrowserInitParams in Zygote process.
  // Note that this requires Mojo, but Mojo cannot be fully initialized this
  // due to dependencies on base::FeatureList. So we also temporarily initialize
  // Mojo and then shut it down immediately after preparing the FD. This is
  // inexpensive, an the features which control Mojo behavior aren't relevant
  // for this operation.
  //
  // TODO(crbug.com/40058840): This will need to be changed before
  // MojoIpcz experimentation can happen on Lacros, as it results in
  // inconsistent MojoIpcz feature status across Mojo initializations.
  mojo::core::Init();
  base::ScopedFD cros_startup_fd =
      chromeos::BrowserInitParams::CreateStartupData();
  mojo::core::ShutDown();

  if (cros_startup_fd.is_valid()) {
    constexpr int kStartupDataFD =
        kCrosStartupDataDescriptor + base::GlobalDescriptors::kBaseDescriptor;
    command_line->AppendSwitchASCII(chromeos::switches::kCrosStartupDataFD,
                                    base::NumberToString(kStartupDataFD));
    mappings->Transfer(kStartupDataFD, std::move(cros_startup_fd));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
std::wstring ChromeContentBrowserClient::GetAppContainerSidForSandboxType(
    sandbox::mojom::Sandbox sandbox_type,
    AppContainerFlags flags) {
  // TODO(wfh): Add support for more process types here. crbug.com/499523
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kRenderer:
      if (flags & AppContainerFlags::kAppContainerFlagDisableAppContainer)
        return std::wstring();
      return std::wstring(install_static::GetSandboxSidPrefix()) + L"129201922";
    case sandbox::mojom::Sandbox::kUtility:
      return std::wstring();
    case sandbox::mojom::Sandbox::kGpu:
      return std::wstring();
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
      return std::wstring();
    case sandbox::mojom::Sandbox::kNoSandbox:
    case sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges:
    case sandbox::mojom::Sandbox::kXrCompositing:
    case sandbox::mojom::Sandbox::kNetwork:
    case sandbox::mojom::Sandbox::kCdm:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
    case sandbox::mojom::Sandbox::kAudio:
    case sandbox::mojom::Sandbox::kScreenAI:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    case sandbox::mojom::Sandbox::kVideoEffects:
#endif
    case sandbox::mojom::Sandbox::kSpeechRecognition:
    case sandbox::mojom::Sandbox::kPdfConversion:
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kServiceWithJit:
    case sandbox::mojom::Sandbox::kIconReader:
    case sandbox::mojom::Sandbox::kMediaFoundationCdm:
    case sandbox::mojom::Sandbox::kWindowsSystemProxyResolver:
      // Should never reach here.
      CHECK(0);
      return std::wstring();
  }
}

bool ChromeContentBrowserClient::IsAppContainerDisabled(
    sandbox::mojom::Sandbox sandbox_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  constexpr auto kSandboxPolicyPrefMapping =
      base::MakeFixedFlatMap<sandbox::mojom::Sandbox, std::string_view>({
          {sandbox::mojom::Sandbox::kRenderer,
           prefs::kRendererAppContainerEnabled},
          {sandbox::mojom::Sandbox::kPrintCompositor,
           prefs::kPrintingLPACSandboxEnabled},
      });
  auto iter = kSandboxPolicyPrefMapping.find(sandbox_type);

  if (iter == kSandboxPolicyPrefMapping.end()) {
    return false;
  }

  PrefService* local_state = g_browser_process->local_state();
  const PrefService::Preference* pref =
      local_state->FindPreference(iter->second);
  // App Container is disabled if managed pref is set to false.
  if (pref && pref->IsManaged() && !pref->GetValue()->GetBool())
    return true;

  return false;
}

std::wstring
ChromeContentBrowserClient::GetLPACCapabilityNameForNetworkService() {
  // Use a different LPAC capability name for each Chrome channel so network
  // service data between hannels is isolated.
  version_info::Channel channel = chrome::GetChannel();
  switch (channel) {
    case version_info::Channel::CANARY:
      return std::wstring(L"lpacChromeCanaryNetworkSandbox");
    case version_info::Channel::BETA:
      return std::wstring(L"lpacChromeBetaNetworkSandbox");
    case version_info::Channel::DEV:
      return std::wstring(L"lpacChromeDevNetworkSandbox");
    case version_info::Channel::STABLE:
      return std::wstring(L"lpacChromeStableNetworkSandbox");
    case version_info::Channel::UNKNOWN:
      return std::wstring(L"lpacChromeNetworkSandbox");
  }
}

// Note: Only use sparingly to add Chrome specific sandbox functionality here.
// Other code should reside in the content layer. Changes to this function
// should be reviewed by the security team.
bool ChromeContentBrowserClient::PreSpawnChild(
    sandbox::TargetConfig* config,
    sandbox::mojom::Sandbox sandbox_type,
    ChildSpawnFlags flags) {
  DCHECK(!config->IsConfigured());
// Does not work under component build because all the component DLLs would need
// to be manually added and maintained. Does not work under ASAN build because
// ASAN has not yet fully initialized its instrumentation by the time the CIG
// intercepts run.
#if !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
  bool enforce_code_integrity = false;

  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kRenderer:
      enforce_code_integrity =
          (flags & ChildSpawnFlags::kChildSpawnFlagRendererCodeIntegrity);
      break;
    case sandbox::mojom::Sandbox::kNetwork:
      enforce_code_integrity = base::FeatureList::IsEnabled(
          sandbox::policy::features::kNetworkServiceCodeIntegrity);
      break;
    case sandbox::mojom::Sandbox::kServiceWithJit:
      enforce_code_integrity = true;
      break;
    case sandbox::mojom::Sandbox::kUtility:
    case sandbox::mojom::Sandbox::kGpu:
#if BUILDFLAG(ENABLE_PPAPI)
    case sandbox::mojom::Sandbox::kPpapi:
#endif
    case sandbox::mojom::Sandbox::kNoSandbox:
    case sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges:
    case sandbox::mojom::Sandbox::kXrCompositing:
    case sandbox::mojom::Sandbox::kCdm:
#if BUILDFLAG(ENABLE_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
#endif
    case sandbox::mojom::Sandbox::kPrintCompositor:
#if !BUILDFLAG(IS_ANDROID)
    case sandbox::mojom::Sandbox::kScreenAI:
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    case sandbox::mojom::Sandbox::kVideoEffects:
#endif
    case sandbox::mojom::Sandbox::kAudio:
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
    case sandbox::mojom::Sandbox::kSpeechRecognition:
    case sandbox::mojom::Sandbox::kPdfConversion:
    case sandbox::mojom::Sandbox::kService:
    case sandbox::mojom::Sandbox::kIconReader:
    case sandbox::mojom::Sandbox::kMediaFoundationCdm:
    case sandbox::mojom::Sandbox::kWindowsSystemProxyResolver:
      break;
  }

  if (!enforce_code_integrity)
    return true;

  // Only enable signing mitigation if launching from chrome.exe.
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path))
    return true;
  if (chrome::kBrowserProcessExecutableName != exe_path.BaseName().value())
    return true;

  sandbox::MitigationFlags mitigations = config->GetProcessMitigations();
  mitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
  sandbox::ResultCode result = config->SetProcessMitigations(mitigations);
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Allow loading Chrome's DLLs.
  for (const auto* dll : {chrome::kBrowserResourcesDll, chrome::kElfDll}) {
    result = config->AllowExtraDlls(GetModulePath(dll).value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return false;
  }
#endif  // !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
  return true;
}

bool ChromeContentBrowserClient::IsRendererCodeIntegrityEnabled() {
  // Emergency 'on switch' to re-enable the policy if force-disabling it causes
  // issues.
  if (base::FeatureList::IsEnabled(
          sandbox::policy::features::kWinSboxForceRendererCodeIntegrity)) {
    return true;
  }

  PrefService* local_state = g_browser_process->local_state();

  // If kWinSboxForceRendererCodeIntegrity is set to disabled, then code
  // integrity defaults to enabled, unless specifically overridden by a policy
  // controlled pref being set to false.
  return !local_state->HasPrefPath(prefs::kRendererCodeIntegrityEnabled) ||
         local_state->GetBoolean(prefs::kRendererCodeIntegrityEnabled);
}

// Note: Only use sparingly to add Chrome specific sandbox functionality here.
// Other code should reside in the content layer. Changes to this function
// should be reviewed by the security team.
bool ChromeContentBrowserClient::IsUtilityCetCompatible(
    const std::string& utility_sub_type) {
  if (utility_sub_type == chrome::mojom::UtilWin::Name_)
    return false;
  return true;
}

void ChromeContentBrowserClient::SessionEnding(
    std::optional<DWORD> control_type) {
  chrome::SessionEnding();
}

bool ChromeContentBrowserClient::ShouldEnableAudioProcessHighPriority() {
  return IsAudioProcessHighPriorityEnabled();
}

bool ChromeContentBrowserClient::ShouldUseSkiaFontManager(
    const GURL& site_url) {
  return IsTopChromeWebUIURL(site_url) &&
         base::FeatureList::IsEnabled(features::kSkiaFontService);
}

#endif  // BUILDFLAG(IS_WIN)

void ChromeContentBrowserClient::
    RegisterMojoBinderPoliciesForSameOriginPrerendering(
        content::MojoBinderPolicyMap& policy_map) {
  // Changes to `policy_map` should be made in
  // RegisterChromeMojoBinderPoliciesForSameOriginPrerendering() which requires
  // security review.
  RegisterChromeMojoBinderPoliciesForSameOriginPrerendering(policy_map);
}

void ChromeContentBrowserClient::RegisterMojoBinderPoliciesForPreview(
    content::MojoBinderPolicyMap& policy_map) {
  // Changes to `policy_map` should be made in
  // RegisterChromeMojoBinderPoliciesForPreview() which requires security
  // review.
  RegisterChromeMojoBinderPoliciesForPreview(policy_map);
}

void ChromeContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context = site_instance->GetBrowserContext();

#if BUILDFLAG(IS_ANDROID)
  ServiceTabLauncher::GetInstance()->LaunchTab(browser_context, params,
                                               std::move(callback));
#else
  NavigateParams nav_params(Profile::FromBrowserContext(browser_context),
                            params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);

  Navigate(&nav_params);
  std::move(callback).Run(nav_params.navigated_or_inserted_contents);
#endif
}

content::ControllerPresentationServiceDelegate*
ChromeContentBrowserClient::GetControllerPresentationServiceDelegate(
    content::WebContents* web_contents) {
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    return media_router::PresentationServiceDelegateImpl::
        GetOrCreateForWebContents(web_contents);
  }
  return nullptr;
}

content::ReceiverPresentationServiceDelegate*
ChromeContentBrowserClient::GetReceiverPresentationServiceDelegate(
    content::WebContents* web_contents) {
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    // ReceiverPresentationServiceDelegateImpl exists only for WebContents
    // created for offscreen presentations. The WebContents must belong to
    // an incognito profile.
    if (auto* impl = media_router::ReceiverPresentationServiceDelegateImpl::
            FromWebContents(web_contents)) {
      DCHECK(web_contents->GetBrowserContext()->IsOffTheRecord());
      return impl;
    }
  }
  return nullptr;
}

void ChromeContentBrowserClient::AddPresentationObserver(
    content::PresentationObserver* observer,
    content::WebContents* web_contents) {
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    media_router::WebContentsPresentationManager::Get(web_contents)
        ->AddObserver(observer);
  }
}

void ChromeContentBrowserClient::RemovePresentationObserver(
    content::PresentationObserver* observer,
    content::WebContents* web_contents) {
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    media_router::WebContentsPresentationManager::Get(web_contents)
        ->RemoveObserver(observer);
  }
}

bool ChromeContentBrowserClient::AddPrivacySandboxAttestationsObserver(
    content::PrivacySandboxAttestationsObserver* observer) {
  return privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->AddObserver(observer);
}

void ChromeContentBrowserClient::RemovePrivacySandboxAttestationsObserver(
    content::PrivacySandboxAttestationsObserver* observer) {
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()->RemoveObserver(
      observer);
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ChromeContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

  // MetricsNavigationThrottle requires that it runs before NavigationThrottles
  // that may delay or cancel navigations, so only NavigationThrottles that
  // don't delay or cancel navigations (e.g. throttles that are only observing
  // callbacks without affecting navigation behavior) should be added before
  // MetricsNavigationThrottle.
  if (handle->IsInMainFrame()) {
    throttles.push_back(
        page_load_metrics::MetricsNavigationThrottle::Create(handle));
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(davidben): This is insufficient to integrate with prerender properly.
  // https://crbug.com/370595
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          handle->GetWebContents());
  if (!no_state_prefetch_contents) {
    MaybeAddThrottle(
        navigation_interception::InterceptNavigationDelegate::
            MaybeCreateThrottleFor(
                handle, navigation_interception::SynchronyMode::kAsync),
        &throttles);
  }
  throttles.push_back(InterceptOMADownloadNavigationThrottle::Create(handle));

#if BUILDFLAG(DFMIFY_DEV_UI)
  // If the DevUI DFM is already installed, then this is a no-op, except for the
  // side effect of ensuring that the DevUI DFM is loaded.
  MaybeAddThrottle(dev_ui::DevUiLoaderThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

#elif BUILDFLAG(ENABLE_PLATFORM_APPS)
  // Redirect some navigations to apps that have registered matching URL
  // handlers ('url_handlers' in the manifest).
  MaybeAddThrottle(
      PlatformAppNavigationRedirector::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of main frames.
  if (handle->IsInMainFrame()) {
    // Add interstitial page while merge session process (cookie reconstruction
    // from OAuth2 refresh token in ChromeOS login) is still in progress while
    // we are attempting to load a google property.
    if (ash::merge_session_throttling_utils::ShouldAttachNavigationThrottle() &&
        !ash::merge_session_throttling_utils::AreAllSessionMergedAlready() &&
        handle->GetURL().SchemeIsHTTPOrHTTPS()) {
      throttles.push_back(ash::MergeSessionNavigationThrottle::Create(handle));
    }
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  auto disabled_app_throttle =
      apps::ChromeOsDisabledAppsThrottle::MaybeCreate(handle);
  if (disabled_app_throttle) {
    throttles.push_back(std::move(disabled_app_throttle));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<apps::LinkCapturingNavigationThrottle::Delegate>
      link_capturing_delegate;

#if BUILDFLAG(IS_CHROMEOS)
  link_capturing_delegate =
      std::make_unique<apps::ChromeOsLinkCapturingDelegate>();
#else
  link_capturing_delegate =
      std::make_unique<web_app::WebAppLinkCapturingDelegate>();
#endif
  std::unique_ptr<content::NavigationThrottle> url_to_apps_throttle =
      apps::LinkCapturingNavigationThrottle::MaybeCreate(
          handle, std::move(link_capturing_delegate));
  if (url_to_apps_throttle) {
    throttles.push_back(std::move(url_to_apps_throttle));
  }

  std::unique_ptr<content::NavigationThrottle>
      navigation_capturing_redirection_throttle =
          web_app::NavigationCapturingRedirectionThrottle::MaybeCreate(handle);
  if (navigation_capturing_redirection_throttle) {
    throttles.push_back(std::move(navigation_capturing_redirection_throttle));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    throttles.push_back(
        std::make_unique<extensions::ExtensionNavigationThrottle>(handle));

    MaybeAddThrottle(extensions::ExtensionsBrowserClient::Get()
                         ->GetUserScriptListener()
                         ->CreateNavigationThrottle(handle),
                     &throttles);
  }
#endif

  MaybeAddThrottle(
      SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(handle),
      &throttles);

  if (base::FeatureList::IsEnabled(
          supervised_user::kClassifyUrlOnProcessResponseEvent)) {
    MaybeAddThrottle(
        supervised_user::MaybeCreateClassifyUrlNavigationThrottleFor(handle),
        &throttles);
  } else {
    MaybeAddThrottle(
        SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(handle),
        &throttles);
  }

  if (auto* throttle_manager =
          subresource_filter::ContentSubresourceFilterThrottleManager::
              FromNavigationHandle(*handle)) {
    throttle_manager->MaybeAppendNavigationThrottles(handle, &throttles);
  }

  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionFeatureEnabled()) {
    if (auto* throttle_manager = fingerprinting_protection_filter::
            ThrottleManager::FromNavigationHandle(*handle)) {
      throttle_manager->MaybeAppendNavigationThrottles(handle, &throttles);
    }
  }

  MaybeAddThrottle(
      LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(handle),
      &throttles);

  MaybeAddThrottle(PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);
#if BUILDFLAG(ENABLE_PDF)
  throttles.push_back(std::make_unique<pdf::PdfNavigationThrottle>(
      handle, std::make_unique<ChromePdfStreamDelegate>()));
#endif  // BUILDFLAG(ENABLE_PDF)

  MaybeAddThrottle(TabUnderNavigationThrottle::MaybeCreate(handle), &throttles);

  MaybeAddThrottle(
      WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);

  MaybeAddThrottle(
      PasswordManagerNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);

  throttles.push_back(std::make_unique<PolicyBlocklistNavigationThrottle>(
      handle, handle->GetWebContents()->GetBrowserContext()));

  // Before setting up SSL error detection, configure SSLErrorHandler to invoke
  // the relevant extension API whenever an SSL interstitial is shown.
  SSLErrorHandler::SetClientCallbackOnInterstitialsShown(
      base::BindRepeating(&MaybeTriggerSecurityInterstitialShownEvent));
  throttles.push_back(std::make_unique<SSLErrorNavigationThrottle>(
      handle,
      base::BindOnce(&HandleSSLErrorWrapper), base::BindOnce(&IsInHostedApp),
      base::BindOnce(
          &ShouldIgnoreSslInterstitialBecauseNavigationDefaultedToHttps)));

  throttles.push_back(std::make_unique<LoginNavigationThrottle>(handle));

  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps)) {
    MaybeAddThrottle(
        TypedNavigationUpgradeThrottle::MaybeCreateThrottleFor(handle),
        &throttles);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  MaybeAddThrottle(
      WebAppSettingsNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
  MaybeAddThrottle(profile_management::ProfileManagementNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
  MaybeAddThrottle(
      profile_management::OidcAuthResponseCaptureNavigationThrottle::
          MaybeCreateThrottleFor(handle),
      &throttles);
  MaybeAddThrottle(
      ManagedProfileRequiredNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  MaybeAddThrottle(enterprise_connectors::DeviceTrustNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
  MaybeAddThrottle(DevToolsWindow::MaybeCreateNavigationThrottle(handle),
                   &throttles);

  MaybeAddThrottle(NewTabPageNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);

  MaybeAddThrottle(
      web_app::TabbedWebAppNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);

  MaybeAddThrottle(
      web_app::WebUIWebAppNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

  // g_browser_process->safe_browsing_service() may be null in unittests.
  safe_browsing::SafeBrowsingUIManager* ui_manager =
      g_browser_process->safe_browsing_service()
          ? g_browser_process->safe_browsing_service()->ui_manager().get()
          : nullptr;
  MaybeAddThrottle(
      safe_browsing::SafeBrowsingNavigationThrottle::MaybeCreateThrottleFor(
          handle, ui_manager),
      &throttles);

  if (base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings)) {
    throttles.push_back(
        std::make_unique<safe_browsing::DelayedWarningNavigationThrottle>(
            handle));
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  MaybeAddThrottle(browser_switcher::BrowserSwitcherNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MaybeAddThrottle(
      chromeos::KioskSettingsNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
  if (ash::boca_util::IsEnabled()) {
    MaybeAddThrottle(
        ash::OnTaskLockedSessionNavigationThrottle::MaybeCreateThrottleFor(
            handle),
        &throttles);
  }
#endif

#if BUILDFLAG(IS_MAC)
  MaybeAddThrottle(MaybeCreateAuthSessionThrottleFor(handle), &throttles);
#endif

  auto* performance_manager_registry =
      performance_manager::PerformanceManagerRegistry::GetInstance();
  if (performance_manager_registry) {
    MaybeAddThrottles(
        performance_manager_registry->CreateThrottlesForNavigation(handle),
        &throttles);
  }

  if (profile && profile->GetPrefs()) {
    MaybeAddThrottle(
        security_interstitials::InsecureFormNavigationThrottle::
            MaybeCreateNavigationThrottle(
                handle, std::make_unique<ChromeSecurityBlockingPageFactory>(),
                profile->GetPrefs()),
        &throttles);
  }

  if (IsErrorPageAutoReloadEnabled()) {
    MaybeAddThrottle(
        error_page::NetErrorAutoReloader::MaybeCreateThrottleFor(handle),
        &throttles);
  }

  MaybeAddThrottle(
      payments::PaymentHandlerNavigationThrottle::MaybeCreateThrottleFor(
          handle),
      &throttles);

  MaybeAddThrottle(
      prerender::NoStatePrefetchNavigationThrottle::MaybeCreateThrottleFor(
          handle),
      &throttles);

#if defined(TOOLKIT_VIEWS)
  if (profile && IsSideSearchEnabled(profile)) {
    MaybeAddThrottle(
        SideSearchSideContentsHelper::MaybeCreateThrottleFor(handle),
        &throttles);
  }
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  if (lens::features::IsLensSidePanelEnabled()) {
    MaybeAddThrottle(
        lens::LensSidePanelNavigationHelper::MaybeCreateThrottleFor(handle),
        &throttles);
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  MaybeAddThrottle(ReadAnythingSidePanelNavigationThrottle::CreateFor(handle),
                   &throttles);

  if (lens::features::IsLensOverlayEnabled()) {
    if (profile) {
      if (ThemeService* theme_service =
              ThemeServiceFactory::GetForProfile(profile)) {
        MaybeAddThrottle(
            lens::LensOverlaySidePanelNavigationThrottle::MaybeCreateFor(
                handle, theme_service),
            &throttles);
      }
    }
  }
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  MaybeAddThrottle(
      offline_pages::OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
          handle),
      &throttles);
#endif

  if (profile) {
    MaybeAddThrottle(
        HttpsUpgradesNavigationThrottle::MaybeCreateThrottleFor(
            handle, std::make_unique<ChromeSecurityBlockingPageFactory>(),
            profile),
        &throttles);
  }

  MaybeAddThrottle(MaybeCreateNavigationAblationThrottle(handle), &throttles);

#if !BUILDFLAG(IS_ANDROID)
  MaybeAddThrottle(MaybeCreateAboutThisSiteThrottleFor(handle), &throttles);
#endif

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (privacy_sandbox_settings &&
      privacy_sandbox_settings->AreRelatedWebsiteSetsEnabled()) {
    MaybeAddThrottle(first_party_sets::FirstPartySetsNavigationThrottle::
                         MaybeCreateNavigationThrottle(handle),
                     &throttles);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Don't perform platform authentication in incognito and guest profiles.
  if (profile && !profile->IsOffTheRecord()) {
    MaybeAddThrottle(
        enterprise_auth::PlatformAuthNavigationThrottle::MaybeCreateThrottleFor(
            handle),
        &throttles);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(b:296844164) Handle captive portal signin properly.
  if (profile && profile->IsIncognitoProfile() && profile->IsOffTheRecord() &&
      !profile->GetOTRProfileID().IsCaptivePortal()) {
    MaybeAddThrottle(
        chromeos::IncognitoNavigationThrottle::MaybeCreateThrottleFor(handle),
        &throttles);
  }

  MaybeAddThrottle(apps::AppInstallNavigationThrottle::MaybeCreate(handle),
                   &throttles);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  MaybeAddThrottle(PreviewNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);
#endif  // !BUILDFLAG(IS_ANDROID)

  MaybeAddThrottle(MaybeCreateVisitedLinkNavigationThrottleFor(handle),
                   &throttles);

  MaybeAddThrottle(
      data_sharing::DataSharingNavigationThrottle::MaybeCreateThrottleFor(
          handle),
      &throttles);

  return throttles;
}

std::vector<std::unique_ptr<content::CommitDeferringCondition>>
ChromeContentBrowserClient::CreateCommitDeferringConditionsForNavigation(
    content::NavigationHandle* navigation_handle,
    content::CommitDeferringCondition::NavigationType navigation_type) {
  auto conditions =
      std::vector<std::unique_ptr<content::CommitDeferringCondition>>();

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  MaybeAddCondition(
      safe_browsing::MaybeCreateCommitDeferringCondition(*navigation_handle),
      &conditions);
#endif

  return conditions;
}

std::unique_ptr<content::NavigationUIData>
ChromeContentBrowserClient::GetNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  return std::make_unique<ChromeNavigationUIData>(navigation_handle);
}

std::unique_ptr<media::ScreenEnumerator>
ChromeContentBrowserClient::CreateScreenEnumerator() const {
  return std::make_unique<ChromeScreenEnumerator>();
}

bool ChromeContentBrowserClient::EnforceSystemAudioEchoCancellation() {
  // TODO(b/270042522): This is a short term solution to enforce the system
  // audio cancellation and will be removed before Lacros is released. The
  // short term solution will not work on Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CRAS)
  bool system_aec_enabled = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kDeviceSystemAecEnabled,
                                       &system_aec_enabled);
  return system_aec_enabled;
#else
  return false;
#endif
}

std::unique_ptr<content::DevToolsManagerDelegate>
ChromeContentBrowserClient::CreateDevToolsManagerDelegate() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DevToolsManagerDelegateAndroid>();
#else
  return std::make_unique<ChromeDevToolsManagerDelegate>();
#endif
}

void ChromeContentBrowserClient::UpdateDevToolsBackgroundServiceExpiration(
    content::BrowserContext* browser_context,
    int service,
    base::Time expiration_time) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  auto* pref_service = profile->GetPrefs();
  DCHECK(pref_service);

  ScopedDictPrefUpdate pref_update(
      pref_service, prefs::kDevToolsBackgroundServicesExpirationDict);
  base::Value::Dict& exp_dict = pref_update.Get();

  // Convert |expiration_time| to minutes since that is the most granular
  // option that returns an int. base::Value does not accept int64.
  int expiration_time_minutes =
      expiration_time.ToDeltaSinceWindowsEpoch().InMinutes();
  exp_dict.Set(base::NumberToString(service), expiration_time_minutes);
}

base::flat_map<int, base::Time>
ChromeContentBrowserClient::GetDevToolsBackgroundServiceExpirations(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  auto* pref_service = profile->GetPrefs();
  DCHECK(pref_service);

  const auto& expiration_dict =
      pref_service->GetDict(prefs::kDevToolsBackgroundServicesExpirationDict);

  base::flat_map<int, base::Time> expiration_times;
  for (auto it : expiration_dict) {
    // key.
    int service = 0;
    bool did_convert = base::StringToInt(it.first, &service);
    DCHECK(did_convert);

    // value.
    DCHECK(it.second.is_int());
    base::TimeDelta delta = base::Minutes(it.second.GetInt());
    base::Time expiration_time = base::Time::FromDeltaSinceWindowsEpoch(delta);

    expiration_times[service] = expiration_time;
  }

  return expiration_times;
}

std::optional<base::TimeDelta>
ChromeContentBrowserClient::GetSpareRendererDelayForSiteURL(
    const GURL& site_url) {
  if (IsTopChromeWebUIURL(site_url)) {
    // Experiments have shown that delaying 2s brings the most significant
    // improvements to Top Chrome WebUIs. See crbug.com/41490050.
    return base::Seconds(2);
  }

#if BUILDFLAG(IS_ANDROID)
  // Delay the creation of the renderer on Android since the CPU tends
  // to be busy during loading.
  if (base::FeatureList::IsEnabled(
          features::kAndroidWarmUpSpareRendererWithTimeout)) {
    return base::Milliseconds(
        features::kAndroidSpareRendererCreationDelayMs.Get());
  }
#endif

  return std::nullopt;
}

std::unique_ptr<content::TracingDelegate>
ChromeContentBrowserClient::CreateTracingDelegate() {
  return std::make_unique<ChromeTracingDelegate>();
}

bool ChromeContentBrowserClient::IsSystemWideTracingEnabled() {
  return ChromeTracingDelegate::IsSystemWideTracingEnabled();
}

bool ChromeContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::
      IsPluginAllowedToCallRequestOSFileHandle(browser_context, url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::
      IsPluginAllowedToUseDevChannelAPIs(browser_context, url);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::InitOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  safe_browsing_service_ = g_browser_process->safe_browsing_service();

  // Initialize `network_contexts_parent_directory_`.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  network_contexts_parent_directory_.push_back(user_data_dir);

  base::FilePath cache_dir;
  chrome::GetUserCacheDirectory(user_data_dir, &cache_dir);
  DCHECK(!cache_dir.empty());
  // On some platforms, the cache is a child of the user_data_dir so only
  // return the one path.
  if (!user_data_dir.IsParent(cache_dir))
    network_contexts_parent_directory_.push_back(cache_dir);

  // If the cache location has been overridden by a switch or preference,
  // include that as well.
  if (auto* local_state = g_browser_process->local_state()) {
    base::FilePath pref_cache_dir =
        local_state->GetFilePath(prefs::kDiskCacheDir);
    if (!pref_cache_dir.empty() && !user_data_dir.IsParent(cache_dir))
      network_contexts_parent_directory_.push_back(pref_cache_dir);
  }
}

void ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
    base::CommandLine* to_command_line,
    const base::CommandLine& from_command_line,
    version_info::Channel channel) {
#if BUILDFLAG(IS_ANDROID)
  const version_info::Channel kMaxDisableEncryptionChannel =
      version_info::Channel::BETA;
#else
  const version_info::Channel kMaxDisableEncryptionChannel =
      version_info::Channel::DEV;
#endif
  if (channel <= kMaxDisableEncryptionChannel) {
    static const char* const kWebRtcDevSwitchNames[] = {
        switches::kDisableWebRtcEncryption,
    };
    to_command_line->CopySwitchesFrom(from_command_line, kWebRtcDevSwitchNames);
  }
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
void ChromeContentBrowserClient::CreateMediaRemoter(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  CastRemotingConnector::CreateMediaRemoter(
      render_frame_host, std::move(source), std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

base::FilePath ChromeContentBrowserClient::GetLoggingFileName(
    const base::CommandLine& command_line) {
  return logging::GetLogFileName(command_line);
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
std::unique_ptr<blink::URLLoaderThrottle>
ChromeContentBrowserClient::MaybeCreateSafeBrowsingURLLoaderThrottle(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id,
    Profile* profile) {
  bool matches_enterprise_allowlist = safe_browsing::IsURLAllowlistedByPolicy(
      request.url, *profile->GetPrefs());
  if (!matches_enterprise_allowlist) {
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
    auto* connectors_service =
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            browser_context);
    bool has_valid_dm_token =
        connectors_service &&
        connectors_service->GetDMTokenForRealTimeUrlCheck().has_value();
    bool is_enterprise_lookup_enabled =
        safe_browsing::RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
            profile->GetPrefs(), has_valid_dm_token, profile->IsOffTheRecord(),
            profile->IsGuestSession());
#else
    bool is_enterprise_lookup_enabled = false;
#endif
    bool is_consumer_lookup_enabled =
        safe_browsing::RealTimePolicyEngine::CanPerformFullURLLookup(
            profile->GetPrefs(), profile->IsOffTheRecord(),
            g_browser_process->variations_service());

    // |url_lookup_service| is used when real time url check is enabled.
    safe_browsing::RealTimeUrlLookupServiceBase* url_lookup_service =
        GetUrlLookupService(browser_context, is_enterprise_lookup_enabled,
                            is_consumer_lookup_enabled);
    safe_browsing::HashRealTimeService* hash_realtime_service =
        safe_browsing_service_
            ? safe_browsing_service_->GetHashRealTimeService(profile)
            : nullptr;
    safe_browsing::hash_realtime_utils::HashRealTimeSelection
        hash_realtime_selection =
            safe_browsing::hash_realtime_utils::DetermineHashRealTimeSelection(
                profile->IsOffTheRecord(), profile->GetPrefs(),
                safe_browsing::hash_realtime_utils::GetCountryCode(
                    g_browser_process->variations_service()),
                /*log_usage_histograms=*/true);
    safe_browsing::AsyncCheckTracker* async_check_tracker =
        GetAsyncCheckTracker(wc_getter, is_enterprise_lookup_enabled,
                             is_consumer_lookup_enabled,
                             hash_realtime_selection, frame_tree_node_id);

    return safe_browsing::BrowserURLLoaderThrottle::Create(
        base::BindRepeating(
            &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
            base::Unretained(this),
            safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs()),
            // Should check for enterprise when safe browsing is disabled.
            /*should_check_on_sb_disabled=*/is_enterprise_lookup_enabled,
            safe_browsing::GetURLAllowlistByPolicy(profile->GetPrefs())),
        wc_getter, frame_tree_node_id, navigation_id,
        url_lookup_service ? url_lookup_service->GetWeakPtr() : nullptr,
        hash_realtime_service ? hash_realtime_service->GetWeakPtr() : nullptr,
        hash_realtime_selection,
        async_check_tracker ? async_check_tracker->GetWeakPtr() : nullptr);
  }
  return nullptr;
}
#endif

#if BUILDFLAG(IS_ANDROID)
std::tuple<std::string /*client_data_header*/, bool /*is_custom_tab*/>
GetClientDataHeader(content::FrameTreeNodeId frame_tree_node_id) {
  std::string client_data_header;
  bool is_custom_tab = false;
  if (frame_tree_node_id) {
    auto* web_contents = WebContents::FromFrameTreeNodeId(frame_tree_node_id);
    // Could be null if the FrameTreeNode's RenderFrameHost is shutting down.
    if (web_contents) {
      auto* client_data_header_observer =
          customtabs::ClientDataHeaderWebContentsObserver::FromWebContents(
              web_contents);
      if (client_data_header_observer) {
        client_data_header = client_data_header_observer->header();
      }

      auto* delegate =
          TabAndroid::FromWebContents(web_contents)
              ? static_cast<android::TabWebContentsDelegateAndroid*>(
                    web_contents->GetDelegate())
              : nullptr;
      if (delegate) {
        is_custom_tab = delegate->IsCustomTab();
      }
    }
  }
  return {client_data_header, is_custom_tab};
}
#endif

std::unique_ptr<blink::URLLoaderThrottle> CreateGoogleURLLoaderThrottle(
#if BUILDFLAG(IS_ANDROID)
    const std::string& client_data_header,
#endif
    Profile* profile) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  BoundSessionCookieRefreshService* bound_session_cookie_refresh_service =
      BoundSessionCookieRefreshServiceFactory::GetForProfile(profile);
  std::unique_ptr<BoundSessionRequestThrottledHandler>
      bound_session_request_throttled_handler;
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
      bound_session_throttler_params;

  if (bound_session_cookie_refresh_service) {
    bound_session_request_throttled_handler =
        std::make_unique<BoundSessionRequestThrottledHandlerBrowserImpl>(
            *bound_session_cookie_refresh_service);
    bound_session_throttler_params =
        bound_session_cookie_refresh_service->GetBoundSessionThrottlerParams();
  }
#endif

  chrome::mojom::DynamicParamsPtr dynamic_params =
      chrome::mojom::DynamicParams::New(
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
          std::move(bound_session_throttler_params),
#endif
          profile->GetPrefs()->GetBoolean(
              policy::policy_prefs::kForceGoogleSafeSearch),
          profile->GetPrefs()->GetInteger(
              policy::policy_prefs::kForceYouTubeRestrict),
          profile->GetPrefs()->GetString(prefs::kAllowedDomainsForApps));
  return std::make_unique<GoogleURLLoaderThrottle>(
#if BUILDFLAG(IS_ANDROID)
      client_data_header,
#endif
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      std::move(bound_session_request_throttled_handler),
#endif
      std::move(dynamic_params));
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ChromeContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  DCHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (auto safe_browsing_throttle = MaybeCreateSafeBrowsingURLLoaderThrottle(
          request, browser_context, wc_getter, frame_tree_node_id,
          navigation_id, profile);
      safe_browsing_throttle) {
    result.push_back(std::move(safe_browsing_throttle));
  }
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  result.push_back(
      std::make_unique<captive_portal::CaptivePortalURLLoaderThrottle>(
          wc_getter.Run()));
#endif

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
  if (request_header_integrity::RequestHeaderIntegrityURLLoaderThrottle::
          IsFeatureEnabled()) {
    result.push_back(
        std::make_unique<request_header_integrity::
                             RequestHeaderIntegrityURLLoaderThrottle>());
  }
#endif

  if (chrome_navigation_ui_data &&
      chrome_navigation_ui_data->is_no_state_prefetching()) {
    result.push_back(
        std::make_unique<prerender::NoStatePrefetchURLLoaderThrottle>(
            GetPrerenderCanceler(wc_getter)));
  }

#if BUILDFLAG(IS_ANDROID)
  auto [client_data_header, is_custom_tab] =
      GetClientDataHeader(frame_tree_node_id);
#endif

  if (auto google_throttle = CreateGoogleURLLoaderThrottle(
#if BUILDFLAG(IS_ANDROID)
          client_data_header,
#endif
          profile);
      google_throttle) {
    result.push_back(std::move(google_throttle));
  }

  {
    auto* factory =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(browser_context);
    // null in unit tests.
    if (factory) {
      result.push_back(
          std::make_unique<custom_handlers::ProtocolHandlerThrottle>(*factory));
    }
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  result.push_back(std::make_unique<PluginResponseInterceptorURLLoaderThrottle>(
      request.destination, frame_tree_node_id));
#endif

#if BUILDFLAG(IS_ANDROID)
  auto delegate = std::make_unique<signin::HeaderModificationDelegateImpl>(
      profile, /*incognito_enabled=*/!is_custom_tab);
#else
  auto delegate =
      std::make_unique<signin::HeaderModificationDelegateImpl>(profile);
#endif

  auto signin_throttle =
      signin::URLLoaderThrottle::MaybeCreate(std::move(delegate), wc_getter);
  if (signin_throttle)
    result.push_back(std::move(signin_throttle));

  return result;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ChromeContentBrowserClient::CreateURLLoaderThrottlesForKeepAlive(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  DCHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (auto safe_browsing_throttle = MaybeCreateSafeBrowsingURLLoaderThrottle(
          request, browser_context, wc_getter, frame_tree_node_id,
          /*navigation_id=*/std::nullopt, profile);
      safe_browsing_throttle) {
    result.push_back(std::move(safe_browsing_throttle));
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  auto [client_data_header, unused_is_custom_tab] =
      GetClientDataHeader(frame_tree_node_id);
#endif

  if (auto google_throttle = CreateGoogleURLLoaderThrottle(
#if BUILDFLAG(IS_ANDROID)
          client_data_header,
#endif
          profile);
      google_throttle) {
    result.push_back(std::move(google_throttle));
  }

  return result;
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ChromeContentBrowserClient::CreateNonNetworkNavigationURLLoaderFactory(
    const std::string& scheme,
    content::FrameTreeNodeId frame_tree_node_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    !BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (scheme == extensions::kExtensionScheme) {
    if (!ChromeContentBrowserClientExtensionsPart::
            AreExtensionsDisabledForProfile(browser_context)) {
      bool is_guest = false;
#if BUILDFLAG(ENABLE_GUEST_VIEW)
      is_guest =
          !!extensions::WebViewGuest::FromFrameTreeNodeId(frame_tree_node_id);
#endif

      return extensions::CreateExtensionNavigationURLLoaderFactory(
          browser_context, is_guest);
    }

    return {};
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  Profile* profile = Profile::FromBrowserContext(browser_context);
  // KeyedServices could be disabled based on the profile type, e.g. System
  // Profile doesn't construct services by default.
  if (AreKeyedServicesDisabledForProfileByDefault(profile))
    return {};

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (scheme == content::kExternalFileScheme) {
    return ash::ExternalFileURLLoaderFactory::Create(
        profile, content::ChildProcessHost::kInvalidUniqueID);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
  if (scheme == chrome::kIsolatedAppScheme) {
    if (content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
            browser_context) &&
        !browser_context->ShutdownStarted()) {
      return web_app::IsolatedWebAppURLLoaderFactory::CreateForFrame(
          browser_context, /*app_origin=*/std::nullopt, frame_tree_node_id);
    }

    return {};
  }
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE) || BUILDFLAG(IS_CHROMEOS_ASH) ||
        // !BUILDFLAG(IS_ANDROID)

  return {};
}

void ChromeContentBrowserClient::
    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

#if !BUILDFLAG(IS_ANDROID)
  if (content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
          browser_context) &&
      !browser_context->ShutdownStarted()) {
    factories->emplace(chrome::kIsolatedAppScheme,
                       web_app::IsolatedWebAppURLLoaderFactory::Create(
                           browser_context, /*app_origin=*/std::nullopt));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  DCHECK(!ChromeContentBrowserClientExtensionsPart::
             AreExtensionsDisabledForProfile(browser_context));

  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionWorkerMainResourceURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

void ChromeContentBrowserClient::
    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

#if !BUILDFLAG(IS_ANDROID)
  if (content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
          browser_context) &&
      !browser_context->ShutdownStarted()) {
    factories->emplace(chrome::kIsolatedAppScheme,
                       web_app::IsolatedWebAppURLLoaderFactory::Create(
                           browser_context, /*app_origin=*/std::nullopt));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          browser_context)) {
    return;
  }

  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionServiceWorkerScriptURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

namespace {

// The SpecialAccessFileURLLoaderFactory provided to the extension background
// pages.  Checks with the ChildProcessSecurityPolicy to validate the file
// access.
class SpecialAccessFileURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed
  // SpecialAccessFileURLLoaderFactory.  The factory is self-owned - it will
  // delete itself once there are no more receivers (including the receiver
  // associated with the returned mojo::PendingRemote and the receivers bound by
  // the Clone method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      int child_id) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

    // The SpecialAccessFileURLLoaderFactory will delete itself when there are
    // no more receivers - see the
    // network::SelfDeletingURLLoaderFactory::OnDisconnect method.
    new SpecialAccessFileURLLoaderFactory(
        child_id, pending_remote.InitWithNewPipeAndPassReceiver());

    return pending_remote;
  }

  SpecialAccessFileURLLoaderFactory(const SpecialAccessFileURLLoaderFactory&) =
      delete;
  SpecialAccessFileURLLoaderFactory& operator=(
      const SpecialAccessFileURLLoaderFactory&) = delete;

 private:
  explicit SpecialAccessFileURLLoaderFactory(
      int child_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
      : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
        child_id_(child_id) {}

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
            child_id_, request.url)) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_ACCESS_DENIED));
      return;
    }
    content::CreateFileURLLoaderBypassingSecurityChecks(
        request, std::move(loader), std::move(client),
        /*observer=*/nullptr,
        /* allow_directory_listing */ true);
  }

  int child_id_;
};

#if BUILDFLAG(IS_CHROMEOS)
bool IsSystemFeatureDisabled(policy::SystemFeature system_feature) {
  return policy::SystemFeaturesDisableListPolicyHandler::
      IsSystemFeatureDisabled(system_feature, g_browser_process->local_state());
}

bool IsSystemFeatureURLDisabled(const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme) &&
      !url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return false;
  }

  // chrome://os-settings/pwa.html shouldn't be replaced to let the settings app
  // installation complete successfully.
  if (url.DomainIs(chrome::kChromeUIOSSettingsHost) &&
      url.path() != "/pwa.html") {
    return IsSystemFeatureDisabled(policy::SystemFeature::kOsSettings);
  }

  if (url.DomainIs(chrome::kChromeUISettingsHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kBrowserSettings);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.DomainIs(chrome::kChromeUIUntrustedCroshHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kCrosh);
  }

  if (url.DomainIs(ash::kChromeUIScanningAppHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kScanning);
  }

  if (url.DomainIs(ash::kChromeUICameraAppHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kCamera);
  }

  if (url.DomainIs(ash::kChromeUIHelpAppHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kExplore);
  }

  if (url.DomainIs(ash::kChromeUIMediaAppHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kGallery);
  }

  if (url.DomainIs(chrome::kChromeUIUntrustedTerminalHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kTerminal);
  }

  if (url.DomainIs(ash::kChromeUIPrintManagementHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kPrintJobs);
  }

  if (url.DomainIs(ash::kChromeUIShortcutCustomizationAppHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kKeyShortcuts);
  }

  if (url.DomainIs(ash::kChromeUIRecorderAppHost)) {
    return IsSystemFeatureDisabled(policy::SystemFeature::kRecorder);
  }

#endif

  return false;
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void InitializeFileURLLoaderFactoryForExtension(
    int render_process_id,
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    ChromeContentBrowserClient::NonNetworkURLLoaderFactoryMap* factories) {
  // Extensions with the necessary permissions get access to file:// URLs that
  // gets approval from ChildProcessSecurityPolicy. Keep this logic in sync with
  // ExtensionWebContentsObserver::RenderFrameCreated.
  Manifest::Type type = extension->GetType();
  if ((type == Manifest::TYPE_EXTENSION ||
       type == Manifest::TYPE_LEGACY_PACKAGED_APP) &&
      extensions::util::AllowFileAccess(extension->id(), browser_context)) {
    factories->emplace(
        url::kFileScheme,
        SpecialAccessFileURLLoaderFactory::Create(render_process_id));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
void AddChromeSchemeFactories(
    int render_process_id,
    content::RenderFrameHost* frame_host,
    content::WebContents* web_contents,
    const extensions::Extension* extension,
    ChromeContentBrowserClient::NonNetworkURLLoaderFactoryMap* factories) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  // The test below matches when a remote 3P NTP is loaded. The effective
  // URL is chrome-search://remote-ntp. This is to allow the use of the NTP
  // public api and to embed most-visited tiles
  // (chrome-search://most-visited/title.html).
  //
  // InstantService might be null for some irregular profiles, e.g. the System
  // Profile.
  if (instant_service && instant_service->IsInstantProcess(render_process_id)) {
    factories->emplace(
        chrome::kChromeSearchScheme,
        content::CreateWebUIURLLoaderFactory(
            frame_host, chrome::kChromeSearchScheme,
            /*allowed_webui_hosts=*/base::flat_set<std::string>()));
  }

  extensions::ExtensionWebContentsObserver* web_observer =
      extensions::ExtensionWebContentsObserver::GetForWebContents(web_contents);

  // There is nothing to do if no ExtensionWebContentsObserver is attached
  // to the |web_contents| or no enabled extension exists.
  if (!web_observer || !extension)
    return;

  std::vector<std::string> allowed_webui_hosts;
  // Support for chrome:// scheme if appropriate.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      Manifest::IsComponentLocation(extension->location())) {
    // Components of chrome that are implemented as extensions or platform apps
    // are allowed to use chrome://resources/ and chrome://theme/ URLs.
    allowed_webui_hosts.emplace_back(content::kChromeUIResourcesHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIThemeHost);
    // For testing purposes chrome://webui-test/ is also allowed.
    allowed_webui_hosts.emplace_back(chrome::kChromeUIWebUITestHost);
  }
  if (extension->is_extension() || extension->is_legacy_packaged_app() ||
      (extension->is_platform_app() &&
       Manifest::IsComponentLocation(extension->location()))) {
    // Extensions, legacy packaged apps, and component platform apps are allowed
    // to use chrome://favicon/, chrome://extension-icon/ and chrome://app-icon
    // URLs. Hosted apps are not allowed because they are served via web servers
    // (and are generally never given access to Chrome APIs).
    allowed_webui_hosts.emplace_back(chrome::kChromeUIExtensionIconHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIFaviconHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIAppIconHost);
  }
  if (!allowed_webui_hosts.empty()) {
    factories->emplace(content::kChromeUIScheme,
                       content::CreateWebUIURLLoaderFactory(
                           frame_host, content::kChromeUIScheme,
                           std::move(allowed_webui_hosts)));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}  // namespace

void ChromeContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        const std::optional<url::Origin>& request_initiator_origin,
        NonNetworkURLLoaderFactoryMap* factories) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_EXTENSIONS) || \
    !BUILDFLAG(IS_ANDROID)
  content::RenderFrameHost* frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(frame_host);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_EXTENSIONS) || \
        // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    factories->emplace(
        content::kExternalFileScheme,
        ash::ExternalFileURLLoaderFactory::Create(profile, render_process_id));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
  {
    auto* rph = content::RenderProcessHost::FromID(render_process_id);
    content::BrowserContext* browser_context = rph->GetBrowserContext();
    DCHECK(browser_context);
    bool is_initiator_iwa =
        request_initiator_origin.has_value() &&
        request_initiator_origin->scheme() == chrome::kIsolatedAppScheme;
    if (content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
            browser_context) &&
        !browser_context->ShutdownStarted() && is_initiator_iwa) {
      if (frame_host != nullptr) {
        factories->emplace(
            chrome::kIsolatedAppScheme,
            web_app::IsolatedWebAppURLLoaderFactory::CreateForFrame(
                browser_context, request_initiator_origin,
                frame_host->GetFrameTreeNodeId()));
      } else {
        factories->emplace(chrome::kIsolatedAppScheme,
                           web_app::IsolatedWebAppURLLoaderFactory::Create(
                               browser_context, request_initiator_origin));
      }
    }
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content::BrowserContext* browser_context =
      content::RenderProcessHost::FromID(render_process_id)
          ->GetBrowserContext();
  if (ChromeContentBrowserClientExtensionsPart::AreExtensionsDisabledForProfile(
          browser_context))
    return;

  factories->emplace(extensions::kExtensionScheme,
                     extensions::CreateExtensionURLLoaderFactory(
                         render_process_id, render_frame_id));

  const extensions::Extension* extension = nullptr;
  if (request_initiator_origin != std::nullopt) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(
            Profile::FromBrowserContext(browser_context));
    DCHECK(registry);
    extension = registry->enabled_extensions().GetExtensionOrAppByURL(
        request_initiator_origin->GetURL());
  }

  // For service worker contexts, we only allow file access. The remainder of
  // this code is used to allow extensions to access chrome:-scheme
  // resources, which we are moving away from.
  // TODO(crbug.com/40811448) Factories should not be created for unloaded
  // extensions.
  if (extension) {
    InitializeFileURLLoaderFactoryForExtension(
        render_process_id, browser_context, extension, factories);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // This logic should match
  // ChromeExtensionWebContentsObserver::RenderFrameCreated.
  if (web_contents) {
    AddChromeSchemeFactories(render_process_id, frame_host, web_contents,
                             extension, factories);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

void ChromeContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (web_request_api) {
    bool use_proxy_for_web_request =
        web_request_api->MaybeProxyURLLoaderFactory(
            browser_context, frame, render_process_id, type,
            std::move(navigation_id), ukm_source_id, factory_builder,
            header_client, navigation_response_task_runner, request_initiator);
    if (bypass_redirect_checks)
      *bypass_redirect_checks = use_proxy_for_web_request;
  }
#endif

  signin::ProxyingURLLoaderFactory::MaybeProxyRequest(
      frame, type == URLLoaderFactoryType::kNavigation, request_initiator,
      isolation_info, factory_builder);

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  if (disable_secure_dns) {
    WebContents* web_contents = WebContents::FromRenderFrameHost(frame);
    *disable_secure_dns =
        web_contents &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents) &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents)
            ->is_captive_portal_window();
  }
#endif

  // WARNING: This must be the last interceptor in the chain as the proxying
  // URLLoaderFactory installed by this needs to be the one actually sending
  // packets over the network (to effectively target `bound_network`).
  MaybeProxyNetworkBoundRequest(browser_context,
                                GetBoundNetworkFromRenderFrameHost(frame),
                                factory_builder, factory_override,
                                isolation_info);
}

std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
ChromeContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    content::FrameTreeNodeId frame_tree_node_id,
    int64_t navigation_id,
    bool force_no_https_upgrade,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
      interceptors;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  interceptors.push_back(
      std::make_unique<offline_pages::OfflinePageURLLoaderRequestInterceptor>(
          navigation_ui_data, frame_tree_node_id));
#endif

#if BUILDFLAG(ENABLE_PDF)
  {
    std::unique_ptr<content::URLLoaderRequestInterceptor> pdf_interceptor =
        pdf::PdfURLLoaderRequestInterceptor::MaybeCreateInterceptor(
            frame_tree_node_id, std::make_unique<ChromePdfStreamDelegate>());
    if (pdf_interceptor)
      interceptors.push_back(std::move(pdf_interceptor));
  }
#endif

  interceptors.push_back(std::make_unique<SearchPrefetchURLLoaderInterceptor>(
      frame_tree_node_id, navigation_id, navigation_response_task_runner));

  if (!force_no_https_upgrade) {
    auto https_upgrades_interceptor =
        HttpsUpgradesInterceptor::MaybeCreateInterceptor(frame_tree_node_id,
                                                         navigation_ui_data);
    if (https_upgrades_interceptor) {
      interceptors.push_back(std::move(https_upgrades_interceptor));
    }
  }

  return interceptors;
}

content::ContentBrowserClient::URLLoaderRequestHandler
ChromeContentBrowserClient::
    CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
        content::FrameTreeNodeId frame_tree_node_id,
        const network::ResourceRequest& resource_request) {
  SearchPrefetchURLLoader::RequestHandler prefetch_handler =
      SearchPrefetchURLLoaderInterceptor::MaybeCreateLoaderForRequest(
          resource_request, frame_tree_node_id);
  return prefetch_handler;
}

bool ChromeContentBrowserClient::WillInterceptWebSocket(
    content::RenderFrameHost* frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!frame) {
    return false;
  }
  const auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          frame->GetBrowserContext());

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (!web_request_api)
    return false;

  return (web_request_api->MayHaveProxies() ||
          web_request_api->MayHaveWebsocketProxiesForExtensionTelemetry() ||
          web_request_api->IsAvailableToWebViewEmbedderFrame(frame));
#else
  return false;
#endif
}

void ChromeContentBrowserClient::CreateWebSocket(
    content::RenderFrameHost* frame,
    WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(crbug.com/40195467): Request w/o a frame also should be proxied.
  if (!frame) {
    return;
  }
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          frame->GetBrowserContext());

  DCHECK(web_request_api);
  web_request_api->ProxyWebSocket(frame, std::move(factory), url,
                                  site_for_cookies, user_agent,
                                  std::move(handshake_client));
#endif
}

void ChromeContentBrowserClient::WillCreateWebTransport(
    int process_id,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    WillCreateWebTransportCallback callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/40195467): Add a unit test which calls
  // ChromeContentBrowserClient::WillCreateWebTransport() with invalid process
  // id and routing id.
  auto* render_process_host = content::RenderProcessHost::FromID(process_id);
  if (!render_process_host) {
    std::move(callback).Run(std::move(handshake_client), std::nullopt);
    return;
  }
  content::BrowserContext* browser_context =
      render_process_host->GetBrowserContext();
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  // NOTE: Some unit test environments do not initialize BrowserContextKeyedAPI
  // factories like WebRequestAPI.
  if (!web_request_api) {
    std::move(callback).Run(std::move(handshake_client), std::nullopt);
    return;
  }
  web_request_api->ProxyWebTransport(
      *render_process_host, frame_routing_id, url, initiator_origin,
      std::move(handshake_client), std::move(callback));
#else
  std::move(callback).Run(std::move(handshake_client), std::nullopt);
#endif
}

bool ChromeContentBrowserClient::WillCreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    bool is_service_worker,
    int process_id,
    int routing_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    DCHECK_EQ(network::mojom::RestrictedCookieManagerRole::SCRIPT, role);
    extensions::ChromeExtensionCookies::Get(browser_context)
        ->CreateRestrictedCookieManager(origin, isolation_info,
                                        std::move(*receiver));
    return true;
  }
#endif
  return false;
}

void ChromeContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  PrefService* local_state;
  if (g_browser_process) {
    DCHECK(g_browser_process->local_state());
    local_state = g_browser_process->local_state();
  } else {
    DCHECK(startup_data_.chrome_feature_list_creator()->local_state());
    local_state = startup_data_.chrome_feature_list_creator()->local_state();
  }

  // Create SystemNetworkContextManager if it has not been created yet. We need
  // to set up global NetworkService state before anything else uses it and this
  // is the first opportunity to initialize SystemNetworkContextManager with the
  // NetworkService.
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(local_state);

  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);

#if !BUILDFLAG(IS_ANDROID)
  if (task_manager::TaskManagerImpl::IsCreated() &&
      task_manager::TaskManagerImpl::GetInstance()->is_running()) {
    network_service->EnableDataUseUpdates(true);
  }
#endif
}

void ChromeContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ProfileNetworkContextService* service =
      ProfileNetworkContextServiceFactory::GetForContext(context);
  if (service) {
    service->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                           network_context_params,
                                           cert_verifier_creation_params);
  } else {
    // Set default params.
    network_context_params->user_agent = GetUserAgentBasedOnPolicy(context);
    network_context_params->accept_language = GetApplicationLocale();
  }
}

std::vector<base::FilePath>
ChromeContentBrowserClient::GetNetworkContextsParentDirectory() {
  DCHECK(!network_contexts_parent_directory_.empty());
  return network_contexts_parent_directory_;
}

base::Value::Dict ChromeContentBrowserClient::GetNetLogConstants() {
  return net_log::GetPlatformConstantsForNetLog(
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
}

bool ChromeContentBrowserClient::AllowRenderingMhtmlOverHttp(
    content::NavigationUIData* navigation_ui_data) {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // It is OK to load the saved offline copy, in MHTML format.
  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);
  if (!chrome_navigation_ui_data)
    return false;
  offline_pages::OfflinePageNavigationUIData* offline_page_data =
      chrome_navigation_ui_data->GetOfflinePageNavigationUIData();
  return offline_page_data && offline_page_data->is_offline_page();
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldForceDownloadResource(
    content::BrowserContext* browser_context,
    const GURL& url,
    const std::string& mime_type) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Special-case user scripts to get downloaded instead of viewed.
  if (extensions::UserScript::IsURLUserScript(url, mime_type)) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // QuickOffice file interception is deprecated. If QuickOffice would
  // have intercepted this file and this feature is disabled, download
  // it instead.
  if (browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    bool force_download = profile->GetPrefs()->GetBoolean(
        quickoffice::kQuickOfficeForceFileDownloadEnabled);

    if (base::FeatureList::IsEnabled(features::kQuickOfficeForceFileDownload) &&
        force_download) {
      std::string extension_id =
          PluginUtils::GetExtensionIdForMimeType(browser_context, mime_type);

      if (extension_misc::IsQuickOfficeExtension(extension_id)) {
        return true;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
}

content::BluetoothDelegate* ChromeContentBrowserClient::GetBluetoothDelegate() {
  if (!bluetooth_delegate_) {
    bluetooth_delegate_ = std::make_unique<ChromeBluetoothDelegate>(
        std::make_unique<ChromeBluetoothDelegateImplClient>());
  }
  return bluetooth_delegate_.get();
}

content::UsbDelegate* ChromeContentBrowserClient::GetUsbDelegate() {
  if (!usb_delegate_)
    usb_delegate_ = std::make_unique<ChromeUsbDelegate>();
  return usb_delegate_.get();
}

content::PrivateNetworkDeviceDelegate*
ChromeContentBrowserClient::GetPrivateNetworkDeviceDelegate() {
  if (!private_network_device_delegate_) {
    private_network_device_delegate_ =
        std::make_unique<ChromePrivateNetworkDeviceDelegate>();
  }
  return private_network_device_delegate_.get();
}

bool ChromeContentBrowserClient::IsSecurityLevelAcceptableForWebAuthn(
    content::RenderFrameHost* rfh,
    const url::Origin& caller_origin) {
  const Profile* profile =
      Profile::FromBrowserContext(rfh->GetBrowserContext());
  if (profile->GetPrefs()->GetBoolean(
          webauthn::pref_names::kAllowWithBrokenCerts)) {
    return true;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (caller_origin.scheme() == extensions::kExtensionScheme) {
    return true;
  }
#endif
  if (net::IsLocalhost(caller_origin.GetURL())) {
    return true;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  security_state::SecurityLevel security_level = helper->GetSecurityLevel();
  return security_level == security_state::SecurityLevel::SECURE ||
         security_level ==
             security_state::SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIgnoreCertificateErrors);
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeContentBrowserClient::CreateDeviceInfoService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) {
  DCHECK(render_frame_host);
  DeviceServiceImpl::Create(render_frame_host, std::move(receiver));
}

void ChromeContentBrowserClient::CreateManagedConfigurationService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver) {
  DCHECK(render_frame_host);
  ManagedConfigurationServiceImpl::Create(render_frame_host,
                                          std::move(receiver));
}

content::SerialDelegate* ChromeContentBrowserClient::GetSerialDelegate() {
  if (!serial_delegate_)
    serial_delegate_ = std::make_unique<ChromeSerialDelegate>();
  return serial_delegate_.get();
}

content::HidDelegate* ChromeContentBrowserClient::GetHidDelegate() {
  if (!hid_delegate_)
    hid_delegate_ = std::make_unique<ChromeHidDelegate>();
  return hid_delegate_.get();
}

content::DirectSocketsDelegate*
ChromeContentBrowserClient::GetDirectSocketsDelegate() {
  if (!direct_sockets_delegate_) {
    direct_sockets_delegate_ = std::make_unique<ChromeDirectSocketsDelegate>();
  }
  return direct_sockets_delegate_.get();
}

content::WebAuthenticationDelegate*
ChromeContentBrowserClient::GetWebAuthenticationDelegate() {
  if (!web_authentication_delegate_) {
    web_authentication_delegate_ =
        std::make_unique<ChromeWebAuthenticationDelegate>();
  }
  return web_authentication_delegate_.get();
}

std::unique_ptr<content::AuthenticatorRequestClientDelegate>
ChromeContentBrowserClient::GetWebAuthenticationRequestDelegate(
    content::RenderFrameHost* render_frame_host) {
  return AuthenticatorRequestScheduler::CreateRequestDelegate(
      render_frame_host);
}
#endif

void ChromeContentBrowserClient::CreatePaymentCredential(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver) {
  payments::CreatePaymentCredential(render_frame_host, std::move(receiver));
}

std::unique_ptr<net::ClientCertStore>
ChromeContentBrowserClient::CreateClientCertStore(
    content::BrowserContext* browser_context) {
  return ProfileNetworkContextServiceFactory::GetForContext(browser_context)
      ->CreateClientCertStore();
}

std::unique_ptr<content::LoginDelegate>
ChromeContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Negotiate challenge is handled via GSSAPI library, which can not receive
  // external credentials. However, on ChromeOS we can suggest the user to
  // create a TGT using their credentials. Note that the credentials are NOT
  // passed to the browser and everything happens on OS level, hence we return
  // nullptr instead of LoginDelegate to fail authentication. (See b/260522530).
  if (base::FeatureList::IsEnabled(net::features::kKerberosInBrowserRedirect) &&
      auth_info.scheme ==
          net::HttpAuth::SchemeToString(net::HttpAuth::AUTH_SCHEME_NEGOTIATE)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::KerberosInBrowserDialog::Show();
#else
    // Requests to show Kerberos ui via crosapi mojo call.
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::KerberosInBrowser>()
        ->ShowKerberosInBrowserDialog();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* system_proxy_manager = ash::SystemProxyManager::Get();
  // For Managed Guest Session and Kiosk devices, the credentials configured
  // via the policy SystemProxySettings may be used for proxy authentication.
  // Note: |system_proxy_manager| may be missing in tests.
  if (system_proxy_manager && system_proxy_manager->CanUsePolicyCredentials(
                                  auth_info, first_auth_attempt)) {
    return system_proxy_manager->CreateLoginDelegate(
        std::move(auth_required_callback));
  }

  if (ash::HttpAuthDialog::IsEnabled()) {
    return ash::HttpAuthDialog::Create(auth_info, web_contents, url,
                                       std::move(auth_required_callback));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!http_auth_coordinator_) {
    http_auth_coordinator_ = CreateHttpAuthCoordinator();
  }
  // Once Lacros ships this logic will no longer need to be included in
  // ash-chrome.
  return http_auth_coordinator_->CreateLoginDelegate(
      web_contents, browser_context, auth_info, request_id,
      is_request_for_primary_main_frame, is_request_for_navigation, url,
      response_headers, std::move(auth_required_callback));
}

bool ChromeContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // External protocols are disabled for guests. An exception is made for the
  // "mailto" protocol, so that pages that utilize it work properly in a
  // WebView.
  ChromeNavigationUIData* chrome_data =
      static_cast<ChromeNavigationUIData*>(navigation_data);
  if ((chrome_data &&
       chrome_data->GetExtensionNavigationUIData()->is_web_view()) &&
      !url.SchemeIs(url::kMailToScheme)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_ANDROID)
  // Main frame external protocols are handled by
  // InterceptNavigationResourceThrottle.
  if (is_primary_main_frame)
    return false;
#endif  // defined(ANDROID)

  auto weak_initiator_document = initiator_document
                                     ? initiator_document->GetWeakDocumentPtr()
                                     : content::WeakDocumentPtr();

  // On Android, populate the `out_factory` param.
  LaunchURL(weak_factory_.GetWeakPtr(), url, std::move(web_contents_getter),
            page_transition, is_primary_main_frame, is_in_fenced_frame_tree,
            sandbox_flags, has_user_gesture, initiating_origin,
            std::move(weak_initiator_document), out_factory);
  return true;
}

std::unique_ptr<content::VideoOverlayWindow>
ChromeContentBrowserClient::CreateWindowForVideoPictureInPicture(
    content::VideoPictureInPictureWindowController* controller) {
  // Note: content::VideoOverlayWindow::Create() is defined by platform-specific
  // implementation in chrome/browser/ui/views. This layering hack, which goes
  // through //content and ContentBrowserClient, allows us to work around the
  // dependency constraints that disallow directly calling
  // chrome/browser/ui/views code either from here or from other code in
  // chrome/browser.
  return content::VideoOverlayWindow::Create(controller);
}

void ChromeContentBrowserClient::RegisterRendererPreferenceWatcher(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (PrefWatcher* pref_watcher = PrefWatcher::Get(profile))
    pref_watcher->RegisterRendererPreferenceWatcher(std::move(watcher));
}

// Static; handles rewriting Web UI URLs.
bool ChromeContentBrowserClient::HandleWebUI(
    GURL* url,
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);

  // Rewrite chrome://help to chrome://settings/help.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIHelpHost) {
    *url = ReplaceURLHostAndPath(*url, chrome::kChromeUISettingsHost,
                                 chrome::kChromeUIHelpHost);
  }

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  if (base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2) &&
      url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUISettingsHost &&
      url->path() == chrome::kChromeUICertificateRedirectPath) {
    *url = GURL(chrome::kChromeUICertificateManagerDialogURL);
    return true;
  }
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(profile);
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kTrackingProtection3pcdUx) &&
      tracking_protection_settings &&
      tracking_protection_settings->IsTrackingProtection3pcdEnabled()) {
    // Redirect from cookies to trackingProtection in experiment.
    if (url->SchemeIs(content::kChromeUIScheme) &&
        url->host() == chrome::kChromeUISettingsHost &&
        url->path() == chrome::kCookiesSubPagePath) {
      GURL::Replacements replacements;
      replacements.SetPathStr(chrome::kTrackingProtectionSubPagePath);
      *url = url->ReplaceComponents(replacements);
      UMA_HISTOGRAM_BOOLEAN("Settings.TrackingProtection.Redirect", true);
    } else if (url->path() == chrome::kTrackingProtectionSubPagePath) {
      UMA_HISTOGRAM_BOOLEAN("Settings.TrackingProtection.Redirect", false);
    }
  } else {
    // Redirect from trackingProtection to cookies outside experiment.
    if (url->SchemeIs(content::kChromeUIScheme) &&
        url->host() == chrome::kChromeUISettingsHost &&
        url->path() == chrome::kTrackingProtectionSubPagePath) {
      GURL::Replacements replacements;
      replacements.SetPathStr(chrome::kCookiesSubPagePath);
      *url = url->ReplaceComponents(replacements);
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40647483): Remove when issue is resolved.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIWelcomeWin10Host) {
    *url =
        ReplaceURLHostAndPath(*url, chrome::kChromeUIWelcomeHost, url->path());
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (!ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
          browser_context, *url) &&
      !content::WebUIConfigMap::GetInstance().GetConfig(browser_context,
                                                        *url)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Special case : in ChromeOS in Guest mode bookmarks and history are
  // disabled for security reasons. New tab page explains the reasons, so
  // we redirect user to new tab page.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    if (url->SchemeIs(content::kChromeUIScheme) &&
        (url->DomainIs(chrome::kChromeUIBookmarksHost) ||
         url->DomainIs(chrome::kChromeUIHistoryHost))) {
      // Rewrite with new tab URL
      *url = GURL(chrome::kChromeUINewTabURL);
    }
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (IsSystemFeatureURLDisabled(*url)) {
    *url = GURL(chrome::kChromeUIAppDisabledURL);
    return true;
  }
#endif

  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
content::SmartCardDelegate* ChromeContentBrowserClient::GetSmartCardDelegate() {
  if (!smart_card_delegate_) {
    smart_card_delegate_ = std::make_unique<ChromeOsSmartCardDelegate>();
  }
  return smart_card_delegate_.get();
}
#endif

bool ChromeContentBrowserClient::ShowPaymentHandlerWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(bool, int, int)> callback) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  payments::PaymentRequestDisplayManagerFactory::GetInstance()
      ->GetForBrowserContext(browser_context)
      ->ShowPaymentHandlerWindow(url, std::move(callback));
  return true;
#endif
}

// static
bool ChromeContentBrowserClient::HandleWebUIReverse(
    GURL* url,
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40647483): Remove when issue is resolved.
  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://welcome-win10 to chrome://welcome.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIWelcomeHost) {
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://settings/certificates to
  // chrome://certificate-manager
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUICertificateManagerHost) {
    return true;
  }
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://help to chrome://settings/help.
  return url->SchemeIs(content::kChromeUIScheme) &&
         url->host() == chrome::kChromeUISettingsHost;
}

const ui::NativeTheme* ChromeContentBrowserClient::GetWebTheme() const {
  return ui::NativeTheme::GetInstanceForWeb();
}

void ChromeContentBrowserClient::AddExtraPart(
    ChromeContentBrowserClientParts* part) {
  extra_parts_.push_back(base::WrapUnique(part));
}

std::unique_ptr<HttpAuthCoordinator>
ChromeContentBrowserClient::CreateHttpAuthCoordinator() {
  return std::make_unique<HttpAuthCoordinator>();
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate(
    bool safe_browsing_enabled_for_profile,
    bool should_check_on_sb_disabled,
    const std::vector<std::string>& allowlist_domains) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Should not bypass safe browsing check if the check is for enterprise
  // lookup.
  if (!safe_browsing_enabled_for_profile && !should_check_on_sb_disabled)
    return nullptr;

  // |safe_browsing_service_| may be unavailable in tests.
  if (safe_browsing_service_ && !safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ =
        base::MakeRefCounted<safe_browsing::UrlCheckerDelegateImpl>(
            safe_browsing_service_->database_manager(),
            safe_browsing_service_->ui_manager());
  }

  // Update allowlist domains.
  if (safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_->SetPolicyAllowlistDomains(
        allowlist_domains);
  }

  return safe_browsing_url_checker_delegate_;
}

safe_browsing::RealTimeUrlLookupServiceBase*
ChromeContentBrowserClient::GetUrlLookupService(
    content::BrowserContext* browser_context,
    bool is_enterprise_lookup_enabled,
    bool is_consumer_lookup_enabled) {
  // |safe_browsing_service_| may be unavailable in tests.
  if (!safe_browsing_service_) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  if (is_enterprise_lookup_enabled) {
    return safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
        GetForProfile(profile);
  }
#endif

  if (is_consumer_lookup_enabled) {
    return safe_browsing::RealTimeUrlLookupServiceFactory::GetForProfile(
        profile);
  }
  return nullptr;
}

safe_browsing::AsyncCheckTracker*
ChromeContentBrowserClient::GetAsyncCheckTracker(
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    bool is_enterprise_lookup_enabled,
    bool is_consumer_lookup_enabled,
    safe_browsing::hash_realtime_utils::HashRealTimeSelection
        hash_realtime_selection,
    content::FrameTreeNodeId frame_tree_node_id) {
  content::WebContents* contents = wc_getter.Run();
  if (!contents || !safe_browsing_service_ ||
      !safe_browsing_service_->ui_manager()) {
    return nullptr;
  }
  if (is_enterprise_lookup_enabled) {
    // No async checks for enterprise real-time checks. URL filtering rules
    // need to be applied before the navigation is completed.
    return nullptr;
  }
  if (!is_consumer_lookup_enabled &&
      hash_realtime_selection ==
          safe_browsing::hash_realtime_utils::HashRealTimeSelection::kNone) {
    return nullptr;
  }
  if (prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          contents) ||
      contents->IsPrerenderedFrame(frame_tree_node_id)) {
    return nullptr;
  }
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    return nullptr;
  }
  return safe_browsing::AsyncCheckTracker::GetOrCreateForWebContents(
      contents, safe_browsing_service_->ui_manager().get(),
      safe_browsing::AsyncCheckTracker::
          IsPlatformEligibleForSyncCheckerCheckAllowlist());
}

void ChromeContentBrowserClient::ReportLegacyTechEvent(
    content::RenderFrameHost* render_frame_host,
    const std::string& type,
    const GURL& url,
    const GURL& frame_url,
    const std::string& filename,
    uint64_t line,
    uint64_t column,
    std::optional<content::LegacyTechCookieIssueDetails> cookie_issue_details) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }
  enterprise_reporting::LegacyTechService* service =
      enterprise_reporting::LegacyTechServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  service->ReportEvent(type, url, frame_url, filename, line, column,
                       cookie_issue_details);
}

bool ChromeContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  // We require --user-data-dir flag too so that no dangerous changes are made
  // in the user's regular profile.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUserDataDir);
}

void ChromeContentBrowserClient::OnNetworkServiceDataUseUpdate(
    content::GlobalRenderFrameHostId render_frame_host_id,
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
#if !BUILDFLAG(IS_ANDROID)
  task_manager::TaskManagerInterface::UpdateAccumulatedStatsNetworkForRoute(
      render_frame_host_id, recv_bytes, sent_bytes);
#endif
}

base::FilePath
ChromeContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  if (!g_browser_process || !g_browser_process->profile_manager())
    return base::FilePath();
  return g_browser_process->profile_manager()->user_data_dir();
}

bool ChromeContentBrowserClient::ShouldSandboxAudioService() {
  return IsAudioServiceSandboxEnabled();
}

bool ChromeContentBrowserClient::ShouldSandboxNetworkService() {
  return SystemNetworkContextManager::IsNetworkSandboxEnabled();
}

bool ChromeContentBrowserClient::ShouldRunOutOfProcessSystemDnsResolution() {
// This enterprise policy is supported on Android, but the feature will not be
// launched there.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
  // This is possibly called before `g_browser_process` is initialized.
  PrefService* local_state;
  if (g_browser_process) {
    local_state = g_browser_process->local_state();
  } else {
    local_state = startup_data_.chrome_feature_list_creator()->local_state();
  }
  if (local_state && local_state->HasPrefPath(
                         prefs::kOutOfProcessSystemDnsResolutionEnabled)) {
    return local_state->GetBoolean(
        prefs::kOutOfProcessSystemDnsResolutionEnabled);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

  return ContentBrowserClient::ShouldRunOutOfProcessSystemDnsResolution();
}

void ChromeContentBrowserClient::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, feature);
}

void ChromeContentBrowserClient::LogWebDXFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebDXFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, feature);
}

std::string ChromeContentBrowserClient::GetProduct() {
  return std::string(version_info::GetProductNameAndVersionForUserAgent());
}

std::string ChromeContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

std::string ChromeContentBrowserClient::GetUserAgentBasedOnPolicy(
    content::BrowserContext* context) {
  const PrefService* prefs = Profile::FromBrowserContext(context)->GetPrefs();
  embedder_support::UserAgentReductionEnterprisePolicyState
      user_agent_reduction =
          embedder_support::GetUserAgentReductionFromPrefs(prefs);
  return embedder_support::GetUserAgent(user_agent_reduction);
}

blink::UserAgentMetadata ChromeContentBrowserClient::GetUserAgentMetadata() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return embedder_support::GetUserAgentMetadata(
      g_browser_process->local_state());
}

std::optional<gfx::ImageSkia> ChromeContentBrowserClient::GetProductLogo() {
  // This icon is available on Android, but adds 19KiB to the APK. Since it
  // isn't used on Android we exclude it to avoid bloat.
#if !BUILDFLAG(IS_ANDROID)
  return std::optional<gfx::ImageSkia>(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PRODUCT_LOGO_256));
#else
  return std::nullopt;
#endif
}

bool ChromeContentBrowserClient::IsBuiltinComponent(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return ChromeContentBrowserClientExtensionsPart::IsBuiltinComponent(
      browser_context, origin);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldBlockRendererDebugURL(
    const GURL& url,
    content::BrowserContext* context,
    content::RenderFrameHost* render_frame_host) {
#if !BUILDFLAG(IS_ANDROID)
  // If devtools access is blocked for the page, debug URLs should also be
  // blocked for the page.
  Profile* profile = Profile::FromBrowserContext(context);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!DevToolsWindow::AllowDevToolsFor(profile, web_contents)) {
    return true;
  }
#endif

  // If the debug URL being visited is listed in the URLBlocklist policy it
  // should be blocked.
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(context);
  using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
  URLBlocklistState blocklist_state = service->GetURLBlocklistState(url);
  return blocklist_state == URLBlocklistState::URL_IN_BLOCKLIST;
}

#if BUILDFLAG(IS_ANDROID)
content::ContentBrowserClient::WideColorGamutHeuristic
ChromeContentBrowserClient::GetWideColorGamutHeuristic() {
  if (viz::AlwaysUseWideColorGamut() ||
      features::IsDynamicColorGamutEnabled()) {
    return WideColorGamutHeuristic::kUseDisplay;
  }

  if (display::HasForceDisplayColorProfile() &&
      display::GetForcedDisplayColorProfile() ==
          gfx::ColorSpace::CreateDisplayP3D65()) {
    return WideColorGamutHeuristic::kUseDisplay;
  }

  return WideColorGamutHeuristic::kNone;
}
#endif

base::flat_set<std::string>
ChromeContentBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    content::BrowserContext* browser_context) {
  base::flat_set<std::string> mime_types;
#if BUILDFLAG(ENABLE_PLUGINS)
  auto map = PluginUtils::GetMimeTypeToExtensionIdMap(browser_context);
  for (const auto& pair : map)
    mime_types.insert(pair.first);
#endif
#if BUILDFLAG(ENABLE_PDF)
  mime_types.insert(pdf::kInternalPluginMimeType);
#endif
  return mime_types;
}

void ChromeContentBrowserClient::AugmentNavigationDownloadPolicy(
    content::RenderFrameHost* frame_host,
    bool user_gesture,
    blink::NavigationDownloadPolicy* download_policy) {
  const auto* throttle_manager =
      subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
          frame_host->GetPage());
  if (throttle_manager &&
      throttle_manager->IsRenderFrameHostTaggedAsAd(frame_host)) {
    download_policy->SetAllowed(blink::NavigationDownloadType::kAdFrame);
    if (!user_gesture) {
      download_policy->SetDisallowed(
          blink::NavigationDownloadType::kAdFrameNoGesture);
    }
  }
}

bool ChromeContentBrowserClient::HandleTopicsWebApi(
    const url::Origin& context_origin,
    content::RenderFrameHost* main_frame,
    browsing_topics::ApiCallerSource caller_source,
    bool get_topics,
    bool observe,
    std::vector<blink::mojom::EpochTopicPtr>& topics) {
  browsing_topics::BrowsingTopicsService* browsing_topics_service =
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(
              content::WebContents::FromRenderFrameHost(main_frame)
                  ->GetBrowserContext()));

  if (!browsing_topics_service)
    return {};

  bool allowed = browsing_topics_service->HandleTopicsWebApi(
      context_origin, main_frame, caller_source, get_topics, observe, topics);

  if (main_frame) {
    ChromeBrowsingDataModelDelegate::BrowsingDataAccessed(
        main_frame, context_origin,
        ChromeBrowsingDataModelDelegate::StorageType::kTopics, !allowed);
  }

  return allowed;
}

int ChromeContentBrowserClient::NumVersionsInTopicsEpochs(
    content::RenderFrameHost* main_frame) const {
  browsing_topics::BrowsingTopicsService* browsing_topics_service =
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(
              content::WebContents::FromRenderFrameHost(main_frame)
                  ->GetBrowserContext()));

  CHECK(browsing_topics_service);
  return browsing_topics_service->NumVersionsInEpochs(
      main_frame->GetLastCommittedOrigin());
}

bool ChromeContentBrowserClient::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_SCANNING) == CONTENT_SETTING_BLOCK) {
    return true;
  }

  return false;
}

void ChromeContentBrowserClient::BlockBluetoothScanning(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  content_settings->SetContentSettingDefaultScope(
      requesting_origin.GetURL(), embedding_origin.GetURL(),
      ContentSettingsType::BLUETOOTH_SCANNING, CONTENT_SETTING_BLOCK);
}

void ChromeContentBrowserClient::GetMediaDeviceIDSalt(
    content::RenderFrameHost* rfh,
    const net::SiteForCookies& site_for_cookies,
    const blink::StorageKey& storage_key,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  GURL url = rfh->GetLastCommittedURL();
  url::Origin top_frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();
  content::BrowserContext* browser_context = rfh->GetBrowserContext();

  // Persistent MediaDevice IDs are allowed if cookies are allowed or if the
  // user is in the default state in 3PCD.
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  privacy_sandbox::TrackingProtectionSettings* tracking_protection =
      TrackingProtectionSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  bool allowed = cookie_settings->IsFullCookieAccessAllowed(
                     url, site_for_cookies, top_frame_origin,
                     cookie_settings->SettingOverridesForStorage()) ||
                 (tracking_protection->IsTrackingProtection3pcdEnabled() &&
                  !tracking_protection->AreAllThirdPartyCookiesBlocked());
  ChromeBrowsingDataModelDelegate::BrowsingDataAccessed(
      rfh, storage_key,
      ChromeBrowsingDataModelDelegate::StorageType::kMediaDeviceSalt, !allowed);
  media_device_salt::MediaDeviceSaltService* salt_service =
      MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
          browser_context);
  if (!allowed || !salt_service) {
    // Use ephemeral salt.
    std::move(callback).Run(allowed, browser_context->UniqueId());
    return;
  }

  salt_service->GetSalt(rfh->GetStorageKey(),
                        base::BindOnce(std::move(callback), allowed));
}

#if !BUILDFLAG(IS_ANDROID)
base::OnceClosure ChromeContentBrowserClient::FetchRemoteSms(
    content::WebContents* web_contents,
    const std::vector<url::Origin>& origin_list,
    base::OnceCallback<void(std::optional<std::vector<url::Origin>>,
                            std::optional<std::string>,
                            std::optional<content::SmsFetchFailureType>)>
        callback) {
  return ::FetchRemoteSms(web_contents, origin_list, std::move(callback));
}
#endif

bool ChromeContentBrowserClient::IsClipboardPasteAllowed(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  // Paste requires either (1) user activation, ...
  if (WebContents::FromRenderFrameHost(render_frame_host)
          ->HasRecentInteraction()) {
    return true;
  }

  // (2) granted web permission, ...
  content::BrowserContext* browser_context =
      render_frame_host->GetBrowserContext();
  content::PermissionController* permission_controller =
      browser_context->GetPermissionController();
  blink::mojom::PermissionStatus status =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::CLIPBOARD_READ_WRITE, render_frame_host);
  if (status == blink::mojom::PermissionStatus::GRANTED)
    return true;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // (3) origination directly from a Chrome extension, ...
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  const GURL& url =
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin().GetURL();
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    return URLHasExtensionPermission(extensions::ProcessMap::Get(profile),
                                     registry, url,
                                     render_frame_host->GetProcess()->GetID(),
                                     APIPermissionID::kClipboardRead);
  }

  // or (4) origination from a process that at least might be running a
  // content script from an extension with the clipboardRead permission.
  // Note that we currently don't allow clipboard operations based just on user
  // script injections.
  extensions::ExtensionIdSet extension_ids = extensions::
      ScriptInjectionTracker::GetExtensionsThatRanContentScriptsInProcess(
          *render_frame_host->GetProcess());
  for (const auto& extension_id : extension_ids) {
    const Extension* extension =
        registry->enabled_extensions().GetByID(extension_id);
    if (extension && extension->permissions_data()->HasAPIPermission(
                         APIPermissionID::kClipboardRead)) {
      return true;
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  return false;
}

void ChromeContentBrowserClient::IsClipboardPasteAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteAllowedCallback callback) {
// TODO(b/352728209): Add Android-specific hook for Data Controls.
#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS) && !BUILDFLAG(IS_ANDROID)
  enterprise_data_protection::PasteAllowedRequest::StartPasteAllowedRequest(
      source, destination, metadata, std::move(clipboard_paste_data),
      std::move(callback));
#else
  std::move(callback).Run(std::move(clipboard_paste_data));
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
}

void ChromeContentBrowserClient::IsClipboardCopyAllowedByPolicy(
    const content::ClipboardEndpoint& source,
    const content::ClipboardMetadata& metadata,
    const ClipboardPasteData& data,
    IsClipboardCopyAllowedCallback callback) {
// TODO(b/352728209): Add Android-specific hook for Data Controls.
#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS) && !BUILDFLAG(IS_ANDROID)
  enterprise_data_protection::IsClipboardCopyAllowedByPolicy(
      source, metadata, data, std::move(callback));
#else
  std::u16string replacement_data;
  ClipboardRestrictionService* service =
      ClipboardRestrictionServiceFactory::GetInstance()->GetForBrowserContext(
          source.browser_context());
  GURL url = source.data_transfer_endpoint() &&
                     source.data_transfer_endpoint()->IsUrlType() &&
                     source.data_transfer_endpoint()->GetURL()
                 ? *source.data_transfer_endpoint()->GetURL()
                 : GURL();
  if (service->IsUrlAllowedToCopy(std::move(url), metadata.size.value_or(0),
                                  &replacement_data)) {
    std::move(callback).Run(metadata.format_type, data, std::nullopt);
  } else {
    std::move(callback).Run(metadata.format_type, data,
                            std::move(replacement_data));
  }
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS) && !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(ENABLE_VR)
content::XrIntegrationClient*
ChromeContentBrowserClient::GetXrIntegrationClient() {
  if (!xr_integration_client_)
    xr_integration_client_ = std::make_unique<vr::ChromeXrIntegrationClient>(
        base::PassKey<ChromeContentBrowserClient>());
  return xr_integration_client_.get();
}
#endif  // BUILDFLAG(ENABLE_VR)

void ChromeContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService::Get()->BindReceiver(
      chrome::GetVersionString(chrome::WithExtendedStable(true)));
#endif
}

bool ChromeContentBrowserClient::
    ShouldInheritCrossOriginEmbedderPolicyImplicitly(const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return url.SchemeIs(extensions::kExtensionScheme);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::
    ShouldServiceWorkerInheritPolicyContainerFromCreator(const GURL& url) {
  if (url.SchemeIsLocal()) {
    return true;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return url.SchemeIs(extensions::kExtensionScheme);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::
    GrantAdditionalRequestPrivilegesToWorkerProcess(int child_id,
                                                    const GURL& script_url) {
#if !BUILDFLAG(IS_ANDROID)
  // IWA Service Workers need to be explicitly granted access to their origin
  // because isolated-app: isn't a web-safe scheme that can be accessed by
  // default.
  if (script_url.SchemeIs(chrome::kIsolatedAppScheme)) {
    ChildProcessSecurityPolicy::GetInstance()->GrantRequestOrigin(
        child_id, url::Origin::Create(script_url));
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

content::ContentBrowserClient::PrivateNetworkRequestPolicyOverride
ChromeContentBrowserClient::ShouldOverridePrivateNetworkRequestPolicy(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  // The host content settings map might no be null for some irregular profiles,
  // e.g. the System Profile.
  if (HostContentSettingsMap* service =
          HostContentSettingsMapFactory::GetForProfile(browser_context)) {
    if (content_settings::ShouldAllowInsecurePrivateNetworkRequests(service,
                                                                    origin)) {
      return content::ContentBrowserClient::
          PrivateNetworkRequestPolicyOverride::kForceAllow;
    }
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          kPrivateNetworkAccessRestrictionsForAutomotive) &&
      base::android::BuildInfo::GetInstance()->is_automotive()) {
    return content::ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
        kBlockInsteadOfWarn;
  }
#endif

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile->GetPrefs()->GetBoolean(
          prefs::kManagedPrivateNetworkAccessRestrictionsEnabled)) {
    return content::ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
        kBlockInsteadOfWarn;
  }

  return content::ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
      kDefault;
}

bool ChromeContentBrowserClient::IsJitDisabledForSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  // Special case to determine if any policy is set.
  if (map && site_url.is_empty()) {
    return map->GetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_JIT,
                                         nullptr) == CONTENT_SETTING_BLOCK;
  }

  // Only disable JIT for web schemes.
  if (!site_url.SchemeIsHTTPOrHTTPS())
    return false;

  return (map && map->GetContentSetting(site_url, site_url,
                                        ContentSettingsType::JAVASCRIPT_JIT) ==
                     CONTENT_SETTING_BLOCK);
}

bool ChromeContentBrowserClient::AreV8OptimizationsDisabledForSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  // Special case to determine if any policy is set.
  if (map && site_url.is_empty()) {
    return map->GetDefaultContentSetting(
               ContentSettingsType::JAVASCRIPT_OPTIMIZER, nullptr) ==
           CONTENT_SETTING_BLOCK;
  }

  // Only disable optimizations for schemes that might atually load web content.
  auto* policy = ChildProcessSecurityPolicy::GetInstance();
  if (!policy->IsWebSafeScheme(site_url.scheme())) {
    return false;
  }

  return (map &&
          map->GetContentSetting(site_url, site_url,
                                 ContentSettingsType::JAVASCRIPT_OPTIMIZER) ==
              CONTENT_SETTING_BLOCK);
}

ukm::UkmService* ChromeContentBrowserClient::GetUkmService() {
  return g_browser_process->GetMetricsServicesManager()->GetUkmService();
}

blink::mojom::OriginTrialsSettingsPtr
ChromeContentBrowserClient::GetOriginTrialsSettings() {
  return g_browser_process->GetOriginTrialsSettingsStorage()->GetSettings();
}

void ChromeContentBrowserClient::OnKeepaliveRequestStarted(
    content::BrowserContext* context) {
#if !BUILDFLAG(IS_ANDROID)
  DVLOG(1) << "OnKeepaliveRequestStarted: " << num_keepalive_requests_
           << " ==> " << num_keepalive_requests_ + 1;
  ++num_keepalive_requests_;
  DCHECK_GT(num_keepalive_requests_, 0u);

  if (!context) {
    // We somehow failed to associate the request and the BrowserContext. Bail
    // out.
    return;
  }

  const auto now = base::TimeTicks::Now();
  const auto timeout = GetKeepaliveTimerTimeout(context);
  keepalive_deadline_ = std::max(keepalive_deadline_, now + timeout);
  if (keepalive_deadline_ > now && !keepalive_timer_.IsRunning()) {
    if (!KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
      DVLOG(1) << "Starting a keepalive timer(" << timeout.InSecondsF()
               << " seconds)";
      keepalive_timer_.Start(
          FROM_HERE, keepalive_deadline_ - now,
          base::BindOnce(
              &ChromeContentBrowserClient::OnKeepaliveTimerFired,
              weak_factory_.GetWeakPtr(),
              std::make_unique<ScopedKeepAlive>(
                  KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED)));
    } else {
      DVLOG(1) << "Keepalive timer not started as browser is shutting down";
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeContentBrowserClient::OnKeepaliveRequestFinished() {
#if !BUILDFLAG(IS_ANDROID)
  DCHECK_GT(num_keepalive_requests_, 0u);
  DVLOG(1) << "OnKeepaliveRequestFinished: " << num_keepalive_requests_
           << " ==> " << num_keepalive_requests_ - 1;
  --num_keepalive_requests_;
  if (num_keepalive_requests_ == 0) {
    DVLOG(1) << "Stopping the keepalive timer";
    keepalive_timer_.Stop();
    // This deletes the keep alive handle attached to the timer function and
    // unblock the shutdown sequence.
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_MAC)
bool ChromeContentBrowserClient::SetupEmbedderSandboxParameters(
    sandbox::mojom::Sandbox sandbox_type,
    sandbox::SandboxCompiler* compiler) {
  if (sandbox_type == sandbox::mojom::Sandbox::kSpeechRecognition) {
    base::FilePath soda_component_path = speech::GetSodaDirectory();
    CHECK(!soda_component_path.empty());
    CHECK(compiler->SetParameter(sandbox::policy::kParamSodaComponentPath,
                                 soda_component_path.value()));

    base::FilePath soda_language_pack_path =
        speech::GetSodaLanguagePacksDirectory();
    CHECK(!soda_language_pack_path.empty());
    CHECK(compiler->SetParameter(sandbox::policy::kParamSodaLanguagePackPath,
                                 soda_language_pack_path.value()));
    return true;
  } else if (sandbox_type == sandbox::mojom::Sandbox::kScreenAI) {
    // ScreenAI service needs read access to ScreenAI component binary path to
    // load it.
    base::FilePath screen_ai_binary_path =
        screen_ai::ScreenAIInstallState::GetInstance()
            ->get_component_binary_path();
    if (screen_ai_binary_path.empty()) {
      VLOG(1) << "Screen AI component not found.";
      return false;
    }
    return compiler->SetParameter(sandbox::policy::kParamScreenAiComponentPath,
                                  screen_ai_binary_path.value());
  } else if (sandbox_type == sandbox::mojom::Sandbox::kOnDeviceTranslation) {
    auto translatekit_binary_path =
        OnDeviceTranslationServiceController::GetTranslateKitComponentPath();
    if (translatekit_binary_path.empty()) {
      VLOG(1) << "TranslationKit component not found.";
      return false;
    }
    return compiler->SetParameter(
        sandbox::policy::kParamTranslatekitComponentPath,
        translatekit_binary_path.value());
  }

  return false;
}

#endif  // BUILDFLAG(IS_MAC)

void ChromeContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
#if BUILDFLAG(USE_MINIKIN_HYPHENATION) && !BUILDFLAG(IS_ANDROID)
  // Chrome for Testing bundles hyphenation dictionaries however, if the
  // component updater is disabled (which is commonly the case with test
  // automation frameworks), the hyphenation dictionaries directory is never
  // initialized. To work around this, provide the well known hyphenation
  // directory here bypassing the disabled component installer.
#if BUILDFLAG(CHROME_FOR_TESTING)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableComponentUpdate)) {
    base::FilePath dir;
    if (base::PathService::Get(base::DIR_EXE, &dir)) {
      dir = dir.AppendASCII("hyphen-data");
      std::move(callback).Run(dir);
      return;
    }
  }
#endif  // BUILDFLAG(CHROME_FOR_TESTING)

  component_updater::HyphenationComponentInstallerPolicy::
      GetHyphenationDictionary(std::move(callback));
#endif
}

bool ChromeContentBrowserClient::HasErrorPage(int http_status_code) {
  // Use an internal error page, if we have one for the status code.
  return error_page::LocalizedError::HasStrings(
      error_page::Error::kHttpErrorDomain, http_status_code);
}

std::unique_ptr<content::IdentityRequestDialogController>
ChromeContentBrowserClient::CreateIdentityRequestDialogController(
    content::WebContents* web_contents) {
  return std::make_unique<IdentityDialogController>(web_contents);
}

std::unique_ptr<content::DigitalIdentityProvider>
ChromeContentBrowserClient::CreateDigitalIdentityProvider() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DigitalIdentityProviderAndroid>();
#else
  return std::make_unique<DigitalIdentityProviderDesktop>();
#endif
}

bool ChromeContentBrowserClient::SuppressDifferentOriginSubframeJSDialogs(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile->GetPrefs()->GetBoolean(
          prefs::kSuppressDifferentOriginSubframeJSDialogs)) {
    return false;
  }
  return ContentBrowserClient::SuppressDifferentOriginSubframeJSDialogs(
      browser_context);
}

bool ChromeContentBrowserClient::IsFindInPageDisabledForOrigin(
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_PDF)
  // For PDF viewing with the PPAPI-free PDF Viewer, find-in-page should only
  // display results from the PDF content, and not from the UI.
  return IsPdfExtensionOrigin(origin);
#else
  return false;
#endif
}

std::unique_ptr<content::AnchorElementPreconnectDelegate>
ChromeContentBrowserClient::CreateAnchorElementPreconnectDelegate(
    content::RenderFrameHost& render_frame_host) {
  return std::make_unique<AnchorElementPreloader>(render_frame_host);
}

std::unique_ptr<content::SpeculationHostDelegate>
ChromeContentBrowserClient::CreateSpeculationHostDelegate(
    content::RenderFrameHost& render_frame_host) {
  return std::make_unique<ChromeSpeculationHostDelegate>(render_frame_host);
}

std::unique_ptr<content::PrefetchServiceDelegate>
ChromeContentBrowserClient::CreatePrefetchServiceDelegate(
    content::BrowserContext* browser_context) {
  return std::make_unique<ChromePrefetchServiceDelegate>(browser_context);
}

std::unique_ptr<content::PrerenderWebContentsDelegate>
ChromeContentBrowserClient::CreatePrerenderWebContentsDelegate() {
  return std::make_unique<PrerenderWebContentsDelegateImpl>();
}

void ChromeContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  // NOTE: Please don't add additional code to this method - attaching universal
  // WebContentsObservers goes through the separate function, to ensure that the
  // (rare) additions of universal helpers are code reviewed by separate OWNERS.
  AttachUniversalWebContentsObservers(web_contents);
}

#if !BUILDFLAG(IS_ANDROID)
base::TimeDelta ChromeContentBrowserClient::GetKeepaliveTimerTimeout(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return base::TimeDelta();
  }

  const int seconds =
      prefs->GetInteger(prefs::kFetchKeepaliveDurationOnShutdown);
  // The preference is set only be the corresponding enterprise policy, and
  // we have minimum/maximum values on it.
  DCHECK_LE(0, seconds);
  DCHECK_LE(seconds, 5);
  return base::Seconds(seconds);
}

void ChromeContentBrowserClient::OnKeepaliveTimerFired(
    std::unique_ptr<ScopedKeepAlive> keep_alive_handle) {
  const auto now = base::TimeTicks::Now();
  const auto then = keepalive_deadline_;
  if (now < then) {
    keepalive_timer_.Start(
        FROM_HERE, then - now,
        base::BindOnce(&ChromeContentBrowserClient::OnKeepaliveTimerFired,
                       weak_factory_.GetWeakPtr(),
                       std::move(keep_alive_handle)));
  }
}
#endif

bool ChromeContentBrowserClient::ShouldPreconnectNavigation(
    content::RenderFrameHost* render_frame_host) {
  content::BrowserContext* browser_context =
      render_frame_host->GetBrowserContext();
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // An extension could be blocking connections for privacy reasons, so skip
  // optimization if there are any extensions with WebRequest permissions.
  const auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  if (!web_request_api || web_request_api->MayHaveProxies() ||
      web_request_api->IsAvailableToWebViewEmbedderFrame(render_frame_host)) {
    return false;
  }
#endif
  return prefetch::IsSomePreloadingEnabled(
             *Profile::FromBrowserContext(browser_context)->GetPrefs()) ==
         content::PreloadingEligibility::kEligible;
}

bool ChromeContentBrowserClient::ShouldDisableOriginAgentClusterDefault(
    content::BrowserContext* browser_context) {
  // The enterprise policy for kOriginAgentClusterDefaultEnabled defaults to
  // true to defer to Chromium's decision. If it is set to false, it should
  // override Chromium's decision and use site-keyed agent clusters by default
  // instead.
  return !Profile::FromBrowserContext(browser_context)
              ->GetPrefs()
              ->GetBoolean(prefs::kOriginAgentClusterDefaultEnabled);
}

bool ChromeContentBrowserClient::WillProvidePublicFirstPartySets() {
  return !is_minimal_mode_ &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableComponentUpdate);
}

content::mojom::AlternativeErrorPageOverrideInfoPtr
ChromeContentBrowserClient::GetAlternativeErrorPageOverrideInfo(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context,
    int32_t error_code) {
#if !BUILDFLAG(IS_ANDROID)
  if (content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
          browser_context) &&
      url.SchemeIs(chrome::kIsolatedAppScheme)) {
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_override_info =
            web_app::MaybeGetIsolatedWebAppErrorPageInfo(
                url, render_frame_host, browser_context, error_code);
    if (alternative_error_page_override_info) {
      alternative_error_page_override_info->alternative_error_page_params.Set(
          error_page::kOverrideErrorPage, base::Value(true));
      return alternative_error_page_override_info;
    }
  }
#endif

  if (error_code == net::ERR_INTERNET_DISCONNECTED) {
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_override_info = web_app::GetOfflinePageInfo(
            url, render_frame_host, browser_context);
    if (alternative_error_page_override_info) {
      // Use the alternative error page dictionary to override the error page.
      alternative_error_page_override_info->alternative_error_page_params.Set(
          error_page::kOverrideErrorPage, base::Value(true));
      web_app::TrackOfflinePageVisibility(render_frame_host);
      return alternative_error_page_override_info;
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  using PortalState = chromeos::network_config::mojom::PortalState;
  auto portal_state = ash::network_health::NetworkHealthManager::GetInstance()
                          ->helper()
                          ->WiFiPortalState();
  if (portal_state != PortalState::kUnknown) {
    auto alternative_error_page_override_info =
        content::mojom::AlternativeErrorPageOverrideInfo::New();
    bool is_portal_state = portal_state == PortalState::kPortal ||
                           portal_state == PortalState::kPortalSuspected;
    // Use the alternative error page dictionary to provide additional
    // suggestions in the default error page.
    alternative_error_page_override_info->alternative_error_page_params.Set(
        error_page::kOverrideErrorPage, base::Value(false));
    alternative_error_page_override_info->alternative_error_page_params.Set(
        error_page::kIsPortalStateKey, base::Value(is_portal_state));
    return alternative_error_page_override_info;
  }
#endif

  return nullptr;
}

void ChromeContentBrowserClient::OnSharedStorageWorkletHostCreated(
    content::RenderFrameHost* rfh) {
  if (auto* observer =
          page_load_metrics::MetricsWebContentsObserver::FromWebContents(
              WebContents::FromRenderFrameHost(rfh))) {
    observer->OnSharedStorageWorkletHostCreated(rfh);
  }
}

void ChromeContentBrowserClient::OnSharedStorageSelectURLCalled(
    content::RenderFrameHost* main_rfh) {
  if (auto* observer =
          page_load_metrics::MetricsWebContentsObserver::FromWebContents(
              WebContents::FromRenderFrameHost(main_rfh))) {
    observer->OnSharedStorageSelectURLCalled(main_rfh);
  }
}

bool ChromeContentBrowserClient::ShouldSendOutermostOriginToRenderer(
    const url::Origin& outermost_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // We only want to send the outermost origin if it is an extension scheme.
  // We do not send the outermost origin to every renderer to avoid leaking
  // additional information into the renderer about the embedder. For
  // extensions though this is required for the way content injection API
  // works. We do not want one extension injecting content into the context
  // of another extension.
  return outermost_origin.scheme() == extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsFileSystemURLNavigationAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  // filesystem: URLs for Chrome Apps are in the following format:
  // `filesystem:chrome-extension://<extension-id>/...`
  if (!url.SchemeIsFileSystem())
    return false;
  // Once converted into an origin, we expect the following:
  // scheme() is chrome-extension: (filesystem: is automatically discarded)
  // host() is the extension-id
  const url::Origin origin = url::Origin::Create(url);
  if (origin.scheme() == extensions::kExtensionScheme) {
    const Extension* extension =
        extensions::ExtensionRegistry::Get(browser_context)
            ->enabled_extensions()
            .GetByID(origin.host());
    DCHECK(extension);
    return extension->is_platform_app();
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)
  return false;
}

bool ChromeContentBrowserClient::AreIsolatedWebAppsEnabled(
    content::BrowserContext* browser_context) {
#if !BUILDFLAG(IS_ANDROID)
  return ChromeContentBrowserClientIsolatedWebAppsPart::
      AreIsolatedWebAppsEnabled(browser_context);
#else  // BUILDFLAG(IS_ANDROID)
  return false;
#endif
}

bool ChromeContentBrowserClient::IsThirdPartyStoragePartitioningAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_level_origin) {
  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!content_settings) {
    // We fail permissive as this function is used to check whether partitioning
    // should be blocked, but isn't the final word on if it's allowed.
    return true;
  }
  return content_settings->GetContentSetting(
             top_level_origin.GetURL(), top_level_origin.GetURL(),
             ContentSettingsType::THIRD_PARTY_STORAGE_PARTITIONING) ==
         CONTENT_SETTING_ALLOW;
}

bool ChromeContentBrowserClient::AreDeprecatedAutomaticBeaconCredentialsAllowed(
    content::BrowserContext* browser_context,
    const GURL& destination_url,
    const url::Origin& top_frame_origin) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  return cookie_settings->IsFullCookieAccessAllowed(
      destination_url, net::SiteForCookies(), top_frame_origin,
      cookie_settings->SettingOverridesForStorage());
}

bool ChromeContentBrowserClient::
    IsTransientActivationRequiredForShowFileOrDirectoryPicker(
        content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  return IsFileOrDirectoryPickerWithoutGestureAllowed(web_contents);
#else   // !BUILDFLAG(IS_ANDROID)
  return true;
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_MAC)
std::string ChromeContentBrowserClient::GetChildProcessSuffix(int child_flags) {
  if (child_flags == chrome::kChildProcessHelperAlerts) {
    return chrome::kMacHelperSuffixAlerts;
  }
  NOTREACHED_IN_MIGRATION() << "Unsupported child process flags!";
  return {};
}
#endif  // BUILDFLAG(IS_MAC)

bool ChromeContentBrowserClient::ShouldUseFirstPartyStorageKey(
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return origin.scheme() == extensions::kExtensionScheme;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

std::unique_ptr<content::ResponsivenessCalculatorDelegate>
ChromeContentBrowserClient::CreateResponsivenessCalculatorDelegate() {
#if !BUILDFLAG(IS_ANDROID)
  return ChromeResponsivenessCalculatorDelegate::Create();
#else
  return nullptr;
#endif
}

// static
bool ChromeContentBrowserClient::DoesGaiaOriginRequireDedicatedProcess() {
#if !BUILDFLAG(IS_ANDROID)
  return true;
#else
  // Sign-in process isolation is not strictly needed on Android, see
  // https://crbug.com/739418. On Android, it's more optional but it does
  // improve security generally and specifically it allows the exposure of
  // certain optional privileged APIs.

  // Kill switch that falls back to the legacy behavior.
  if (!base::FeatureList::IsEnabled(kAllowGaiaOriginIsolationOnAndroid)) {
    return false;
  }

  if (site_isolation::SiteIsolationPolicy::
          ShouldDisableSiteIsolationDueToMemoryThreshold(
              content::SiteIsolationMode::kPartialSiteIsolation)) {
    // Insufficient memory to isolate Gaia's origin.
    return false;
  }

  return true;
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool ChromeContentBrowserClient::CanBackForwardCachedPageReceiveCookieChanges(
    content::BrowserContext& browser_context,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& top_frame_origin,
    const net::CookieSettingOverrides overrides) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(&browser_context));
  CHECK(cookie_settings);
  return cookie_settings->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin, overrides);
}

void ChromeContentBrowserClient::GetCloudIdentifiers(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    GetCloudIdentifiersCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  cloud_identifier::GetCloudIdentifierFromAsh(url, handle_type,
                                              std::move(callback));
#else   // BUILDFLAG(IS_CHROMEOS)
  return ContentBrowserClient::GetCloudIdentifiers(url, handle_type,
                                                   std::move(callback));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool ChromeContentBrowserClient::
    ShouldAllowBackForwardCacheForCacheControlNoStorePage(
        content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS)
  // Do not store CCNS page into BFCache in the kiosk session.
  if (chromeos::IsKioskSession()) {
    return false;
  }
#endif

  if (IsRunningInAppMode()) {
    return false;
  }

  const PrefService::Preference* pref =
      Profile::FromBrowserContext(browser_context)
          ->GetPrefs()
          ->FindPreference(
              policy::policy_prefs::
                  kAllowBackForwardCacheForCacheControlNoStorePageEnabled);
  if (pref && pref->IsManaged() && pref->GetValue()->is_bool()) {
    return pref->GetValue()->GetBool();
  }
  // If the pref is not found or not managed, BFCaching CCNS page should be
  // enabled by default.
  return true;
}

void ChromeContentBrowserClient::SetIsMinimalMode(bool minimal) {
  is_minimal_mode_ = minimal;
}

bool ChromeContentBrowserClient::
    UseOutermostMainFrameOrEmbedderForSubCaptureTargets() const {
#if BUILDFLAG(PLATFORM_CFM)
  return true;
#else
  return false;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeContentBrowserClient::BindVideoEffectsManager(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<media::mojom::VideoEffectsManager>
        video_effects_manager) {
  media_effects::BindVideoEffectsManager(device_id, browser_context,
                                         std::move(video_effects_manager));
}

void ChromeContentBrowserClient::BindVideoEffectsProcessor(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
  media_effects::BindVideoEffectsProcessor(device_id, browser_context,
                                           std::move(video_effects_processor));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ChromeContentBrowserClient::PreferenceRankAudioDeviceInfos(
    content::BrowserContext* browser_context,
    blink::WebMediaDeviceInfoArray& infos) {
  if (!user_prefs::UserPrefs::IsInitialized(browser_context)) {
    return;
  }
  auto* prefs = user_prefs::UserPrefs::Get(browser_context);
  CHECK(prefs);
  media_prefs::PreferenceRankAudioDeviceInfos(*prefs, infos);
}

void ChromeContentBrowserClient::PreferenceRankVideoDeviceInfos(
    content::BrowserContext* browser_context,
    blink::WebMediaDeviceInfoArray& infos) {
  if (!user_prefs::UserPrefs::IsInitialized(browser_context)) {
    return;
  }
  auto* prefs = user_prefs::UserPrefs::Get(browser_context);
  CHECK(prefs);
  media_prefs::PreferenceRankVideoDeviceInfos(*prefs, infos);
}

network::mojom::IpProtectionProxyBypassPolicy
ChromeContentBrowserClient::GetIpProtectionProxyBypassPolicy() {
  return network::mojom::IpProtectionProxyBypassPolicy::
      kFirstPartyToTopLevelFrame;
}

void ChromeContentBrowserClient::MaybePrewarmHttpDiskCache(
    content::BrowserContext& browser_context,
    const std::optional<url::Origin>& initiator_origin,
    const GURL& navigation_url) {
  Profile* profile = Profile::FromBrowserContext(&browser_context);
  CHECK(profile);

  // `loading_predictor` can be nullptr if the profile `IsOffTheRecord`.
  if (predictors::LoadingPredictor* loading_predictor =
          predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    loading_predictor->MaybePrewarmResources(initiator_origin, navigation_url);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void ChromeContentBrowserClient::NotifyMultiCaptureStateChanged(
    content::GlobalRenderFrameHostId capturer_rfh_id,
    const std::string& label,
    MultiCaptureChanged state) {
  switch (state) {
    case MultiCaptureChanged::kStarted: {
      content::WebContents* const web_contents =
          WebContents::FromRenderFrameHost(
              RenderFrameHost::FromID(capturer_rfh_id));
      if (!web_contents) {
        return;
      }
      NotifyMultiCaptureStarted(
          label, web_contents,
          web_app::WebAppTabHelper::GetAppId(web_contents));
    } break;
    case MultiCaptureChanged::kStopped:
      NotifyMultiCaptureStopped(label);
      break;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<content::DipsDelegate>
ChromeContentBrowserClient::CreateDipsDelegate() {
  return ChromeDipsDelegate::Create();
}

bool ChromeContentBrowserClient::ShouldSuppressAXLoadComplete(
    RenderFrameHost* rfh) {
  CHECK(rfh);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);

  const GURL& url = web_contents->GetVisibleURL();
  return url == GURL(chrome::kChromeUINewTabURL) ||
         url == GURL(chrome::kChromeUINewTabPageURL);
}

void ChromeContentBrowserClient::BindAIManager(
    content::BrowserContext* browser_context,
    std::variant<content::RenderFrameHost*, base::SupportsUserData*> context,
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  auto* ai_manager =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(browser_context);
  ai_manager->AddReceiver(std::move(receiver), context);
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeContentBrowserClient::QueryInstalledWebAppsByManifestId(
    const GURL& frame_url,
    const GURL& manifest_id,
    content::BrowserContext* browser_context,
    base::OnceCallback<void(std::optional<blink::mojom::RelatedApplication>)>
        callback) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  web_app::WebAppProvider* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);

  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);

  if (app_id.empty()) {
    return std::move(callback).Run(std::nullopt);
  }

  // arg_for_shutdown must be explicitly defined, otherwise
  // ScheduleCallbackWithResult cannot infer the optional type the nullopt
  // is associated with.
  std::optional<blink::mojom::RelatedApplication> arg_for_shutdown =
      std::nullopt;
  web_app::AppLockDescription lock_description(app_id);

  provider->scheduler().ScheduleCallbackWithResult<web_app::AppLock>(
      "QueryInstalledWebAppsByManifestId", std::move(lock_description),
      base::BindOnce(
          [](webapps::AppId app_id, webapps::ManifestId manifest_id,
             GURL frame_url, web_app::AppLock& lock,
             base::Value::Dict& debug_value)
              -> std::optional<blink::mojom::RelatedApplication> {
            debug_value.Set("input", base::Value::Dict()
                                         .Set("manifest_id", manifest_id.spec())
                                         .Set("frame_url", frame_url.spec()));

            if (!lock.registrar().IsInstallState(
                    app_id, {web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                             web_app::proto::INSTALLED_WITH_OS_INTEGRATION})) {
              debug_value.Set("did_find_application", false);
              return std::nullopt;
            }

            if (!lock.registrar().IsUrlInAppScope(frame_url, app_id)) {
              debug_value.Set("did_find_application", false);
              return std::nullopt;
            }

            blink::mojom::RelatedApplication application;
            application.platform = "webapp";
            application.id = lock.registrar().GetAppManifestId(app_id).spec();
            // Note: This url is the manifest_url for purely legacy reasons
            // where Android used to implement the unique identifier using the
            // manifest url.
            if (lock.registrar().GetAppManifestUrl(app_id).is_valid()) {
              application.url =
                  lock.registrar().GetAppManifestUrl(app_id).spec();
            }

            debug_value.Set("did_find_application", true);
            debug_value.Set(
                "application",
                base::Value::Dict()
                    .Set("app_id", application.id.value_or(""))
                    .Set("manifest_url", application.url.value_or("")));
            return application;
          },
          std::move(app_id), std::move(manifest_id), std::move(frame_url)),
      std::move(callback), std::move(arg_for_shutdown));
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool ChromeContentBrowserClient::IsSaveableNavigation(
    content::NavigationHandle* navigation_handle) {
  return tab_groups::TabGroupSyncUtils::IsSaveableNavigation(navigation_handle);
}

#if BUILDFLAG(IS_WIN)
void ChromeContentBrowserClient::OnUiaProviderRequested(
    bool uia_provider_enabled) {
  if (handled_uia_provider_request_) {
    return;
  }
  handled_uia_provider_request_ = true;

  if (features::kUiaProvider.default_state ==
      base::FEATURE_ENABLED_BY_DEFAULT) {
    // Do nothing if the feature has launched.
    // TODO: Remove all code relating to this synthetic field trial.
    return;
  }

  // The "Control_NNNN" and "Enabled_NNNN" groups in the UiaProviderWin study
  // are equal-sized arms for which the UiaProvider feature is disabled and
  // enabled, respectively. (The feature may also be disabled in other groups,
  // such as "Default_NNNN" or preperiod groups.) Analyzing data from users in
  // these two groups alone does not provide an accurate picture of the impact
  // of the feature, because the browser must check whether or not the feature
  // is enable during startup regardless of whether or not a UI automation
  // client connects to the browser. Filtering data by whether or not
  // accessibility is enabled is also insufficient, as this will include
  // browsers for which an MSAA/IAccessible2 client connects. To measure the
  // impact of the UiaProvider feature, we only want to consider clients where a
  // UI automation client connected and said connection was either refused
  // because the client is in the control group, or was accepted because the
  // client is in the enabled group. We do this by enrolling the client in one
  // of two groups of a synthetic field trial in only these two situations.
  if (auto* trial = base::FeatureList::GetFieldTrial(features::kUiaProvider)) {
    static constexpr std::string_view kControl = "Control";
    static constexpr std::string_view kEnabled = "Enabled";
    const auto& trial_group_name = trial->GetGroupNameWithoutActivation();
    std::string_view group_name;

    if (base::StartsWith(trial_group_name, "Control")) {
      group_name = kControl;
    } else if (base::StartsWith(trial_group_name, "Enabled")) {
      group_name = kEnabled;
    }
    if (!group_name.empty()) {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          "UiaProviderActiveSynthetic", group_name,
          variations::SyntheticTrialAnnotationMode::kCurrentLog);
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

void ChromeContentBrowserClient::SetSamplingProfiler(
    std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler) {
  sampling_profiler_ = std::move(sampling_profiler);
}

base::ReadOnlySharedMemoryRegion
ChromeContentBrowserClient::GetPerformanceScenarioRegionForProcess(
    content::RenderProcessHost* process_host) {
  return performance_manager::GetSharedScenarioRegionForProcess(process_host);
}

base::ReadOnlySharedMemoryRegion
ChromeContentBrowserClient::GetGlobalPerformanceScenarioRegion() {
  return performance_manager::GetGlobalSharedScenarioRegion();
}
