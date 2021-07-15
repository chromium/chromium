// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/character_encoding.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/chrome_content_browser_client_binder_policies.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/device_api/managed_configuration_service.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/federated_learning/floc_eligibility_observer.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/font_access/chrome_font_access_delegate.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/gpu/chrome_browser_main_extra_parts_gpu.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/media/audio_service_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/audio_debug_recordings_handler.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/browser/memory/chrome_browser_main_extra_parts_memory.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/payments/payment_request_display_manager_factory.h"
#include "chrome/browser/performance_manager/chrome_browser_main_extra_parts_performance_manager.h"
#include "chrome/browser/performance_manager/chrome_content_browser_client_performance_manager_part.h"
#include "chrome/browser/performance_monitor/chrome_browser_main_extra_parts_performance_monitor.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_navigation_throttle.h"
#include "chrome/browser/prefetch/prefetch_proxy/chrome_speculation_host_delegate.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_url_loader_interceptor.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader_interceptor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/profiling_host/chrome_browser_main_extra_parts_profiling.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/delayed_warning_navigation_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"
#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"
#include "chrome/browser/signin/chrome_signin_url_loader_throttle.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_defaulted_callbacks.h"
#include "chrome/browser/ssl/https_only_mode_navigation_throttle.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_interceptor.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/ssl/ssl_client_auth_metrics.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_navigation_throttle.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/usb/frame_usb_services.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/private_network_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/embedder_support/content_settings_utils.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/error_page/common/localized_error.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/google/core/common/google_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/caption_util.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/media_router/browser/presentation/receiver_presentation_service_delegate_impl.h"
#include "components/metrics/client_info.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/net_log/chrome_net_log.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"
#include "components/no_state_prefetch/common/prerender_final_status.h"
#include "components/no_state_prefetch/common/prerender_url_loader_throttle.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/payments/content/payment_handler_navigation_throttle.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/quota_permission_context_impl.h"
#include "components/policy/content/policy_blocklist_navigation_throttle.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"
#include "components/security_interstitials/content/origin_policy_ui.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/pref_names.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/url_fixer.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/font_access_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "gpu/config/gpu_switches.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "services/strings/grit/services_strings.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/user_agent/user_agent_metadata.mojom.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "base/strings/string_tokenizer.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "sandbox/win/src/sandbox_policy.h"
#elif defined(OS_MAC)
#include "chrome/browser/chrome_browser_main_mac.h"
#include "chrome/browser/mac/auth_session_request.h"
#include "components/soda/constants.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/params.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/scanning/url_constants.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_backend_delegate.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_backend_delegate.h"
#include "chrome/browser/ash/drive/fileapi/drivefs_file_system_backend_delegate.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_system_provider/fileapi/backend_delegate.h"
#include "chrome/browser/ash/login/signin/merge_session_navigation_throttle.h"
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/handlers/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ash/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/fileapi/smbfs_file_system_backend_delegate.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/chrome_content_browser_client_chromeos_part.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_loader_factory.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/fileapi/mtp_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/net/system_proxy_manager.h"
#include "chrome/browser/speech/tts_chromeos.h"
#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/service_manager/public/mojom/interface_provider_spec.mojom.h"
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chrome_browser_main_linux.h"
#elif defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#include "base/feature_list.h"
#include "chrome/android/features/dev_ui/buildflags.h"
#include "chrome/android/modules/extra_icu/provider/module_provider.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"
#include "chrome/browser/android/service_tab_launcher.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/chrome_browser_main_android.h"
#include "chrome/browser/download/android/available_offline_content_provider.h"
#include "chrome/browser/download/android/intercept_oma_download_navigation_throttle.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_descriptors.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/android/java_interfaces.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#if BUILDFLAG(DFMIFY_DEV_UI)
#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"
#endif  // BUILDFLAG(DFMIFY_DEV_UI)
#elif defined(OS_POSIX)
#include "chrome/browser/chrome_browser_main_posix.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/accessibility/accessibility_features.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/serial/chrome_serial_delegate.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/new_tab_page_navigation_throttle.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/grit/chrome_unscaled_resources.h"  // nogncheck crbug.com/1125897
#endif  //  !defined(OS_ANDROID)

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MAC)
#if !defined(OS_ANDROID)
#include "base/debug/leak_annotations.h"
#include "components/crash/core/app/breakpad_linux.h"
#endif  // !defined(OS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

// TODO(crbug/1169547) Remove `BUILDFLAG(IS_CHROMEOS_LACROS)` once the
// migration is complete.
#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"
#endif  // defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN)

// TODO(crbug.com/939205):  Once the upcoming App Service is available, use a
// single navigation throttle to display the intent picker on all platforms.
#if !defined(OS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/apps/intent_helper/common_apps_navigation_throttle.h"
#else
#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#endif
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11) || defined(USE_OZONE)
#include "chrome/browser/chrome_browser_main_extra_parts_ozone.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "components/captive_portal/content/captive_portal_url_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/platform_apps/platform_app_navigation_redirector.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/switches.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"
#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"
#include "chrome/common/pdf_util.h"
#include "components/pdf/browser/pdf_navigation_throttle.h"
#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"
#include "components/pdf/common/internal_plugin_helpers.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_limit_navigation_throttle.h"
#include "chrome/browser/speech/tts_controller_delegate_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "chrome/browser/media/cast_remoting_connector.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
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

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"  // nogncheck crbug.com/1125897
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/chrome_content_browser_client_tab_strip_part.h"
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
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_lacros.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "ui/base/ui_base_switches.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION) && !defined(OS_ANDROID)
#include "chrome/browser/component_updater/hyphenation_component_installer.h"
#endif

using base::FileDescriptor;
using blink::mojom::EffectiveConnectionType;
using blink::web_pref::WebPreferences;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::BrowsingDataFilterBuilder;
using content::ChildProcessSecurityPolicy;
using content::QuotaPermissionContext;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using message_center::NotifierId;

#if defined(OS_POSIX)
using content::PosixFileDescriptorInfo;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::APIPermission;
using extensions::ChromeContentBrowserClientExtensionsPart;
using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::APIPermissionID;
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
using plugins::ChromeContentBrowserClientPluginsPart;
#endif

namespace {

#if defined(OS_WIN) && !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
// Enables pre-launch Code Integrity Guard (CIG) for Chrome renderers, when
// running on Windows 10 1511 and above. See
// https://blogs.windows.com/blog/tag/code-integrity-guard/.
const base::Feature kRendererCodeIntegrity{"RendererCodeIntegrity",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
// Enables pre-launch Code Integrity Guard (CIG) for Chrome network service
// process, when running on Windows 10 1511 and above. This has no effect if
// NetworkServiceSandbox feature is disabled. See
// https://blogs.windows.com/blog/tag/code-integrity-guard/.
const base::Feature kNetworkServiceCodeIntegrity{
    "NetworkServiceCodeIntegrity", base::FEATURE_DISABLED_BY_DEFAULT};

#endif  // defined(OS_WIN) && !defined(COMPONENT_BUILD) &&
        // !defined(ADDRESS_SANITIZER)

bool IsSSLErrorOverrideAllowedForOrigin(const GURL& request_url,
                                        PrefService* prefs) {
  DCHECK(request_url.SchemeIsCryptographic());

  if (prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed))
    return true;

  if (!prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins))
    return false;

  base::Value::ConstListView allow_list_urls =
      prefs->GetList(prefs::kSSLErrorOverrideAllowedForOrigins)->GetList();
  if (allow_list_urls.empty())
    return false;

  for (auto const& value : allow_list_urls) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern == ContentSettingsPattern::Wildcard() || !pattern.IsValid())
      continue;

    // Despite |request_url| being a GURL, the path is ignored when matching.
    if (pattern.Matches(request_url))
      return true;
  }

  return false;
}

// Wrapper for SSLErrorHandler::HandleSSLError() that supplies //chrome-level
// parameters.
void HandleSSLErrorWrapper(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Profile should always outlive a WebContents
  DCHECK(profile);

  captive_portal::CaptivePortalService* captive_portal_service = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service = CaptivePortalServiceFactory::GetForProfile(profile);
#endif

  SSLErrorHandler::HandleSSLError(
      web_contents, cert_error, ssl_info, request_url,
      std::move(ssl_cert_reporter), std::move(blocking_page_ready_callback),
      g_browser_process->network_time_tracker(), captive_portal_service,
      std::make_unique<ChromeSecurityBlockingPageFactory>(),
      IsSSLErrorOverrideAllowedForOrigin(request_url, profile->GetPrefs()));
}

enum AppLoadedInTabSource {
  // A platform app page tried to load one of its own URLs in a tab.
  APP_LOADED_IN_TAB_SOURCE_APP = 0,

  // A platform app background page tried to load one of its own URLs in a tab.
  APP_LOADED_IN_TAB_SOURCE_BACKGROUND_PAGE,

  // An extension or app tried to load a resource of a different platform app in
  // a tab.
  APP_LOADED_IN_TAB_SOURCE_OTHER_EXTENSION,

  // A non-app and non-extension page tried to load a platform app in a tab.
  APP_LOADED_IN_TAB_SOURCE_OTHER,

  APP_LOADED_IN_TAB_SOURCE_MAX
};

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
  url::Replacements<char> replacements;
  replacements.SetHost(host.c_str(), url::Component(0, host.length()));
  replacements.SetPath(path.c_str(), url::Component(0, path.length()));
  return url.ReplaceComponents(replacements);
}

// Handles the rewriting of the new tab page URL based on group policy.
bool HandleNewTabPageLocationOverride(
    GURL* url,
    content::BrowserContext* browser_context) {
  if (!url->SchemeIs(content::kChromeUIScheme) ||
      url->host() != chrome::kChromeUINewTabHost)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
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

#if !defined(OS_ANDROID)
// Check if the current url is allowlisted based on a list of allowlisted urls.
bool IsURLAllowlisted(const GURL& current_url,
                      base::Value::ConstListView allowlisted_urls) {
  // Only check on HTTP and HTTPS pages.
  if (!current_url.SchemeIsHTTPOrHTTPS())
    return false;

  for (auto const& value : allowlisted_urls) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern == ContentSettingsPattern::Wildcard() || !pattern.IsValid())
      continue;
    if (pattern.Matches(current_url))
      return true;
  }

  return false;
}

// Check if autoplay is allowed by policy configuration.
bool IsAutoplayAllowedByPolicy(content::WebContents* contents,
                               PrefService* prefs) {
  DCHECK(prefs);

  // Check if we have globally allowed autoplay by policy.
  if (prefs->GetBoolean(prefs::kAutoplayAllowed) &&
      prefs->IsManagedPreference(prefs::kAutoplayAllowed)) {
    return true;
  }

  if (!contents)
    return false;

  // Check if the current URL matches a URL pattern on the allowlist.
  const base::ListValue* autoplay_allowlist =
      prefs->GetList(prefs::kAutoplayWhitelist);
  return autoplay_allowlist &&
         prefs->IsManagedPreference(prefs::kAutoplayWhitelist) &&
         IsURLAllowlisted(contents->GetURL(), autoplay_allowlist->GetList());
}
#endif

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
    NOTREACHED();
  }

#if !defined(OS_ANDROID)
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
  } else if (web_contents->GetMainFrame()->IsFeatureEnabled(
                 blink::mojom::PermissionsPolicyFeature::kAutoplay) &&
             IsAutoplayAllowedByPolicy(web_contents->GetOuterWebContents(),
                                       prefs)) {
    // If the domain policy allows autoplay and has delegated that to an iframe,
    // allow autoplay within the iframe. Only allow a nesting of single depth.
    result = blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  }
#endif  // !defined(OS_ANDROID)
  return result;
}

#if defined(OS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif defined(OS_POSIX) && !defined(OS_MAC)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    bool upload = !getenv(env_vars::kHeadless);
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path, upload);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (crash_reporter::IsCrashpadEnabled()) {
    int fd;
    pid_t pid;
    return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
  }

  // Extensions have the same process type as renderers.
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kUtilityProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_ANDROID)

void SetApplicationLocaleOnIOThread(const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetIOThreadApplicationLocale() = locale;
}

// An implementation of the SSLCertReporter interface used by
// SSLErrorHandler. Uses CertificateReportingService to send reports. The
// service handles queueing and re-sending of failed reports. Each certificate
// error creates a new instance of this class.
class CertificateReportingServiceCertReporter : public SSLCertReporter {
 public:
  explicit CertificateReportingServiceCertReporter(
      content::WebContents* web_contents)
      : service_(CertificateReportingServiceFactory::GetForBrowserContext(
            web_contents->GetBrowserContext())) {}
  ~CertificateReportingServiceCertReporter() override {}

  // SSLCertReporter implementation
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {
    service_->Send(serialized_report);
  }

 private:
  CertificateReportingService* service_;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceCertReporter);
};

#if BUILDFLAG(ENABLE_EXTENSIONS)

AppLoadedInTabSource ClassifyAppLoadedInTabSource(
    const GURL& opener_url,
    const extensions::Extension* target_platform_app) {
  if (!opener_url.SchemeIs(extensions::kExtensionScheme)) {
    // The forbidden app URL was being opened by a non-extension page (e.g.
    // http).
    return APP_LOADED_IN_TAB_SOURCE_OTHER;
  }

  if (opener_url.host_piece() != target_platform_app->id()) {
    // The forbidden app URL was being opened by a different app or extension.
    return APP_LOADED_IN_TAB_SOURCE_OTHER_EXTENSION;
  }

  // This platform app was trying to window.open() one of its own URLs.
  if (opener_url ==
      extensions::BackgroundInfo::GetBackgroundURL(target_platform_app)) {
    // Source was the background page.
    return APP_LOADED_IN_TAB_SOURCE_BACKGROUND_PAGE;
  }

  // Source was a different page inside the app.
  return APP_LOADED_IN_TAB_SOURCE_APP;
}

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

#endif

mojo::PendingRemote<prerender::mojom::PrerenderCanceler> GetPrerenderCanceler(
    base::OnceCallback<content::WebContents*()> wc_getter) {
  mojo::PendingRemote<prerender::mojom::PrerenderCanceler> canceler;
  prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
      std::move(wc_getter).Run())
      ->AddPrerenderCancelerReceiver(canceler.InitWithNewPipeAndPassReceiver());
  return canceler;
}

