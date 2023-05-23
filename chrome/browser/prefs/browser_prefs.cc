// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <string>

#include "ash/constants/ash_constants.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/component_updater_prefs.h"
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
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/preloading/prefetch/prefetch_service/prefetch_origin_decider.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/side_panel/side_panel_prefs.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/user_education/browser_feature_promo_snooze_service.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/commerce/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/lens/buildflags.h"
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
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/permissions/permission_actions_history.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/services/storage/public/cpp/storage_prefs.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/tracing/common/pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_server_properties_manager.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/preinstalled_apps.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/pref_names.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/device_name/device_name_store.h"
#include "chrome/browser/ash/extensions/extensions_permissions_tracker.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/policy/networking/euicc_status_uploader.h"
#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/browser/extensions/api/shared_storage/shared_storage_private_api.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_pref_names.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/pref_names.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#endif

#include "components/feed/buildflags.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/preferences/browser_prefs_android.h"
#include "chrome/browser/android/preferences/shared_preferences_migrator_android.h"
#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "chrome/browser/lens/android/lens_prefs.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/1125897
#include "components/content_creation/notes/core/note_prefs.h"
#include "components/ntp_snippets/register_prefs.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/query_tiles/tile_service_prefs.h"
#include "components/webapps/browser/android/install_prompt_prefs.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/companion/core/promo_handler.h"
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/hid/hid_policy_allowed_devices.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/new_tab_page/modules/drive/drive_service.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes_service.h"
#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_handler.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/headless/policy/headless_mode_prefs.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/user_notes/user_notes_prefs.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"
#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_prefs.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#include "chrome/browser/memory/oom_kills_monitor.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif  // BUILDFLAG(USE_CUPS)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ash_prefs.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_manager_edu_coexistence_controller.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"
#include "chrome/browser/ash/bluetooth/hats_bluetooth_revamp_trigger_impl.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/child_accounts/family_user_chrome_activity_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/ash/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/ash/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/registry.h"
#include "chrome/browser/ash/first_run/first_run.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_resources_remover.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"
#include "chrome/browser/ash/login/saml/saml_profile_prefs.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"
#include "chrome/browser/ash/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"
#include "chrome/browser/ash/net/network_throttling_observer.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/dm_token_storage.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/ash/power/power_metrics_reporter.h"
#include "chrome/browser/ash/preferences.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/enterprise_printers_provider.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"
#include "chrome/browser/ash/scheduler_configuration_manager.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_notification_controller.h"
#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/fast_transition_observer.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/bluetooth_config/bluetooth_power_controller_impl.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager_impl.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_prefs.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/onc/onc_pref_names.h"
#include "components/quirks/quirks_manager.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"
#include "chrome/browser/font_prewarmer_tab_helper.h"
#include "chrome/browser/media/cdm_pref_service_helper.h"
#include "chrome/browser/media/media_foundation_service_monitor.h"
#include "chrome/browser/os_crypt/app_bound_encryption_metrics_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_prefs_manager.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "components/os_crypt/sync/os_crypt.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/device_signals/core/browser/pref_names.h"  // nogncheck due to crbug.com/1125897
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#include "chrome/browser/lacros/account_manager/account_cache.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/lacros/net/proxy_config_service_lacros.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/startup/first_run_service.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#include "chrome/browser/downgrade/downgrade_prefs.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_service_log.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/color/system_theme.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_prefs.h"
#endif

namespace {

// Please keep the list of deprecated prefs in chronological order. i.e. Add to
// the bottom of the list, not here at the top.

// Deprecated 06/2022.
const char kBackgroundTracingLastUpload[] = "background_tracing.last_upload";
const char kStabilityGpuCrashCount[] =
    "user_experience_metrics.stability.gpu_crash_count";
const char kStabilityRendererCrashCount[] =
    "user_experience_metrics.stability.renderer_crash_count";
const char kStabilityExtensionRendererCrashCount[] =
    "user_experience_metrics.stability.extension_renderer_crash_count";
const char kPrivacySandboxPreferencesReconciled[] =
    "privacy_sandbox.preferences_reconciled";
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kTokenServiceDiceCompatible[] = "token_service.dice_compatible";
#endif
#if !BUILDFLAG(IS_ANDROID)
const char kStabilityPageLoadCount[] =
    "user_experience_metrics.stability.page_load_count";
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kImprovedShortcutsNotificationShownCount[] =
    "ash.improved_shortcuts_notification_shown_count";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_ANDROID)
const char kDownloadLaterPromptStatus[] =
    "download.download_later_prompt_status";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 07/2022
const char kPrivacySandboxFlocEnabled[] = "privacy_sandbox.floc_enabled";
const char kPrivacySandboxFlocDataAccessibleSince[] =
    "privacy_sandbox.floc_data_accessible_since";
const char kStabilityCrashCount[] =
    "user_experience_metrics.stability.crash_count";
const char kPrivacySandboxApisEnabledV2Init[] =
    "privacy_sandbox.apis_enabled_v2_init";

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Deprecated 06/2022.
const char kU2fSecurityKeyApiEnabled[] =
    "extensions.u2f_security_key_api_enabled";

// Deprecated 07/2022.
const char kExtensionToolbar[] = "extensions.toolbar";

// Deprecated 10/2022.
const char kLoadCryptoTokenExtension[] =
    "extensions.load_cryptotoken_extension";
#endif

// Deprecated 10/2022.
const char kOriginTrialPrefKey[] = "origin_trials.persistent_trials";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 07/2022.
// The name of a boolean pref that determines whether we can show the folder
// selection user nudge for the screen capture tool. When this pref is false, it
// means that we showed the nudge at some point and the user interacted with the
// capture mode session UI in such a way that the nudge no longer needs to be
// displayed again.
constexpr char kCanShowFolderSelectionNudge[] =
    "ash.capture_mode.can_show_folder_selection_nudge";
const char kSettingsShowOSBanner[] = "settings.cros.show_os_banner";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 08/2022.
constexpr char kSecurityTokenSessionNotificationDisplayed[] =
    "security_token_session_notification_displayed";
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 08/2022.
const char kProfileAvatarTutorialShown[] =
    "profile.avatar_bubble_tutorial_shown";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 09/2022.
constexpr char kClipboardHistoryNewFeatureBadgeCount[] =
    "ash.clipboard.multipaste_nudges.new_feature_shown_count";
constexpr char kUsersLastInputMethod[] = "UsersLRUInputMethod";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 09/2022.
const char kPrivacySandboxFirstPartySetsDataAccessAllowed[] =
    "privacy_sandbox.first_party_sets_data_access_allowed";

// Deprecated 09/2022.
const char kFirstPartySetsEnabled[] = "first_party_sets.enabled";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 09/2022.
const char kDeprecatedAutofillAssistantConsent[] = "autofill_assistant_switch";
const char kDeprecatedAutofillAssistantEnabled[] =
    "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED";
const char kDeprecatedAutofillAssistantTriggerScriptsEnabled[] =
    "Chrome.AutofillAssistant.ProactiveHelp";
const char kDeprecatedAutofillAssistantTriggerScriptsIsFirstTimeUser[] =
    "Chrome.AutofillAssistant.LiteScriptFirstTimeUser";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Deprecated 10/2022.
const char kSuggestedContentInfoShownInLauncher[] =
    "ash.launcher.suggested_content_info_shown";
const char kSuggestedContentInfoDismissedInLauncher[] =
    "ash.launcher.suggested_content_info_dismissed";
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE) && BUILDFLAG(IS_MAC)
// Deprecated 11/2022.
const char kUserRemovedLoginItem[] = "background_mode.user_removed_login_item";
const char kChromeCreatedLoginItem[] =
    "background_mode.chrome_created_login_item";
