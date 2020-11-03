// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <string>

#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/policy/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/chromeos/policy/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/chromeos/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/chromeos/scheduler_configuration_manager.h"
#include "chrome/browser/component_updater/component_updater_prefs.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
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
#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/origin_trial_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/shopping_tasks/shopping_tasks_service.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/storage/appcache_feature_prefs.h"
#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/in_product_help/feature_promo_snooze_service.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/webui/flags_ui.h"
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
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/sync/base/sync_prefs.h"
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
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "extensions/browser/api/audio/audio_api.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/extensions_permissions_tracker.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/chromeos/policy/system_proxy_manager.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/plugins/plugins_resource_service.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/supervised_user_allowlist_service.h"
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
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/video_tutorials/prefs.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/1125897
#include "components/feed/buildflags.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/games/core/games_prefs.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/query_tiles/tile_service_prefs.h"
#include "components/feed/core/common/pref_names.h"
#else   // defined(OS_ANDROID)
#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/metrics/tab_stats_tracker.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/chromeos/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/chromeos/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"
#if defined(USE_CUPS)
#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"
#endif
#include "chrome/browser/chromeos/child_accounts/secondary_account_consent_logger.h"
#include "chrome/browser/chromeos/file_system_provider/registry.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_detector.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/saml/saml_profile_prefs.h"
#include "chrome/browser/chromeos/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/signin_error_notifier_ash.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/chromeos/policy/app_install_event_logger.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log_manager_wrapper.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/chromeos/power/power_metrics_reporter.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/enterprise_printers_provider.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/local_search_service/search_metrics_reporter.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/fast_transition_observer.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_prefs.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/arc/arc_prefs.h"
#include "components/onc/onc_pref_names.h"
#include "components/quirks/quirks_manager.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/browser/web_applications/components/app_shim_registry_mac.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_prefs_manager.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC)
#include "components/os_crypt/os_crypt.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/device_identity//device_oauth2_token_store_desktop.h"
#include "chrome/browser/downgrade/downgrade_prefs.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#endif


namespace {

// Deprecated 9/2019
const char kGoogleServicesUsername[] = "google.services.username";
const char kGoogleServicesUserAccountId[] = "google.services.user_account_id";
const char kDataReductionProxySavingsClearedNegativeSystemClock[] =
    "data_reduction.savings_cleared_negative_system_clock";
const char kDataReductionNetworkProperties[] =
    "data_reduction.network_properties";

#if defined(OS_CHROMEOS)
// Deprecated 10/2019
const char kDisplayRotationAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.display_rotation_accelerator_dialog_has_been_accepted";
#endif  // defined(OS_CHROMEOS)

// Deprecated 11/2019
const char kBlacklistedCredentialsNormalized[] =
    "profile.blacklisted_credentials_normalized";

// Deprecated 1/2020
#if defined(OS_MAC)
const char kKeyCreated[] = "os_crypt.key_created";
#endif  // defined(OS_MAC)

const char kGCMChannelStatus[] = "gcm.channel_status";
const char kGCMChannelPollIntervalSeconds[] = "gcm.poll_interval";
const char kGCMChannelLastCheckTime[] = "gcm.check_time";

// Deprecated 2/2020
const char kInvalidatorClientId[] = "invalidator.client_id";
const char kInvalidatorInvalidationState[] = "invalidator.invalidation_state";
const char kInvalidatorSavedInvalidations[] = "invalidator.saved_invalidations";

#if defined(OS_CHROMEOS)
// Deprecated 4/2020
const char kAmbientModeTopicSource[] = "settings.ambient_mode.topic_source";

// Deprecated 4/2020
const char kPrintingAllowedPageSizes[] = "printing.allowed_page_sizes";
#endif  // defined(OS_CHROMEOS)

// Deprecated 4/2020
const char kExcludedSchemes[] = "protocol_handler.excluded_schemes";

// Deprecated 4/2020
const char kPreviewsLPRHostBlacklist[] = "previews.litepage.host-blacklist";
const char kPreviewsLPRProbeCache[] = "Availability.Prober.cache.Litepages";
const char kPreviewsLPROriginProbeCache[] =
    "Availability.Prober.cache.LitepagesOriginCheck";

#if defined(OS_CHROMEOS)
// Deprecated 4/2020
const char kSupervisedUsersNextId[] = "LocallyManagedUsersNextId";
#endif  // defined(OS_CHROMEOS)

// Deprecated 6/2020
const char kStricterMixedContentTreatmentEnabled[] =
    "security_state.stricter_mixed_content_treatment_enabled";

// Deprecated 7/2020
const char kHashedAvailablePages[] = "previews.offline_helper.available_pages";

// Deprecated 7/2020
const char kObservedSessionTime[] = "profile.observed_session_time";

// Deprecated 9/2020
const char kBlockThirdPartyCookies[] = "profile.block_third_party_cookies";

// Deprecated 9/2020
const char kPluginsDeprecationInfobarLastShown[] =
    "plugins.deprecation_infobar_last_shown";

const char kPasswordManagerOnboardingState[] =
    "profile.password_manager_onboarding_state";

const char kWasOnboardingFeatureCheckedBefore[] =
    "profile.was_pwm_onboarding_feature_checked_before";

// Register local state used only for migration (clearing or moving to a new
// key).
void RegisterLocalStatePrefsForMigration(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kGCMChannelStatus, true);
  registry->RegisterIntegerPref(kGCMChannelPollIntervalSeconds, 0);
  registry->RegisterInt64Pref(kGCMChannelLastCheckTime, 0);
  registry->RegisterListPref(kInvalidatorSavedInvalidations);
  registry->RegisterStringPref(kInvalidatorInvalidationState, std::string());
  registry->RegisterStringPref(kInvalidatorClientId, std::string());

#if defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(kSupervisedUsersNextId, 0);
#endif  // defined(OS_CHROMEOS)
}