// Encapculates logic to determine if enterprise policies should be honored.
// This is a copy of the code in policy_loader_win.cc but it's ok to duplicate
// as a new central class to replace those checks is in the making.
bool ShouldHonorPolicies() {
#if defined(OS_WIN)
  bool is_enterprise_version =
      base::win::OSInfo::GetInstance()->version_type() != base::win::SUITE_HOME;
  return base::win::IsEnrolledToDomain() ||
         (base::win::IsDeviceRegisteredWithManagement() &&
          is_enterprise_version);
#else   // defined(OS_WIN)
  // TODO(pastarmovj): Replace this with check for MacOS and the new management
  // service once it is ready.
  return true;
#endif  // defined(OS_WIN)
}

void LaunchURL(const GURL& url,
               content::WebContents::Getter web_contents_getter,
               ui::PageTransition page_transition,
               bool has_user_gesture,
               const absl::optional<url::Origin>& initiating_origin) {
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
  ProtocolHandlerRegistry* protocol_handler_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (protocol_handler_registry &&
      protocol_handler_registry->IsHandledProtocol(url.scheme()))
    return;

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

  // If the URL is in allowlist, we launch it without asking the user and
  // without any additional security checks. Since the URL is allowlisted,
  // we assume it can be executed.
  if (is_allowlisted) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url, web_contents);
  } else {
    ExternalProtocolHandler::LaunchUrl(url, std::move(web_contents_getter),
                                       page_transition, has_user_gesture,
                                       initiating_origin);
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

#if defined(OS_WIN) && !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
// Returns the full path to |module_name|. Both dev builds (where |module_name|
// is in the current executable's directory) and proper installs (where
// |module_name| is in a versioned sub-directory of the current executable's
// directory) are supported. The identified file is not guaranteed to exist.
base::FilePath GetModulePath(base::WStringPiece module_name) {
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
#endif  // defined(OS_WIN) && !defined(COMPONENT_BUILD) &&
        // !defined(ADDRESS_SANITIZER)

void MaybeAddThrottle(
    std::unique_ptr<content::NavigationThrottle> maybe_throttle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  if (maybe_throttle)
    throttles->push_back(std::move(maybe_throttle));
}

void MaybeAddThrottles(
    std::vector<std::unique_ptr<content::NavigationThrottle>> additional,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* combined) {
  combined->insert(combined->end(), std::make_move_iterator(additional.begin()),
                   std::make_move_iterator(additional.end()));
}

// Returns whether |web_contents| is within a hosted app.
bool IsInHostedApp(WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
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

bool IsTopChromeWebUIURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         base::EndsWith(url.host_piece(), chrome::kChromeUITopChromeDomain);
}

}  // namespace

ChromeContentBrowserClient::ChromeContentBrowserClient() {
#if BUILDFLAG(ENABLE_PLUGINS)
  extra_parts_.push_back(new ChromeContentBrowserClientPluginsPart);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  extra_parts_.push_back(new ChromeContentBrowserClientChromeOsPart);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  extra_parts_.push_back(new ChromeContentBrowserClientTabStripPart);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extra_parts_.push_back(new ChromeContentBrowserClientExtensionsPart);
#endif

  extra_parts_.push_back(new ChromeContentBrowserClientPerformanceManagerPart);
}

ChromeContentBrowserClient::~ChromeContentBrowserClient() {
  for (int i = static_cast<int>(extra_parts_.size()) - 1; i >= 0; --i)
    delete extra_parts_[i];
  extra_parts_.clear();
}

// static
void ChromeContentBrowserClient::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kDiskCacheDir, base::FilePath());
  registry->RegisterIntegerPref(prefs::kDiskCacheSize, 0);
  registry->RegisterStringPref(prefs::kIsolateOrigins, std::string());
  registry->RegisterBooleanPref(prefs::kSitePerProcess, false);
  registry->RegisterBooleanPref(prefs::kTabFreezingEnabled, true);
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
#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kAutoplayAllowed, false);
  registry->RegisterListPref(prefs::kAutoplayWhitelist);
  registry->RegisterIntegerPref(prefs::kFetchKeepaliveDurationOnShutdown, 0);
  registry->RegisterBooleanPref(
      prefs::kSharedArrayBufferUnrestrictedAccessAllowed, false);
#endif
  registry->RegisterBooleanPref(prefs::kSSLErrorOverrideAllowed, true);
  registry->RegisterListPref(prefs::kSSLErrorOverrideAllowedForOrigins);
  registry->RegisterBooleanPref(
      prefs::kSuppressDifferentOriginSubframeJSDialogs, true);
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kWebXRImmersiveArEnabled, true);
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

std::unique_ptr<content::BrowserMainParts>
ChromeContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<ChromeBrowserMainParts> main_parts;
  // Construct the Main browser parts based on the OS type.
#if defined(OS_WIN)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsWin>(parameters, &startup_data_);
#elif defined(OS_MAC)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsMac>(parameters, &startup_data_);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  main_parts = std::make_unique<chromeos::ChromeBrowserMainPartsChromeos>(
      parameters, &startup_data_);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts = std::make_unique<ChromeBrowserMainPartsLacros>(parameters,
                                                              &startup_data_);
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsLinux>(parameters, &startup_data_);
#elif defined(OS_ANDROID)
  main_parts = std::make_unique<ChromeBrowserMainPartsAndroid>(parameters,
                                                               &startup_data_);
#elif defined(OS_POSIX)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsPosix>(parameters, &startup_data_);
#else
  NOTREACHED();
  main_parts =
      std::make_unique<ChromeBrowserMainParts>(parameters, &startup_data_);
#endif

  bool add_profiles_extra_parts = true;
#if defined(OS_ANDROID)
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
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsViewsLinux>());
#else
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsViews>());
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(jamescook): Combine with ChromeBrowserMainPartsChromeos.
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsAsh>());
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsLacros>());
#endif

#if defined(USE_X11) || defined(USE_OZONE)
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

  // Always add ChromeBrowserMainExtraPartsGpu last to make sure
  // GpuDataManager initialization could pick up about:flags settings.
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsGpu>());

  return main_parts;
}

void ChromeContentBrowserClient::PostAfterStartupTask(
    const base::Location& from_here,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::OnceClosure task) {
  AfterStartupTaskUtils::PostTask(from_here, task_runner, std::move(task));

  InitNetworkContextsParentDirectory();

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  safe_browsing_service_ = g_browser_process->safe_browsing_service();
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

content::StoragePartitionId
ChromeContentBrowserClient::GetStoragePartitionIdForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  // The partition ID for webview guest processes is the string value of its
  // SiteInstance URL - "chrome-guest://app_id/persist?partition".
  if (site.SchemeIs(content::kGuestScheme))
    return content::StoragePartitionId(
        site.spec(), GetStoragePartitionConfigForSite(browser_context, site));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The partition ID for extensions with isolated storage is treated similarly
  // to the above.
  else if (extensions::util::IsExtensionSiteWithIsolatedStorage(
               site, browser_context))
    return content::StoragePartitionId(
        site.spec(), GetStoragePartitionConfigForSite(browser_context, site));
#endif

  return content::StoragePartitionId(browser_context);
}

content::StoragePartitionConfig
ChromeContentBrowserClient::GetStoragePartitionConfigForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  // Default to the browser-wide storage partition and override based on |site|
  // below.
  content::StoragePartitionConfig storage_partition_config =
      content::StoragePartitionConfig::CreateDefault(browser_context);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::WebViewGuest::GetGuestPartitionConfigForSite(
          browser_context, site, &storage_partition_config)) {
    return storage_partition_config;
  }

  if (site.SchemeIs(extensions::kExtensionScheme)) {
    // The host in an extension site URL is the extension_id.
    CHECK(site.has_host());
    return extensions::util::GetStoragePartitionConfigForExtensionId(
        site.host(), browser_context);
  }
#endif

  return storage_partition_config;
}

content::WebContentsViewDelegate*
ChromeContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  if (auto* registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    registry->MaybeCreatePageNodeForWebContents(web_contents);
  }
  return CreateWebContentsViewDelegate(web_contents);
}

bool ChromeContentBrowserClient::AllowGpuLaunchRetryOnIOThread() {
#if defined(OS_ANDROID)
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

  WebRtcLoggingController::AttachToRenderProcessHost(
      host, g_browser_process->webrtc_log_uploader());

  // The audio manager outlives the host, so it's safe to hand a raw pointer to
  // it to the AudioDebugRecordingsHandler, which is owned by the host.
  AudioDebugRecordingsHandler* audio_debug_recordings_handler =
      new AudioDebugRecordingsHandler(profile);
  host->SetUserData(
      AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey,
      std::make_unique<base::UserDataAdapter<AudioDebugRecordingsHandler>>(
          audio_debug_recordings_handler));

#if BUILDFLAG(ENABLE_NACL)
  host->AddFilter(new nacl::NaClHostMessageFilter(
      host->GetID(), profile->IsOffTheRecord(), profile->GetPath()));
#endif

#if defined(OS_ANDROID)
  // Data cannot be persisted if the profile is off the record.
  host->AddFilter(
      new cdm::CdmMessageFilterAndroid(!profile->IsOffTheRecord(), false));

  // Register CrashMemoryMetricsCollector to report oom related metrics.
  host->SetUserData(
      CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey,
      std::make_unique<CrashMemoryMetricsCollector>(host));
#endif

  Profile* original_profile = profile->GetOriginalProfile();
  RendererUpdaterFactory::GetForProfile(original_profile)
      ->InitializeRenderer(host);

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->RenderProcessWillLaunch(host);
}

GURL ChromeContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return url;

#if !defined(OS_ANDROID)
  // If the input |url| should be assigned to the Instant renderer, make its
  // effective URL distinct from other URLs on the search provider's domain.
  // This needs to happen even if |url| corresponds to an isolated origin; see
  // https://crbug.com/755595.
  if (search::ShouldAssignURLToInstantRenderer(url, profile))
    return search::GetEffectiveURLForInstant(url, profile);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
        bool is_main_frame,
        const GURL& candidate_url,
        const GURL& destination_url) {
  DCHECK(browser_context);
  DCHECK(candidate_site_instance);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldCompareEffectiveURLsForSiteInstanceSelection(
          browser_context, candidate_site_instance, is_main_frame,
          candidate_url, destination_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::ShouldUseMobileFlingCurve() {
#if defined(OS_ANDROID)
  return true;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode();
#else
  return false;
#endif  // defined(OS_ANDROID)
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

  // The web footer experiment should share its renderer to not effectively
  // instantiate one per window. See https://crbug.com/993502.
  if (site_url == GURL(chrome::kChromeUIWebFooterExperimentURL))
    return true;

#if !defined(OS_ANDROID)
  if (search::ShouldUseProcessPerSiteForInstantSiteURL(site_url, profile))
    return true;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ChromeContentBrowserClientExtensionsPart::ShouldUseProcessPerSite(
          profile, site_url))
    return true;
#endif

  // Non-extension, non-NTP URLs should generally use process-per-site-instance
  // (rather than process-per-site).
  return false;
}

bool ChromeContentBrowserClient::ShouldUseSpareRenderProcessHost(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  // Top Chrome WebUI should share a RendererProcessHost. Return false here to
  // ensure the Spare Renderer is not assigned.
  if (IsTopChromeWebUIURL(site_url))
    return false;

#if !defined(OS_ANDROID)
  // Instant renderers should not use a spare process, because they require
  // passing switches::kInstantProcess to the renderer process when it
  // launches.  A spare process is launched earlier, before it is known which
  // navigation will use it, so it lacks this flag.
  if (search::ShouldAssignURLToInstantRenderer(site_url, profile))
    return false;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldUseSpareRenderProcessHost(profile, site_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess(
          browser_context, effective_site_url)) {
    return true;
  }
#endif
  return false;
}

bool ChromeContentBrowserClient::ShouldLockProcessToSite(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!ChromeContentBrowserClientExtensionsPart::ShouldLockProcessToSite(
          browser_context, effective_site_url)) {
    return false;
  }
#endif
  return true;
}

bool ChromeContentBrowserClient::DoesWebUISchemeRequireProcessLock(
    base::StringPiece scheme) {
  // Note: This method can be called from multiple threads. It is not safe to
  // assume it runs only on the UI thread.

  // chrome-search: documents commit only in the NTP instant process and are not
  // locked to chrome-search: origin.  Locking to chrome-search would kill
  // processes upon legitimate requests for cookies from the search engine's
  // domain.
  if (scheme == chrome::kChromeSearchScheme)
    return false;

  // All other WebUIs must be locked to origin.
  return true;
}

bool ChromeContentBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    base::StringPiece scheme,
    bool is_embedded_origin_secure) {
  // This is needed to bypass the normal SameSite rules for any chrome:// page
  // embedding a secure origin, regardless of the registrable domains of any
  // intervening frames. For example, this is needed for browser UI to interact
  // with SameSite cookies on accounts.google.com, which are used for logging
  // into Cloud Print from chrome://print, for displaying a list of available
  // accounts on the NTP (chrome://new-tab-page), etc.
  if (is_embedded_origin_secure && scheme == content::kChromeUIScheme)
    return true;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return scheme == extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::
    ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
        base::StringPiece scheme,
        bool is_embedded_origin_secure) {
  return is_embedded_origin_secure && scheme == content::kChromeUIScheme;
}

// TODO(crbug.com/1087559): This is based on SubframeTask::GetTitle()
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeContentBrowserClientExtensionsPart::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
#endif
}

// These are treated as WebUI schemes but do not get WebUI bindings. Also,
// view-source is allowed for these schemes.
void ChromeContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(chrome::kChromeSearchScheme);
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
  additional_schemes->push_back(content::kChromeDevToolsScheme);
}

void ChromeContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  additional_schemes->push_back(extensions::kExtensionScheme);
#endif
}

