// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <string>

#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/ash/scheduler_configuration_manager.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_notification_controller.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/component_updater_prefs.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/storage/appcache_feature_prefs.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/federated_learning/floc_id.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/permissions/permission_actions_history.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_server_properties_manager.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/preinstalled_apps.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/device_name/device_name_store.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/policy/handlers/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/chromeos/extensions/extensions_permissions_tracker.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/plugins/plugins_resource_service.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/preferences/browser_prefs_android.h"
#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "chrome/browser/lens/android/lens_prefs.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/1125897
#include "components/content_creation/notes/core/note_prefs.h"
#include "components/feed/buildflags.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/query_tiles/tile_service_prefs.h"
#else  // defined(OS_ANDROID)
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/new_tab_page/modules/drive/drive_service.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_handler.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module_service.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#if defined(USE_CUPS)
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif  // defined(USE_CUPS)
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/audio/audio_devices_pref_handler_impl.h"
#include "ash/components/device_activity/device_activity_controller.h"
#include "ash/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "chrome/browser/apps/app_service/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/account_manager/account_manager_edu_coexistence_controller.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/child_accounts/family_user_chrome_activity_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/ash/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "chrome/browser/ash/child_accounts/secondary_account_consent_logger.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/ash/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/file_system_provider/registry.h"
#include "chrome/browser/ash/first_run/first_run.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_detector.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_resources_remover.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/saml/saml_profile_prefs.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"
#include "chrome/browser/ash/net/network_throttling_observer.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/dm_token_storage.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "chrome/browser/ash/policy/reporting/extension_install_event_log_manager_wrapper.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/ash/power/power_metrics_reporter.h"
#include "chrome/browser/ash/preferences.h"
#include "chrome/browser/ash/printing/enterprise_printers_provider.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chromeos/components/local_search_service/search_metrics_reporter.h"
#include "chromeos/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/network/cellular_metrics_logger.h"
#include "chromeos/network/fast_transition_observer.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_prefs.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/arc/arc_prefs.h"
#include "components/onc/onc_pref_names.h"
#include "components/quirks/quirks_manager.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/browser/media/cdm_pref_service_helper.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_prefs_manager.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC)
#include "components/os_crypt/os_crypt.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/web_applications/url_handler_prefs.h"
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/lacros/net/proxy_config_service_lacros.h"
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#include "chrome/browser/downgrade/downgrade_prefs.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_service_log.h"
#endif

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#endif  // BUILDFLAG(ENABLE_SIDE_SEARCH)

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 5/2021
const char kFeatureUsageDailySampleESim[] = "feature_usage.daily_sample.ESim";
const char kFeatureUsageDailySampleFingerprint[] =
    "feature_usage.daily_sample.Fingerprint";

// Deprecated 12/2020
const char kLocalSearchServiceSyncMetricsDailySample[] =
    "local_search_service_sync.metrics.daily_sample";
const char kLocalSearchServiceSyncMetricsCrosSettingsCount[] =
    "local_search_service_sync.metrics.cros_settings_count";
const char kLocalSearchServiceSyncMetricsHelpAppCount[] =
    "local_search_service_sync.metrics.help_app_count";

// Deprecated 11/2020
const char kRegisteredSupervisedUserAllowlists[] =
    "supervised_users.whitelists";

// Deprecated 11/2020
const char kSupervisedUserAllowlists[] = "profile.managed.whitelists";

// Deprecated 12/2020
const char kFirstRunTrialGroup[] = "help_app_first_run.trial_group";

// Deprecated 10/2021
const char kHasCameraAppMigratedToSWA[] = "camera.has_migrated_to_swa";

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
// Deprecated 11/2020
const char kLocalDiscoveryEnabled[] = "local_discovery.enabled";
const char kLocalDiscoveryNotificationsEnabled[] =
    "local_discovery.notifications_enabled";
#endif

// Deprecated 11/2020
#if defined(OS_LINUX) && !BUILDFLAG(IS_CHROMECAST)
const char kMigrationToLoginDBStep[] = "profile.migration_to_logindb_step";
#endif

// Deprecated 11/2020
const char kSettingsLaunchedPasswordChecks[] =
    "profile.settings_launched_password_checks";

// Deprecated 11/2020
const char kDRMSalt[] = "settings.privacy.drm_salt";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 12/2020
const char kAssistantPrivacyInfoShownInLauncher[] =
    "ash.launcher.assistant_privacy_info_shown";

const char kAssistantPrivacyInfoDismissedInLauncher[] =
    "ash.launcher.assistant_privacy_info_dismissed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 12/2020
const char kAssistantQuickAnswersEnabled[] =
    "settings.voice_interaction.quick_answers.enabled";

// Deprecated 01/2021
const char kGoogleServicesHostedDomain[] = "google.services.hosted_domain";

const char kDataReductionProxyLastConfigRetrievalTime[] =
    "data_reduction.last_config_retrieval_time";
const char kDataReductionProxyConfig[] = "data_reduction.config";

// Deprecated 2/2021.
const char kRapporCohortSeed[] = "rappor.cohort_seed";
const char kRapporLastDailySample[] = "rappor.last_daily_sample";
const char kRapporSecret[] = "rappor.secret";

// Deprecated 02/2021
const char kStabilityDebuggerPresent[] =
    "user_experience_metrics.stability.debugger_present";
const char kStabilityDebuggerNotPresent[] =
    "user_experience_metrics.stability.debugger_not_present";
const char kStabilityBreakpadRegistrationSuccess[] =
    "user_experience_metrics.stability.breakpad_registration_ok";
const char kStabilityBreakpadRegistrationFail[] =
    "user_experience_metrics.stability.breakpad_registration_fail";

// Deprecated 02/2021
const char kGamesInstallDirPref[] = "games.data_files_paths";
const char kLiteModeUserNeedsNotification[] =
    "previews.litepage.user-needs-notification";

#if !defined(OS_ANDROID)
// Deprecated 02/2021
const char kCartModuleRemoved[] = "cart_module_removed";
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Deprecated 03/2021
const char kPinnedExtensionsMigrationComplete[] =
    "extensions.pinned_extension_migration";
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// Deprecated 03/2021
const char kRunAllFlashInAllowMode[] = "plugins.run_all_flash_in_allow_mode";
#endif

// Deprecated 04/2021.
const char kSessionStatisticFCPMean[] =
    "optimization_guide.session_statistic.fcp_mean";