const char kMigratedLoginItemPref[] =
    "background_mode.migrated_login_item_pref";
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kPrimaryProfileFirstRunFinished[] =
    "lacros.primary_profile_first_run_finished";
#endif

// Deprecated 11/2022.
const char kLocalConsentsDictionary[] = "local_consents";

// Deprecated 11/2022.
const char kAutofillAssistantConsent[] = "autofill_assistant.consent";
const char kAutofillAssistantEnabled[] = "autofill_assistant.enabled";
const char kAutofillAssistantTriggerScriptsEnabled[] =
    "autofill_assistant.trigger_scripts.enabled";
const char kAutofillAssistantTriggerScriptsIsFirstTimeUser[] =
    "autofill_assistant.trigger_scripts.is_first_time_user";

// Deprecated 12/2022.
const char kAutofillWalletImportStorageCheckboxState[] =
    "autofill.wallet_import_storage_checkbox_state";
const char kDeprecatedReadingListHasUnseenEntries[] =
    "reading_list.has_unseen_entries";

// Deprecated 01/2023
const char kSendDownloadToCloudPref[] =
    "enterprise_connectors.send_download_to_cloud";

#if BUILDFLAG(IS_MAC)
const char kDeviceTrustDisableKeyCreationPref[] =
    "enterprise_connectors.device_trust.disable_key_creation";
#endif  // BUILDFLAG(IS_MAC)

// Deprecated 01/2023.
const char kFileSystemSyncAccessHandleAsyncInterfaceEnabled[] =
    "policy.file_system_sync_access_handle_async_interface_enabled";

// Deprecated 01/2023.
#if !BUILDFLAG(IS_ANDROID)
const char kMediaRouterTabMirroringSources[] =
    "media_router.tab_mirroring_sources";
#endif  // !BUILDFLAG(IS_ANDROID)

// Deprecated 01/2023.
const char kAutofillCreditCardSigninPromoImpressionCount[] =
    "autofill.credit_card_signin_promo_impression_count";

// Deprecated 01/2023
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEventSequenceLastSystemUptime[] =
    "metrics.event_sequence.last_system_uptime";

// Keeps track of the device reset counter.
const char kEventSequenceResetCounter[] =
    "metrics.event_sequence.reset_counter";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kArcTermsShownInOobe[] = "arc.terms.shown_in_oobe";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 02/2023
const char kSyncInvalidationVersions[] = "sync.invalidation_versions";
const char kSyncInvalidationVersions2[] = "sync.invalidation_versions2";

// Deprecated 02/2023.
const char kClearPluginLSODataEnabled[] = "browser.clear_lso_data_enabled";
const char kContentSettingsPluginAllowlist[] =
    "profile.content_settings.plugin_whitelist";
const char kPepperFlashSettingsEnabled[] =
    "browser.pepper_flash_settings_enabled";
const char kPluginsAllowOutdated[] = "plugins.allow_outdated";
const char kPluginsLastInternalDirectory[] = "plugins.last_internal_directory";
const char kPluginsPluginsList[] = "plugins.plugins_list";
const char kPluginsShowDetails[] = "plugins.show_details";

// Deprecated 02/2023.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
const char kWebAppsUrlHandlerInfo[] = "web_apps.url_handler_info";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Deprecated 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kHasSeenSmartLockSignInRemovedNotification[] =
    "easy_unlock.has_seen_smart_lock_sign_in_removed_notification";
const char kEasyUnlockLocalStateTpmKeys[] = "easy_unlock.public_tpm_keys";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 03/2023.
const char kGoogleSearchDomainMixingMetricsEmitterLastMetricsTime[] =
    "browser.last_google_search_domain_mixing_metrics_time";

// Deprecated 03/2023
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kGlanceablesSignoutScreenshotDuration[] =
    "ash.signout_screenshot.duration";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEasyUnlockLocalStateUserPrefs[] = "easy_unlock.user_prefs";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 03/2023
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kDarkLightModeNudgeLeftToShowCount[] =
    "ash.dark_light_mode.educational_nudge";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 03/2023.
#if BUILDFLAG(IS_WIN)
const char kWebAuthnLastOperationWasNativeAPI[] =
    "webauthn.last_op_used_native_api";
#endif

// Deprecated 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEasyUnlockHardlockState[] = "easy_unlock.hardlock_state";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kBentoBarEnabled[] = "ash.bento_bar.enabled";
const char kUserHasUsedDesksRecently[] = "ash.user_has_used_desks_recently";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_ANDROID)
const char kUserSettingEnabled[] = "offline_prefetch.enabled";
const char kBackoff[] = "offline_prefetch.backoff";
const char kLimitlessPrefetchingEnabledTimePref[] =
    "offline_prefetch.limitless_prefetching_enabled_time";
const char kPrefetchTestingHeaderPref[] =
    "offline_prefetch.testing_header_value";
const char kEnabledByServer[] = "offline_prefetch.enabled_by_server";
const char kNextForbiddenCheckTimePref[] = "offline_prefetch.next_gpb_check";
const char kPrefetchCachedGCMToken[] = "offline_prefetch.gcm_token";
#endif

// Deprecated 04/2023.
const char kTypeSubscribedForInvalidations[] =
    "invalidation.registered_for_invalidation";
const char kActiveRegistrationToken[] =
    "invalidation.active_registration_token";
const char kFCMInvalidationClientIDCache[] = "fcm.invalidation.client_id_cache";

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kConsolidatedConsentTrial[] = "per_user_metrics.trial_group";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kOfficeFilesAlwaysMove[] = "filebrowser.office.always_move";
const char kOfficeMoveConfirmationShown[] =
    "filebrowser.office.move_confirmation_shown";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kProximityAuthIsChromeOSLoginEnabled[] =
    "proximity_auth.is_chromeos_login_enabled";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEnrollmentIdUploadedOnChromad[] = "chromad.enrollment_id_uploaded";
const char kLastChromadMigrationAttemptTime[] =
    "chromad.last_migration_attempt_time";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kSmartLockSigninAllowed[] = "smart_lock_signin.allowed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023
#if BUILDFLAG(IS_ANDROID)
const char kVideoTutorialsPreferredLocaleKey[] =
    "video_tutorials.perferred_locale";
const char kVideoTutorialsLastUpdatedTimeKey[] =
    "video_tutorials.last_updated_time";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 05/2023
const char kForceEnablePepperVideoDecoderDevAPI[] =
    "policy.force_enable_pepper_video_decoder_dev_api";

// Deprecated 05/2023
const char kUseMojoVideoDecoderForPepperAllowed[] =
    "policy.use_mojo_video_decoder_for_pepper_allowed";

// Deprecated 05/2023.
const char kPPAPISharedImagesSwapChainAllowed[] =
    "policy.ppapi_shared_images_swap_chain_allowed";

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kOfficeSetupComplete[] = "filebrowser.office.setup_complete";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023.
#if BUILDFLAG(IS_ANDROID)
const char kTimesUPMAuthErrorShown[] = "times_upm_auth_error_shown";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kSamlPasswordSyncToken[] = "saml.password_sync_token";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
const char kScreenAIScheduledDeletionTimePrefName[] =
    "accessibility.screen_ai.scheduled_deletion_time";
#endif

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kEventRemappedToRightClick[] =
    "ash.settings.event_remapped_to_right_click";
#endif