// Register prefs used only for migration (clearing or moving to a new key).
void RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kGoogleServicesUsername, std::string());
  registry->RegisterStringPref(kGoogleServicesUserAccountId, std::string());
  registry->RegisterInt64Pref(
      kDataReductionProxySavingsClearedNegativeSystemClock, 0);
  registry->RegisterDictionaryPref(kDataReductionNetworkProperties);

#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(
      kDisplayRotationAcceleratorDialogHasBeenAccepted, false);
#endif  // defined(OS_CHROMEOS)

  registry->RegisterBooleanPref(kBlacklistedCredentialsNormalized, false);

  registry->RegisterBooleanPref(kGCMChannelStatus, true);
  registry->RegisterIntegerPref(kGCMChannelPollIntervalSeconds, 0);
  registry->RegisterInt64Pref(kGCMChannelLastCheckTime, 0);

  registry->RegisterListPref(kInvalidatorSavedInvalidations);
  registry->RegisterStringPref(kInvalidatorInvalidationState, std::string());
  registry->RegisterStringPref(kInvalidatorClientId, std::string());

  chrome_browser_net::secure_dns::RegisterProbesSettingBackupPref(registry);

#if defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(kAmbientModeTopicSource, 0);
  registry->RegisterListPref(kPrintingAllowedPageSizes);