const char kSessionStatisticFCPStdDev[] =
    "optimization_guide.session_statistic.fcp_std_dev";
#if !defined(OS_ANDROID)
const char kWebAuthnLastTransportUsedPrefName[] =
    "webauthn.last_transport_used";
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Deprecated 04/2021
const char kToolbarIconSurfacingBubbleAcknowledged[] =
    "toolbar_icon_surfacing_bubble_acknowledged";
const char kToolbarIconSurfacingBubbleLastShowTime[] =
    "toolbar_icon_surfacing_bubble_show_time";
#endif

// Deprecated 04/2021
const char kTranslateLastDeniedTimeForLanguage[] =
    "translate_last_denied_time_for_language";
const char kTranslateTooOftenDeniedForLanguage[] =
    "translate_too_often_denied_for_language";

// Deprecated 05/2021.
#if BUILDFLAG(ENABLE_EXTENSIONS)
const char kToolbarSize[] = "extensions.toolbarsize";
#endif
const char kSessionExitedCleanly[] = "profile.exited_cleanly";

// Deprecated 05/2021
const char kSpellCheckBlacklistedDictionaries[] =
    "spellcheck.blacklisted_dictionaries";

// Deprecated 06/2021.
const char kDataReductionProxy[] = "auth.spdyproxy.origin";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 06/2021.
const char kQuickAnswersConsented[] = "settings.quick_answers.user_consented";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 06/2021.
const char kWasPhishedCredentialsUploadedToSync[] =
    "profile.was_phished_credentials_uploaded_to_sync";
const char kHintsFetcherTopHostBlocklist[] =
    "optimization_guide.hintsfetcher.top_host_blacklist";