// Register local state used only for migration (clearing or moving to a new
// key).
void RegisterLocalStatePrefsForMigration(PrefRegistrySimple* registry) {
  // Deprecated 06/2022.
  registry->RegisterInt64Pref(kBackgroundTracingLastUpload, 0);
  registry->RegisterIntegerPref(kStabilityGpuCrashCount, 0);
  registry->RegisterIntegerPref(kStabilityRendererCrashCount, 0);
  registry->RegisterIntegerPref(kStabilityExtensionRendererCrashCount, 0);
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(kStabilityPageLoadCount, 0);
#endif

  // Deprecated 07/2022.
  registry->RegisterIntegerPref(kStabilityCrashCount, 0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Deprecated 09/2022
  registry->RegisterDictionaryPref(kUsersLastInputMethod);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 11/2022.
#if BUILDFLAG(ENABLE_BACKGROUND_MODE) && BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kUserRemovedLoginItem, false);
  registry->RegisterBooleanPref(kChromeCreatedLoginItem, false);
  registry->RegisterBooleanPref(kMigratedLoginItemPref, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  registry->RegisterBooleanPref(kPrimaryProfileFirstRunFinished, false);
#endif

  // Deprecated 11/2022.
  registry->RegisterDictionaryPref(kLocalConsentsDictionary);

  // Deprecated 01/2023.
  registry->RegisterListPref(kSendDownloadToCloudPref);

#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kDeviceTrustDisableKeyCreationPref, false);
#endif  // BUILDFLAG(IS_MAC)

  // Deprecated 01/2023
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(kEventSequenceResetCounter, 0);
  registry->RegisterInt64Pref(kEventSequenceLastSystemUptime, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 02/2023.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  registry->RegisterDictionaryPref(kWebAppsUrlHandlerInfo);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Deprecated 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kEasyUnlockLocalStateTpmKeys);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterTimeDeltaPref(kGlanceablesSignoutScreenshotDuration,
                                  base::TimeDelta());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kEasyUnlockLocalStateUserPrefs);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterDictionaryPref(kEasyUnlockHardlockState);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 04/2023.
  registry->RegisterDictionaryPref(kTypeSubscribedForInvalidations);
  registry->RegisterStringPref(kActiveRegistrationToken, std::string());
  registry->RegisterStringPref(kFCMInvalidationClientIDCache, std::string());

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterStringPref(kConsolidatedConsentTrial, std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kEnrollmentIdUploadedOnChromad, false);
  registry->RegisterTimePref(kLastChromadMigrationAttemptTime,
                             /*default_value=*/base::Time());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 05/2023.
  registry->RegisterBooleanPref(kForceEnablePepperVideoDecoderDevAPI, false);

  // Deprecated 05/2023.
  registry->RegisterBooleanPref(kUseMojoVideoDecoderForPepperAllowed, true);

  // Deprecated 05/2023.
  registry->RegisterBooleanPref(kPPAPISharedImagesSwapChainAllowed, true);

// Deprecated 05/2023.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  registry->RegisterTimePref(kScreenAIScheduledDeletionTimePrefName,
                             base::Time());
#endif
}