network::mojom::IPAddressSpace
ChromeContentBrowserClient::DetermineAddressSpaceFromURL(const GURL& url) {
  if (url.SchemeIs(chrome::kChromeSearchScheme))
    return network::mojom::IPAddressSpace::kLocal;
  if (url.SchemeIs(dom_distiller::kDomDistillerScheme))
    return network::mojom::IPAddressSpace::kPublic;
#if BUILDFLAG(ENABLE_EXTENSIONS)
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
  if (ProtocolHandlerRegistry* protocol_handler_registry =
          ProtocolHandlerRegistryFactory::GetForBrowserContext(
              browser_context)) {
    return protocol_handler_registry->IsHandledProtocol(scheme);
  }

  return false;
}

bool ChromeContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::CanCommitURL(process_host,
                                                                url);
#else
  return true;
#endif
}

void ChromeContentBrowserClient::OverrideNavigationParams(
    SiteInstance* site_instance,
    ui::PageTransition* transition,
    bool* is_renderer_initiated,
    content::Referrer* referrer,
    absl::optional<url::Origin>* initiator_origin) {
  DCHECK(transition);
  DCHECK(is_renderer_initiated);
  DCHECK(referrer);
  // While using SiteInstance::GetSiteURL() is unreliable and the wrong thing to
  // use for making security decisions 99.44% of the time, for detecting the NTP
  // it is reliable and the correct way. See http://crbug.com/624410.
  if (site_instance && search::IsNTPURL(site_instance->GetSiteURL()) &&
      ui::PageTransitionCoreTypeIs(*transition, ui::PAGE_TRANSITION_LINK)) {
    // Clicks on tiles of the new tab page should be treated as if a user
    // clicked on a bookmark.  This is consistent with native implementations
    // like Android's.  This also helps ensure that security features (like
    // Sec-Fetch-Site and SameSite-cookies) will treat the navigation as
    // browser-initiated.
    *transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    *is_renderer_initiated = false;
    *referrer = content::Referrer();
    *initiator_origin = absl::nullopt;
  }
}

bool ChromeContentBrowserClient::ShouldStayInParentProcessForNTP(
    const GURL& url,
    SiteInstance* parent_site_instance) {
  // While using SiteInstance::GetSiteURL() is unreliable and the wrong thing to
  // use for making security decisions 99.44% of the time, for detecting the NTP
  // it is reliable and the correct way. See http://crbug.com/624410.
  return url.SchemeIs(chrome::kChromeSearchScheme) && parent_site_instance &&
         search::IsNTPURL(parent_site_instance->GetSiteURL());
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

#if !defined(OS_ANDROID)
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      GetProcessCountToIgnoreForLimit();
#else
  return 0;
#endif
}

bool ChromeContentBrowserClient::ShouldTryToUseExistingProcessHost(
    content::BrowserContext* browser_context,
    const GURL& url) {
  // It has to be a valid URL for us to check for an extension.
  if (!url.is_valid())
    return false;

  // Top Chrome WebUI should try to share a RenderProcessHost with other
  // existing Top Chrome WebUI.
  if (IsTopChromeWebUIURL(url))
    return true;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return ChromeContentBrowserClientExtensionsPart::
      ShouldTryToUseExistingProcessHost(profile, url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldSubframesTryToReuseExistingProcess(
    content::RenderFrameHost* main_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldSubframesTryToReuseExistingProcess(main_frame);
#else
  return true;
#endif
}

void ChromeContentBrowserClient::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  CHECK(site_instance->HasProcess());

  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  if (!profile)
    return;

#if !defined(OS_ANDROID)
  // Remember the ID of the Instant process to signal the renderer process
  // on startup in |AppendExtraCommandLineSwitches| below.
  if (search::ShouldAssignURLToInstantRenderer(site_instance->GetSiteURL(),
                                               profile)) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile);
    if (instant_service)
      instant_service->AddInstantProcess(site_instance->GetProcess()->GetID());
  }
#endif

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->SiteInstanceGotProcess(site_instance);
}

void ChromeContentBrowserClient::SiteInstanceDeleting(
    SiteInstance* site_instance) {
  if (!site_instance->HasProcess())
    return;

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->SiteInstanceDeleting(site_instance);
}

bool ChromeContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_effective_url,
    const GURL& destination_effective_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
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

bool ChromeContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return !url.SchemeIs(chrome::kChromeNativeScheme);
}

std::vector<url::Origin>
ChromeContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  std::vector<url::Origin> isolated_origin_list;

// Sign-in process isolation is not needed on Android, see
// https://crbug.com/739418.
#if !defined(OS_ANDROID)
  isolated_origin_list.push_back(
      url::Origin::Create(GaiaUrls::GetInstance()->gaia_url()));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
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

bool ChromeContentBrowserClient::ShouldDisableSiteIsolation() {
  return site_isolation::SiteIsolationPolicy::
      ShouldDisableSiteIsolationDueToMemoryThreshold();
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
    // group, we must call GetVariationParams for each field trial first
    // (for example, before checking HasSwitch() and returning), since
    // GetVariationParams has the side-effect of assigning the client to
    // a field trial group.
    std::map<std::string, std::string> params;
    if (variations::GetVariationParams(field_trial_name, &params)) {
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
#if defined(OS_MAC)
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info) {
    command_line->AppendSwitchASCII(switches::kMetricsClientID,
                                    client_info->client_id);
  }
#elif defined(OS_POSIX)
#if defined(OS_ANDROID)
  bool enable_crash_reporter = true;
#else
  bool enable_crash_reporter = false;
  if (crash_reporter::IsCrashpadEnabled()) {
    command_line->AppendSwitch(switches::kEnableCrashpad);
    enable_crash_reporter = true;

    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  } else {
    enable_crash_reporter = breakpad::IsCrashReporterEnabled();
  }
#endif
  if (enable_crash_reporter) {
    std::string switch_value;
    std::unique_ptr<metrics::ClientInfo> client_info =
        GoogleUpdateSettings::LoadMetricsClientInfo();
    if (client_info)
      switch_value = client_info->client_id;
    switch_value.push_back(',');
    switch_value.append(
        chrome::GetChannelName(chrome::WithExtendedStable(true)));
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
                                    switch_value);
  }
#endif

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  static const char* const kCommonSwitchNames[] = {
      embedder_support::kUserAgent,
      switches::kUserDataDir,  // Make logs go to the right file.
  };
  command_line->CopySwitchesFrom(browser_command_line, kCommonSwitchNames,
                                 base::size(kCommonSwitchNames));

  static const char* const kDinosaurEasterEggSwitches[] = {
      error_page::switches::kDisableDinosaurEasterEgg,
  };
  command_line->CopySwitchesFrom(browser_command_line,
                                 kDinosaurEasterEggSwitches,
                                 base::size(kDinosaurEasterEggSwitches));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS need to pass primary user homedir (in multi-profiles session).
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  command_line->AppendSwitchASCII(chromeos::switches::kHomedir,
                                  homedir.value().c_str());
#endif

  if (process_type == switches::kRendererProcess) {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(child_process_id);
    Profile* profile =
        process ? Profile::FromBrowserContext(process->GetBrowserContext())
                : nullptr;
    for (size_t i = 0; i < extra_parts_.size(); ++i) {
      extra_parts_[i]->AppendExtraRendererCommandLineSwitches(command_line,
                                                              process, profile);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    const std::string& login_profile = browser_command_line.GetSwitchValueASCII(
        chromeos::switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      login_profile);
#endif

    MaybeCopyDisableWebRtcEncryptionSwitch(command_line, browser_command_line,
                                           chrome::GetChannel());
    if (process) {
      PrefService* prefs = profile->GetPrefs();
      // Currently this pref is only registered if applied via a policy.
      if (prefs->HasPrefPath(prefs::kDisable3DAPIs) &&
          prefs->GetBoolean(prefs::kDisable3DAPIs)) {
        // Turn this policy into a command line switch.
        command_line->AppendSwitch(switches::kDisable3DAPIs);
      }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
      bool client_side_detection_enabled =
          safe_browsing::IsSafeBrowsingEnabled(*prefs) &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              ::switches::kDisableClientSidePhishingDetection);
#if BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
      client_side_detection_enabled &= base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionForAndroid);
#endif  // BUILDFLAG(SAFE_BROWSING_DB_REMOTE)

      // Disable client-side phishing detection in the renderer if it is
      // disabled in the Profile preferences, or by command line flag, or by not
      // being enabled on Android.
      if (!client_side_detection_enabled) {
        command_line->AppendSwitch(
            switches::kDisableClientSidePhishingDetection);
      }
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

      if (prefs->GetBoolean(prefs::kPrintPreviewDisabled))
        command_line->AppendSwitch(switches::kDisablePrintPreview);

#if !defined(OS_ANDROID)
      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile);
      if (instant_service &&
          instant_service->IsInstantProcess(process->GetID())) {
        command_line->AppendSwitch(switches::kInstantProcess);
      }

      // Enable SharedArrayBuffer on desktop if allowed by Enterprise Policy.
      // TODO(crbug.com/1144104) Remove when migration to COOP+COEP is complete.
      if (prefs->GetBoolean(
              prefs::kSharedArrayBufferUnrestrictedAccessAllowed)) {
        command_line->AppendSwitch(
            switches::kSharedArrayBufferUnrestrictedAccessAllowed);
      }
#endif

      if (prefs->HasPrefPath(prefs::kAllowDinosaurEasterEgg) &&
          !prefs->GetBoolean(prefs::kAllowDinosaurEasterEgg)) {
        command_line->AppendSwitch(
            error_page::switches::kDisableDinosaurEasterEgg);
      }

      MaybeAppendSecureOriginsAllowlistSwitch(command_line);

      if (prefs->HasPrefPath(prefs::kAllowSyncXHRInPageDismissal) &&
          prefs->GetBoolean(prefs::kAllowSyncXHRInPageDismissal)) {
        command_line->AppendSwitch(switches::kAllowSyncXHRInPageDismissal);
      }

      if (prefs->HasPrefPath(prefs::kScrollToTextFragmentEnabled) &&
          !prefs->GetBoolean(prefs::kScrollToTextFragmentEnabled)) {
        command_line->AppendSwitch(switches::kDisableScrollToTextFragment);
      }

      if (prefs->HasPrefPath(prefs::kAppCacheForceEnabled) &&
          prefs->GetBoolean(prefs::kAppCacheForceEnabled)) {
        command_line->AppendSwitch(switches::kAppCacheForceEnabled);
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

      // Same as above, but for the blink side of UserAgentClientHints
      if (!local_state->GetBoolean(
              policy::policy_prefs::kUserAgentClientHintsEnabled)) {
        command_line->AppendSwitch(
            blink::switches::kUserAgentClientHintDisable);
      }

      if (!local_state->GetBoolean(
              policy::policy_prefs::kTargetBlankImpliesNoOpener)) {
        command_line->AppendSwitch(
            switches::kDisableTargetBlankImpliesNoOpener);
      }

#if defined(OS_ANDROID)
      // Communicating to content/ for BackForwardCache.
      if (prefs->HasPrefPath(policy::policy_prefs::kBackForwardCacheEnabled) &&
          !prefs->GetBoolean(policy::policy_prefs::kBackForwardCacheEnabled)) {
        command_line->AppendSwitch(switches::kDisableBackForwardCache);
      }
#endif  // defined(OS_ANDROID)
    }

    MaybeAppendBlinkSettingsSwitchForFieldTrial(browser_command_line,
                                                command_line);

#if defined(OS_ANDROID)
    // If the platform is Android, force the distillability service on.
    command_line->AppendSwitch(switches::kEnableDistillabilityService);
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
      extensions::switches::kSetExtensionThrottleTestParams,  // For tests only.
      extensions::switches::kAllowlistedExtensionID,
#endif
      switches::kAllowInsecureLocalhost,
      switches::kAppsGalleryURL,
      switches::kCloudPrintURL,
      switches::kCloudPrintXmppEndpoint,
      switches::kDisableJavaScriptHarmonyShipping,
      variations::switches::kEnableBenchmarking,
      switches::kEnableDistillabilityService,
      switches::kEnableNaCl,
#if BUILDFLAG(ENABLE_NACL)
      switches::kEnableNaClDebug,
      switches::kEnableNaClNonSfiMode,
#endif
      switches::kEnableNetBenchmarking,
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
      embedder_support::kOriginTrialDisabledTokens,
      embedder_support::kOriginTrialPublicKey,
      switches::kReaderModeHeuristics,
      translate::switches::kTranslateSecurityOrigin,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    static const char* const kSwitchNames[] = {
        extensions::switches::kAllowHTTPBackgroundPage,
        extensions::switches::kEnableExperimentalExtensionApis,
        extensions::switches::kExtensionsOnChromeURLs,
        extensions::switches::kAllowlistedExtensionID,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
#endif
    MaybeAppendSecureOriginsAllowlistSwitch(command_line);
  } else if (process_type == switches::kZygoteProcess) {
    // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
#if BUILDFLAG(ENABLE_NACL)
    static const char* const kSwitchNames[] = {
        switches::kEnableNaClDebug,
        switches::kEnableNaClNonSfiMode,
        switches::kForcePNaClSubzero,
        switches::kNaClDangerousNoSandboxNonSfi,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Ensure zygote loads the resource bundle for the right locale.
    static const char* const kMoreSwitchNames[] = {switches::kLang};
    command_line->CopySwitchesFrom(browser_command_line, kMoreSwitchNames,
                                   base::size(kMoreSwitchNames));
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
    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
  }
#endif

  ThreadProfilerConfiguration::Get()->AppendCommandLineSwitchForChildProcess(
      command_line);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Processes may only query perf_event_open with the BPF sandbox disabled.
  if (browser_command_line.HasSwitch(switches::kEnableThreadInstructionCount) &&
      command_line->HasSwitch(sandbox::policy::switches::kNoSandbox)) {
    command_line->AppendSwitch(switches::kEnableThreadInstructionCount);
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

  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile && data_reduction_proxy::DataReductionProxySettings::
                        IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                                 profile->GetPrefs());
}

void ChromeContentBrowserClient::UpdateRendererPreferencesForWorker(
    content::BrowserContext* browser_context,
    blink::RendererPreferences* out_prefs) {
  DCHECK(browser_context);
  DCHECK(out_prefs);
  renderer_preferences_util::UpdateFromSystemSettings(
      out_prefs, Profile::FromBrowserContext(browser_context));
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return embedder_support::AllowAppCache(
      manifest_url, site_for_cookies, top_frame_origin,
      CookieSettingsFactory::GetForProfile(Profile::FromBrowserContext(context))
          .get());
}