#endif  // defined(OS_CHROMEOS)

  registry->RegisterDictionaryPref(kExcludedSchemes);
  registry->RegisterDictionaryPref(kPreviewsLPRHostBlacklist);
  registry->RegisterDictionaryPref(kPreviewsLPRProbeCache);
  registry->RegisterDictionaryPref(kPreviewsLPROriginProbeCache);

  registry->RegisterBooleanPref(kStricterMixedContentTreatmentEnabled, true);

  registry->RegisterDictionaryPref(kHashedAvailablePages);

  registry->RegisterDictionaryPref(kObservedSessionTime);

  registry->RegisterBooleanPref(kBlockThirdPartyCookies, false);

  registry->RegisterTimePref(kPluginsDeprecationInfobarLastShown, base::Time());

  registry->RegisterIntegerPref(kPasswordManagerOnboardingState, 0);
  registry->RegisterBooleanPref(kWasOnboardingFeatureCheckedBefore, false);
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
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  ChromeTracingDelegate::RegisterPrefs(registry);
  component_updater::RegisterPrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  invalidation::FCMInvalidationService::RegisterPrefs(registry);
  language::GeoLanguageProvider::RegisterLocalStatePrefs(registry);
  language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(registry);
  memory::EnterpriseMemoryLimitPrefObserver::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  OriginTrialPrefs::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileAttributesEntry::RegisterLocalStatePrefs(registry);
  ProfileInfoCache::RegisterPrefs(registry);
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry);
  profiles::RegisterPrefs(registry);
  rappor::RapporServiceImpl::RegisterPrefs(registry);
  RegisterScreenshotPrefs(registry);
  safe_browsing::RegisterLocalStatePrefs(registry);
  secure_origin_allowlist::RegisterPrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(registry);
  syncer::InvalidatorRegistrarWithMemory::RegisterPrefs(registry);
  syncer::PerUserTopicSubscriptionManager::RegisterPrefs(registry);
  SystemNetworkContextManager::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  // Individual preferences. If you have multiple preferences that should
  // clearly be grouped together, please group them together into a helper
  // function called above. Please keep this list alphabetized.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled, false);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kUserAgentClientHintsEnabled, true);
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
#else  // defined(OS_ANDROID)
  enterprise_connectors::RegisterLocalStatePrefs(registry);
  enterprise_reporting::RegisterLocalStatePrefs(registry);
  gcm::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  media_router::RegisterLocalStatePrefs(registry);
  metrics::TabStatsTracker::RegisterPrefs(registry);
  RegisterBrowserPrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  arc::prefs::RegisterLocalStatePrefs(registry);
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  chromeos::ArcKioskAppManager::RegisterPrefs(registry);
  chromeos::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  chromeos::cert_provisioning::RegisterLocalStatePrefs(registry);
  chromeos::ChromeUserManagerImpl::RegisterPrefs(registry);
  chromeos::CupsPrintersManager::RegisterLocalStatePrefs(registry);
  chromeos::DemoModeDetector::RegisterPrefs(registry);
  chromeos::DemoModeResourcesRemover::RegisterLocalStatePrefs(registry);
  chromeos::DemoSession::RegisterLocalStatePrefs(registry);
  chromeos::DemoSetupController::RegisterLocalStatePrefs(registry);
  chromeos::DeviceOAuth2TokenStoreChromeOS::RegisterPrefs(registry);
  chromeos::device_settings_cache::RegisterPrefs(registry);
  chromeos::EasyUnlockService::RegisterPrefs(registry);
  chromeos::echo_offer::RegisterPrefs(registry);
  chromeos::EnableAdbSideloadingScreen::RegisterPrefs(registry);
  chromeos::EnableDebuggingScreenHandler::RegisterPrefs(registry);
  chromeos::ExistingUserController::RegisterLocalStatePrefs(registry);
  chromeos::FastTransitionObserver::RegisterPrefs(registry);
  chromeos::HIDDetectionScreenHandler::RegisterPrefs(registry);
  chromeos::KerberosCredentialsManager::RegisterLocalStatePrefs(registry);
  chromeos::KioskAppManager::RegisterPrefs(registry);
  chromeos::KioskCryptohomeRemover::RegisterPrefs(registry);
  chromeos::language_prefs::RegisterPrefs(registry);
  chromeos::local_search_service::SearchMetricsReporter::
      RegisterLocalStatePrefs(registry);
  chromeos::MultiProfileUserController::RegisterPrefs(registry);
  chromeos::NetworkMetadataStore::RegisterPrefs(registry);
  chromeos::NetworkThrottlingObserver::RegisterPrefs(registry);
  chromeos::PowerMetricsReporter::RegisterLocalStatePrefs(registry);
  chromeos::power::auto_screen_brightness::MetricsReporter::
      RegisterLocalStatePrefs(registry);
  chromeos::Preferences::RegisterPrefs(registry);
  chromeos::ResetScreen::RegisterPrefs(registry);
  chromeos::SchedulerConfigurationManager::RegisterLocalStatePrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterPrefs(registry);
  chromeos::SigninScreenHandler::RegisterPrefs(registry);
  chromeos::StartupUtils::RegisterPrefs(registry);
  chromeos::StatsReportingController::RegisterLocalStatePrefs(registry);
  chromeos::system::AutomaticRebootManager::RegisterPrefs(registry);
  chromeos::TimeZoneResolver::RegisterPrefs(registry);
  chromeos::UserImageManager::RegisterPrefs(registry);
  chromeos::UserSessionManager::RegisterPrefs(registry);
  chromeos::WebKioskAppManager::RegisterPrefs(registry);
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
  policy::BrowserPolicyConnectorChromeOS::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(registry);
  policy::DlpRulesManager::RegisterPrefs(registry);
  policy::DMTokenStorage::RegisterPrefs(registry);
  policy::EnrollmentRequisitionManager::RegisterPrefs(registry);
  policy::MinimumVersionPolicyHandler::RegisterPrefs(registry);
  policy::PolicyCertServiceFactory::RegisterPrefs(registry);
  policy::TPMAutoUpdateModePolicyHandler::RegisterPrefs(registry);
  policy::WebUsbAllowDevicesForUrlsPolicyHandler::RegisterPrefs(registry);
  policy::SystemFeaturesDisableListPolicyHandler::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);
  UpgradeDetectorChromeos::RegisterPrefs(registry);
  RegisterNearbySharingLocalPrefs(registry);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MAC)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
  system_media_permissions::RegisterSystemMediaPermissionStatesPrefs(registry);
  AppShimRegistry::Get()->RegisterLocalPrefs(registry);
  registry->RegisterBooleanPref(kKeyCreated, false);