// Register prefs used only for migration (clearing or moving to a new key).
void RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kCanShowFolderSelectionNudge,
                                /*default_value=*/true);

  registry->RegisterIntegerPref(kImprovedShortcutsNotificationShownCount, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  chrome_browser_net::secure_dns::RegisterProbesSettingBackupPref(registry);

#if !BUILDFLAG(IS_ANDROID)
  // Removed in M91.
  registry->RegisterBooleanPref(prefs::kMediaFeedsBackgroundFetching, false);
  registry->RegisterBooleanPref(prefs::kMediaFeedsSafeSearchEnabled, false);
  registry->RegisterBooleanPref(prefs::kMediaFeedsAutoSelectEnabled, false);

#endif

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(
      prefs::kManagedProfileSerialAllowAllPortsForUrlsDeprecated);
  registry->RegisterListPref(
      prefs::kManagedProfileSerialAllowUsbDevicesForUrlsDeprecated);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterBooleanPref(kTokenServiceDiceCompatible, false);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  registry->RegisterBooleanPref(kPrivacySandboxPreferencesReconciled, false);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(kDownloadLaterPromptStatus, 0);
#endif  // BUILDFLAG(IS_ANDROID)

  // Deprecated 06/2022
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(kU2fSecurityKeyApiEnabled, false);
#endif

  // Deprecated 07/2022
  registry->RegisterBooleanPref(kPrivacySandboxFlocEnabled, true);
  registry->RegisterBooleanPref(kPrivacySandboxFlocDataAccessibleSince, false);
  registry->RegisterBooleanPref(kPrivacySandboxApisEnabledV2Init, false);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Deprecated 07/2022
  registry->RegisterListPref(kExtensionToolbar);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kSettingsShowOSBanner, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Deprecated 08/2022
  registry->RegisterBooleanPref(kSecurityTokenSessionNotificationDisplayed,
                                false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Deprecated 08/2022.
  registry->RegisterIntegerPref(kProfileAvatarTutorialShown, 0);
#endif

#if BUILDFLAG(IS_LINUX)
  // Deprecated 08/2022.
  registry->RegisterBooleanPref(prefs::kUsesSystemThemeDeprecated, false);
#endif

  // Deprecated 09/2022
  registry->RegisterBooleanPref(kPrivacySandboxFirstPartySetsDataAccessAllowed,
                                true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(kClipboardHistoryNewFeatureBadgeCount, 0);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 09/2022.
  registry->RegisterBooleanPref(kFirstPartySetsEnabled, true);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Deprecated 10/2022.
  registry->RegisterBooleanPref(kLoadCryptoTokenExtension, false);
#endif

// Deprecated 10/2022.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(kSuggestedContentInfoShownInLauncher, 0);
  registry->RegisterBooleanPref(kSuggestedContentInfoDismissedInLauncher,
                                false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // Deprecated 10/2022.
  registry->RegisterDictionaryPref(kOriginTrialPrefKey,
                                   PrefRegistry::LOSSY_PREF);

  // Deprecated 11/2022.
  registry->RegisterBooleanPref(kAutofillAssistantEnabled, true);
  registry->RegisterBooleanPref(kAutofillAssistantConsent, false);
  registry->RegisterBooleanPref(kAutofillAssistantTriggerScriptsEnabled, true);
  registry->RegisterBooleanPref(kAutofillAssistantTriggerScriptsIsFirstTimeUser,
                                true);

  // Deprecated 12/2022.
  registry->RegisterBooleanPref(kAutofillWalletImportStorageCheckboxState,
                                true);
  registry->RegisterBooleanPref(kDeprecatedReadingListHasUnseenEntries, false);

  // Deprecated 01/2023.
  registry->RegisterBooleanPref(
      kFileSystemSyncAccessHandleAsyncInterfaceEnabled, false);

  // Deprecated 01/2023.
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(kMediaRouterTabMirroringSources);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Deprecated 01/2023.
  registry->RegisterIntegerPref(kAutofillCreditCardSigninPromoImpressionCount,
                                0);

  // Deprecated 01/2023.
  registry->RegisterListPref(kSendDownloadToCloudPref);

// Deprecated 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kArcTermsShownInOobe, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 02/2023.
  registry->RegisterDictionaryPref(kSyncInvalidationVersions);
  registry->RegisterDictionaryPref(kSyncInvalidationVersions2);

  // Deprecated 02/2023.
  registry->RegisterBooleanPref(kClearPluginLSODataEnabled, false);
  registry->RegisterDictionaryPref(kContentSettingsPluginAllowlist);
  registry->RegisterBooleanPref(kPepperFlashSettingsEnabled, false);
  registry->RegisterBooleanPref(kPluginsAllowOutdated, false);
  registry->RegisterFilePathPref(kPluginsLastInternalDirectory,
                                 base::FilePath());
  registry->RegisterListPref(kPluginsPluginsList);
  registry->RegisterBooleanPref(kPluginsShowDetails, false);

// Deprecated 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kHasSeenSmartLockSignInRemovedNotification,
                                false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 03/2023.
  registry->RegisterTimePref(
      kGoogleSearchDomainMixingMetricsEmitterLastMetricsTime, base::Time());

  // Deprecated 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(kDarkLightModeNudgeLeftToShowCount,
                                ash::kDarkLightModeNudgeMaxShownCount);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 03/2023.
#if BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(kWebAuthnLastOperationWasNativeAPI, false);
#endif

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kBentoBarEnabled, false);
  registry->RegisterBooleanPref(kUserHasUsedDesksRecently, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(kBackoff);
  registry->RegisterBooleanPref(kUserSettingEnabled, true);
  registry->RegisterTimePref(kLimitlessPrefetchingEnabledTimePref,
                             base::Time());
  registry->RegisterStringPref(kPrefetchTestingHeaderPref, std::string());
  registry->RegisterBooleanPref(kEnabledByServer, false);
  registry->RegisterTimePref(kNextForbiddenCheckTimePref, base::Time());
  registry->RegisterStringPref(kPrefetchCachedGCMToken, std::string());
#endif

  // Deprecated 04/2023.
  registry->RegisterDictionaryPref(kTypeSubscribedForInvalidations);
  registry->RegisterStringPref(kActiveRegistrationToken, std::string());
  registry->RegisterStringPref(kFCMInvalidationClientIDCache, std::string());

  // Deprecated 04/2023.
#if BUILDFLAG(IS_ANDROID)
  ntp_snippets::prefs::RegisterProfilePrefsForMigrationApril2023(registry);
#endif

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kOfficeFilesAlwaysMove, false);
  registry->RegisterBooleanPref(kOfficeMoveConfirmationShown, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kProximityAuthIsChromeOSLoginEnabled, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kSmartLockSigninAllowed, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Deprecated 05/2023.
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(kVideoTutorialsPreferredLocaleKey,
                               std::string());
  registry->RegisterTimePref(kVideoTutorialsLastUpdatedTimeKey, base::Time());
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kOfficeSetupComplete, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023.
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(kTimesUPMAuthErrorShown, 0);
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterStringPref(kSamlPasswordSyncToken, std::string());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Deprecated 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kEventRemappedToRightClick, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Call outs to individual subsystems that register Local State (browser-wide)
  // prefs en masse. See RegisterProfilePrefs for per-profile prefs. Please
  // keep this list alphabetized.
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterLocalStatePrefs(registry);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  chrome_labs_prefs::RegisterLocalStatePrefs(registry);
#endif
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  chrome::enterprise_util::RegisterLocalStatePrefs(registry);
  component_updater::RegisterPrefs(registry);
  domain_reliability::RegisterPrefs(registry);
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
  metrics::RegisterDemographicsLocalStatePrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  optimization_guide::prefs::RegisterLocalStatePrefs(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::ManagementService::RegisterLocalStatePrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileAttributesEntry::RegisterLocalStatePrefs(registry);
  ProfileAttributesStorage::RegisterPrefs(registry);
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry);
  profiles::RegisterPrefs(registry);
#if BUILDFLAG(IS_ANDROID)
  PushMessagingServiceImpl::RegisterPrefs(registry);
#endif
  RegisterScreenshotPrefs(registry);
  safe_browsing::RegisterLocalStatePrefs(registry);
  secure_origin_allowlist::RegisterPrefs(registry);
  segmentation_platform::SegmentationPlatformService::RegisterLocalStatePrefs(
      registry);
#if !BUILDFLAG(IS_ANDROID)
  SerialPolicyAllowedPorts::RegisterPrefs(registry);
  HidPolicyAllowedDevices::RegisterLocalStatePrefs(registry);
#endif
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(registry);
  SystemNetworkContextManager::RegisterPrefs(registry);
  tracing::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  // Individual preferences. If you have multiple preferences that should
  // clearly be grouped together, please group them together into a helper
  // function called above. Please keep this list alphabetized.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled, false);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kUserAgentClientHintsGREASEUpdateEnabled, true);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(policy::policy_prefs::kBackForwardCacheEnabled,
                                true);
#endif  // BUILDFLAG(IS_ANDROID)

  // Below this point is for platform-specific and compile-time conditional
  // calls. Please follow the helper-function-first-then-direct-calls pattern
  // established above, and keep things alphabetized.

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID)
  ::android::RegisterPrefs(registry);

  registry->RegisterIntegerPref(first_run::kTosDialogBehavior, 0);
  registry->RegisterBooleanPref(lens::kLensCameraAssistedSearchEnabled, true);
#else   // BUILDFLAG(IS_ANDROID)
  enterprise_connectors::RegisterLocalStatePrefs(registry);
  gcm::RegisterPrefs(registry);
  headless::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  media_router::RegisterLocalStatePrefs(registry);
  metrics::TabStatsTracker::RegisterPrefs(registry);
  performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(registry);
  RegisterBrowserPrefs(registry);
  speech::SodaInstaller::RegisterLocalStatePrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  WhatsNewUI::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
  FirstRunService::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  arc::prefs::RegisterLocalStatePrefs(registry);
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  ash::ArcKioskAppManager::RegisterPrefs(registry);
  ash::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  ash::cert_provisioning::RegisterLocalStatePrefs(registry);
  ash::CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(registry);
  ash::ManagedCellularPrefHandler::RegisterLocalStatePrefs(registry);
  ash::ChromeUserManagerImpl::RegisterPrefs(registry);
  crosapi::browser_util::RegisterLocalStatePrefs(registry);
  ash::CupsPrintersManager::RegisterLocalStatePrefs(registry);
  ash::BrowserDataMigratorImpl::RegisterLocalStatePrefs(registry);
  ash::bluetooth_config::BluetoothPowerControllerImpl::RegisterLocalStatePrefs(
      registry);
  ash::bluetooth_config::DeviceNameManagerImpl::RegisterLocalStatePrefs(
      registry);
  ash::DemoModeResourcesRemover::RegisterLocalStatePrefs(registry);
  ash::DemoSession::RegisterLocalStatePrefs(registry);
  ash::DemoSetupController::RegisterLocalStatePrefs(registry);
  ash::DeviceNameStore::RegisterLocalStatePrefs(registry);
  chromeos::DeviceOAuth2TokenStoreChromeOS::RegisterPrefs(registry);
  ash::device_settings_cache::RegisterPrefs(registry);
  ash::EnableAdbSideloadingScreen::RegisterPrefs(registry);
  ash::device_activity::DeviceActivityController::RegisterPrefs(registry);
  ash::EnableDebuggingScreenHandler::RegisterPrefs(registry);
  ash::FastTransitionObserver::RegisterPrefs(registry);
  ash::HWDataUsageController::RegisterLocalStatePrefs(registry);
  ash::KerberosCredentialsManager::RegisterLocalStatePrefs(registry);
  ash::KioskAppManager::RegisterLocalStatePrefs(registry);
  ash::KioskCryptohomeRemover::RegisterPrefs(registry);
  ash::language_prefs::RegisterPrefs(registry);
  ash::local_search_service::SearchMetricsReporter::RegisterLocalStatePrefs(
      registry);
  ash::login::SecurityTokenSessionController::RegisterLocalStatePrefs(registry);
  ash::reporting::LoginLogoutReporter::RegisterPrefs(registry);
  ash::MultiProfileUserController::RegisterPrefs(registry);
  ash::NetworkMetadataStore::RegisterPrefs(registry);
  ash::NetworkThrottlingObserver::RegisterPrefs(registry);
  ash::PowerMetricsReporter::RegisterLocalStatePrefs(registry);
  ash::platform_keys::KeyPermissionsManagerImpl::RegisterLocalStatePrefs(
      registry);
  ash::power::auto_screen_brightness::MetricsReporter::RegisterLocalStatePrefs(
      registry);
  ash::Preferences::RegisterPrefs(registry);
  ash::ResetScreen::RegisterPrefs(registry);
  ash::SchedulerConfigurationManager::RegisterLocalStatePrefs(registry);
  ash::ServicesCustomizationDocument::RegisterPrefs(registry);
  ash::StartupUtils::RegisterPrefs(registry);
  ash::StatsReportingController::RegisterLocalStatePrefs(registry);
  ash::system::AutomaticRebootManager::RegisterPrefs(registry);
  ash::TimeZoneResolver::RegisterPrefs(registry);
  ash::UserImageManager::RegisterPrefs(registry);
  ash::UserSessionManager::RegisterPrefs(registry);
  ash::WebKioskAppManager::RegisterPrefs(registry);
  component_updater::MetadataTable::RegisterPrefs(registry);
  ash::CryptAuthDeviceIdProviderImpl::RegisterLocalPrefs(registry);
  extensions::ExtensionAssetsManagerChromeOS::RegisterPrefs(registry);
  extensions::ExtensionsPermissionsTracker::RegisterLocalStatePrefs(registry);
  extensions::lock_screen_data::LockScreenItemStorage::RegisterLocalState(
      registry);
  extensions::login_api::RegisterLocalStatePrefs(registry);
  ::onc::RegisterPrefs(registry);
  metrics::structured::ChromeStructuredMetricsRecorder::RegisterLocalStatePrefs(
      registry);
  policy::AdbSideloadingAllowanceModePolicyHandler::RegisterPrefs(registry);
  // TODO(b/265923216): Replace with EnrollmentStateFetcher::RegisterPrefs.
  policy::AutoEnrollmentClientImpl::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorAsh::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerAsh::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(registry);
  policy::DMTokenStorage::RegisterPrefs(registry);
  policy::EnrollmentRequisitionManager::RegisterPrefs(registry);
  policy::MinimumVersionPolicyHandler::RegisterPrefs(registry);
  policy::EuiccStatusUploader::RegisterLocalStatePrefs(registry);
  policy::TPMAutoUpdateModePolicyHandler::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);
  UpgradeDetectorChromeos::RegisterPrefs(registry);
  RegisterNearbySharingLocalPrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  chromeos::echo_offer::RegisterPrefs(registry);
  memory::OOMKillsMonitor::RegisterPrefs(registry);
  policy::SystemFeaturesDisableListPolicyHandler::RegisterPrefs(registry);
  policy::DlpRulesManagerImpl::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
  system_media_permissions::RegisterSystemMediaPermissionStatesPrefs(registry);
  AppShimRegistry::Get()->RegisterLocalPrefs(registry);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  AccountCache::RegisterLocalStatePrefs(registry);
  lacros_prefs::RegisterLocalStatePrefs(registry);
  KioskSessionServiceLacros::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(IS_WIN)
  OSCrypt::RegisterLocalPrefs(registry);
  registry->RegisterBooleanPref(prefs::kRendererCodeIntegrityEnabled, true);
  registry->RegisterBooleanPref(prefs::kRendererAppContainerEnabled, true);
  registry->RegisterBooleanPref(prefs::kBlockBrowserLegacyExtensionPoints,
                                true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kNativeWindowOcclusionEnabled, true);
  component_updater::RegisterPrefsForSwReporter(registry);
  safe_browsing::RegisterChromeCleanerScanCompletionTimePref(registry);
  MediaFoundationServiceMonitor::RegisterPrefs(registry);
  os_crypt::RegisterLocalStatePrefs(registry);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  IncompatibleApplicationsUpdater::RegisterLocalStatePrefs(registry);
  ModuleDatabase::RegisterLocalStatePrefs(registry);
  ThirdPartyConflictsManager::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterDefaultBrowserPromptPrefs(registry);
  downgrade::RegisterPrefs(registry);
  DeviceOAuth2TokenStoreDesktop::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  screen_ai::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