content::AllowServiceWorkerResult
ChromeContentBrowserClient::AllowServiceWorker(
    const GURL& scope,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
  return embedder_support::AllowServiceWorker(
      scope, site_for_cookies, top_frame_origin,
      CookieSettingsFactory::GetForProfile(profile).get(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

bool ChromeContentBrowserClient::AllowSharedWorker(
    const GURL& worker_url,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
    content::BrowserContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if cookies are allowed.
  return embedder_support::AllowSharedWorker(
      worker_url, site_for_cookies, top_frame_origin, name, storage_key,
      render_process_id, render_frame_id,
      CookieSettingsFactory::GetForProfile(Profile::FromBrowserContext(context))
          .get());
}

bool ChromeContentBrowserClient::DoesSchemeAllowCrossOriginSharedWorker(
    const std::string& scheme) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
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

void ChromeContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback) {
  // An empty list is passed for render_frames here since we manually notify
  // PageSpecificContentSettings that the file system was accessed below.
  bool allow = embedder_support::AllowWorkerFileSystem(
      url, {},
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context))
          .get());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  GuestPermissionRequestHelper(url, render_frames, std::move(callback), allow);
#else
  FileSystemAccessed(url, render_frames, std::move(callback), allow);
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeContentBrowserClient::GuestPermissionRequestHelper(
    const GURL& url,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback,
    bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::map<int, int> process_map;
  std::map<int, int>::const_iterator it;
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
  it = process_map.begin();

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(it->first, it->second);
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
    content_settings::PageSpecificContentSettings::FileSystemAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }
  std::move(callback).Run(allow);
}

bool ChromeContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  return embedder_support::AllowWorkerIndexedDB(
      url, render_frames,
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context))
          .get());
}

bool ChromeContentBrowserClient::AllowWorkerCacheStorage(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  return embedder_support::AllowWorkerCacheStorage(
      url, render_frames,
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context))
          .get());
}

bool ChromeContentBrowserClient::AllowWorkerWebLocks(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalRenderFrameHostId>& render_frames) {
  return embedder_support::AllowWorkerWebLocks(
      url, CookieSettingsFactory::GetForProfile(
               Profile::FromBrowserContext(browser_context))
               .get());
}

ChromeContentBrowserClient::AllowWebBluetoothResult
ChromeContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  // TODO(crbug.com/598890): Don't disable if
  // base::CommandLine::ForCurrentProcess()->
  // HasSwitch(switches::kEnableWebBluetooth) is true.
  if (variations::GetVariationParamValue(
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
  return variations::GetVariationParamValue("WebBluetoothBlocklist",
                                            "blocklist_additions");
}

bool ChromeContentBrowserClient::IsInterestGroupAPIAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const GURL& api_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrivacySandboxSettings* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);

  return privacy_sandbox_settings &&
         privacy_sandbox_settings->IsFledgeAllowed(top_frame_origin, api_url);
}

bool ChromeContentBrowserClient::IsConversionMeasurementOperationAllowed(
    content::BrowserContext* browser_context,
    ConversionMeasurementOperation operation,
    const url::Origin* impression_origin,
    const url::Origin* conversion_origin,
    const url::Origin* reporting_origin) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  PrivacySandboxSettings* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (!privacy_sandbox_settings)
    return false;

  switch (operation) {
    case ConversionMeasurementOperation::kImpression:
      DCHECK(impression_origin);
      DCHECK(reporting_origin);
      return privacy_sandbox_settings->IsConversionMeasurementAllowed(
          *impression_origin, *reporting_origin);
    case ConversionMeasurementOperation::kConversion:
      DCHECK(conversion_origin);
      DCHECK(reporting_origin);
      return privacy_sandbox_settings->IsConversionMeasurementAllowed(
          *conversion_origin, *reporting_origin);
    case ConversionMeasurementOperation::kReport:
      DCHECK(impression_origin);
      DCHECK(conversion_origin);
      DCHECK(reporting_origin);
      return privacy_sandbox_settings->ShouldSendConversionReport(
          *impression_origin, *conversion_origin, *reporting_origin);
    case ConversionMeasurementOperation::kAny:
      return privacy_sandbox_settings->IsPrivacySandboxAllowed();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeContentBrowserClient::OnTrustAnchorUsed(
    content::BrowserContext* browser_context) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(
            Profile::FromBrowserContext(browser_context));
    if (user && !user->username_hash().empty()) {
      policy::PolicyCertServiceFactory::SetUsedPolicyCertificates(
          user->GetAccountId().GetUserEmail());
    }
  }
}
#endif

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

device::GeolocationManager*
ChromeContentBrowserClient::GetGeolocationManager() {
#if defined(OS_MAC)
  return g_browser_process->platform_part()->geolocation_manager();
#else
  return nullptr;
#endif
}

#if defined(OS_ANDROID)
bool ChromeContentBrowserClient::ShouldUseGmsCoreGeolocationProvider() {
  // Indicate that Chrome uses the GMS core location provider.
  return true;
}
#endif

scoped_refptr<content::QuotaPermissionContext>
ChromeContentBrowserClient::CreateQuotaPermissionContext() {
  return new permissions::QuotaPermissionContextImpl();
}

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

void ChromeContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  DCHECK(web_contents);
  if (!is_main_frame_request) {
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

#if !defined(OS_ANDROID)
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

#if !defined(OS_ANDROID)
blink::mojom::PreferredColorScheme ToBlinkPreferredColorScheme(
    ui::NativeTheme::PreferredColorScheme native_theme_scheme) {
  switch (native_theme_scheme) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      return blink::mojom::PreferredColorScheme::kDark;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      return blink::mojom::PreferredColorScheme::kLight;
  }

  NOTREACHED();
}
#endif  // !defined(OS_ANDROID)

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

#if defined(OS_ANDROID)
  auto* delegate = TabAndroid::FromWebContents(web_contents)
                       ? static_cast<android::TabWebContentsDelegateAndroid*>(
                             web_contents->GetDelegate())
                       : nullptr;
  if (delegate) {
    web_prefs->preferred_color_scheme =
        delegate->IsNightModeEnabled()
            ? blink::mojom::PreferredColorScheme::kDark
            : blink::mojom::PreferredColorScheme::kLight;
  }
#else
  // Update based on native theme scheme.
  web_prefs->preferred_color_scheme =
      ToBlinkPreferredColorScheme(native_theme->GetPreferredColorScheme());
#endif  // defined(OS_ANDROID)

  bool force_light = false;
  // Force a light preferred color scheme on certain URLs if kWebUIDarkMode is
  // disabled; some of the UI is not yet correctly themed.
  if (!base::FeatureList::IsEnabled(features::kWebUIDarkMode)) {
    // Update based on last committed url.
    force_light |= url.SchemeIs(content::kChromeUIScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    force_light |= url.SchemeIs(extensions::kExtensionScheme) &&
                   url.host_piece() == extension_misc::kPdfExtensionId;
#endif
  }

  // Reauth WebUI doesn't support dark mode yet because it shares the dialog
  // with GAIA web contents that is not correctly themed.
  force_light |= url.SchemeIs(content::kChromeUIScheme) &&
                 url.host_piece() == chrome::kChromeUISigninReauthHost;

  if (force_light) {
    web_prefs->preferred_color_scheme =
        blink::mojom::PreferredColorScheme::kLight;
  }

  return old_preferred_color_scheme != web_prefs->preferred_color_scheme;
}

}  // namespace

base::OnceClosure ChromeContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(
        prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED);
    return base::OnceClosure();
  }

  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());
  DCHECK(requesting_url.is_valid())
      << "Invalid URL string: https://"
      << cert_request_info->host_and_port.ToString();

  bool may_show_cert_selection = true;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    // On the sign-in profile, never show certificate selection to the user. A
    // client certificate is an identifier that can be stable for a long time,
    // so only the administrator is allowed to decide which endpoints should see
    // it.
    may_show_cert_selection = false;

    content::StoragePartition* storage_partition =
        profile->GetStoragePartition(web_contents->GetSiteInstance());
    chromeos::login::SigninPartitionManager* signin_partition_manager =
        chromeos::login::SigninPartitionManager::Factory::GetForBrowserContext(
            profile);

    // On the sign-in profile, only allow client certs in the context of the
    // sign-in frame.
    if (!signin_partition_manager->IsCurrentSigninStoragePartition(
            storage_partition)) {
      LOG(WARNING)
          << "Client cert requested in sign-in profile in wrong context.";
      // Continue without client certificate. We do this to mimic the case of no
      // client certificate being present in the profile's certificate store.
      delegate->ContinueWithCertificate(nullptr, nullptr);
      return base::OnceClosure();
    }
    VLOG(1) << "Client cert requested in sign-in profile.";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<net::ClientCertIdentity> auto_selected_identity =
      chrome::enterprise_util::AutoSelectCertificate(profile, requesting_url,
                                                     client_certs);
  if (auto_selected_identity) {
    // The callback will own |auto_selected_identity| and |delegate|, keeping
    // them alive until after ContinueWithCertificate is called.
    scoped_refptr<net::X509Certificate> cert =
        auto_selected_identity->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move(auto_selected_identity),
        base::BindOnce(
            &content::ClientCertificateDelegate::ContinueWithCertificate,
            std::move(delegate), std::move(cert)));
    LogClientAuthResult(ClientCertSelectionResult::kAutoSelect);
    return base::OnceClosure();
  }

  if (!may_show_cert_selection) {
    LOG(WARNING) << "No client cert matched by policy and user selection is "
                    "not allowed.";
    LogClientAuthResult(ClientCertSelectionResult::kNoSelectionAllowed);
    // Continue without client certificate. We do this to mimic the case of no
    // client certificate being present in the profile's certificate store.
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  return chrome::ShowSSLClientCertificateSelector(
      web_contents, cert_request_info, std::move(client_certs),
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

content::PlatformNotificationService*
ChromeContentBrowserClient::GetPlatformNotificationService(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return PlatformNotificationServiceFactory::GetForProfile(profile);
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          opener->GetProcess()->GetID())) {
    return true;
  }

  if (target_url.SchemeIs(extensions::kExtensionScheme)) {
    // Intentionally duplicating |registry| code from above because we want to
    // reduce calls to retrieve them as this function is a SYNC IPC handler.
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    const Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(target_url);
    if (extension && extension->is_platform_app()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.AppLoadedInTab",
          ClassifyAppLoadedInTabSource(opener_url, extension),
          APP_LOADED_IN_TAB_SOURCE_MAX);

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
  return blocked_content::MaybeBlockPopup(
             web_contents, &opener_top_level_frame_url,
             std::make_unique<ChromePopupNavigationDelegate>(
                 std::move(nav_params)),
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

content::TtsPlatform* ChromeContentBrowserClient::GetTtsPlatform() {
#if !defined(OS_ANDROID)
  content::TtsController::GetInstance()->SetTtsEngineDelegate(
      TtsExtensionEngine::GetInstance());
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return TtsPlatformImplChromeOs::GetInstance();
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
#if !defined(OS_ANDROID)
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitStandardFontFamilyMap,
                                     &web_prefs->standard_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitFixedFontFamilyMap,
                                     &web_prefs->fixed_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitSerifFontFamilyMap,
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
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitPictographFontFamilyMap,
                                     &web_prefs->pictograph_font_family_map);

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
#if defined(OS_ANDROID)
  web_prefs->font_scale_factor =
      static_cast<float>(prefs->GetDouble(prefs::kWebKitFontScaleFactor));
  web_prefs->force_enable_zoom =
      prefs->GetBoolean(prefs::kWebKitForceEnableZoom);
#endif
  web_prefs->force_dark_mode_enabled =
      prefs->GetBoolean(prefs::kWebKitForceDarkModeEnabled);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_prefs->always_show_focus =
      prefs->GetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled);
#else
  if (features::IsAccessibilityFocusHighlightEnabled()) {
    web_prefs->always_show_focus =
        prefs->GetBoolean(prefs::kAccessibilityFocusHighlightEnabled);
  }
#endif

#if defined(OS_ANDROID)
  web_prefs->password_echo_enabled =
      prefs->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
#else
  web_prefs->password_echo_enabled = browser_defaults::kPasswordEchoEnabled;
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

  web_prefs->data_saver_enabled = IsDataSaverEnabled(profile);

  web_prefs->data_saver_holdback_web_api_enabled =
      base::GetFieldTrialParamByFeatureAsBool(features::kDataSaverHoldback,
                                              "holdback_web", false);

  if (web_contents) {
#if defined(OS_ANDROID)
    auto* delegate = TabAndroid::FromWebContents(web_contents)
                         ? static_cast<android::TabWebContentsDelegateAndroid*>(
                               web_contents->GetDelegate())
                         : nullptr;
    if (delegate) {
      web_prefs->embedded_media_experience_enabled =
          delegate->ShouldEnableEmbeddedMediaExperience();

      web_prefs->picture_in_picture_enabled =
          delegate->IsPictureInPictureEnabled();
    }
#endif  // defined(OS_ANDROID)

    // web_app_scope value is platform specific.
#if defined(OS_ANDROID)
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
      Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
      if (browser && browser->app_controller() &&
          browser->app_controller()->HasAppId()) {
        const web_app::AppId& app_id = browser->app_controller()->GetAppId();
        const web_app::WebAppRegistrar& registrar =
            web_app::WebAppProvider::Get(profile)->registrar();
        if (registrar.IsLocallyInstalled(app_id))
          web_prefs->web_app_scope = registrar.GetAppScope(app_id);

        if (browser->app_controller()->is_for_system_web_app()) {
          auto system_app_type = browser->app_controller()->system_app_type();
          const web_app::SystemWebAppManager& system_web_app_manager =
              web_app::WebAppProvider::Get(profile)->system_web_app_manager();
          web_prefs->allow_scripts_to_close_windows =
              system_web_app_manager.AllowScriptsToCloseWindows(
                  system_app_type.value());
        }
      }
    }
#endif

    web_prefs->immersive_mode_enabled = vr::VrTabHelper::IsInVr(web_contents);
  }

  web_prefs->lazy_load_enabled =
      !web_contents || !web_contents->GetDelegate() ||
      web_contents->GetDelegate()->ShouldAllowLazyLoad();

  if (base::FeatureList::IsEnabled(features::kLazyFrameLoading)) {
    const char* param_name =
        web_prefs->data_saver_enabled
            ? "lazy_frame_loading_distance_thresholds_px_by_ect"
            : "lazy_frame_loading_distance_thresholds_px_by_ect_with_data_"
              "saver_enabled";

    base::StringPairs pairs;
    base::SplitStringIntoKeyValuePairs(
        base::GetFieldTrialParamValueByFeature(features::kLazyFrameLoading,
                                               param_name),
        ':', ',', &pairs);

    for (const auto& pair : pairs) {
      absl::optional<net::EffectiveConnectionType> effective_connection_type =
          net::GetEffectiveConnectionTypeForName(pair.first);
      int value = 0;
      if (effective_connection_type && base::StringToInt(pair.second, &value)) {
        web_prefs->lazy_frame_loading_distance_thresholds_px[static_cast<
            EffectiveConnectionType>(effective_connection_type.value())] =
            value;
      }
    }
  }

  if (base::FeatureList::IsEnabled(features::kLazyImageLoading)) {
    const char* param_name =
        web_prefs->data_saver_enabled
            ? "lazy_image_loading_distance_thresholds_px_by_ect"
            : "lazy_image_loading_distance_thresholds_px_by_ect_with_data_"
              "saver_enabled";

    base::StringPairs pairs;
    base::SplitStringIntoKeyValuePairs(
        base::GetFieldTrialParamValueByFeature(features::kLazyImageLoading,
                                               param_name),
        ':', ',', &pairs);

    for (const auto& pair : pairs) {
      absl::optional<net::EffectiveConnectionType> effective_connection_type =
          net::GetEffectiveConnectionTypeForName(pair.first);
      int value = 0;
      if (effective_connection_type && base::StringToInt(pair.second, &value)) {
        web_prefs->lazy_image_loading_distance_thresholds_px[static_cast<
            EffectiveConnectionType>(effective_connection_type.value())] =
            value;
      }
    }

    pairs.clear();
    base::SplitStringIntoKeyValuePairs(
        base::GetFieldTrialParamValueByFeature(features::kLazyImageLoading,
                                               "lazy_image_first_k_fully_load"),
        ':', ',', &pairs);

    for (const auto& pair : pairs) {
      absl::optional<net::EffectiveConnectionType> effective_connection_type =
          net::GetEffectiveConnectionTypeForName(pair.first);
      int value = 0;
      if (effective_connection_type && base::StringToInt(pair.second, &value)) {
        web_prefs->lazy_image_first_k_fully_load[static_cast<
            EffectiveConnectionType>(effective_connection_type.value())] =
            value;
      }
    }
  }

  if (base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    std::string effective_connection_type_param =
        base::GetFieldTrialParamValueByFeature(
            features::kNetworkQualityEstimatorWebHoldback,
            "web_effective_connection_type_override");

    absl::optional<net::EffectiveConnectionType> effective_connection_type =
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
  }

  UpdatePreferredColorScheme(
      web_prefs, web_contents->GetMainFrame()->GetSiteInstance()->GetSiteURL(),
      web_contents, GetWebTheme());

  web_prefs->translate_service_available = TranslateService::IsAvailable(prefs);

  absl::optional<ui::CaptionStyle> style =
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
    web_prefs->text_track_window_padding = style->window_padding;
    web_prefs->text_track_window_radius = style->window_radius;
  }

  if (subresource_redirect::ShouldEnablePublicImageHintsBasedCompression() ||
      subresource_redirect::ShouldEnableLoginRobotsCheckedImageCompression()) {
    web_prefs->litepage_subresource_redirect_origin =
        subresource_redirect::GetSubresourceRedirectOrigin();
  }