#endif

#if defined(OS_WIN)
  OSCrypt::RegisterLocalPrefs(registry);
  registry->RegisterBooleanPref(prefs::kRendererCodeIntegrityEnabled, true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kNativeWindowOcclusionEnabled, true);
  component_updater::RegisterPrefsForSwReporter(registry);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  IncompatibleApplicationsUpdater::RegisterLocalStatePrefs(registry);
  ModuleDatabase::RegisterLocalStatePrefs(registry);
  ThirdPartyConflictsManager::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
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
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  HttpsImageCompressionInfoBarDecider::RegisterProfilePrefs(registry);
  image_fetcher::ImageCache::RegisterProfilePrefs(registry);
  ImportantSitesUtil::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  lookalikes::RegisterProfilePrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaEngagementService::RegisterProfilePrefs(registry);
  MediaStorageIdSalt::RegisterProfilePrefs(registry);
  NotificationDisplayServiceImpl::RegisterProfilePrefs(registry);
  NotifierStateTracker::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  password_bubble_experiment::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(registry);
  PlatformNotificationServiceImpl::RegisterProfilePrefs(registry);
  policy::DeveloperToolsPolicyHandler::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  PreviewsHTTPSNotificationInfoBarDecider::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProfileNetworkContextService::RegisterProfilePrefs(registry);
  ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  QuietNotificationPermissionUiState::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
  blocked_content::SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
      registry);
  security_interstitials::InsecureFormBlockingPage::RegisterProfilePrefs(
      registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  SharingSyncPreference::RegisterProfilePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
  syncer::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  omnibox::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionWebUI::RegisterProfilePrefs(registry);
  RegisterAnimationPolicyPrefs(registry);
  ToolbarActionsBar::RegisterProfilePrefs(registry);
  extensions::api::CryptotokenRegisterProfilePrefs(registry);
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::AudioAPI::RegisterUserPrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::ExtensionsUI::RegisterProfilePrefs(registry);
  extensions::NtpOverriddenBubbleDelegate::RegisterPrefs(registry);
  extensions::RuntimeAPI::RegisterPrefs(registry);
  update_client::RegisterProfilePrefs(registry);
  web_app::WebAppProvider::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflineMetricsCollectorImpl::RegisterPrefs(registry);
  offline_pages::prefetch_prefs::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  DeviceIDFetcher::RegisterProfilePrefs(registry);
  PepperFlashSettingsManager::RegisterProfilePrefs(registry);
  PluginInfoHostImpl::RegisterUserPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PolicySettings::RegisterProfilePrefs(registry);
  printing::PrintPreviewStickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  LocalDiscoveryUI::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  ChildAccountService::RegisterProfilePrefs(registry);
  SupervisedUserService::RegisterProfilePrefs(registry);
  SupervisedUserAllowlistService::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  explore_sites::HistoryStatisticsReporter::RegisterPrefs(registry);
  games::prefs::RegisterProfilePrefs(registry);
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
#else   // defined(OS_ANDROID)
  AppShortcutManager::RegisterProfilePrefs(registry);
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  captions::CaptionController::RegisterProfilePrefs(registry);
  ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
  enterprise_reporting::RegisterProfilePrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  FeaturePromoSnoozeService::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  gcm::RegisterProfilePrefs(registry);
  HatsService::RegisterProfilePrefs(registry);
  HistoryUI::RegisterProfilePrefs(registry);
  InstantService::RegisterProfilePrefs(registry);
  media_router::RegisterProfilePrefs(registry);
  NewTabPageHandler::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  PromoService::RegisterProfilePrefs(registry);
  SearchSuggestService::RegisterProfilePrefs(registry);
  settings::SettingsUI::RegisterProfilePrefs(registry);
  send_tab_to_self::SendTabToSelfBubbleController::RegisterProfilePrefs(
      registry);
  ShoppingTasksService::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
  UnifiedAutoplayConfig::RegisterProfilePrefs(registry);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  app_list::AppListSyncableService::RegisterProfilePrefs(registry);
  app_list::ArcAppReinstallSearchProvider::RegisterProfilePrefs(registry);
  arc::prefs::RegisterProfilePrefs(registry);
  ArcAppListPrefs::RegisterProfilePrefs(registry);
  certificate_manager::CertificatesHandler::RegisterProfilePrefs(registry);
  chromeos::AccountManager::RegisterPrefs(registry);
  chromeos::ApkWebAppService::RegisterProfilePrefs(registry);
  chromeos::app_time::AppActivityRegistry::RegisterProfilePrefs(registry);
  chromeos::app_time::AppTimeController::RegisterProfilePrefs(registry);
  chromeos::assistant::prefs::RegisterProfilePrefs(registry);
  chromeos::bluetooth::DebugLogsManager::RegisterPrefs(registry);
  chromeos::ClientAppMetadataProviderService::RegisterProfilePrefs(registry);
  chromeos::CupsPrintersManager::RegisterProfilePrefs(registry);
  chromeos::device_sync::RegisterProfilePrefs(registry);
  chromeos::FamilyUserMetricsService::RegisterProfilePrefs(registry);
  chromeos::FamilyUserSessionMetrics::RegisterProfilePrefs(registry);
  chromeos::first_run::RegisterProfilePrefs(registry);
  chromeos::file_system_provider::RegisterProfilePrefs(registry);
  chromeos::KerberosCredentialsManager::RegisterProfilePrefs(registry);
  chromeos::multidevice_setup::MultiDeviceSetupService::RegisterProfilePrefs(
      registry);
  chromeos::MultiProfileUserController::RegisterProfilePrefs(registry);
  chromeos::NetworkMetadataStore::RegisterPrefs(registry);
  chromeos::ReleaseNotesStorage::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::FingerprintStorage::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::PinStoragePrefs::RegisterProfilePrefs(registry);
  chromeos::Preferences::RegisterProfilePrefs(registry);
  chromeos::printing::print_management::PrintingManagerFactory ::
      RegisterProfilePrefs(registry);
  chromeos::PrintJobHistoryService::RegisterProfilePrefs(registry);
  chromeos::EnterprisePrintersProvider::RegisterProfilePrefs(registry);
  chromeos::parent_access::ParentAccessService::RegisterProfilePrefs(registry);
  chromeos::quick_answers::prefs::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::RegisterProfilePrefs(registry);
  chromeos::RegisterSamlProfilePrefs(registry);
  chromeos::ScreenTimeController::RegisterProfilePrefs(registry);
  SecondaryAccountConsentLogger::RegisterPrefs(registry);
  SigninErrorNotifier::RegisterPrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  chromeos::settings::OSSettingsUI::RegisterProfilePrefs(registry);
  chromeos::UserImageSyncObserver::RegisterProfilePrefs(registry);
  crostini::prefs::RegisterProfilePrefs(registry);
  chromeos::attestation::TpmChallengeKey::RegisterProfilePrefs(registry);
  extensions::EPKPChallengeKey::RegisterProfilePrefs(registry);
#if defined(USE_CUPS)
  extensions::PrintingAPIHandler::RegisterProfilePrefs(registry);
#endif
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
  guest_os::prefs::RegisterProfilePrefs(registry);
  lock_screen_apps::StateController::RegisterProfilePrefs(registry);
  plugin_vm::prefs::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogger::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogManagerWrapper::RegisterProfilePrefs(registry);
  policy::ExtensionInstallEventLogManagerWrapper::RegisterProfilePrefs(
      registry);
  policy::StatusCollector::RegisterProfilePrefs(registry);
  policy::SystemProxyManager::RegisterProfilePrefs(registry);
  RegisterChromeLauncherUserPrefs(registry);
  ::onc::RegisterProfilePrefs(registry);
  chromeos::cert_provisioning::RegisterProfilePrefs(registry);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
  component_updater::RegisterProfilePrefsForSwReporter(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
  safe_browsing::SettingsResetPromptPrefsManager::RegisterProfilePrefs(
      registry);
  safe_browsing::PostCleanupSettingsResetter::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  browser_switcher::BrowserSwitcherPrefs::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  default_apps::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  accessibility_prefs::RegisterInvertBubbleUserPrefs(registry);
  RegisterBrowserViewProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID)
  media_feeds::MediaFeedsService::RegisterProfilePrefs(registry);
  kaleidoscope::prefs::RegisterProfilePrefs(registry);
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
#if defined(OS_CHROMEOS)
  ash::RegisterUserProfilePrefs(registry);
#endif
}

void RegisterScreenshotPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

#if defined(OS_CHROMEOS)
void RegisterSigninProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterProfilePrefs(registry, g_browser_process->GetApplicationLocale());
  ash::RegisterSigninProfilePrefs(registry);
}

#endif

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteLocalStatePrefs(PrefService* local_state) {
  // Added 1/2020
#if defined(OS_MAC)
  local_state->ClearPref(kKeyCreated);
#endif  // defined(OS_MAC)
  local_state->ClearPref(kGCMChannelStatus);
  local_state->ClearPref(kGCMChannelPollIntervalSeconds);
  local_state->ClearPref(kGCMChannelLastCheckTime);

  // Added 2/2020.
  local_state->ClearPref(kInvalidatorSavedInvalidations);
  local_state->ClearPref(kInvalidatorInvalidationState);
  local_state->ClearPref(kInvalidatorClientId);

#if defined(OS_CHROMEOS)
  // Added 4/2020.
  local_state->ClearPref(kSupervisedUsersNextId);
#endif  // defined(OS_CHROMEOS)
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteProfilePrefs(Profile* profile) {
  PrefService* profile_prefs = profile->GetPrefs();

  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(profile_prefs);

  // Added 7/2019. Keep at least until 7/2021 as a missing migration would
  // disable sync.
  syncer::MigrateSyncSuppressedPref(profile_prefs);

  // Added 9/2019
  profile_prefs->ClearPref(kGoogleServicesUsername);
  profile_prefs->ClearPref(kGoogleServicesUserAccountId);
  profile_prefs->ClearPref(
      kDataReductionProxySavingsClearedNegativeSystemClock);

  // Added 10/2019.
  syncer::DeviceInfoPrefs::MigrateRecentLocalCacheGuidsPref(profile_prefs);
#if defined(OS_CHROMEOS)
  // Added 10/2019.
  profile_prefs->ClearPref(kDisplayRotationAcceleratorDialogHasBeenAccepted);
#endif  // defined(OS_CHROMEOS)

  // Added 11/2019.
  profile_prefs->ClearPref(kBlacklistedCredentialsNormalized);

  // Added 1/2020.
  profile_prefs->ClearPref(kGCMChannelStatus);
  profile_prefs->ClearPref(kGCMChannelPollIntervalSeconds);
  profile_prefs->ClearPref(kGCMChannelLastCheckTime);

  // Added 2/2020.
  profile_prefs->ClearPref(kInvalidatorSavedInvalidations);
  profile_prefs->ClearPref(kInvalidatorInvalidationState);
  profile_prefs->ClearPref(kInvalidatorClientId);

  // Added 3/2020.
  profile_prefs->ClearPref(kDataReductionNetworkProperties);
  chrome_browser_net::secure_dns::MigrateProbesSettingToOrFromBackup(
      profile_prefs);

#if defined(OS_CHROMEOS)
  // Added 4/2020.
  profile_prefs->ClearPref(kAmbientModeTopicSource);

  // Added 4/2020.
  profile_prefs->ClearPref(kPrintingAllowedPageSizes);
#endif

  // Added 4/2020
  profile_prefs->ClearPref(kExcludedSchemes);

  // Added 4/2020.
  profile_prefs->ClearPref(kPreviewsLPRHostBlacklist);
  profile_prefs->ClearPref(kPreviewsLPRProbeCache);
  profile_prefs->ClearPref(kPreviewsLPROriginProbeCache);

  // Added 6/2020
  profile_prefs->ClearPref(kStricterMixedContentTreatmentEnabled);

  // Added 7/2020.
  profile_prefs->ClearPref(kHashedAvailablePages);

  // Added 7/2020
  profile_prefs->ClearPref(kObservedSessionTime);

  // Added 9/2020
  profile_prefs->ClearPref(kBlockThirdPartyCookies);

  // Added 9/2020
  profile_prefs->ClearPref(kPluginsDeprecationInfobarLastShown);
  profile_prefs->ClearPref(kPasswordManagerOnboardingState);
  profile_prefs->ClearPref(kWasOnboardingFeatureCheckedBefore);
}