#if BUILDFLAG(IS_WIN)
  PlatformAuthPolicyObserver::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_WIN)

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
  autofill::prefs::RegisterProfilePrefs(registry);
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);
  certificate_transparency::prefs::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  chrome_labs_prefs::RegisterProfilePrefs(registry);
  ChromeLocationBarModelDelegate::RegisterProfilePrefs(registry);
  StatefulSSLHostStateDelegate::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::NetErrorTabHelper::RegisterProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  commerce::RegisterPrefs(registry);
  DocumentProvider::RegisterProfilePrefs(registry);
  enterprise::RegisterIdentifiersProfilePrefs(registry);
  enterprise_reporting::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  dom_distiller::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  history_clusters::prefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
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
  performance_manager::user_tuning::prefs::RegisterProfilePrefs(registry);
  permissions::PermissionActionsHistory::RegisterProfilePrefs(registry);
  PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(registry);
  PlatformNotificationServiceImpl::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  prefetch::RegisterPredictionOptionsProfilePrefs(registry);
  PrefetchOriginDecider::RegisterPrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  privacy_sandbox::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProfileNetworkContextService::RegisterProfilePrefs(registry);
  custom_handlers::ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  QuietNotificationPermissionUiState::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  safe_browsing::file_type::RegisterProfilePrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
  SearchPrefetchService::RegisterProfilePrefs(registry);
  blocked_content::SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
      registry);
  security_interstitials::InsecureFormBlockingPage::RegisterProfilePrefs(
      registry);
  segmentation_platform::SegmentationPlatformService::RegisterProfilePrefs(
      registry);
  segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
      registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  SharingSyncPreference::RegisterProfilePrefs(registry);
  site_engagement::SiteEngagementService::RegisterProfilePrefs(registry);
  storage::RegisterProfilePrefs(registry);
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
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::AudioAPI::RegisterUserPrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::ExtensionsUI::RegisterProfilePrefs(registry);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extensions::shared_storage::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  extensions::PermissionsManager::RegisterProfilePrefs(registry);
  extensions::RuntimeAPI::RegisterPrefs(registry);
  // TODO(devlin): This would be more inline with the other calls here if it
  // were nested in either a class or separate namespace with a simple
  // Register[Profile]Prefs() name.
  extensions::RegisterSettingsOverriddenUiPrefs(registry);
  update_client::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflineMetricsCollectorImpl::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PDF)
  registry->RegisterListPref(prefs::kPdfLocalFileAccessAllowedForDomains,
                             base::Value::List());
  registry->RegisterBooleanPref(prefs::kPdfUseSkiaRendererEnabled, true);
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PolicySettings::RegisterProfilePrefs(registry);
  printing::PrintPreviewStickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  ChildAccountService::RegisterProfilePrefs(registry);
  supervised_user::SupervisedUserService::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_FEED_V2)
  feed::prefs::RegisterFeedSharedProfilePrefs(registry);
  feed::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  content_creation::prefs::RegisterProfilePrefs(registry);
  explore_sites::HistoryStatisticsReporter::RegisterPrefs(registry);
  KnownInterceptionDisclosureInfoBarDelegate::RegisterProfilePrefs(registry);
  MediaDrmOriginIdManager::RegisterProfilePrefs(registry);
  NotificationChannelsProviderAndroid::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  OomInterventionDecider::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  permissions::GeolocationPermissionContextAndroid::RegisterProfilePrefs(
      registry);
  query_tiles::RegisterPrefs(registry);
  RecentTabsPagePrefs::RegisterProfilePrefs(registry);
  usage_stats::UsageStatsBridge::RegisterProfilePrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  webapps::InstallPromptPrefs::RegisterLocalPrefs(registry);