#if defined(OS_ANDROID)
  // If the pref is not set, the default value (true) will be used:
  web_prefs->webxr_immersive_ar_allowed =
      prefs->GetBoolean(prefs::kWebXRImmersiveArEnabled);
#endif

  for (ChromeContentBrowserClientParts* parts : extra_parts_)
    parts->OverrideWebkitPrefs(web_contents, web_prefs);
}

bool ChromeContentBrowserClient::OverrideWebPreferencesAfterNavigation(
    WebContents* web_contents,
    WebPreferences* prefs) {
  const auto autoplay_policy = GetAutoplayPolicyForWebContents(web_contents);
  const bool new_autoplay_policy_needed =
      prefs->autoplay_policy != autoplay_policy;
  if (new_autoplay_policy_needed)
    prefs->autoplay_policy = autoplay_policy;

  return new_autoplay_policy_needed ||
         UpdatePreferredColorScheme(prefs, web_contents->GetLastCommittedURL(),
                                    web_contents, GetWebTheme());
}

void ChromeContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  // The group policy NTP URL handler must be registered before the other NTP
  // URL handlers below. Also register it before the "parts" handlers, so the
  // NTP policy takes precedence over extensions that override the NTP.
  handler->AddHandlerPair(&HandleNewTabPageLocationOverride,
                          BrowserURLHandler::null_handler());

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->BrowserURLHandlerCreated(handler);

  // Handler to rewrite chrome://about and chrome://sync URLs.
  handler->AddHandlerPair(&HandleChromeAboutAndChromeSyncRewrite,
                          BrowserURLHandler::null_handler());

#if defined(OS_ANDROID)
  // Handler to rewrite chrome://newtab on Android.
  handler->AddHandlerPair(&chrome::android::HandleAndroidNativePageURL,
                          BrowserURLHandler::null_handler());
#else   // defined(OS_ANDROID)
  // Handler to rewrite chrome://newtab for InstantExtended.
  handler->AddHandlerPair(&search::HandleNewTabURLRewrite,
                          &search::HandleNewTabURLReverseRewrite);
#endif  // defined(OS_ANDROID)

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

base::FilePath ChromeContentBrowserClient::GetFontLookupTableCacheDir() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("FontLookupTableCache"));
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
  for (size_t i = 0; i < extra_parts_.size(); ++i) {
    extra_parts_[i]->GetAdditionalAllowedSchemesForFileSystem(
        additional_allowed_schemes);
  }
}

void ChromeContentBrowserClient::GetSchemesBypassingSecureContextCheckAllowlist(
    std::set<std::string>* schemes) {
  *schemes = secure_origin_allowlist::GetSchemesBypassingSecureContextCheck();
}