const char kHintsFetcherTopHostBlocklistState[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_state";
const char kTimeHintsFetcherTopHostBlocklistLastInitialized[] =
    "optimization_guide.hintsfetcher.time_blacklist_last_initialized";
const char kHintsFetcherTopHostBlocklistMinimumEngagementScore[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_min_engagement_score";

// Deprecated 07/2021.
#if defined(OS_MAC)
const char kPasswordRecovery[] = "password_manager.password_recovery";
#endif
const char kWasSignInPasswordPromoClicked[] =
    "profile.was_sign_in_password_promo_clicked";
const char kNumberSignInPasswordPromoShown[] =
    "profile.number_sign_in_password_promo_shown";
const char kSignInPasswordPromoRevive[] =
    "profile.sign_in_password_promo_revive";
const char kGuestProfilesNumCreated[] = "profile.guest_profiles_created";
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
constexpr char kProfileSwitchInterceptionDeclinedPref[] =
    "signin.ProfileSwitchInterceptionDeclinedPref";
const char kDiceMigrationCompletePref[] = "signin.DiceMigrationComplete";
#endif

const char kSuggestionsBlocklist[] = "suggestions.blacklist";
const char kSuggestionsData[] = "suggestions.data";
const char kUserAgentClientHintsEnabled[] =
    "policy.user_agent_client_hints_enabled";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Deprecated 07/2021.
const char kExtensionCheckupOnStartup[] = "extensions.checkup_on_startup";
#endif

#if !defined(OS_ANDROID)
// Deprecated 07/2021
const char kCloudPrintDeprecationWarningsSuppressed[] =
    "cloud_print.deprecation_warnings_suppressed";

// Deprecated 07/2021.
const char kForceEnablePrivetPrinting[] =
    "printing.force_enable_privet_printing";
#endif

// Deprecated 07/2021.
const char kAccountStorageExists[] = "profile.password_account_storage_exists";

// Deprecated 07/2021.
const char kUserLanguageProfile[] = "language_profile";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 08/2021.
const char kAccountManagerNumTimesMigrationRanSuccessfully[] =
    "account_manager.num_times_migration_ran_successfully";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
// Deprecated 09/2021.
const char kNtpSearchSuggestionsBlocklist[] =
    "ntp.search_suggestions_blocklist";
const char kNtpSearchSuggestionsImpressions[] =
    "ntp.search_suggestions_impressions";
const char kNtpSearchSuggestionsOptOut[] = "ntp.search_suggestions_opt_out";
#endif

// Deprecated 09/2021.
const char kAutofillAcceptSaveCreditCardPromptState[] =
    "autofill.accept_save_credit_card_prompt_state";
const char kPrivacyBudgetActiveSurfaces[] = "privacy_budget.active_surfaces";
const char kPrivacyBudgetRetiredSurfaces[] = "privacy_budget.retired_surfaces";
const char kPrivacyBudgetSeed[] = "privacy_budget.randomizer_seed";
const char kCloudPolicyOverridesPlatformPolicy[] = "policy.cloud_override";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 09/2021.
const char kNearbySharingFastInitiationNotificationEnabledPrefName[] =
    "nearby_sharing.fast_initiation_notification_enabled";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Register local state used only for migration (clearing or moving to a new
// key).
void RegisterLocalStatePrefsForMigration(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kRegisteredSupervisedUserAllowlists);
  registry->RegisterStringPref(kFirstRunTrialGroup, std::string());

  registry->RegisterInt64Pref(kLocalSearchServiceSyncMetricsDailySample, 0);
  registry->RegisterIntegerPref(kLocalSearchServiceSyncMetricsHelpAppCount, 0);
  registry->RegisterIntegerPref(kLocalSearchServiceSyncMetricsCrosSettingsCount,
                                0);

  registry->RegisterInt64Pref(kFeatureUsageDailySampleESim, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
  registry->RegisterListPref(enterprise_connectors::kOnFileAttachedPref);
  registry->RegisterListPref(enterprise_connectors::kOnFileDownloadedPref);
  registry->RegisterListPref(enterprise_connectors::kOnBulkDataEntryPref);
  registry->RegisterListPref(enterprise_connectors::kOnSecurityEventPref);
#endif  // !defined(OS_ANDROID)

  registry->RegisterIntegerPref(kRapporCohortSeed, -1);
  registry->RegisterInt64Pref(kRapporLastDailySample, 0);
  registry->RegisterStringPref(kRapporSecret, std::string());

  registry->RegisterIntegerPref(kStabilityBreakpadRegistrationFail, 0);
  registry->RegisterIntegerPref(kStabilityBreakpadRegistrationSuccess, 0);
  registry->RegisterIntegerPref(kStabilityDebuggerPresent, 0);
  registry->RegisterIntegerPref(kStabilityDebuggerNotPresent, 0);

  registry->RegisterBooleanPref(kUserAgentClientHintsEnabled, true);

  registry->RegisterBooleanPref(kCloudPolicyOverridesPlatformPolicy, false);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(kPinnedExtensionsMigrationComplete, false);
#endif

  registry->RegisterStringPref(kPrivacyBudgetActiveSurfaces, std::string());
  registry->RegisterStringPref(kPrivacyBudgetRetiredSurfaces, std::string());
  registry->RegisterUint64Pref(kPrivacyBudgetSeed, 0u);
}

// Register prefs used only for migration (clearing or moving to a new key).
void RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(kAccountManagerNumTimesMigrationRanSuccessfully,
                                0);
  registry->RegisterDictionaryPref(kSupervisedUserAllowlists);
  ash::HelpAppNotificationController::RegisterObsoletePrefsForMigration(
      registry);
  registry->RegisterBooleanPref(kHasCameraAppMigratedToSWA, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  chrome_browser_net::secure_dns::RegisterProbesSettingBackupPref(registry);

  registry->RegisterBooleanPref(prefs::kWebAppsUserDisplayModeCleanedUp, false);

#if defined(OS_LINUX) && !BUILDFLAG(IS_CHROMECAST)
  registry->RegisterIntegerPref(kMigrationToLoginDBStep, 0);
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  registry->RegisterBooleanPref(kLocalDiscoveryEnabled, true);
  registry->RegisterBooleanPref(kLocalDiscoveryNotificationsEnabled, false);
#endif

  registry->RegisterIntegerPref(kSettingsLaunchedPasswordChecks, 0);
  registry->RegisterStringPref(kDRMSalt, "");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(kAssistantPrivacyInfoShownInLauncher, 0);
  registry->RegisterBooleanPref(kAssistantPrivacyInfoDismissedInLauncher,
                                false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  registry->RegisterBooleanPref(kAssistantQuickAnswersEnabled, true);

  registry->RegisterStringPref(kGoogleServicesHostedDomain, std::string());

  registry->RegisterInt64Pref(kDataReductionProxyLastConfigRetrievalTime, 0L);
  registry->RegisterStringPref(kDataReductionProxyConfig, std::string());

  registry->RegisterFilePathPref(kGamesInstallDirPref, base::FilePath());
  registry->RegisterBooleanPref(kLiteModeUserNeedsNotification, true);

#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(kCartModuleRemoved, false);
#endif

#if !defined(OS_ANDROID)
  registry->RegisterStringPref(
      enterprise_connectors::kDeviceTrustPrivateKeyPref, std::string());
  registry->RegisterStringPref(enterprise_connectors::kDeviceTrustPublicKeyPref,
                               std::string());
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  registry->RegisterBooleanPref(kRunAllFlashInAllowMode, false);
#endif

#if !defined(OS_ANDROID)
  // Removed in M91.
  registry->RegisterBooleanPref(prefs::kMediaFeedsBackgroundFetching, false);
  registry->RegisterBooleanPref(prefs::kMediaFeedsSafeSearchEnabled, false);
  registry->RegisterBooleanPref(prefs::kMediaFeedsAutoSelectEnabled, false);
  registry->RegisterStringPref(kWebAuthnLastTransportUsedPrefName,
                               std::string());
#endif

  registry->RegisterDoublePref(kSessionStatisticFCPStdDev, -1.0f);
  registry->RegisterDoublePref(kSessionStatisticFCPMean, -1.0f);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(kToolbarIconSurfacingBubbleAcknowledged, false);
  registry->RegisterInt64Pref(kToolbarIconSurfacingBubbleLastShowTime, 0);
#endif

  registry->RegisterDictionaryPref(kTranslateLastDeniedTimeForLanguage);
  registry->RegisterDictionaryPref(kTranslateTooOftenDeniedForLanguage);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterIntegerPref(kToolbarSize, -1);
#endif
  registry->RegisterBooleanPref(kSessionExitedCleanly, true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterInt64Pref(kFeatureUsageDailySampleFingerprint, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  registry->RegisterListPref(kSpellCheckBlacklistedDictionaries);

#if !defined(OS_ANDROID)
  registry->RegisterListPref(
      prefs::kManagedProfileSerialAllowAllPortsForUrlsDeprecated);
  registry->RegisterListPref(
      prefs::kManagedProfileSerialAllowUsbDevicesForUrlsDeprecated);
#endif

  registry->RegisterStringPref(kDataReductionProxy, std::string());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kQuickAnswersConsented, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  registry->RegisterBooleanPref(kWasPhishedCredentialsUploadedToSync, true);

  registry->RegisterDictionaryPref(kHintsFetcherTopHostBlocklist,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(kHintsFetcherTopHostBlocklistState, 0,
                                PrefRegistry::LOSSY_PREF);
  registry->RegisterDoublePref(kTimeHintsFetcherTopHostBlocklistLastInitialized,
                               0, PrefRegistry::LOSSY_PREF);
  registry->RegisterDoublePref(
      kHintsFetcherTopHostBlocklistMinimumEngagementScore, 0,
      PrefRegistry::LOSSY_PREF);

#if defined(OS_MAC)
  registry->RegisterTimePref(kPasswordRecovery, base::Time());
#endif

  registry->RegisterBooleanPref(kWasSignInPasswordPromoClicked, false);
  registry->RegisterIntegerPref(kNumberSignInPasswordPromoShown, 0);
  registry->RegisterBooleanPref(kSignInPasswordPromoRevive, false);
  registry->RegisterIntegerPref(kGuestProfilesNumCreated, 1);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterDictionaryPref(kProfileSwitchInterceptionDeclinedPref);
  registry->RegisterDictionaryPref(kDiceMigrationCompletePref);
#endif

  registry->RegisterStringPref(kSuggestionsBlocklist, std::string());
  registry->RegisterStringPref(kSuggestionsData, std::string());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(kExtensionCheckupOnStartup, false);
#endif

#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(kCloudPrintDeprecationWarningsSuppressed,
                                false);
  registry->RegisterBooleanPref(kForceEnablePrivetPrinting, false);
#endif

  registry->RegisterBooleanPref(kAccountStorageExists, false);

  registry->RegisterDictionaryPref(kUserLanguageProfile);

#if !defined(OS_ANDROID)
  registry->RegisterDictionaryPref(kNtpSearchSuggestionsBlocklist);
  registry->RegisterDictionaryPref(kNtpSearchSuggestionsImpressions);
  registry->RegisterBooleanPref(kNtpSearchSuggestionsOptOut, false);
#endif

  registry->RegisterIntegerPref(kAutofillAcceptSaveCreditCardPromptState, 0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(
      kNearbySharingFastInitiationNotificationEnabledPrefName, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Call outs to individual subsystems that register Local State (browser-wide)
  // prefs en masse. See RegisterProfilePrefs for per-profile prefs. Please
  // keep this list alphabetized.
  browser_shutdown::RegisterPrefs(registry);
  data_reduction_proxy::RegisterPrefs(registry);
  data_use_measurement::ChromeDataUseMeasurement::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterLocalStatePrefs(registry);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  chrome_labs_prefs::RegisterLocalStatePrefs(registry);
#endif
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  ChromeTracingDelegate::RegisterPrefs(registry);
  chrome::enterprise_util::RegisterLocalStatePrefs(registry);
  component_updater::RegisterPrefs(registry);
  embedder_support::OriginTrialPrefs::RegisterPrefs(registry);
  enterprise_reporting::RegisterLocalStatePrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  invalidation::FCMInvalidationService::RegisterPrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterPrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterPrefs(registry);
  language::GeoLanguageProvider::RegisterLocalStatePrefs(registry);
  language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(registry);
  memory::EnterpriseMemoryLimitPrefObserver::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileAttributesEntry::RegisterLocalStatePrefs(registry);
  ProfileAttributesStorage::RegisterPrefs(registry);
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry);
  profiles::RegisterPrefs(registry);
  RegisterScreenshotPrefs(registry);
  safe_browsing::RegisterLocalStatePrefs(registry);
  secure_origin_allowlist::RegisterPrefs(registry);
#if !defined(OS_ANDROID)
  SerialPolicyAllowedPorts::RegisterPrefs(registry);
#endif
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(registry);
  SystemNetworkContextManager::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  // Individual preferences. If you have multiple preferences that should
  // clearly be grouped together, please group them together into a helper
  // function called above. Please keep this list alphabetized.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled, false);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kTargetBlankImpliesNoOpener, true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kWebSQLInThirdPartyContextEnabled, false);
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(policy::policy_prefs::kBackForwardCacheEnabled,
                                true);
#endif  // defined(OS_ANDROID)

  // Below this point is for platform-specific and compile-time conditional
  // calls. Please follow the helper-function-first-then-direct-calls pattern
  // established above, and keep things alphabetized.

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginsResourceService::RegisterPrefs(registry);
#endif

#if defined(OS_ANDROID)
  ::android::RegisterPrefs(registry);

  registry->RegisterIntegerPref(first_run::kTosDialogBehavior, 0);
  registry->RegisterBooleanPref(lens::kLensCameraAssistedSearchEnabled, true);
#else  // defined(OS_ANDROID)
  gcm::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  media_router::RegisterLocalStatePrefs(registry);
  metrics::TabStatsTracker::RegisterPrefs(registry);
  RegisterBrowserPrefs(registry);
  speech::SodaInstaller::RegisterLocalStatePrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  WhatsNewUI::RegisterLocalStatePrefs(registry);
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  arc::prefs::RegisterLocalStatePrefs(registry);
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  chromeos::ArcKioskAppManager::RegisterPrefs(registry);
  ash::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  ash::cert_provisioning::RegisterLocalStatePrefs(registry);
  chromeos::CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(registry);
  ash::ChromeUserManagerImpl::RegisterPrefs(registry);
  crosapi::browser_util::RegisterLocalStatePrefs(registry);
  chromeos::CupsPrintersManager::RegisterLocalStatePrefs(registry);
  ash::DemoModeDetector::RegisterPrefs(registry);
  ash::DemoModeResourcesRemover::RegisterLocalStatePrefs(registry);
  ash::DemoSession::RegisterLocalStatePrefs(registry);
  ash::DemoSetupController::RegisterLocalStatePrefs(registry);
  ash::DeviceNameStore::RegisterLocalStatePrefs(registry);
  chromeos::DeviceOAuth2TokenStoreChromeOS::RegisterPrefs(registry);
  ash::device_settings_cache::RegisterPrefs(registry);
  ash::EasyUnlockService::RegisterPrefs(registry);
  chromeos::echo_offer::RegisterPrefs(registry);
  ash::EnableAdbSideloadingScreen::RegisterPrefs(registry);
  ash::device_activity::DeviceActivityController::RegisterPrefs(registry);
  chromeos::EnableDebuggingScreenHandler::RegisterPrefs(registry);
  chromeos::FastTransitionObserver::RegisterPrefs(registry);
  chromeos::HIDDetectionScreenHandler::RegisterPrefs(registry);
  ash::KerberosCredentialsManager::RegisterLocalStatePrefs(registry);
  ash::KioskAppManager::RegisterPrefs(registry);
  ash::KioskCryptohomeRemover::RegisterPrefs(registry);
  chromeos::language_prefs::RegisterPrefs(registry);
  chromeos::local_search_service::SearchMetricsReporter::
      RegisterLocalStatePrefs(registry);
  ash::login::SecurityTokenSessionController::RegisterLocalStatePrefs(registry);
  ash::MultiProfileUserController::RegisterPrefs(registry);
  chromeos::NetworkMetadataStore::RegisterPrefs(registry);
  chromeos::NetworkThrottlingObserver::RegisterPrefs(registry);
  ash::PowerMetricsReporter::RegisterLocalStatePrefs(registry);
  chromeos::platform_keys::KeyPermissionsManagerImpl::RegisterLocalStatePrefs(
      registry);
  ash::power::auto_screen_brightness::MetricsReporter::RegisterLocalStatePrefs(
      registry);
  chromeos::Preferences::RegisterPrefs(registry);
  ash::ResetScreen::RegisterPrefs(registry);
  chromeos::SchedulerConfigurationManager::RegisterLocalStatePrefs(registry);
  ash::ServicesCustomizationDocument::RegisterPrefs(registry);
  chromeos::SigninScreenHandler::RegisterPrefs(registry);
  ash::StartupUtils::RegisterPrefs(registry);
  ash::StatsReportingController::RegisterLocalStatePrefs(registry);
  ash::system::AutomaticRebootManager::RegisterPrefs(registry);
  chromeos::TimeZoneResolver::RegisterPrefs(registry);
  ash::UserImageManager::RegisterPrefs(registry);
  ash::UserSessionManager::RegisterPrefs(registry);
  ash::WebKioskAppManager::RegisterPrefs(registry);
  component_updater::MetadataTable::RegisterPrefs(registry);
  cryptauth::CryptAuthDeviceIdProviderImpl::RegisterLocalPrefs(registry);
  extensions::ExtensionAssetsManagerChromeOS::RegisterPrefs(registry);
  extensions::ExtensionsPermissionsTracker::RegisterLocalStatePrefs(registry);
  extensions::lock_screen_data::LockScreenItemStorage::RegisterLocalState(
      registry);
  extensions::login_api::RegisterLocalStatePrefs(registry);
  ::onc::RegisterPrefs(registry);
  policy::AdbSideloadingAllowanceModePolicyHandler::RegisterPrefs(registry);
  policy::AutoEnrollmentClientImpl::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorAsh::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerAsh::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(registry);
  policy::DlpRulesManagerImpl::RegisterPrefs(registry);
  policy::DMTokenStorage::RegisterPrefs(registry);
  policy::EnrollmentRequisitionManager::RegisterPrefs(registry);
  policy::MinimumVersionPolicyHandler::RegisterPrefs(registry);
  policy::PolicyCertServiceFactory::RegisterPrefs(registry);
  policy::TPMAutoUpdateModePolicyHandler::RegisterPrefs(registry);
  policy::SystemFeaturesDisableListPolicyHandler::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);
  UpgradeDetectorChromeos::RegisterPrefs(registry);
  RegisterNearbySharingLocalPrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug/1169547) Remove `BUILDFLAG(IS_CHROMEOS_LACROS)` once the
// migration is complete.
#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  enterprise_connectors::RegisterLocalPrefs(registry);
#endif  // defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN)

#if defined(OS_MAC)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
  system_media_permissions::RegisterSystemMediaPermissionStatesPrefs(registry);
  AppShimRegistry::Get()->RegisterLocalPrefs(registry);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_prefs::RegisterLocalStatePrefs(registry);
#endif

#if defined(OS_WIN)
  OSCrypt::RegisterLocalPrefs(registry);
  registry->RegisterBooleanPref(prefs::kRendererCodeIntegrityEnabled, true);
  registry->RegisterBooleanPref(prefs::kBlockBrowserLegacyExtensionPoints,
                                true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kNativeWindowOcclusionEnabled, true);
  component_updater::RegisterPrefsForSwReporter(registry);
  safe_browsing::RegisterChromeCleanerScanCompletionTimePref(registry);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  IncompatibleApplicationsUpdater::RegisterLocalStatePrefs(registry);
  ModuleDatabase::RegisterLocalStatePrefs(registry);
  ThirdPartyConflictsManager::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  web_app::url_handler_prefs::RegisterLocalStatePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterDefaultBrowserPromptPrefs(registry);
  downgrade::RegisterPrefs(registry);
  DeviceOAuth2TokenStoreDesktop::RegisterPrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewLocalPrefs(registry);
#endif

  // This is intentionally last.
  RegisterLocalStatePrefsForMigration(registry);
}

// Register prefs applicable to all profiles.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                          const std::string& locale) {
  TRACE_EVENT0("browser", "chrome::RegisterProfilePrefs");
  // User prefs. Please keep this list alphabetized.
  AccessibilityLabelsService::RegisterProfilePrefs(registry);
  AccessibilityUIMessageHandler::RegisterProfilePrefs(registry);
  AnnouncementNotificationService::RegisterProfilePrefs(registry);
  appcache_feature_prefs::RegisterProfilePrefs(registry);
  AvailabilityProber::RegisterProfilePrefs(registry);
  autofill::prefs::RegisterProfilePrefs(registry);
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);
  certificate_transparency::prefs::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  chrome_labs_prefs::RegisterProfilePrefs(registry);
  ChromeLocationBarModelDelegate::RegisterProfilePrefs(registry);
  StatefulSSLHostStateDelegate::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::NetErrorTabHelper::RegisterProfilePrefs(registry);
  chrome_browser_net::RegisterPredictionOptionsProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  DocumentProvider::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  dom_distiller::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  federated_learning::FlocId::RegisterPrefs(registry);
  history_clusters::prefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  HttpsImageCompressionInfoBarDecider::RegisterProfilePrefs(registry);
  image_fetcher::ImageCache::RegisterProfilePrefs(registry);
  site_engagement::ImportantSitesUtil::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  login_detection::prefs::RegisterProfilePrefs(registry);
  lookalikes::RegisterProfilePrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaEngagementService::RegisterProfilePrefs(registry);
  MediaStorageIdSalt::RegisterProfilePrefs(registry);
  metrics::RegisterDemographicsProfilePrefs(registry);
  NotificationDisplayServiceImpl::RegisterProfilePrefs(registry);
  NotifierStateTracker::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  permissions::PermissionActionsHistory::RegisterProfilePrefs(registry);
  PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(registry);
  PlatformNotificationServiceImpl::RegisterProfilePrefs(registry);
  policy::DeveloperToolsPolicyHandler::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PrefetchProxyOriginDecider::RegisterPrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  privacy_sandbox::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProfileNetworkContextService::RegisterProfilePrefs(registry);
  ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  QuietNotificationPermissionUiState::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
  SearchPrefetchService::RegisterProfilePrefs(registry);
  blocked_content::SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
      registry);
  security_interstitials::InsecureFormBlockingPage::RegisterProfilePrefs(
      registry);
  segmentation_platform::SegmentationPlatformService::RegisterProfilePrefs(
      registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  SharingSyncPreference::RegisterProfilePrefs(registry);
  site_engagement::SiteEngagementService::RegisterProfilePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::SyncTransportDataPrefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  omnibox::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  RegisterSessionServiceLogProfilePrefs(registry);
  SessionDataService::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionWebUI::RegisterProfilePrefs(registry);
  RegisterAnimationPolicyPrefs(registry);
  extensions::api::CryptotokenRegisterProfilePrefs(registry);
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::AudioAPI::RegisterUserPrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::ExtensionsUI::RegisterProfilePrefs(registry);
  extensions::RuntimeAPI::RegisterPrefs(registry);
  // TODO(devlin): This would be more inline with the other calls here if it
  // were nested in either a class or separate namespace with a simple
  // Register[Profile]Prefs() name.
  extensions::RegisterSettingsOverriddenUiPrefs(registry);
  update_client::RegisterProfilePrefs(registry);
  web_app::WebAppProvider::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflineMetricsCollectorImpl::RegisterPrefs(registry);
  offline_pages::prefetch_prefs::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginInfoHostImpl::RegisterUserPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PolicySettings::RegisterProfilePrefs(registry);
  printing::PrintPreviewStickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  ChildAccountService::RegisterProfilePrefs(registry);
  SupervisedUserService::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  content_creation::prefs::RegisterProfilePrefs(registry);
  explore_sites::HistoryStatisticsReporter::RegisterPrefs(registry);
  permissions::GeolocationPermissionContextAndroid::RegisterProfilePrefs(
      registry);
  KnownInterceptionDisclosureInfoBarDelegate::RegisterProfilePrefs(registry);
  MediaDrmOriginIdManager::RegisterProfilePrefs(registry);
  NotificationChannelsProviderAndroid::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  OomInterventionDecider::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  query_tiles::RegisterPrefs(registry);
  RecentTabsPagePrefs::RegisterProfilePrefs(registry);
  usage_stats::UsageStatsBridge::RegisterProfilePrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  video_tutorials::RegisterPrefs(registry);
  feed::prefs::RegisterFeedSharedProfilePrefs(registry);
  feed::RegisterProfilePrefs(registry);
#else  // defined(OS_ANDROID)
  AppShortcutManager::RegisterProfilePrefs(registry);
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  captions::LiveCaptionController::RegisterProfilePrefs(registry);
  ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(registry);
  DeviceServiceImpl::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
  DriveService::RegisterProfilePrefs(registry);
  enterprise_connectors::RegisterProfilePrefs(registry);
  enterprise_reporting::RegisterProfilePrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  FeaturePromoSnoozeService::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  gcm::RegisterProfilePrefs(registry);
  HatsService::RegisterProfilePrefs(registry);
  NtpCustomBackgroundService::RegisterProfilePrefs(registry);
  media_router::RegisterProfilePrefs(registry);
  NewTabPageHandler::RegisterProfilePrefs(registry);
  NewTabPageUI::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  ntp::SafeBrowsingHandler::RegisterProfilePrefs(registry);
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
  PhotosService::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  PromoService::RegisterProfilePrefs(registry);
  settings::SettingsUI::RegisterProfilePrefs(registry);
  send_tab_to_self::SendTabToSelfBubbleController::RegisterProfilePrefs(
      registry);
  signin::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
  tab_search_prefs::RegisterProfilePrefs(registry);
  TaskModuleService::RegisterProfilePrefs(registry);
  UnifiedAutoplayConfig::RegisterProfilePrefs(registry);
  CartService::RegisterProfilePrefs(registry);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  extensions::platform_keys::RegisterProfilePrefs(registry);
#if defined(USE_CUPS)
  extensions::PrintingAPIHandler::RegisterProfilePrefs(registry);
#endif  // defined(USE_CUPS)
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_list::AppListSyncableService::RegisterProfilePrefs(registry);
  app_list::ArcAppReinstallSearchProvider::RegisterProfilePrefs(registry);
  apps::AppPlatformMetricsService::RegisterProfilePrefs(registry);
  apps::webapk_prefs::RegisterProfilePrefs(registry);
  arc::prefs::RegisterProfilePrefs(registry);
  ArcAppListPrefs::RegisterProfilePrefs(registry);
  certificate_manager::CertificatesHandler::RegisterProfilePrefs(registry);
  account_manager::AccountManager::RegisterPrefs(registry);
  ash::ApkWebAppService::RegisterProfilePrefs(registry);
  ash::app_time::AppActivityRegistry::RegisterProfilePrefs(registry);
  ash::app_time::AppTimeController::RegisterProfilePrefs(registry);
  chromeos::assistant::prefs::RegisterProfilePrefs(registry);
  ash::bluetooth::DebugLogsManager::RegisterPrefs(registry);
  ash::ClientAppMetadataProviderService::RegisterProfilePrefs(registry);
  chromeos::CupsPrintersManager::RegisterProfilePrefs(registry);
  chromeos::device_sync::RegisterProfilePrefs(registry);
  ash::FamilyUserChromeActivityMetrics::RegisterProfilePrefs(registry);
  ash::FamilyUserMetricsService::RegisterProfilePrefs(registry);
  ash::FamilyUserSessionMetrics::RegisterProfilePrefs(registry);
  crosapi::NetworkSettingsServiceAsh::RegisterProfilePrefs(registry);
  chromeos::InlineLoginHandlerChromeOS::RegisterProfilePrefs(registry);
  ash::first_run::RegisterProfilePrefs(registry);
  ash::file_system_provider::RegisterProfilePrefs(registry);
  ash::full_restore::RegisterProfilePrefs(registry);
  ash::KerberosCredentialsManager::RegisterProfilePrefs(registry);
  ash::login::SecurityTokenSessionController::RegisterProfilePrefs(registry);
  chromeos::multidevice_setup::MultiDeviceSetupService::RegisterProfilePrefs(
      registry);
  ash::MultiProfileUserController::RegisterProfilePrefs(registry);
  chromeos::NetworkMetadataStore::RegisterPrefs(registry);
  ash::ReleaseNotesStorage::RegisterProfilePrefs(registry);
  ash::HelpAppNotificationController::RegisterProfilePrefs(registry);
  ash::quick_unlock::FingerprintStorage::RegisterProfilePrefs(registry);
  ash::quick_unlock::PinStoragePrefs::RegisterProfilePrefs(registry);
  chromeos::Preferences::RegisterProfilePrefs(registry);
  ash::EnterprisePrintersProvider::RegisterProfilePrefs(registry);
  ash::parent_access::ParentAccessService::RegisterProfilePrefs(registry);
  ash::quick_answers::prefs::RegisterProfilePrefs(registry);
  ash::quick_unlock::RegisterProfilePrefs(registry);
  ash::RegisterSamlProfilePrefs(registry);
  ash::ScreenTimeController::RegisterProfilePrefs(registry);
  ash::SecondaryAccountConsentLogger::RegisterPrefs(registry);
  ash::EduCoexistenceConsentInvalidationController::RegisterProfilePrefs(
      registry);
  ash::SigninErrorNotifier::RegisterPrefs(registry);
  ash::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  chromeos::settings::OSSettingsUI::RegisterProfilePrefs(registry);
  ash::StartupUtils::RegisterOobeProfilePrefs(registry);
  ash::UserImageSyncObserver::RegisterProfilePrefs(registry);
  crostini::prefs::RegisterProfilePrefs(registry);
  ash::attestation::TpmChallengeKey::RegisterProfilePrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
  guest_os::prefs::RegisterProfilePrefs(registry);
  lock_screen_apps::StateController::RegisterProfilePrefs(registry);
  plugin_vm::prefs::RegisterProfilePrefs(registry);
  policy::ArcAppInstallEventLogger::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogManagerWrapper::RegisterProfilePrefs(registry);
  policy::ExtensionInstallEventLogManagerWrapper::RegisterProfilePrefs(
      registry);
  policy::StatusCollector::RegisterProfilePrefs(registry);
  chromeos::SystemProxyManager::RegisterProfilePrefs(registry);
  ChromeShelfPrefs::RegisterProfilePrefs(registry);
  ::onc::RegisterProfilePrefs(registry);
  ash::cert_provisioning::RegisterProfilePrefs(registry);
  borealis::prefs::RegisterProfilePrefs(registry);
  ash::ChromeScanningAppDelegate::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_prefs::RegisterProfilePrefs(registry);
  chromeos::ProxyConfigServiceLacros::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN)
  CdmPrefServiceHelper::RegisterProfilePrefs(registry);
  component_updater::RegisterProfilePrefsForSwReporter(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
  safe_browsing::SettingsResetPromptPrefsManager::RegisterProfilePrefs(
      registry);
  safe_browsing::PostCleanupSettingsResetter::RegisterProfilePrefs(registry);
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  browser_switcher::BrowserSwitcherPrefs::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  preinstalled_apps::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  sharing_hub::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  accessibility_prefs::RegisterInvertBubbleUserPrefs(registry);
  RegisterBrowserViewProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
  side_search_prefs::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_SIDE_SEARCH)

#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kLensRegionSearchEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif

  RegisterProfilePrefsForMigration(registry);
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterUserProfilePrefs(registry, g_browser_process->GetApplicationLocale());
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                              const std::string& locale) {
  RegisterProfilePrefs(registry, locale);

#if defined(OS_ANDROID)
  ::android::RegisterUserProfilePrefs(registry);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterUserProfilePrefs(registry);
#endif
}

void RegisterScreenshotPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterSigninProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry, g_browser_process->GetApplicationLocale());
  ash::RegisterSigninProfilePrefs(registry);
}