#else  // BUILDFLAG(IS_ANDROID)
  bookmarks_webui::RegisterProfilePrefs(registry);
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  BrowserFeaturePromoSnoozeService::RegisterProfilePrefs(registry);
  captions::LiveTranslateController::RegisterProfilePrefs(registry);
  ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(registry);
  companion::PromoHandler::RegisterProfilePrefs(registry);
  DeviceServiceImpl::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
  DriveService::RegisterProfilePrefs(registry);
  enterprise_connectors::RegisterProfilePrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  gcm::RegisterProfilePrefs(registry);
  HatsService::RegisterProfilePrefs(registry);
  NtpCustomBackgroundService::RegisterProfilePrefs(registry);
  media_router::RegisterAccessCodeProfilePrefs(registry);
  media_router::RegisterProfilePrefs(registry);
  NewTabPageHandler::RegisterProfilePrefs(registry);
  NewTabPageUI::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  ntp::SafeBrowsingHandler::RegisterProfilePrefs(registry);
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
  PhotosService::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  policy::DeveloperToolsPolicyHandler::RegisterProfilePrefs(registry);
  PromoService::RegisterProfilePrefs(registry);
  RegisterReadAnythingProfilePrefs(registry);
  settings::SettingsUI::RegisterProfilePrefs(registry);
  send_tab_to_self::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
  tab_search_prefs::RegisterProfilePrefs(registry);
  RecipesService::RegisterProfilePrefs(registry);
  UnifiedAutoplayConfig::RegisterProfilePrefs(registry);
  CartService::RegisterProfilePrefs(registry);
  commerce::ShoppingListUiTabHelper::RegisterProfilePrefs(registry);
  user_notes::RegisterProfilePrefs(registry);
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  captions::LiveCaptionController::RegisterProfilePrefs(registry);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  apps::SupportedLinksInfoBarPrefsService::RegisterProfilePrefs(registry);
  extensions::login_api::RegisterProfilePrefs(registry);
  extensions::platform_keys::RegisterProfilePrefs(registry);
  certificate_manager::CertificatesHandler::RegisterProfilePrefs(registry);
  policy::PolicyCertService::RegisterProfilePrefs(registry);
  registry->RegisterBooleanPref(prefs::kDeskAPIThirdPartyAccessEnabled, false);
  registry->RegisterListPref(prefs::kDeskAPIThirdPartyAllowlist);
  registry->RegisterBooleanPref(prefs::kInsightsExtensionEnabled, false);
  // By default showing Sync Consent is set to true. It can changed by policy.
  registry->RegisterBooleanPref(prefs::kEnableSyncConsent, true);
  registry->RegisterListPref(
      chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
      PrefRegistry::PUBLIC);
#if BUILDFLAG(USE_CUPS)
  extensions::PrintingAPIHandler::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(USE_CUPS)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_list::AppListSyncableService::RegisterProfilePrefs(registry);
  apps::AppPlatformMetricsService::RegisterProfilePrefs(registry);
  apps::AppPreloadService::RegisterProfilePrefs(registry);
  apps::deduplication::AppDeduplicationService::RegisterProfilePrefs(registry);
  apps::webapk_prefs::RegisterProfilePrefs(registry);
  arc::prefs::RegisterProfilePrefs(registry);
  ArcAppListPrefs::RegisterProfilePrefs(registry);
  ash::AccountAppsAvailability::RegisterPrefs(registry);
  account_manager::AccountManager::RegisterPrefs(registry);
  ash::ApkWebAppService::RegisterProfilePrefs(registry);
  ash::app_time::AppActivityRegistry::RegisterProfilePrefs(registry);
  ash::app_time::AppTimeController::RegisterProfilePrefs(registry);
  ash::assistant::prefs::RegisterProfilePrefs(registry);
  ash::auth::AuthFactorConfig::RegisterPrefs(registry);
  ash::bluetooth::DebugLogsManager::RegisterPrefs(registry);
  ash::bluetooth_config::BluetoothPowerControllerImpl::RegisterProfilePrefs(
      registry);
  ash::HatsBluetoothRevampTriggerImpl::RegisterProfilePrefs(registry);
  ash::ClientAppMetadataProviderService::RegisterProfilePrefs(registry);
  ash::CupsPrintersManager::RegisterProfilePrefs(registry);
  ash::device_sync::RegisterProfilePrefs(registry);
  ash::FamilyUserChromeActivityMetrics::RegisterProfilePrefs(registry);
  ash::FamilyUserMetricsService::RegisterProfilePrefs(registry);
  ash::FamilyUserSessionMetrics::RegisterProfilePrefs(registry);
  crosapi::NetworkSettingsServiceAsh::RegisterProfilePrefs(registry);
  ash::InlineLoginHandlerImpl::RegisterProfilePrefs(registry);
  ash::first_run::RegisterProfilePrefs(registry);
  ash::file_system_provider::RegisterProfilePrefs(registry);
  ash::full_restore::RegisterProfilePrefs(registry);
  ash::KerberosCredentialsManager::RegisterProfilePrefs(registry);
  ash::multidevice_setup::MultiDeviceSetupService::RegisterProfilePrefs(
      registry);
  ash::MultiProfileUserController::RegisterProfilePrefs(registry);
  ash::NetworkMetadataStore::RegisterPrefs(registry);
  ash::ReleaseNotesStorage::RegisterProfilePrefs(registry);
  ash::HelpAppNotificationController::RegisterProfilePrefs(registry);
  ash::quick_unlock::FingerprintStorage::RegisterProfilePrefs(registry);
  ash::quick_unlock::PinStoragePrefs::RegisterProfilePrefs(registry);
  ash::Preferences::RegisterProfilePrefs(registry);
  ash::EnterprisePrintersProvider::RegisterProfilePrefs(registry);
  ash::parent_access::ParentAccessService::RegisterProfilePrefs(registry);
  quick_answers::prefs::RegisterProfilePrefs(registry);
  ash::quick_unlock::RegisterProfilePrefs(registry);
  ash::RegisterSamlProfilePrefs(registry);
  ash::ScreenTimeController::RegisterProfilePrefs(registry);
  ash::EduCoexistenceConsentInvalidationController::RegisterProfilePrefs(
      registry);
  ash::EduCoexistenceLoginHandler::RegisterProfilePrefs(registry);
  ash::SigninErrorNotifier::RegisterPrefs(registry);
  ash::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  ash::settings::OSSettingsUI::RegisterProfilePrefs(registry);
  ash::StartupUtils::RegisterOobeProfilePrefs(registry);
  ash::user_image::prefs::RegisterProfilePrefs(registry);
  ash::UserImageSyncObserver::RegisterProfilePrefs(registry);
  ChromeMetricsServiceClient::RegisterProfilePrefs(registry);
  crostini::prefs::RegisterProfilePrefs(registry);
  ash::attestation::TpmChallengeKey::RegisterProfilePrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
  guest_os::prefs::RegisterProfilePrefs(registry);
  lock_screen_apps::StateController::RegisterProfilePrefs(registry);
  plugin_vm::prefs::RegisterProfilePrefs(registry);
  policy::ArcAppInstallEventLogger::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogManagerWrapper::RegisterProfilePrefs(registry);
  policy::StatusCollector::RegisterProfilePrefs(registry);
  ash::SystemProxyManager::RegisterProfilePrefs(registry);
  ChromeShelfPrefs::RegisterProfilePrefs(registry);
  ::onc::RegisterProfilePrefs(registry);
  ash::cert_provisioning::RegisterProfilePrefs(registry);
  borealis::prefs::RegisterProfilePrefs(registry);
  ash::ChromeScanningAppDelegate::RegisterProfilePrefs(registry);
  ProjectorAppClientImpl::RegisterProfilePrefs(registry);
  ash::floating_workspace_util::RegisterProfilePrefs(registry);
  policy::RebootNotificationsScheduler::RegisterProfilePrefs(registry);
  ash::KioskAppManager::RegisterProfilePrefs(registry);
  file_manager::file_tasks::RegisterProfilePrefs(registry);
  file_manager::prefs::RegisterProfilePrefs(registry);
  bruschetta::prefs::RegisterProfilePrefs(registry);
  wallpaper_handlers::prefs::RegisterProfilePrefs(registry);
  ash::reporting::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_prefs::RegisterProfilePrefs(registry);
  chromeos::ProxyConfigServiceLacros::RegisterProfilePrefs(registry);
  lacros_prefs::RegisterExtensionControlledAshPrefs(registry);
  KioskSessionServiceLacros::RegisterProfilePrefs(registry);
  apps::WebsiteMetricsServiceLacros::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_WIN)
  CdmPrefServiceHelper::RegisterProfilePrefs(registry);
  component_updater::RegisterProfilePrefsForSwReporter(registry);
  FontPrewarmerTabHelper::RegisterProfilePrefs(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
  safe_browsing::SettingsResetPromptPrefsManager::RegisterProfilePrefs(
      registry);
  safe_browsing::PostCleanupSettingsResetter::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  device_signals::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  browser_switcher::BrowserSwitcherPrefs::RegisterProfilePrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  preinstalled_apps::RegisterProfilePrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  sharing_hub::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  accessibility_prefs::RegisterInvertBubbleUserPrefs(registry);
  side_search_prefs::RegisterProfilePrefs(registry);
  RegisterBrowserViewProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP)
  registry->RegisterBooleanPref(
      prefs::kLensRegionSearchEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kLensDesktopNTPSearchEnabled, true);