void ChromeContentBrowserClient::GetURLRequestAutoMountHandlers(
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->GetURLRequestAutoMountHandlers(handlers);
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
  auto backend = std::make_unique<chromeos::FileSystemBackend>(
      Profile::FromBrowserContext(browser_context),
      std::make_unique<ash::file_system_provider::BackendDelegate>(),
      std::make_unique<chromeos::MTPFileSystemBackendDelegate>(
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

  for (size_t i = 0; i < extra_parts_.size(); ++i) {
    extra_parts_[i]->GetAdditionalFileSystemBackends(
        browser_context, storage_partition_path,
        GetQuarantineConnectionCallback(), additional_backends);
  }
}

#if defined(OS_POSIX) && !defined(OS_MAC)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_POSIX) && !defined(OS_MAC)

#if defined(OS_WIN)
std::wstring ChromeContentBrowserClient::GetAppContainerSidForSandboxType(
    sandbox::policy::SandboxType sandbox_type) {
  // TODO(wfh): Add support for more process types here. crbug.com/499523
  switch (sandbox_type) {
    case sandbox::policy::SandboxType::kRenderer:
      return std::wstring(install_static::GetSandboxSidPrefix()) + L"129201922";
    case sandbox::policy::SandboxType::kUtility:
      return std::wstring();
    case sandbox::policy::SandboxType::kGpu:
      return std::wstring();
    case sandbox::policy::SandboxType::kPpapi:
      return std::wstring(install_static::GetSandboxSidPrefix()) + L"129201925";
    case sandbox::policy::SandboxType::kNoSandbox:
    case sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges:
    case sandbox::policy::SandboxType::kXrCompositing:
    case sandbox::policy::SandboxType::kNetwork:
    case sandbox::policy::SandboxType::kCdm:
#if BUILDFLAG(ENABLE_PRINTING)
    case sandbox::policy::SandboxType::kPrintBackend:
#endif
    case sandbox::policy::SandboxType::kPrintCompositor:
    case sandbox::policy::SandboxType::kAudio:
    case sandbox::policy::SandboxType::kSpeechRecognition:
    case sandbox::policy::SandboxType::kProxyResolver:
    case sandbox::policy::SandboxType::kPdfConversion:
    case sandbox::policy::SandboxType::kService:
    case sandbox::policy::SandboxType::kVideoCapture:
    case sandbox::policy::SandboxType::kIconReader:
    case sandbox::policy::SandboxType::kMediaFoundationCdm:
      // Should never reach here.
      CHECK(0);
      return std::wstring();
  }
}

std::wstring
ChromeContentBrowserClient::GetLPACCapabilityNameForNetworkService() {
  return std::wstring(L"lpacChromeNetworkSandbox");
}

// Note: Only use sparingly to add Chrome specific sandbox functionality here.
// Other code should reside in the content layer. Changes to this function
// should be reviewed by the security team.
bool ChromeContentBrowserClient::PreSpawnChild(
    sandbox::TargetPolicy* policy,
    sandbox::policy::SandboxType sandbox_type,
    ChildSpawnFlags flags) {
// Does not work under component build because all the component DLLs would need
// to be manually added and maintained. Does not work under ASAN build because
// ASAN has not yet fully initialized its instrumentation by the time the CIG
// intercepts run.
#if !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
  bool enforce_code_integrity = false;

  switch (sandbox_type) {
    case sandbox::policy::SandboxType::kRenderer:
      enforce_code_integrity =
          ((flags & ChildSpawnFlags::RENDERER_CODE_INTEGRITY) &&
           base::FeatureList::IsEnabled(kRendererCodeIntegrity));
      break;
    case sandbox::policy::SandboxType::kNetwork:
      enforce_code_integrity =
          base::FeatureList::IsEnabled(kNetworkServiceCodeIntegrity);
      break;
    case sandbox::policy::SandboxType::kUtility:
    case sandbox::policy::SandboxType::kGpu:
    case sandbox::policy::SandboxType::kPpapi:
    case sandbox::policy::SandboxType::kNoSandbox:
    case sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges:
    case sandbox::policy::SandboxType::kXrCompositing:
    case sandbox::policy::SandboxType::kCdm:
#if BUILDFLAG(ENABLE_PRINTING)
    case sandbox::policy::SandboxType::kPrintBackend:
#endif
    case sandbox::policy::SandboxType::kPrintCompositor:
    case sandbox::policy::SandboxType::kAudio:
    case sandbox::policy::SandboxType::kSpeechRecognition:
    case sandbox::policy::SandboxType::kProxyResolver:
    case sandbox::policy::SandboxType::kPdfConversion:
    case sandbox::policy::SandboxType::kService:
    case sandbox::policy::SandboxType::kVideoCapture:
    case sandbox::policy::SandboxType::kIconReader:
    case sandbox::policy::SandboxType::kMediaFoundationCdm:
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

  sandbox::MitigationFlags mitigations = policy->GetProcessMitigations();
  mitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
  sandbox::ResultCode result = policy->SetProcessMitigations(mitigations);
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Allow loading Chrome's DLLs.
  for (const auto* dll : {chrome::kBrowserResourcesDll, chrome::kElfDll}) {
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_SIGNED_BINARY,
                             sandbox::TargetPolicy::SIGNED_ALLOW_LOAD,
                             GetModulePath(dll).value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return false;
  }
#endif  // !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
  return true;
}

bool ChromeContentBrowserClient::IsRendererCodeIntegrityEnabled() {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state &&
      local_state->HasPrefPath(prefs::kRendererCodeIntegrityEnabled) &&
      !local_state->GetBoolean(prefs::kRendererCodeIntegrityEnabled))
    return false;
  return true;
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

void ChromeContentBrowserClient::SessionEnding() {
  chrome::SessionEnding();
}

bool ChromeContentBrowserClient::ShouldEnableAudioProcessHighPriority() {
  return IsAudioProcessHighPriorityEnabled();
}

#endif  // defined(OS_WIN)

void ChromeContentBrowserClient::
    RegisterMojoBinderPoliciesForSameOriginPrerendering(
        content::MojoBinderPolicyMap& policy_map) {
  // Changes to `policy_map` should be made in
  // RegisterChromeMojoBinderPoliciesForSameOriginPrerendering() which requires
  // security review.
  RegisterChromeMojoBinderPoliciesForSameOriginPrerendering(policy_map);
}

void ChromeContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context = site_instance->GetBrowserContext();

#if defined(OS_ANDROID)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MaybeAddThrottle(
      ash::WebTimeLimitNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  MaybeAddThrottle(
      SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

#if defined(OS_ANDROID)
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

#elif BUILDFLAG(ENABLE_EXTENSIONS)
  if (handle->IsInMainFrame()) {
    // Redirect some navigations to apps that have registered matching URL
    // handlers ('url_handlers' in the manifest).
    MaybeAddThrottle(
        PlatformAppNavigationRedirector::MaybeCreateThrottleFor(handle),
        &throttles);
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of main frames.
  if (handle->IsInMainFrame()) {
    // Add interstitial page while merge session process (cookie reconstruction
    // from OAuth2 refresh token in ChromeOS login) is still in progress while
    // we are attempting to load a google property.
    if (merge_session_throttling_utils::ShouldAttachNavigationThrottle() &&
        !merge_session_throttling_utils::AreAllSessionMergedAlready() &&
        handle->GetURL().SchemeIsHTTPOrHTTPS()) {
      throttles.push_back(MergeSessionNavigationThrottle::Create(handle));
    }
  }
#endif

#if !defined(OS_ANDROID)
  auto url_to_apps_throttle =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      apps::CommonAppsNavigationThrottle::MaybeCreate(handle);
#else
      apps::AppsNavigationThrottle::MaybeCreate(handle);
#endif
  if (url_to_apps_throttle)
    throttles.push_back(std::move(url_to_apps_throttle));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  throttles.push_back(
      std::make_unique<extensions::ExtensionNavigationThrottle>(handle));

  MaybeAddThrottle(extensions::ExtensionsBrowserClient::Get()
                       ->GetUserScriptListener()
                       ->CreateNavigationThrottle(handle),
                   &throttles);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  MaybeAddThrottle(
      SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(handle),
      &throttles);
#endif

  content::WebContents* web_contents = handle->GetWebContents();
  if (auto* throttle_manager =
          subresource_filter::ContentSubresourceFilterThrottleManager::
              FromWebContents(web_contents)) {
    throttle_manager->MaybeAppendNavigationThrottles(handle, &throttles);
  }

#if !defined(OS_ANDROID)
  // BackgroundTabNavigationThrottle is used by TabManager, which is only
  // enabled on non-Android platforms.
  MaybeAddThrottle(resource_coordinator::BackgroundTabNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  MaybeAddThrottle(safe_browsing::MaybeCreateNavigationThrottle(handle),
                   &throttles);
#endif

  MaybeAddThrottle(
      LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(handle),
      &throttles);

  MaybeAddThrottle(PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);
#if BUILDFLAG(ENABLE_PDF)
  MaybeAddThrottle(pdf::PdfNavigationThrottle::MaybeCreateThrottleFor(
                       handle, std::make_unique<ChromePdfStreamDelegate>()),
                   &throttles);
#endif  // BUILDFLAG(ENABLE_PDF)

  MaybeAddThrottle(TabUnderNavigationThrottle::MaybeCreate(handle), &throttles);

  MaybeAddThrottle(
      WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);

  policy::PolicyService* policy_service = nullptr;
  Profile* profile = Profile::FromBrowserContext(
      handle->GetWebContents()->GetBrowserContext());
  if (profile && profile->GetProfilePolicyConnector())
    policy_service = profile->GetProfilePolicyConnector()->policy_service();

  throttles.push_back(std::make_unique<PolicyBlocklistNavigationThrottle>(
      handle, handle->GetWebContents()->GetBrowserContext(), policy_service));

  // Before setting up SSL error detection, configure SSLErrorHandler to invoke
  // the relevant extension API whenever an SSL interstitial is shown.
  SSLErrorHandler::SetClientCallbackOnInterstitialsShown(
      base::BindRepeating(&MaybeTriggerSecurityInterstitialShownEvent));
  throttles.push_back(std::make_unique<SSLErrorNavigationThrottle>(
      handle,
      std::make_unique<CertificateReportingServiceCertReporter>(web_contents),
      base::BindOnce(&HandleSSLErrorWrapper), base::BindOnce(&IsInHostedApp),
      base::BindOnce(
          &ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps)));

  throttles.push_back(std::make_unique<LoginNavigationThrottle>(handle));

  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps)) {
    MaybeAddThrottle(
        TypedNavigationUpgradeThrottle::MaybeCreateThrottleFor(handle),
        &throttles);
  }

#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN)
  MaybeAddThrottle(enterprise_connectors::DeviceTrustNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif  // defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN)

#if !defined(OS_ANDROID)
  MaybeAddThrottle(DevToolsWindow::MaybeCreateNavigationThrottle(handle),
                   &throttles);

  MaybeAddThrottle(NewTabPageNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

  throttles.push_back(
      std::make_unique<safe_browsing::SafeBrowsingNavigationThrottle>(handle));

  if (base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings)) {
    throttles.push_back(
        std::make_unique<safe_browsing::DelayedWarningNavigationThrottle>(
            handle));
  }

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  MaybeAddThrottle(browser_switcher::BrowserSwitcherNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MaybeAddThrottle(
      ash::KioskSettingsNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

#if defined(OS_MAC)
  if (__builtin_available(macOS 10.15, *)) {
    MaybeAddThrottle(MaybeCreateAuthSessionThrottleFor(handle), &throttles);
  }
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

  if (IsErrorPageAutoReloadEnabled() && handle->IsInMainFrame()) {
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

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  MaybeAddThrottle(
      offline_pages::OfflinePageNavigationThrottle::MaybeCreateThrottleFor(
          handle),
      &throttles);
#endif

  if (profile && profile->GetPrefs()) {
    MaybeAddThrottle(
        HttpsOnlyModeNavigationThrottle::MaybeCreateThrottleFor(
            handle, std::make_unique<ChromeSecurityBlockingPageFactory>(),
            profile->GetPrefs()),
        &throttles);
  }

  return throttles;
}

std::unique_ptr<content::NavigationUIData>
ChromeContentBrowserClient::GetNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  return std::make_unique<ChromeNavigationUIData>(navigation_handle);
}

std::unique_ptr<content::DevToolsManagerDelegate>
ChromeContentBrowserClient::CreateDevToolsManagerDelegate() {
#if defined(OS_ANDROID)
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

  DictionaryPrefUpdate pref_update(
      pref_service, prefs::kDevToolsBackgroundServicesExpirationDict);
  base::DictionaryValue* exp_dict = pref_update.Get();

  // Convert |expiration_time| to minutes since that is the most granular
  // option that returns an int. base::Value does not accept int64.
  int expiration_time_minutes =
      expiration_time.ToDeltaSinceWindowsEpoch().InMinutes();
  exp_dict->SetInteger(base::NumberToString(service), expiration_time_minutes);
}

base::flat_map<int, base::Time>
ChromeContentBrowserClient::GetDevToolsBackgroundServiceExpirations(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  auto* pref_service = profile->GetPrefs();
  DCHECK(pref_service);

  auto* expiration_dict = pref_service->GetDictionary(
      prefs::kDevToolsBackgroundServicesExpirationDict);
  DCHECK(expiration_dict);

  base::flat_map<int, base::Time> expiration_times;
  for (auto it : expiration_dict->DictItems()) {
    // key.
    int service = 0;
    bool did_convert = base::StringToInt(it.first, &service);
    DCHECK(did_convert);

    // value.
    DCHECK(it.second.is_int());
    base::TimeDelta delta = base::TimeDelta::FromMinutes(it.second.GetInt());
    base::Time expiration_time = base::Time::FromDeltaSinceWindowsEpoch(delta);

    expiration_times[service] = expiration_time;
  }

  return expiration_times;
}

content::TracingDelegate* ChromeContentBrowserClient::GetTracingDelegate() {
  return new ChromeTracingDelegate();
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

void ChromeContentBrowserClient::OverridePageVisibilityState(
    RenderFrameHost* render_frame_host,
    content::PageVisibilityState* visibility_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrerendering(web_contents)) {
    *visibility_state = content::PageVisibilityState::kHiddenButPainting;
  }
}

void ChromeContentBrowserClient::InitNetworkContextsParentDirectory() {
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
#if defined(OS_ANDROID)
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
    to_command_line->CopySwitchesFrom(from_command_line, kWebRtcDevSwitchNames,
                                      base::size(kWebRtcDevSwitchNames));
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

namespace {
// TODO(jam): move this to a separate file.
class ProtocolHandlerThrottle : public blink::URLLoaderThrottle {
 public:
  explicit ProtocolHandlerThrottle(
      ProtocolHandlerRegistry* protocol_handler_registry)
      : protocol_handler_registry_(protocol_handler_registry) {
    DCHECK(protocol_handler_registry);
  }
  ~ProtocolHandlerThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    // Don't translate the urn: scheme URL while loading the resource from the
    // specified web bundle.
    if (request->web_bundle_token_params &&
        request->url.SchemeIs(url::kUrnScheme)) {
      return;
    }
    TranslateUrl(&request->url);
  }

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override {
    TranslateUrl(&redirect_info->new_url);
  }

 private:
  void TranslateUrl(GURL* url) {
    if (!protocol_handler_registry_->IsHandledProtocol(url->scheme()))
      return;
    GURL translated_url = protocol_handler_registry_->Translate(*url);
    if (!translated_url.is_empty())
      *url = translated_url;
  }

  ProtocolHandlerRegistry* protocol_handler_registry_;
};
}  // namespace

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ChromeContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  DCHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
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
            profile->GetPrefs(), has_valid_dm_token, profile->IsOffTheRecord());
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

    result.push_back(safe_browsing::BrowserURLLoaderThrottle::Create(
        base::BindOnce(
            &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
            base::Unretained(this),
            safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs()),
            // Should check for enterprise when safe browsing is disabled.
            /*should_check_on_sb_disabled=*/is_enterprise_lookup_enabled,
            safe_browsing::GetURLAllowlistByPolicy(profile->GetPrefs())),
        wc_getter, frame_tree_node_id,
        url_lookup_service ? url_lookup_service->GetWeakPtr() : nullptr));
  }
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  result.push_back(
      std::make_unique<captive_portal::CaptivePortalURLLoaderThrottle>(
          wc_getter.Run()));
#endif

  if (chrome_navigation_ui_data &&
      chrome_navigation_ui_data->is_no_state_prefetching()) {
    result.push_back(std::make_unique<prerender::PrerenderURLLoaderThrottle>(
        chrome_navigation_ui_data->prerender_histogram_prefix(),
        GetPrerenderCanceler(wc_getter)));
  }

#if defined(OS_ANDROID)
  std::string client_data_header;
  bool is_tab_large_enough = false;
  bool is_custom_tab = false;
  if (frame_tree_node_id != content::RenderFrameHost::kNoFrameTreeNodeId) {
    auto* web_contents = WebContents::FromFrameTreeNodeId(frame_tree_node_id);
    // Could be null if the FrameTreeNode's RenderFrameHost is shutting down.
    if (web_contents) {
      auto* client_data_header_observer =
          customtabs::ClientDataHeaderWebContentsObserver::FromWebContents(
              web_contents);
      if (client_data_header_observer)
        client_data_header = client_data_header_observer->header();

      auto* delegate =
          TabAndroid::FromWebContents(web_contents)
              ? static_cast<android::TabWebContentsDelegateAndroid*>(
                    web_contents->GetDelegate())
              : nullptr;
      if (delegate) {
        is_tab_large_enough = delegate->IsTabLargeEnoughForDesktopSite();
        is_custom_tab = delegate->IsCustomTab();
      }
    }
  }
#endif

  chrome::mojom::DynamicParams dynamic_params = {
      profile->GetPrefs()->GetBoolean(prefs::kForceGoogleSafeSearch),
      profile->GetPrefs()->GetInteger(prefs::kForceYouTubeRestrict),
      profile->GetPrefs()->GetString(prefs::kAllowedDomainsForApps)};
  result.push_back(std::make_unique<GoogleURLLoaderThrottle>(
#if defined(OS_ANDROID)
      client_data_header, is_tab_large_enough,
#endif
      std::move(dynamic_params)));

  {
    auto* factory =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(browser_context);
    // null in unit tests.
    if (factory)
      result.push_back(std::make_unique<ProtocolHandlerThrottle>(factory));
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  result.push_back(std::make_unique<PluginResponseInterceptorURLLoaderThrottle>(
      request.destination, frame_tree_node_id));
#endif

#if defined(OS_ANDROID)
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

void ChromeContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories) {
#if BUILDFLAG(ENABLE_EXTENSIONS) || BUILDFLAG(IS_CHROMEOS_ASH)
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(), ukm_source_id,
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  factories->emplace(content::kExternalFileScheme,
                     chromeos::ExternalFileURLLoaderFactory::Create(
                         profile, content::ChildProcessHost::kInvalidUniqueID));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeContentBrowserClient::
    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionWorkerMainResourceURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void ChromeContentBrowserClient::
    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
  DCHECK(browser_context);
  DCHECK(factories);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionServiceWorkerScriptURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
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
  DISALLOW_COPY_AND_ASSIGN(SpecialAccessFileURLLoaderFactory);
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsSystemFeatureDisabled(policy::SystemFeature system_feature) {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return false;

  const base::ListValue* disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  if (!disabled_system_features_pref)
    return false;

  const auto disabled_system_features =
      disabled_system_features_pref->GetList();
  return base::Contains(disabled_system_features, base::Value(system_feature));
}

bool IsSystemFeatureURLDisabled(const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme))
    return false;

  // chrome://os-settings/pwa.html shouldn't be replaced to let the settings app
  // installation complete successfully.
  if (url.DomainIs(chrome::kChromeUIOSSettingsHost) &&
      url.path() != "/pwa.html" &&
      IsSystemFeatureDisabled(policy::SystemFeature::kOsSettings)) {
    return true;
  }

  if (url.DomainIs(chrome::kChromeUISettingsHost) &&
      IsSystemFeatureDisabled(policy::SystemFeature::kBrowserSettings)) {
    return true;
  }

  if (url.DomainIs(ash::kChromeUIScanningAppHost) &&
      IsSystemFeatureDisabled(policy::SystemFeature::kScanning)) {
    return true;
  }

  if (url.DomainIs(chromeos::kChromeUICameraAppHost) &&
      IsSystemFeatureDisabled(policy::SystemFeature::kCamera)) {
    return true;
  }

  return false;
}
#endif
}  // namespace

void ChromeContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        NonNetworkURLLoaderFactoryMap* factories) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_EXTENSIONS)
  content::RenderFrameHost* frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(frame_host);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    factories->emplace(content::kExternalFileScheme,
                       chromeos::ExternalFileURLLoaderFactory::Create(
                           profile, render_process_id));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  factories->emplace(extensions::kExtensionScheme,
                     extensions::CreateExtensionURLLoaderFactory(
                         render_process_id, render_frame_id));

  // This logic should match
  // ChromeExtensionWebContentsObserver::RenderFrameCreated.
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  // The test below matches when a remote 3P NTP is loaded. The effective
  // URL is chrome-search://remote-ntp. This is to allow the use of the NTP
  // public api and to embed most-visited tiles
  // (chrome-search://most-visited/title.html).
  if (instant_service->IsInstantProcess(render_process_id)) {
    factories->emplace(
        chrome::kChromeSearchScheme,
        content::CreateWebUIURLLoaderFactory(
            frame_host, chrome::kChromeSearchScheme,
            /*allowed_webui_hosts=*/base::flat_set<std::string>()));
  }

  extensions::ChromeExtensionWebContentsObserver* web_observer =
      extensions::ChromeExtensionWebContentsObserver::FromWebContents(
          web_contents);

  // There is nothing to do if no ChromeExtensionWebContentsObserver is attached
  // to the |web_contents|.
  if (!web_observer)
    return;

  const Extension* extension =
      web_observer->GetExtensionFromFrame(frame_host, false);
  if (!extension)
    return;

  std::vector<std::string> allowed_webui_hosts;
  // Support for chrome:// scheme if appropriate.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      Manifest::IsComponentLocation(extension->location())) {
    // Components of chrome that are implemented as extensions or platform apps
    // are allowed to use chrome://resources/ and chrome://theme/ URLs.
    allowed_webui_hosts.emplace_back(content::kChromeUIResourcesHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIThemeHost);
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

  // Extensions with the necessary permissions get access to file:// URLs that
  // gets approval from ChildProcessSecurityPolicy. Keep this logic in sync with
  // ExtensionWebContentsObserver::RenderFrameCreated.
  Manifest::Type type = extension->GetType();
  if ((type == Manifest::TYPE_EXTENSION ||
       type == Manifest::TYPE_LEGACY_PACKAGED_APP) &&
      extensions::util::AllowFileAccess(extension->id(),
                                        web_contents->GetBrowserContext())) {
    factories->emplace(
        url::kFileScheme,
        SpecialAccessFileURLLoaderFactory::Create(render_process_id));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ChromeContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  bool use_proxy = false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (web_request_api) {
    bool use_proxy_for_web_request =
        web_request_api->MaybeProxyURLLoaderFactory(
            browser_context, frame, render_process_id, type,
            std::move(navigation_id), ukm_source_id, factory_receiver,
            header_client);
    if (bypass_redirect_checks)
      *bypass_redirect_checks = use_proxy_for_web_request;
    use_proxy |= use_proxy_for_web_request;
  }
#endif

  use_proxy |= signin::ProxyingURLLoaderFactory::MaybeProxyRequest(
      frame, type == URLLoaderFactoryType::kNavigation, request_initiator,
      factory_receiver);

  auto* prefetch_proxy_service = PrefetchProxyServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  // |frame| is null when |type| is service worker.
  if (frame && prefetch_proxy_service) {
    use_proxy |= prefetch_proxy_service->MaybeProxyURLLoaderFactory(
        frame, render_process_id, type, factory_receiver);
  }

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

  return use_proxy;
}