#endif

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteLocalStatePrefs(PrefService* local_state) {
  // BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

#if BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 11/2020.
  local_state->ClearPref(kRegisteredSupervisedUserAllowlists);

  // Added 12/2020.
  local_state->ClearPref(kFirstRunTrialGroup);
  local_state->ClearPref(kLocalSearchServiceSyncMetricsDailySample);
  local_state->ClearPref(kLocalSearchServiceSyncMetricsCrosSettingsCount);
  local_state->ClearPref(kLocalSearchServiceSyncMetricsHelpAppCount);

  // Added 5/2021
  local_state->ClearPref(kFeatureUsageDailySampleESim);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
  // Added 11/2020
  local_state->ClearPref(enterprise_connectors::kOnFileAttachedPref);
  local_state->ClearPref(enterprise_connectors::kOnFileDownloadedPref);
  local_state->ClearPref(enterprise_connectors::kOnBulkDataEntryPref);
  local_state->ClearPref(enterprise_connectors::kOnSecurityEventPref);
#endif  // !defined(OS_ANDROID)

  // Added 2/2021.
  local_state->ClearPref(kRapporCohortSeed);
  local_state->ClearPref(kRapporLastDailySample);
  local_state->ClearPref(kRapporSecret);

  // Added 02/2021
  local_state->ClearPref(kStabilityBreakpadRegistrationFail);
  local_state->ClearPref(kStabilityBreakpadRegistrationSuccess);
  local_state->ClearPref(kStabilityDebuggerPresent);
  local_state->ClearPref(kStabilityDebuggerNotPresent);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Added 03/2021
  local_state->ClearPref(kPinnedExtensionsMigrationComplete);
#endif

  // Added 07/2021
  local_state->ClearPref(kUserAgentClientHintsEnabled);

  // Added 08/2021
  local_state->ClearPref(kPrivacyBudgetActiveSurfaces);
  local_state->ClearPref(kPrivacyBudgetRetiredSurfaces);
  local_state->ClearPref(kPrivacyBudgetSeed);

  // Added 09/2021.
  local_state->ClearPref(kCloudPolicyOverridesPlatformPolicy);

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS
}

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteProfilePrefs(Profile* profile) {
  // BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

  PrefService* profile_prefs = profile->GetPrefs();

  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(profile_prefs);

  // Added 7/2019. Keep at least until 7/2021 as a missing migration would
  // disable sync.
  syncer::MigrateSyncSuppressedPref(profile_prefs);

  // Added 3/2020.
  // TODO(crbug.com/1062698): Remove this once the privacy settings redesign
  // is fully launched.
  chrome_browser_net::secure_dns::MigrateProbesSettingToOrFromBackup(
      profile_prefs);

  // Added 11/2020
#if defined(OS_LINUX) && !BUILDFLAG(IS_CHROMECAST)
  profile_prefs->ClearPref(kMigrationToLoginDBStep);
#endif

  // Added 11/2020.
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  profile_prefs->ClearPref(kLocalDiscoveryEnabled);
  profile_prefs->ClearPref(kLocalDiscoveryNotificationsEnabled);
#endif

  // Added 11/2020
  profile_prefs->ClearPref(kSettingsLaunchedPasswordChecks);

  // Added 11/2020
  profile_prefs->ClearPref(kDRMSalt);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 11/2020.
  profile_prefs->ClearPref(kSupervisedUserAllowlists);

  // Added 12/2020
  profile_prefs->ClearPref(kAssistantPrivacyInfoShownInLauncher);
  profile_prefs->ClearPref(kAssistantPrivacyInfoDismissedInLauncher);

  // Added 10/2021
  profile_prefs->ClearPref(kHasCameraAppMigratedToSWA);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 12/2020
  profile_prefs->ClearPref(prefs::kWebAppsUserDisplayModeCleanedUp);

  // Added 12/2020
  profile_prefs->ClearPref(kAssistantQuickAnswersEnabled);

  // Added 01/2021
  profile_prefs->ClearPref(kGoogleServicesHostedDomain);
  profile_prefs->ClearPref(kDataReductionProxyLastConfigRetrievalTime);
  profile_prefs->ClearPref(kDataReductionProxyConfig);

#if defined(OS_ANDROID)
  // Added 02/2021
  feed::MigrateObsoleteProfilePrefsFeb_2021(profile_prefs);
#endif  // defined(OS_ANDROID)
  syncer::ClearObsoletePassphrasePromptPrefs(profile_prefs);

  // Added 02/2021
  profile_prefs->ClearPref(kGamesInstallDirPref);

#if !defined(OS_ANDROID)
  // Added 02/2021
  profile_prefs->ClearPref(kCartModuleRemoved);
#endif

  // Added 03/2021
  profile_prefs->ClearPref(kLiteModeUserNeedsNotification);

#if !defined(OS_ANDROID)
  // Added 03/2021
  profile_prefs->ClearPref(enterprise_connectors::kDeviceTrustPrivateKeyPref);
  profile_prefs->ClearPref(enterprise_connectors::kDeviceTrustPublicKeyPref);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Added 03/2021
  profile_prefs->ClearPref(kRunAllFlashInAllowMode);
#endif

#if !defined(OS_ANDROID)
  // Added 04/2021
  profile_prefs->ClearPref(prefs::kMediaFeedsBackgroundFetching);
  profile_prefs->ClearPref(prefs::kMediaFeedsSafeSearchEnabled);
  profile_prefs->ClearPref(prefs::kMediaFeedsAutoSelectEnabled);
  profile_prefs->ClearPref(kWebAuthnLastTransportUsedPrefName);
#endif
  // Added 04/2021.
  profile_prefs->ClearPref(kSessionStatisticFCPMean);
  profile_prefs->ClearPref(kSessionStatisticFCPStdDev);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Added 04/2021
  profile_prefs->ClearPref(kToolbarIconSurfacingBubbleAcknowledged);
  profile_prefs->ClearPref(kToolbarIconSurfacingBubbleLastShowTime);
#endif

  // Added 04/2021
  profile_prefs->ClearPref(kTranslateLastDeniedTimeForLanguage);
  profile_prefs->ClearPref(kTranslateTooOftenDeniedForLanguage);

  // Added 05/2021.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  profile_prefs->ClearPref(kToolbarSize);
#endif
  profile_prefs->ClearPref(kSessionExitedCleanly);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 05/2021
  profile_prefs->ClearPref(kFeatureUsageDailySampleFingerprint);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 05/2021
  profile_prefs->ClearPref(kSpellCheckBlacklistedDictionaries);

#if !defined(OS_ANDROID)
  // Added 05/2021
  profile_prefs->ClearPref(
      prefs::kManagedProfileSerialAllowAllPortsForUrlsDeprecated);
  profile_prefs->ClearPref(
      prefs::kManagedProfileSerialAllowUsbDevicesForUrlsDeprecated);
#endif

  // Added 06/2021
  profile_prefs->ClearPref(kDataReductionProxy);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 06/2021
  profile_prefs->ClearPref(kQuickAnswersConsented);

  // Added 06/2021.
  ash::HelpAppNotificationController::MigrateObsoleteNotificationPrefs(
      profile_prefs);
  ash::HelpAppNotificationController::ClearObsoleteNotificationPrefs(
      profile_prefs);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 06/2021
  profile_prefs->ClearPref(kWasPhishedCredentialsUploadedToSync);
  profile_prefs->ClearPref(kHintsFetcherTopHostBlocklist);
  profile_prefs->ClearPref(kHintsFetcherTopHostBlocklistState);
  profile_prefs->ClearPref(kHintsFetcherTopHostBlocklistMinimumEngagementScore);
  profile_prefs->ClearPref(kTimeHintsFetcherTopHostBlocklistLastInitialized);

#if defined(OS_ANDROID)
  // Added 06/2021
  feed::MigrateObsoleteProfilePrefsJune_2021(profile_prefs);
#endif  // defined(OS_ANDROID)

  // Added 07/2021
#if defined(OS_MAC)
  profile_prefs->ClearPref(kPasswordRecovery);
#endif
  profile_prefs->ClearPref(kWasSignInPasswordPromoClicked);
  profile_prefs->ClearPref(kNumberSignInPasswordPromoShown);
  profile_prefs->ClearPref(kSignInPasswordPromoRevive);
  profile_prefs->ClearPref(kGuestProfilesNumCreated);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  profile_prefs->ClearPref(kProfileSwitchInterceptionDeclinedPref);
#endif

  profile_prefs->ClearPref(kSuggestionsBlocklist);
  profile_prefs->ClearPref(kSuggestionsData);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Added 2021/07.
  profile_prefs->ClearPref(kExtensionCheckupOnStartup);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 2021/07.
  guest_os::GuestOsMimeTypesService::MigrateVerboseMimeTypePrefs(profile_prefs);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
  // Added 2021/07
  profile_prefs->ClearPref(kCloudPrintDeprecationWarningsSuppressed);
  profile_prefs->ClearPref(kForceEnablePrivetPrinting);
#endif

  // Added 2021/07.
  profile_prefs->ClearPref(kAccountStorageExists);

  // Added 07/2021
  profile_prefs->ClearPref(kUserLanguageProfile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 08/2021.
  profile_prefs->ClearPref(kAccountManagerNumTimesMigrationRanSuccessfully);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 2021/08.
  web_app::ExternallyInstalledWebAppPrefs::RemoveTerminalPWA(profile_prefs);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
  // Added 09/2021.
  profile_prefs->ClearPref(kNtpSearchSuggestionsBlocklist);
  profile_prefs->ClearPref(kNtpSearchSuggestionsImpressions);
  profile_prefs->ClearPref(kNtpSearchSuggestionsOptOut);
#endif

  // Added 09/2021.
  profile_prefs->ClearPref(kAutofillAcceptSaveCreditCardPromptState);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Added 09/2021.
  profile_prefs->ClearPref(kDiceMigrationCompletePref);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 09/2021
  profile_prefs->ClearPref(
      kNearbySharingFastInitiationNotificationEnabledPrefName);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_PROFILE_PREFS
}