#endif

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      webauthn::pref_names::kRemoteProxiedRequestsAllowed, false);

  side_panel_prefs::RegisterProfilePrefs(registry);
#endif

  registry->RegisterBooleanPref(webauthn::pref_names::kAllowWithBrokenCerts,
                                false);

  registry->RegisterBooleanPref(prefs::kPrivacyGuideViewed, false);

#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(policy::policy_prefs::kScreenTimeEnabled, true);
#endif

  RegisterProfilePrefsForMigration(registry);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kHighEfficiencyChipExpandedCount, 0);
  registry->RegisterTimePref(prefs::kLastHighEfficiencyChipExpandedTimestamp,
                             base::Time());
  permissions::PermissionHatsTriggerHelper::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kVirtualKeyboardResizesLayoutByDefault,
                                false);
#endif

  registry->RegisterTimePref(prefs::kDIPSTimerLastUpdate, base::Time());

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  registry->RegisterBooleanPref(
      prefs::kAccessibilityPdfOcrAlwaysActive, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterUserProfilePrefs(registry, g_browser_process->GetApplicationLocale());
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                              const std::string& locale) {
  RegisterProfilePrefs(registry, locale);

#if BUILDFLAG(IS_ANDROID)
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
  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.

  // BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

  // Added 06/2022.
  local_state->ClearPref(kBackgroundTracingLastUpload);
  local_state->ClearPref(kStabilityGpuCrashCount);
  local_state->ClearPref(kStabilityRendererCrashCount);
  local_state->ClearPref(kStabilityExtensionRendererCrashCount);
#if !BUILDFLAG(IS_ANDROID)
  local_state->ClearPref(kStabilityPageLoadCount);
#endif

  // Added 07/2002.
  local_state->ClearPref(kStabilityCrashCount);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 09/2022
  local_state->ClearPref(kUsersLastInputMethod);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE) && BUILDFLAG(IS_MAC)
  // Added 11/2022.
  local_state->ClearPref(kUserRemovedLoginItem);
  local_state->ClearPref(kChromeCreatedLoginItem);
  local_state->ClearPref(kMigratedLoginItemPref);
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_state->HasPrefPath(kPrimaryProfileFirstRunFinished)) {
    bool old_value = local_state->GetBoolean(kPrimaryProfileFirstRunFinished);
    local_state->ClearPref(kPrimaryProfileFirstRunFinished);
    local_state->SetBoolean(prefs::kFirstRunFinished, old_value);
  }
#endif

  // Added 11/2022.
  local_state->ClearPref(kLocalConsentsDictionary);

  // Added 01/2023
  local_state->ClearPref(kSendDownloadToCloudPref);

#if BUILDFLAG(IS_MAC)
  local_state->ClearPref(kDeviceTrustDisableKeyCreationPref);
#endif  // BUILDFLAG(IS_MAC)

  // Added 01/2023
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kEventSequenceLastSystemUptime);
  local_state->ClearPref(kEventSequenceResetCounter);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 02/2023
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  local_state->ClearPref(kWebAppsUrlHandlerInfo);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Added 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kEasyUnlockLocalStateTpmKeys);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kGlanceablesSignoutScreenshotDuration);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kEasyUnlockLocalStateUserPrefs);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kEasyUnlockHardlockState);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 04/2023.
  local_state->ClearPref(kTypeSubscribedForInvalidations);
  local_state->ClearPref(kActiveRegistrationToken);
  local_state->ClearPref(kFCMInvalidationClientIDCache);

// Added 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kConsolidatedConsentTrial);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  local_state->ClearPref(kEnrollmentIdUploadedOnChromad);
  local_state->ClearPref(kLastChromadMigrationAttemptTime);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 05/2023.
  local_state->ClearPref(kForceEnablePepperVideoDecoderDevAPI);

  // Added 05/2023.
  local_state->ClearPref(kUseMojoVideoDecoderForPepperAllowed);

  // Added 05/2023
  local_state->ClearPref(kPPAPISharedImagesSwapChainAllowed);