std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
ChromeContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory) {
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

  if (base::FeatureList::IsEnabled(features::kIsolatePrerenders)) {
    interceptors.push_back(std::make_unique<PrefetchProxyURLLoaderInterceptor>(
        frame_tree_node_id));
  }

  if (SearchPrefetchServiceIsEnabled()) {
    interceptors.push_back(std::make_unique<SearchPrefetchURLLoaderInterceptor>(
        frame_tree_node_id));
  }

  if (base::FeatureList::IsEnabled(features::kHttpsOnlyMode)) {
    interceptors.push_back(
        std::make_unique<HttpsOnlyModeUpgradeInterceptor>(frame_tree_node_id));
  }

  return interceptors;
}

content::ContentBrowserClient::URLLoaderRequestHandler
ChromeContentBrowserClient::
    CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
        int frame_tree_node_id,
        const network::ResourceRequest& resource_request) {
  content::ContentBrowserClient::URLLoaderRequestHandler callback;

  // If search prefetch is disabled, nothing needs to be handled.
  if (!SearchPrefetchServiceIsEnabled()) {
    return callback;
  }

  std::unique_ptr<SearchPrefetchURLLoader> loader =
      SearchPrefetchURLLoaderInterceptor::MaybeCreateLoaderForRequest(
          resource_request, frame_tree_node_id);
  if (!loader) {
    return callback;
  }

  auto* raw_loader = loader.get();

  // Hand ownership of the loader to the callback, when it runs, mojo will
  // manage it. If the callback is deleted, the loader will be deleted.
  callback = raw_loader->ServingResponseHandler(std::move(loader));
  return callback;
}

bool ChromeContentBrowserClient::WillInterceptWebSocket(
    content::RenderFrameHost* frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!frame) {
    return false;
  }
  const auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          frame->GetProcess()->GetBrowserContext());

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (!web_request_api)
    return false;

  return web_request_api->MayHaveProxies();
#else
  return false;
#endif
}

void ChromeContentBrowserClient::CreateWebSocket(
    content::RenderFrameHost* frame,
    WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!frame) {
    return;
  }
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          frame->GetProcess()->GetBrowserContext());

  DCHECK(web_request_api);
  web_request_api->ProxyWebSocket(frame, std::move(factory), url,
                                  site_for_cookies.RepresentativeUrl(),
                                  user_agent, std::move(handshake_client));
#endif
}

void ChromeContentBrowserClient::WillCreateWebTransport(
    content::RenderFrameHost* frame,
    const GURL& url,
    WillCreateWebTransportCallback callback) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (!frame) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  int frame_tree_node_id = frame->GetFrameTreeNodeId();
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  auto checker = std::make_unique<safe_browsing::WebApiHandshakeChecker>(
      base::BindOnce(
          &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
          base::Unretained(this),
          safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs()),
          /*should_check_on_sb_disabled=*/false,
          safe_browsing::GetURLAllowlistByPolicy(profile->GetPrefs())),
      base::BindRepeating(&content::WebContents::FromFrameTreeNodeId,
                          frame_tree_node_id),
      frame_tree_node_id);
  auto* raw_checker = checker.get();
  raw_checker->Check(
      url,
      base::BindOnce(
          &ChromeContentBrowserClient::SafeBrowsingWebApiHandshakeChecked,
          weak_factory_.GetWeakPtr(), std::move(checker), std::move(callback)));
#else
  std::move(callback).Run(absl::nullopt);
#endif
}

void ChromeContentBrowserClient::SafeBrowsingWebApiHandshakeChecked(
    std::unique_ptr<safe_browsing::WebApiHandshakeChecker> checker,
    WillCreateWebTransportCallback callback,
    safe_browsing::WebApiHandshakeChecker::CheckResult result) {
  if (result == safe_browsing::WebApiHandshakeChecker::CheckResult::kProceed) {
    std::move(callback).Run(absl::nullopt);
  } else {
    std::move(callback).Run(network::mojom::WebTransportError::New(
        net::ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
        "SafeBrowsing check failed", false));
  }
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

  if (!data_use_measurement::ChromeDataUseMeasurement::GetInstance())
    data_use_measurement::ChromeDataUseMeasurement::CreateInstance(local_state);

  // Create SystemNetworkContextManager if it has not been created yet. We need
  // to set up global NetworkService state before anything else uses it and this
  // is the first opportunity to initialize SystemNetworkContextManager with the
  // NetworkService.
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(local_state);

  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
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
    network_context_params->user_agent = GetUserAgent();
    network_context_params->accept_language = GetApplicationLocale();
  }
}

std::vector<base::FilePath>
ChromeContentBrowserClient::GetNetworkContextsParentDirectory() {
  DCHECK(!network_contexts_parent_directory_.empty());
  return network_contexts_parent_directory_;
}

base::DictionaryValue ChromeContentBrowserClient::GetNetLogConstants() {
  auto platform_dict = net_log::GetPlatformConstantsForNetLog(
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
  if (platform_dict)
    return std::move(*platform_dict);
  else
    return base::DictionaryValue();
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
    const GURL& url,
    const std::string& mime_type) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Special-case user scripts to get downloaded instead of viewed.
  return extensions::UserScript::IsURLUserScript(url, mime_type);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::CreateWebUsbService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    return;

  CHECK(render_frame_host);
  FrameUsbServices::CreateFrameUsbServices(render_frame_host,
                                           std::move(receiver));
}

content::BluetoothDelegate* ChromeContentBrowserClient::GetBluetoothDelegate() {
  if (!bluetooth_delegate_)
    bluetooth_delegate_ = std::make_unique<ChromeBluetoothDelegate>();
  return bluetooth_delegate_.get();
}

#if !defined(OS_ANDROID)
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

content::FontAccessDelegate*
ChromeContentBrowserClient::GetFontAccessDelegate() {
  if (!font_access_delegate_)
    font_access_delegate_ = std::make_unique<ChromeFontAccessDelegate>();
  return static_cast<content::FontAccessDelegate*>(font_access_delegate_.get());
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

void ChromeContentBrowserClient::ShowDirectSocketsConnectionDialog(
    content::RenderFrameHost* owner,
    const std::string& address,
    base::OnceCallback<void(bool, const std::string&, const std::string&)>
        callback) {
  auto* contents = content::WebContents::FromRenderFrameHost(owner);
  if (!contents)
    return;

  auto* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser ||
      browser->tab_strip_model()->GetActiveWebContents() != contents) {
    std::move(callback).Run(false, std::string(), std::string());
    return;
  }

  chrome::ShowDirectSocketsConnectionDialog(browser, address,
                                            std::move(callback));
}
#endif

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
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::SystemProxyManager* system_proxy_manager =
      chromeos::SystemProxyManager::Get();
  // For Managed Guest Session and Kiosk devices, the credentials configured
  // via the policy SystemProxySettings may be used for proxy authentication.
  // Note: |system_proxy_manager| may be missing in tests.
  if (system_proxy_manager && system_proxy_manager->CanUsePolicyCredentials(
                                  auth_info, first_auth_attempt)) {
    return system_proxy_manager->CreateLoginDelegate(
        std::move(auth_required_callback));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // For subresources, create a LoginHandler directly, which may show a login
  // prompt to the user. Main frame resources go through LoginTabHelper, which
  // manages a more complicated flow to avoid confusion about which website is
  // showing the prompt.
  if (is_request_for_main_frame) {
    LoginTabHelper::CreateForWebContents(web_contents);
    return LoginTabHelper::FromWebContents(web_contents)
        ->CreateAndStartMainFrameLoginDelegate(
            auth_info, web_contents, request_id, url, response_headers,
            std::move(auth_required_callback));
  }
  std::unique_ptr<LoginHandler> login_handler = LoginHandler::Create(
      auth_info, web_contents, std::move(auth_required_callback));
  login_handler->StartSubresource(request_id, url, response_headers);
  return login_handler;
}

bool ChromeContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    int child_id,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const absl::optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // External protocols are disabled for guests. An exception is made for the
  // "mailto" protocol, so that pages that utilize it work properly in a
  // WebView.
  ChromeNavigationUIData* chrome_data =
      static_cast<ChromeNavigationUIData*>(navigation_data);
  if ((extensions::WebViewRendererState::GetInstance()->IsGuest(child_id) ||
       (chrome_data &&
        chrome_data->GetExtensionNavigationUIData()->is_web_view())) &&
      !url.SchemeIs(url::kMailToScheme)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_ANDROID)
  // Main frame external protocols are handled by
  // InterceptNavigationResourceThrottle.
  if (is_main_frame)
    return false;
#endif  // defined(ANDROID)

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LaunchURL, url, std::move(web_contents_getter),
                     page_transition, has_user_gesture, initiating_origin));
  return true;
}

std::unique_ptr<content::OverlayWindow>
ChromeContentBrowserClient::CreateWindowForPictureInPicture(
    content::PictureInPictureWindowController* controller) {
  // Note: content::OverlayWindow::Create() is defined by platform-specific
  // implementation in chrome/browser/ui/views. This layering hack, which goes
  // through //content and ContentBrowserClient, allows us to work around the
  // dependency constraints that disallow directly calling
  // chrome/browser/ui/views code either from here or from other code in
  // chrome/browser.
  return content::OverlayWindow::Create(controller);
}

void ChromeContentBrowserClient::RegisterRendererPreferenceWatcher(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefWatcher::Get(profile)->RegisterRendererPreferenceWatcher(
      std::move(watcher));
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

  // Replace deprecated cookie settings URL with the current version.
  if (*url == GURL(chrome::kChromeUICookieSettingsDeprecatedURL)) {
    *url = GURL(chrome::kChromeUICookieSettingsURL);
  }

#if defined(OS_WIN)
  // TODO(crbug.com/1003960): Remove when issue is resolved.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIWelcomeWin10Host) {
    url::Replacements<char> replacements;
    replacements.SetHost(
        chrome::kChromeUIWelcomeHost,
        url::Component(0, strlen(chrome::kChromeUIWelcomeHost)));
    *url = url->ReplaceComponents(replacements);
    return true;
  }
#endif  // defined(OS_WIN)

  if (!ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
          browser_context, *url)) {
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

  if (IsSystemFeatureURLDisabled(*url)) {
    *url = ReplaceURLHostAndPath(*url, chrome::kChromeUIAppDisabledHost, "");
    return true;
  }

#endif

  return true;
}

bool ChromeContentBrowserClient::ShowPaymentHandlerWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(bool, int, int)> callback) {
#if defined(OS_ANDROID)
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
#if defined(OS_WIN)
  // TODO(crbug.com/1003960): Remove when issue is resolved.
  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://welcome-win10 to chrome://welcome.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIWelcomeHost) {
    return true;
  }
#endif  // defined(OS_WIN)

  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://help to chrome://settings/help.
  return url->SchemeIs(content::kChromeUIScheme) &&
         url->host() == chrome::kChromeUISettingsHost;
}

const ui::NativeTheme* ChromeContentBrowserClient::GetWebTheme() const {
  return ui::NativeTheme::GetInstanceForWeb();
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate(
    bool safe_browsing_enabled_for_profile,
    bool should_check_on_sb_disabled,
    const std::vector<std::string>& allowlist_domains) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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

absl::optional<std::string>
ChromeContentBrowserClient::GetOriginPolicyErrorPage(
    network::OriginPolicyState error_reason,
    content::NavigationHandle* handle) {
  return security_interstitials::OriginPolicyUI::GetErrorPageAsHTML(
      error_reason, handle);
}

bool ChromeContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  // We require --user-data-dir flag too so that no dangerous changes are made
  // in the user's regular profile.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUserDataDir);
}

void ChromeContentBrowserClient::OnNetworkServiceDataUseUpdate(
    int process_id,
    int routing_id,
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  if (data_use_measurement::ChromeDataUseMeasurement::GetInstance()) {
    data_use_measurement::ChromeDataUseMeasurement::GetInstance()
        ->ReportNetworkServiceDataUse(network_traffic_annotation_id_hash,
                                      recv_bytes, sent_bytes);
  }
#if !defined(OS_ANDROID)
  task_manager::TaskManagerInterface::UpdateAccumulatedStatsNetworkForRoute(
      process_id, routing_id, recv_bytes, sent_bytes);
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

void ChromeContentBrowserClient::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, feature);
}

std::string ChromeContentBrowserClient::GetProduct() {
  return embedder_support::GetProduct();
}

std::string ChromeContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

blink::UserAgentMetadata ChromeContentBrowserClient::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata();
}

absl::optional<gfx::ImageSkia> ChromeContentBrowserClient::GetProductLogo() {
  // This icon is available on Android, but adds 19KiB to the APK. Since it
  // isn't used on Android we exclude it to avoid bloat.
#if !defined(OS_ANDROID)
  return absl::optional<gfx::ImageSkia>(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PRODUCT_LOGO_256));
#else
  return absl::nullopt;
#endif
}

bool ChromeContentBrowserClient::IsBuiltinComponent(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::IsBuiltinComponent(
      browser_context, origin);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldBlockRendererDebugURL(
    const GURL& url,
    content::BrowserContext* context) {
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(context);

  using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
  URLBlocklistState blocklist_state = service->GetURLBlocklistState(url);
  return blocklist_state == URLBlocklistState::URL_IN_BLOCKLIST;
}

ui::AXMode ChromeContentBrowserClient::GetAXModeForBrowserContext(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return AccessibilityLabelsServiceFactory::GetForProfile(profile)->GetAXMode();
}

#if defined(OS_ANDROID)
content::ContentBrowserClient::WideColorGamutHeuristic
ChromeContentBrowserClient::GetWideColorGamutHeuristic() {
  if (viz::AlwaysUseWideColorGamut() ||
      features::IsDynamicColorGamutEnabled()) {
    return WideColorGamutHeuristic::kUseDisplay;
  }

  if (display::Display::HasForceDisplayColorProfile() &&
      display::Display::GetForcedDisplayColorProfile() ==
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
  if (pdf::IsInternalPluginExternallyHandled())
    mime_types.insert(pdf::kInternalPluginMimeType);
#endif
  return mime_types;
}

void ChromeContentBrowserClient::AugmentNavigationDownloadPolicy(
    content::WebContents* web_contents,
    content::RenderFrameHost* frame_host,
    bool user_gesture,
    blink::NavigationDownloadPolicy* download_policy) {
  const auto* throttle_manager = subresource_filter::
      ContentSubresourceFilterThrottleManager::FromWebContents(web_contents);
  if (throttle_manager && throttle_manager->IsFrameTaggedAsAd(frame_host)) {
    download_policy->SetAllowed(blink::NavigationDownloadType::kAdFrame);
    if (!user_gesture) {
      if (base::FeatureList::IsEnabled(
              blink::features::
                  kBlockingDownloadsInAdFrameWithoutUserActivation)) {
        download_policy->SetDisallowed(
            blink::NavigationDownloadType::kAdFrameNoGesture);
      } else {
        download_policy->SetAllowed(
            blink::NavigationDownloadType::kAdFrameNoGesture);
      }
    }
  }
}

blink::mojom::InterestCohortPtr
ChromeContentBrowserClient::GetInterestCohortForJsApi(
    content::WebContents* web_contents,
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin) {
  federated_learning::FlocEligibilityObserver::GetOrCreateForCurrentDocument(
      web_contents->GetMainFrame())
      ->OnInterestCohortApiUsed();

  federated_learning::FlocIdProvider* floc_id_provider =
      federated_learning::FlocIdProviderFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  if (!floc_id_provider)
    return blink::mojom::InterestCohort::New();

  return floc_id_provider->GetInterestCohortForJsApi(url, top_frame_origin);
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

bool ChromeContentBrowserClient::ShouldLoadExtraIcuDataFile(
    std::string* split_name) {
#if defined(OS_ANDROID)
  *split_name = "extra_icu";
  return extra_icu::ModuleProvider::IsModuleInstalled();
#endif
  return false;
}

bool ChromeContentBrowserClient::ArePersistentMediaDeviceIDsAllowed(
    content::BrowserContext* browser_context,
    const GURL& url,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) {
  // Persistent MediaDevice IDs are allowed if cookies are allowed.
  return CookieSettingsFactory::GetForProfile(
             Profile::FromBrowserContext(browser_context))
      ->IsFullCookieAccessAllowed(url, site_for_cookies, top_frame_origin);
}

#if !defined(OS_ANDROID)
base::OnceClosure ChromeContentBrowserClient::FetchRemoteSms(
    content::WebContents* web_contents,
    const std::vector<url::Origin>& origin_list,
    base::OnceCallback<void(absl::optional<std::vector<url::Origin>>,
                            absl::optional<std::string>,
                            absl::optional<content::SmsFetchFailureType>)>
        callback) {
  return ::FetchRemoteSms(web_contents, origin_list, std::move(callback));
}
#endif

bool ChromeContentBrowserClient::IsClipboardPasteAllowed(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  const GURL& url = render_frame_host->GetLastCommittedOrigin().GetURL();
  content::BrowserContext* browser_context =
      WebContents::FromRenderFrameHost(render_frame_host)->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  content::PermissionController* permission_controller =
      browser_context->GetPermissionController();
  blink::mojom::PermissionStatus status =
      permission_controller->GetPermissionStatusForFrame(
          content::PermissionType::CLIPBOARD_READ_WRITE, render_frame_host,
          url);

  // True if this paste is executed from an extension URL with read permission.
  bool is_extension_paste_allowed = false;
  // True if any active extension can use content scripts to read on this page.
  bool is_content_script_paste_allowed = false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(https://crbug.com/982361): Provide proper browser-side content script
  // tracking below, possibly based on hooks like those in
  // URLLoaderFactoryManager's WillExecuteCode() and ReadyToCommitNavigation().
  // Until this is implemented, platforms supporting extensions (all  platforms
  // except Android) will essentially no-op here and return true.
  is_content_script_paste_allowed = true;
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    auto* process_map = extensions::ProcessMap::Get(profile);
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    is_extension_paste_allowed = URLHasExtensionPermission(
        process_map, registry, url, render_frame_host->GetProcess()->GetID(),
        APIPermissionID::kClipboardRead);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  if (!is_extension_paste_allowed && !is_content_script_paste_allowed &&
      !render_frame_host->HasTransientUserActivation() &&
      status != blink::mojom::PermissionStatus::GRANTED) {
    // Paste requires either (1) origination from a chrome extension, (2) user
    // activation, or (3) granted web permission.
    return false;
  }
  return true;
}

void ChromeContentBrowserClient::IsClipboardPasteContentAllowed(
    content::WebContents* web_contents,
    const GURL& url,
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteContentAllowedCallback callback) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Safe browsing does not support images, so accept without checking.
  // TODO(crbug.com/1013584): check policy on what to do about unsupported
  // types when it is implemented.
  if (data_type == ui::ClipboardFormatType::GetBitmapType()) {
    std::move(callback).Run(ClipboardPasteContentAllowed(true));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  enterprise_connectors::ContentAnalysisDelegate::Data dialog_data;
  if (enterprise_connectors::ContentAnalysisDelegate::IsEnabled(
          profile, url, &dialog_data,
          enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY)) {
    dialog_data.text.push_back(data);
    enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
        web_contents, std::move(dialog_data),
        base::BindOnce(
            [](IsClipboardPasteContentAllowedCallback callback,
               const enterprise_connectors::ContentAnalysisDelegate::Data& data,
               const enterprise_connectors::ContentAnalysisDelegate::Result&
                   result) {
              std::move(callback).Run(
                  ClipboardPasteContentAllowed(result.text_results[0]));
            },
            std::move(callback)),
        safe_browsing::DeepScanAccessPoint::PASTE);
  } else {
    std::move(callback).Run(ClipboardPasteContentAllowed(true));
  }
#else
  std::move(callback).Run(ClipboardPasteContentAllowed(true));
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

#if BUILDFLAG(ENABLE_PLUGINS)
bool ChromeContentBrowserClient::ShouldAllowPluginCreation(
    const url::Origin& embedder_origin,
    const content::PepperPluginInfo& plugin_info) {
#if BUILDFLAG(ENABLE_PDF)
  if (plugin_info.name == ChromeContentClient::kPDFInternalPluginName) {
    return IsPdfInternalPluginAllowedOrigin(embedder_origin);
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_VR)
content::XrIntegrationClient*
ChromeContentBrowserClient::GetXrIntegrationClient() {
  if (!xr_integration_client_)
    xr_integration_client_ = std::make_unique<vr::ChromeXrIntegrationClient>(
        base::PassKey<ChromeContentBrowserClient>());
  return xr_integration_client_.get();
}
#endif  // BUILDFLAG(ENABLE_VR)

bool ChromeContentBrowserClient::IsOriginTrialRequiredForAppCache(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  auto* prefs = profile->GetPrefs();

  if (prefs->HasPrefPath(prefs::kAppCacheForceEnabled) &&
      prefs->GetBoolean(prefs::kAppCacheForceEnabled)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kAppCacheRequireOriginTrial)) {
    return true;
  }

  return false;
}

void ChromeContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::Get()->BindReceiver(
      chrome::GetVersionString(chrome::WithExtendedStable(true)));
#endif
}

bool ChromeContentBrowserClient::
    ShouldInheritCrossOriginEmbedderPolicyImplicitly(const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return url.SchemeIs(extensions::kExtensionScheme);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldAllowInsecurePrivateNetworkRequests(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  return content_settings::ShouldAllowInsecurePrivateNetworkRequests(
      HostContentSettingsMapFactory::GetForProfile(browser_context), origin);
}

bool ChromeContentBrowserClient::IsJitDisabledForSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  // Special case to determine if any policy is set.
  if (site_url.is_empty()) {
    return map->GetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_JIT,
                                         nullptr) == CONTENT_SETTING_BLOCK;
  }

  // Only disable JIT for web schemes.
  if (!site_url.SchemeIsHTTPOrHTTPS())
    return false;

  return (map->GetContentSetting(site_url, site_url,
                                 ContentSettingsType::JAVASCRIPT_JIT) ==
          CONTENT_SETTING_BLOCK);
}

ukm::UkmService* ChromeContentBrowserClient::GetUkmService() {
  return g_browser_process->GetMetricsServicesManager()->GetUkmService();
}

void ChromeContentBrowserClient::OnKeepaliveRequestStarted(
    content::BrowserContext* context) {
#if !defined(OS_ANDROID)
  // TODO(crbug.com/1161996): Remove this entry once the investigation is
  // done.
  VLOG(1) << "OnKeepaliveRequestStarted: " << num_keepalive_requests_ << " ==> "
          << num_keepalive_requests_ + 1;
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
    // TODO(crbug.com/1161996): Remove this entry once the investigation is
    // done.
    VLOG(1) << "Starting a keepalive timer(" << timeout.InSecondsF()
            << " seconds)";
    keepalive_timer_.Start(
        FROM_HERE, keepalive_deadline_ - now,
        base::BindOnce(
            &ChromeContentBrowserClient::OnKeepaliveTimerFired,
            weak_factory_.GetWeakPtr(),
            std::make_unique<ScopedKeepAlive>(
                KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED)));
  }
#endif  // !defined(OS_ANDROID)
}

void ChromeContentBrowserClient::OnKeepaliveRequestFinished() {
#if !defined(OS_ANDROID)
  DCHECK_GT(num_keepalive_requests_, 0u);
  // TODO(crbug.com/1161996): Remove this entry once the investigation is
  // done.
  VLOG(1) << "OnKeepaliveRequestFinished: " << num_keepalive_requests_
          << " ==> " << num_keepalive_requests_ - 1;
  --num_keepalive_requests_;
  if (num_keepalive_requests_ == 0) {
    // TODO(crbug.com/1161996): Remove this entry once the investigation is
    // done.
    VLOG(1) << "Stopping the keepalive timer";
    keepalive_timer_.Stop();
    // This deletes the keep alive handle attached to the timer function and
    // unblock the shutdown sequence.
  }
#endif  // !defined(OS_ANDROID)
}

#if defined(OS_MAC)
bool ChromeContentBrowserClient::SetupEmbedderSandboxParameters(
    sandbox::policy::SandboxType sandbox_type,
    sandbox::SeatbeltExecClient* client) {
  if (sandbox_type == sandbox::policy::SandboxType::kSpeechRecognition) {
    base::FilePath soda_component_path = speech::GetSodaDirectory();
    CHECK(!soda_component_path.empty());
    CHECK(client->SetParameter(sandbox::policy::kParamSodaComponentPath,
                               soda_component_path.value()));

    base::FilePath soda_language_pack_path =
        speech::GetSodaLanguagePacksDirectory();
    CHECK(!soda_language_pack_path.empty());
    CHECK(client->SetParameter(sandbox::policy::kParamSodaLanguagePackPath,
                               soda_language_pack_path.value()));
    return true;
  }

  return false;
}

#endif  // defined(OS_MAC)

void ChromeContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
#if BUILDFLAG(USE_MINIKIN_HYPHENATION) && !defined(OS_ANDROID)
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
ChromeContentBrowserClient::CreateIdentityRequestDialogController() {
  return std::make_unique<IdentityDialogController>();
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

std::unique_ptr<content::SpeculationHostDelegate>
ChromeContentBrowserClient::CreateSpeculationHostDelegate(
    content::RenderFrameHost& render_frame_host) {
  return std::make_unique<ChromeSpeculationHostDelegate>(render_frame_host);
}

#if !defined(OS_ANDROID)
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
  return base::TimeDelta::FromSeconds(seconds);
}

void ChromeContentBrowserClient::OnKeepaliveTimerFired(
    std::unique_ptr<ScopedKeepAlive> keep_alive_handle) {
  // TODO(crbug.com/1161996): Remove this entry once the investigation is done.
  VLOG(1) << "OnKeepaliveTimerFired";
  const auto now = base::TimeTicks::Now();
  const auto then = keepalive_deadline_;
  if (now < then) {
    // TODO(crbug.com/1161996): Remove this entry once the investigation is
    // done.
    VLOG(1) << "Extending keepalive timer";
    keepalive_timer_.Start(
        FROM_HERE, then - now,
        base::BindOnce(&ChromeContentBrowserClient::OnKeepaliveTimerFired,
                       weak_factory_.GetWeakPtr(),
                       std::move(keep_alive_handle)));
  }
}
#endif