// Added 05/2023.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  local_state->ClearPref(kScreenAIScheduledDeletionTimePrefName);
#endif

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS

  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.
}

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteProfilePrefs(Profile* profile) {
  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.

  // BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

  PrefService* profile_prefs = profile->GetPrefs();

  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(profile_prefs);

  // Added 3/2020.
  // TODO(crbug.com/1062698): Remove this once the privacy settings redesign
  // is fully launched.
  chrome_browser_net::secure_dns::MigrateProbesSettingToOrFromBackup(
      profile_prefs);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Added 06/2022.
  profile_prefs->ClearPref(kTokenServiceDiceCompatible);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
  // Added 06/2022.
  syncer::SyncPrefs::MigrateSyncRequestedPrefPostMice(profile_prefs);
  profile_prefs->ClearPref(kDownloadLaterPromptStatus);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 06/2022.
  profile_prefs->ClearPref(kImprovedShortcutsNotificationShownCount);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 06/2022.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  profile_prefs->ClearPref(kU2fSecurityKeyApiEnabled);
#endif
  profile_prefs->ClearPref(prefs::kCloudPrintSubmitEnabled);

  // Added 06/2022.
  profile_prefs->ClearPref(kPrivacySandboxPreferencesReconciled);

  // Added 07/2022
  profile_prefs->ClearPref(kPrivacySandboxFlocEnabled);
  profile_prefs->ClearPref(kPrivacySandboxFlocDataAccessibleSince);
  profile_prefs->ClearPref(kPrivacySandboxApisEnabledV2Init);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Added 07/2022.
  profile_prefs->ClearPref(kExtensionToolbar);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 07/2022.
  profile_prefs->ClearPref(kCanShowFolderSelectionNudge);
  profile_prefs->ClearPref(kSettingsShowOSBanner);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 08/2022.
  profile_prefs->ClearPref(kSecurityTokenSessionNotificationDisplayed);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 08/2022.
  profile_prefs->ClearPref(kProfileAvatarTutorialShown);
#endif

#if BUILDFLAG(IS_LINUX)
  // Added 08/2022.
  if (profile_prefs->HasPrefPath(prefs::kUsesSystemThemeDeprecated)) {
    auto migrated_theme =
        profile_prefs->GetBoolean(prefs::kUsesSystemThemeDeprecated)
            ? ui::SystemTheme::kGtk
            : ui::SystemTheme::kDefault;
    profile_prefs->SetInteger(prefs::kSystemTheme,
                              static_cast<int>(migrated_theme));
  }
  profile_prefs->ClearPref(prefs::kUsesSystemThemeDeprecated);
#endif

  // Added 09/2022.
  profile_prefs->ClearPref(kPrivacySandboxFirstPartySetsDataAccessAllowed);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 09/2022.
  profile_prefs->ClearPref(kClipboardHistoryNewFeatureBadgeCount);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 09/2022.
#if BUILDFLAG(IS_ANDROID)
  auto migrate_shared_pref = [profile_prefs](const std::string& source,
                                             const std::string& target) {
    if (absl::optional<bool> shared_pref =
            android::shared_preferences::GetAndClearBoolean(source);
        shared_pref) {
      profile_prefs->SetBoolean(target, shared_pref.value());
    }
  };

  // These settings will also need to be deleted from ChromePreferenceKeys.java.
  migrate_shared_pref(kDeprecatedAutofillAssistantConsent,
                      kAutofillAssistantConsent);
  migrate_shared_pref(kDeprecatedAutofillAssistantEnabled,
                      kAutofillAssistantEnabled);
  migrate_shared_pref(kDeprecatedAutofillAssistantTriggerScriptsEnabled,
                      kAutofillAssistantTriggerScriptsEnabled);
  migrate_shared_pref(kDeprecatedAutofillAssistantTriggerScriptsIsFirstTimeUser,
                      kAutofillAssistantTriggerScriptsIsFirstTimeUser);
#endif

  // Added 09/2022.
  profile_prefs->ClearPref(kFirstPartySetsEnabled);

  // Added 10/2022
#if BUILDFLAG(IS_ANDROID)
  feed::MigrateObsoleteProfilePrefsOct_2022(profile_prefs);
#endif  // BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kOriginTrialPrefKey);

  // Once this migration is complete, the tracked preference
  // `kGoogleServicesLastAccountIdDeprecated` can be removed.
  if (profile_prefs->HasPrefPath(
          prefs::kGoogleServicesLastAccountIdDeprecated)) {
    std::string account_id =
        profile_prefs->GetString(prefs::kGoogleServicesLastAccountIdDeprecated);
    profile_prefs->ClearPref(prefs::kGoogleServicesLastAccountIdDeprecated);
    bool is_email = account_id.find('@') != std::string::npos;
    if (!is_email && !account_id.empty()) {
      profile_prefs->SetString(prefs::kGoogleServicesLastGaiaId, account_id);
    }
  }

  // Added 10/2022.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  profile_prefs->ClearPref(kLoadCryptoTokenExtension);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Added 10/2022.
  profile_prefs->ClearPref(kSuggestedContentInfoShownInLauncher);
  profile_prefs->ClearPref(kSuggestedContentInfoDismissedInLauncher);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 11/2022.
  profile_prefs->ClearPref(kAutofillAssistantEnabled);
  profile_prefs->ClearPref(kAutofillAssistantConsent);
  profile_prefs->ClearPref(kAutofillAssistantTriggerScriptsEnabled);
  profile_prefs->ClearPref(kAutofillAssistantTriggerScriptsIsFirstTimeUser);

  // Added 12/2022.
  profile_prefs->ClearPref(kDeprecatedReadingListHasUnseenEntries);

  // Added 12/2022.
  profile_prefs->ClearPref(kAutofillWalletImportStorageCheckboxState);

  // Added 01/2023.
  profile_prefs->ClearPref(kFileSystemSyncAccessHandleAsyncInterfaceEnabled);

  // Added 01/2023.
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kMediaRouterTabMirroringSources);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Added 01/2023
  profile_prefs->ClearPref(kAutofillCreditCardSigninPromoImpressionCount);

  // Added 01/2023
  profile_prefs->ClearPref(kSendDownloadToCloudPref);

  // Added 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kArcTermsShownInOobe);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 02/2023.
  profile_prefs->ClearPref(kSyncInvalidationVersions);
  profile_prefs->ClearPref(kSyncInvalidationVersions2);

  // Added 02/2023.
  profile_prefs->ClearPref(kClearPluginLSODataEnabled);
  profile_prefs->ClearPref(kContentSettingsPluginAllowlist);
  profile_prefs->ClearPref(kPepperFlashSettingsEnabled);
  profile_prefs->ClearPref(kPluginsAllowOutdated);
  profile_prefs->ClearPref(kPluginsLastInternalDirectory);
  profile_prefs->ClearPref(kPluginsPluginsList);
  profile_prefs->ClearPref(kPluginsShowDetails);

// Added 02/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kHasSeenSmartLockSignInRemovedNotification);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ambient::prefs::MigrateDeprecatedPrefs(*profile_prefs);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 03/2023
  profile_prefs->ClearPref(
      kGoogleSearchDomainMixingMetricsEmitterLastMetricsTime);

// Added 03/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kDarkLightModeNudgeLeftToShowCount);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Added 03/2023.
#if BUILDFLAG(IS_WIN)
  profile_prefs->ClearPref(kWebAuthnLastOperationWasNativeAPI);
#endif

// Added 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kBentoBarEnabled);
  profile_prefs->ClearPref(kUserHasUsedDesksRecently);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 04/2023.
#if BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kBackoff);
  profile_prefs->ClearPref(kUserSettingEnabled);
  profile_prefs->ClearPref(kLimitlessPrefetchingEnabledTimePref);
  profile_prefs->ClearPref(kPrefetchTestingHeaderPref);
  profile_prefs->ClearPref(kEnabledByServer);
  profile_prefs->ClearPref(kNextForbiddenCheckTimePref);
  profile_prefs->ClearPref(kPrefetchCachedGCMToken);
#endif

  // Added 04/2023.
  profile_prefs->ClearPref(kTypeSubscribedForInvalidations);
  profile_prefs->ClearPref(kActiveRegistrationToken);
  profile_prefs->ClearPref(kFCMInvalidationClientIDCache);

  // Added 04/2023.
#if BUILDFLAG(IS_ANDROID)
  ntp_snippets::prefs::MigrateObsoleteProfilePrefsApril2023(profile_prefs);
#endif

// Added 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kOfficeFilesAlwaysMove);
  profile_prefs->ClearPref(kOfficeMoveConfirmationShown);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 04/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kProximityAuthIsChromeOSLoginEnabled);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kSmartLockSigninAllowed);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 05/2023.
#if BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kVideoTutorialsPreferredLocaleKey);
  profile_prefs->ClearPref(kVideoTutorialsLastUpdatedTimeKey);
#endif  // BUILDFLAG(IS_ANDROID

// Added 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kOfficeSetupComplete);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kEventRemappedToRightClick);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Added 05/2023.
#if BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kTimesUPMAuthErrorShown);
#endif  // BUILDFLAG(IS_ANDROID)

// Added 05/2023.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_prefs->ClearPref(kSamlPasswordSyncToken);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_PROFILE_PREFS

  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.
}
