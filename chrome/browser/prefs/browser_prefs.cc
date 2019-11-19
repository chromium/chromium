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
#include "chrome/browser/chromeos/policy/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/chromeos/scheduler_configuration_manager.h"
#include "chrome/browser/component_updater/component_updater_prefs.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/pepper_flash_settings_manager.h"
#include "chrome/browser/permissions/adaptive_notification_permission_ui_selector.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/origin_trial_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_offline_helper.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/navigation_correction_tab_observer.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/webui/flags_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/web_components_prefs.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/feature_engagement/buildflags.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/request_throttler.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url_prepopulate_data.h"
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
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#else
#include "chrome/browser/extensions/api/enterprise_reporting_private/prefs.h"
#endif
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
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"
#endif

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/preferences/browser_prefs_android.h"
#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/feed/buildflags.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
#include "components/feed/core/pref_names.h"
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#else   // defined(OS_ANDROID)
#include "chrome/browser/enterprise_reporting/prefs.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/metrics/tab_stats_tracker.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/webui/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
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
#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/extensions/echo_private_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"
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
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/net/network_throttling_observer.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/chromeos/policy/app_install_event_logger.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/chromeos/power/power_metrics_reporter.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_settings_cache.h"
#include "chrome/browser/chromeos/system/automatic_reboot_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
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
#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/fast_transition_observer.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/device_sync/device_sync_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/arc/arc_prefs.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/onc/onc_pref_names.h"
#include "components/quirks/quirks_manager.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/apps/platform_apps/app_shim_registry_mac.h"
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
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

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "components/os_crypt/os_crypt.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/media/unified_autoplay_config.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"
#endif

#if defined(OS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/games/core/games_prefs.h"
#endif

namespace {

// Deprecated 8/2018.
const char kDnsPrefetchingStartupList[] = "dns_prefetching.startup_list";
const char kDnsPrefetchingHostReferralList[] =
    "dns_prefetching.host_referral_list";

// Deprecated 9/2018
const char kGeolocationAccessToken[] = "geolocation.access_token";
const char kGoogleServicesPasswordHash[] = "google.services.password_hash";
const char kModuleConflictBubbleShown[] = "module_conflict.bubble_shown";
const char kOptionsWindowLastTabIndex[] = "options_window.last_tab_index";
const char kTrustedDownloadSources[] = "trusted_download_sources";
#if defined(OS_WIN)
const char kLastWelcomedOSVersion[] = "browser.last_welcomed_os_version";
#endif
const char kSupervisedUserCreationAllowed[] =
    "profile.managed_user_creation_allowed";

// Deprecated 10/2018
const char kReverseAutologinEnabled[] = "reverse_autologin.enabled";

// Deprecated 11/2018.
const char kNetworkQualities[] = "net.network_qualities";
const char kForceSessionSync[] = "settings.history_recorded";
const char kOnboardDuringNUX[] = "browser.onboard_during_nux";
const char kNuxOnboardGroup[] = "browser.onboard_group";
// This pref is particularly large, taking up 15+% of the prefs file, so should
// perhaps be kept around longer than the others.
const char kHttpServerProperties[] = "net.http_server_properties";

// Deprecated 1/2019.
const char kNextUpdateCheck[] = "extensions.autoupdate.next_check";
const char kLastUpdateCheck[] = "extensions.autoupdate.last_check";

// Deprecated 3/2019.
const char kCurrentThemeImages[] = "extensions.theme.images";
const char kCurrentThemeColors[] = "extensions.theme.colors";
const char kCurrentThemeTints[] = "extensions.theme.tints";
const char kCurrentThemeDisplayProperties[] = "extensions.theme.properties";

#if defined(OS_ANDROID)
// Deprecated 4/2019.
const char kDismissedAssetDownloadSuggestions[] =
    "ntp_suggestions.downloads.assets.dismissed_ids";
const char kDismissedOfflinePageDownloadSuggestions[] =
    "ntp_suggestions.downloads.offline_pages.dismissed_ids";

// Deprecated 4/2019.
const char kBreakingNewsSubscriptionDataToken[] =
    "ntp_suggestions.breaking_news_subscription_data.token";
const char kBreakingNewsSubscriptionDataIsAuthenticated[] =
    "ntp_suggestions.breaking_news_subscription_data.is_authenticated";
const char kBreakingNewsGCMSubscriptionTokenCache[] =
    "ntp_suggestions.breaking_news_gcm_subscription_token_cache";
const char kBreakingNewsGCMLastTokenValidationTime[] =
    "ntp_suggestions.breaking_news_gcm_last_token_validation_time";
const char kBreakingNewsGCMLastForcedSubscriptionTime[] =
    "ntp_suggestions.breaking_news_gcm_last_forced_subscription_time";

// Deprecated 4/2019.
const char kContentSuggestionsConsecutiveIgnoredPrefName[] =
    "ntp.content_suggestions.notifications.consecutive_ignored";
const char kContentSuggestionsNotificationsSentDay[] =
    "ntp.content_suggestions.notifications.sent_day";
const char kContentSuggestionsNotificationsSentCount[] =
    "ntp.content_suggestions.notifications.sent_count";
const char kNotificationIDWithinCategory[] =
    "ContentSuggestionsNotificationIDWithinCategory";

// Deprecated 5/2019.
const char kContentSuggestionsNotificationsEnabled[] =
    "ntp.content_suggestions.notifications.enabled";

#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
// Deprecated 5/2019
const char kSignInPromoShowOnFirstRunAllowed[] =
    "sync_promo.show_on_first_run_allowed";
const char kSignInPromoShowNTPBubble[] = "sync_promo.show_ntp_bubble";
#endif  // !defined(OS_ANDROID)

// Deprecated 5/2019
const char kBookmarkAppCreationLaunchType[] =
    "extensions.bookmark_app_creation_launch_type";

// Deprecated 6/2019
const char kMediaCacheSize[] = "browser.media_cache_size";

#if defined(OS_WIN)
// Deprecated 6/2019
const char kHasSeenWin10PromoPage[] = "browser.has_seen_win10_promo_page";
#endif  // defined(OS_WIN)

// Deprecated 7/2019
const char kSignedInTime[] = "signin.signedin_time";

#if !defined(OS_ANDROID)
// Deprecated 7/2019
const char kNtpActivateHideShortcutsFieldTrial[] =
    "ntp.activate_hide_shortcuts_field_trial";
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Deprecated 7/2019
// WebAuthn prefs were being erroneously stored on Android. They are registered
// on other platforms.
const char kWebAuthnLastTransportUsedPrefName[] =
    "webauthn.last_transport_used";
const char kWebAuthnBlePairedMacAddressesPrefName[] =
    "webauthn.ble.paired_mac_addresses";
#endif  // defined(OS_ANDROID)

// Deprecated 7/2019
const char kLastKnownGoogleURL[] = "browser.last_known_google_url";
const char kLastPromptedGoogleURL[] = "browser.last_prompted_google_url";
#if defined(USE_X11)
constexpr char kLocalProfileId[] = "profile.local_profile_id";
#endif

// Deprecated 8/2019
const char kInsecureExtensionUpdatesEnabled[] =
    "extension_updates.insecure_extension_updates_enabled";

const char kLastStartupTimestamp[] = "startup_metric.last_startup_timestamp";
const char kLastStartupVersion[] = "startup_metric.last_startup_version";
const char kSameVersionStartupCount[] =
    "startup_metric.same_version_startup_count";

// Deprecated 8/2019
const char kHintLoadedCounts[] = "optimization_guide.hint_loaded_counts";

// Deprecated 9/2019
const char kGoogleServicesUsername[] = "google.services.username";
const char kGoogleServicesUserAccountId[] = "google.services.user_account_id";
const char kDataReductionProxySavingsClearedNegativeSystemClock[] =
    "data_reduction.savings_cleared_negative_system_clock";

#if defined(OS_CHROMEOS)
// Deprecated 10/2019
const char kDisplayRotationAcceleratorDialogHasBeenAccepted[] =
    "settings.a11y.display_rotation_accelerator_dialog_has_been_accepted";
#endif  // defined(OS_CHROMEOS)

// Register prefs used only for migration (clearing or moving to a new key).
void RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kDnsPrefetchingStartupList);
  registry->RegisterListPref(kDnsPrefetchingHostReferralList);

  registry->RegisterStringPref(kGeolocationAccessToken, std::string());
  registry->RegisterStringPref(kGoogleServicesPasswordHash, std::string());
  registry->RegisterIntegerPref(kModuleConflictBubbleShown, 0);
  registry->RegisterIntegerPref(kOptionsWindowLastTabIndex, 0);
  registry->RegisterStringPref(kTrustedDownloadSources, std::string());
  registry->RegisterBooleanPref(kSupervisedUserCreationAllowed, true);

  registry->RegisterBooleanPref(kReverseAutologinEnabled, true);

  registry->RegisterDictionaryPref(kNetworkQualities, PrefRegistry::LOSSY_PREF);
  registry->RegisterBooleanPref(kForceSessionSync, false);
  registry->RegisterBooleanPref(kOnboardDuringNUX, false);
  registry->RegisterIntegerPref(kNuxOnboardGroup, 0);
  registry->RegisterDictionaryPref(kHttpServerProperties,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(kLastUpdateCheck, 0);
  registry->RegisterIntegerPref(kNextUpdateCheck, 0);

  registry->RegisterDictionaryPref(kCurrentThemeImages);
  registry->RegisterDictionaryPref(kCurrentThemeColors);
  registry->RegisterDictionaryPref(kCurrentThemeTints);
  registry->RegisterDictionaryPref(kCurrentThemeDisplayProperties);

#if defined(OS_ANDROID)
  registry->RegisterListPref(kDismissedAssetDownloadSuggestions);
  registry->RegisterListPref(kDismissedOfflinePageDownloadSuggestions);

  registry->RegisterStringPref(kBreakingNewsSubscriptionDataToken,
                               std::string());
  registry->RegisterBooleanPref(kBreakingNewsSubscriptionDataIsAuthenticated,
                                false);
  registry->RegisterStringPref(kBreakingNewsGCMSubscriptionTokenCache,
                               std::string());
  registry->RegisterInt64Pref(kBreakingNewsGCMLastTokenValidationTime, 0);
  registry->RegisterInt64Pref(kBreakingNewsGCMLastForcedSubscriptionTime, 0);

  registry->RegisterIntegerPref(kContentSuggestionsConsecutiveIgnoredPrefName,
                                0);
  registry->RegisterIntegerPref(kContentSuggestionsNotificationsSentDay, 0);
  registry->RegisterIntegerPref(kContentSuggestionsNotificationsSentCount, 0);
  registry->RegisterStringPref(kNotificationIDWithinCategory, std::string());
  registry->RegisterBooleanPref(kContentSuggestionsNotificationsEnabled, true);
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(kSignInPromoShowOnFirstRunAllowed, true);
  registry->RegisterBooleanPref(kSignInPromoShowNTPBubble, false);
#endif  // !defined(OS_ANDROID)

  registry->RegisterIntegerPref(kBookmarkAppCreationLaunchType, 0);

  registry->RegisterIntegerPref(kMediaCacheSize, 0);

  registry->RegisterInt64Pref(kSignedInTime, 0);

#if defined(OS_ANDROID)
  registry->RegisterStringPref(kWebAuthnLastTransportUsedPrefName,
                               std::string());
  registry->RegisterListPref(kWebAuthnBlePairedMacAddressesPrefName);
#endif  // defined(OS_ANDROID)

  registry->RegisterStringPref(kLastKnownGoogleURL, std::string());
  registry->RegisterStringPref(kLastPromptedGoogleURL, std::string());
#if defined(USE_X11)
  registry->RegisterIntegerPref(kLocalProfileId, 0);
#endif

  registry->RegisterBooleanPref(kInsecureExtensionUpdatesEnabled, false);

  registry->RegisterDictionaryPref(kHintLoadedCounts);
  registry->RegisterStringPref(kGoogleServicesUsername, std::string());
  registry->RegisterStringPref(kGoogleServicesUserAccountId, std::string());
  registry->RegisterInt64Pref(
      kDataReductionProxySavingsClearedNegativeSystemClock, 0);

#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(
      kDisplayRotationAcceleratorDialogHasBeenAccepted, false);
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Please keep this list alphabetized.
  browser_shutdown::RegisterPrefs(registry);
  data_reduction_proxy::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterLocalStatePrefs(registry);
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  ChromeTracingDelegate::RegisterPrefs(registry);
  component_updater::RegisterPrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  language::GeoLanguageProvider::RegisterLocalStatePrefs(registry);
  language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(registry);
  memory::EnterpriseMemoryLimitPrefObserver::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  OriginTrialPrefs::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileAttributesEntry::RegisterLocalStatePrefs(registry);
  ProfileInfoCache::RegisterPrefs(registry);
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry);
  profiles::RegisterPrefs(registry);
  rappor::RapporServiceImpl::RegisterPrefs(registry);
  RegisterScreenshotPrefs(registry);
  safe_browsing::RegisterLocalStatePrefs(registry);
  secure_origin_whitelist::RegisterPrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(registry);
  SystemNetworkContextManager::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) and defined(OS_CHROMEOS)
  chromeos::EasyUnlockService::RegisterPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginsResourceService::RegisterPrefs(registry);
#endif

#if defined(OS_ANDROID)
  ::android::RegisterPrefs(registry);
#else
  media_router::RegisterLocalStatePrefs(registry);
  // The native GCM is used on Android instead.
  gcm::GCMChannelStatusSyncer::RegisterPrefs(registry);
  gcm::RegisterPrefs(registry);
  metrics::TabStatsTracker::RegisterPrefs(registry);
  RegisterBrowserPrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  enterprise_reporting::RegisterLocalStatePrefs(registry);
#if !defined(OS_CHROMEOS)
  RegisterDefaultBrowserPromptPrefs(registry);
#endif  // !defined(OS_CHROMEOS)
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  arc::prefs::RegisterLocalStatePrefs(registry);
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  chromeos::ArcKioskAppManager::RegisterPrefs(registry);
  chromeos::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  chromeos::ChromeUserManagerImpl::RegisterPrefs(registry);
  chromeos::DemoModeDetector::RegisterPrefs(registry);
  chromeos::DemoModeResourcesRemover::RegisterLocalStatePrefs(registry);
  chromeos::DemoSession::RegisterLocalStatePrefs(registry);
  chromeos::DemoSetupController::RegisterLocalStatePrefs(registry);
  chromeos::DeviceOAuth2TokenService::RegisterPrefs(registry);
  chromeos::device_settings_cache::RegisterPrefs(registry);
  chromeos::echo_offer::RegisterPrefs(registry);
  chromeos::EnableAdbSideloadingScreen::RegisterPrefs(registry);
  chromeos::EnableDebuggingScreenHandler::RegisterPrefs(registry);
  chromeos::FastTransitionObserver::RegisterPrefs(registry);
  chromeos::HIDDetectionScreenHandler::RegisterPrefs(registry);
  chromeos::KerberosCredentialsManager::RegisterLocalStatePrefs(registry);
  chromeos::KioskAppManager::RegisterPrefs(registry);
  chromeos::KioskCryptohomeRemover::RegisterPrefs(registry);
  chromeos::language_prefs::RegisterPrefs(registry);
  chromeos::MultiProfileUserController::RegisterPrefs(registry);
  chromeos::NetworkThrottlingObserver::RegisterPrefs(registry);
  chromeos::PowerMetricsReporter::RegisterLocalStatePrefs(registry);
  chromeos::power::auto_screen_brightness::MetricsReporter::
      RegisterLocalStatePrefs(registry);
  chromeos::Preferences::RegisterPrefs(registry);
  chromeos::ResetScreen::RegisterPrefs(registry);
  chromeos::ResourceReporter::RegisterPrefs(registry);
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
  extensions::lock_screen_data::LockScreenItemStorage::RegisterLocalState(
      registry);
  extensions::login_api::RegisterLocalStatePrefs(registry);
  invalidation::FCMInvalidationService::RegisterPrefs(registry);
  invalidation::InvalidatorStorage::RegisterPrefs(registry);
  ::onc::RegisterPrefs(registry);
  policy::AutoEnrollmentClientImpl::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorChromeOS::RegisterPrefs(registry);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(registry);
  policy::DMTokenStorage::RegisterPrefs(registry);
  policy::PolicyCertServiceFactory::RegisterPrefs(registry);
  policy::TPMAutoUpdateModePolicyHandler::RegisterPrefs(registry);
  policy::WebUsbAllowDevicesForUrlsPolicyHandler::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);
  UpgradeDetectorChromeos::RegisterPrefs(registry);
  syncer::PerUserTopicRegistrationManager::RegisterPrefs(registry);
  syncer::InvalidatorRegistrarWithMemory::RegisterPrefs(registry);
#endif

#if defined(OS_MACOSX)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
  system_media_permissions::RegisterSystemMediaPermissionStatesPrefs(registry);
  AppShimRegistry::Get()->RegisterLocalPrefs(registry);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
  OSCrypt::RegisterLocalPrefs(registry);
#endif

#if defined(OS_WIN)
  registry->RegisterBooleanPref(prefs::kRendererCodeIntegrityEnabled, true);
  component_updater::RegisterPrefsForSwReporter(registry);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  IncompatibleApplicationsUpdater::RegisterLocalStatePrefs(registry);
  ModuleDatabase::RegisterLocalStatePrefs(registry);
  ThirdPartyConflictsManager::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  registry->RegisterBooleanPref(kHasSeenWin10PromoPage, false);  // DEPRECATED
  registry->RegisterStringPref(kLastWelcomedOSVersion, std::string());
#endif  // defined(OS_WIN)

  // Obsolete. See MigrateObsoleteBrowserPrefs().
  registry->RegisterIntegerPref(metrics::prefs::kStabilityExecutionPhase, 0);
#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(kNtpActivateHideShortcutsFieldTrial, false);
#endif  // !defined(OS_ANDROID)
  registry->RegisterInt64Pref(kLastStartupTimestamp, 0);
  registry->RegisterStringPref(kLastStartupVersion, std::string());
  registry->RegisterIntegerPref(kSameVersionStartupCount, 0);

#if defined(TOOLKIT_VIEWS)
  RegisterBrowserViewLocalPrefs(registry);
#endif
}

// Register prefs applicable to all profiles.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                          const std::string& locale) {
  TRACE_EVENT0("browser", "chrome::RegisterProfilePrefs");
  // User prefs. Please keep this list alphabetized.
  AccessibilityLabelsService::RegisterProfilePrefs(registry);
  AccessibilityUIMessageHandler::RegisterProfilePrefs(registry);
  AdaptiveNotificationPermissionUiSelector::RegisterProfilePrefs(registry);
  AvailabilityProber::RegisterProfilePrefs(registry);
  autofill::prefs::RegisterProfilePrefs(registry);
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);
  certificate_transparency::prefs::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  ChromeSSLHostStateDelegate::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::NetErrorTabHelper::RegisterProfilePrefs(registry);
  chrome_browser_net::RegisterPredictionOptionsProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  DocumentProvider::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  image_fetcher::ImageCache::RegisterProfilePrefs(registry);
  ImportantSitesUtil::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaEngagementService::RegisterProfilePrefs(registry);
  MediaStorageIdSalt::RegisterProfilePrefs(registry);
  MediaStreamDevicesController::RegisterProfilePrefs(registry);
  NavigationCorrectionTabObserver::RegisterProfilePrefs(registry);
  NotifierStateTracker::RegisterProfilePrefs(registry);
  ntp_snippets::ContentSuggestionsService::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsProviderImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(registry);
  ntp_snippets::RequestThrottler::RegisterProfilePrefs(registry);
  ntp_snippets::UserClassifier::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  password_bubble_experiment::RegisterPrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  PlatformNotificationServiceImpl::RegisterProfilePrefs(registry);
  policy::DeveloperToolsPolicyHandler::RegisterProfilePrefs(registry);
  policy::URLBlacklistManager::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  PreviewsLitePageRedirectDecider::RegisterProfilePrefs(registry);
  PreviewsOfflineHelper::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProfileNetworkContextService::RegisterProfilePrefs(registry);
  ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  safe_browsing::RegisterProfilePrefs(registry);
  SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  SharingSyncPreference::RegisterProfilePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::PerUserTopicRegistrationManager::RegisterProfilePrefs(registry);
  syncer::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  web_components_prefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
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

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
  feature_engagement::SessionDurationUpdater::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginInfoHostImpl::RegisterUserPrefs(registry);
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PolicySettings::RegisterProfilePrefs(registry);
  printing::StickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  LocalDiscoveryUI::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  ChildAccountService::RegisterProfilePrefs(registry);
  SupervisedUserService::RegisterProfilePrefs(registry);
  SupervisedUserWhitelistService::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  GeolocationPermissionContextAndroid::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  RecentTabsPagePrefs::RegisterProfilePrefs(registry);
  usage_stats::UsageStatsBridge::RegisterProfilePrefs(registry);
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
  feed::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#else
  AppShortcutManager::RegisterProfilePrefs(registry);
  DeviceIDFetcher::RegisterProfilePrefs(registry);
  DevToolsWindow::RegisterProfilePrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  NewTabUI::RegisterProfilePrefs(registry);
  PepperFlashSettingsManager::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
#endif

#if defined(OS_ANDROID)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  MediaDrmOriginIdManager::RegisterProfilePrefs(registry);
  explore_sites::HistoryStatisticsReporter::RegisterPrefs(registry);
  ntp_snippets::ClickBasedCategoryRanker::RegisterProfilePrefs(registry);
  OomInterventionDecider::RegisterProfilePrefs(registry);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  games::prefs::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  HatsService::RegisterProfilePrefs(registry);
  InstantService::RegisterProfilePrefs(registry);
  PromoService::RegisterProfilePrefs(registry);
  SearchSuggestService::RegisterProfilePrefs(registry);
  gcm::GCMChannelStatusSyncer::RegisterProfilePrefs(registry);
  gcm::RegisterProfilePrefs(registry);
  media_router::RegisterProfilePrefs(registry);
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  default_apps::RegisterProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS)
  app_list::AppListSyncableService::RegisterProfilePrefs(registry);
  app_list::ArcAppReinstallSearchProvider::RegisterProfilePrefs(registry);
  arc::prefs::RegisterProfilePrefs(registry);
  ArcAppListPrefs::RegisterProfilePrefs(registry);
  certificate_manager::CertificatesHandler::RegisterProfilePrefs(registry);
  chromeos::AccountManager::RegisterPrefs(registry);
  chromeos::ApkWebAppService::RegisterProfilePrefs(registry);
  chromeos::assistant::prefs::RegisterProfilePrefs(registry);
  chromeos::bluetooth::DebugLogsManager::RegisterPrefs(registry);
  chromeos::CupsPrintersManager::RegisterProfilePrefs(registry);
  chromeos::device_sync::DeviceSyncImpl::RegisterProfilePrefs(registry);
  chromeos::first_run::RegisterProfilePrefs(registry);
  chromeos::file_system_provider::RegisterProfilePrefs(registry);
  chromeos::KerberosCredentialsManager::RegisterProfilePrefs(registry);
  chromeos::KeyPermissions::RegisterProfilePrefs(registry);
  chromeos::multidevice_setup::MultiDeviceSetupService::RegisterProfilePrefs(
      registry);
  chromeos::MultiProfileUserController::RegisterProfilePrefs(registry);
  chromeos::ReleaseNotesStorage::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::FingerprintStorage::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::PinStoragePrefs::RegisterProfilePrefs(registry);
  chromeos::Preferences::RegisterProfilePrefs(registry);
  chromeos::PrintJobHistoryService::RegisterProfilePrefs(registry);
  chromeos::SyncedPrintersManager::RegisterProfilePrefs(registry);
  chromeos::parent_access::ParentAccessService::RegisterProfilePrefs(registry);
  chromeos::quick_unlock::RegisterProfilePrefs(registry);
  chromeos::RegisterSamlProfilePrefs(registry);
  chromeos::ScreenTimeController::RegisterProfilePrefs(registry);
  chromeos::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  chromeos::UserImageSyncObserver::RegisterProfilePrefs(registry);
  crostini::prefs::RegisterProfilePrefs(registry);
  chromeos::attestation::TpmChallengeKey::RegisterProfilePrefs(registry);
  extensions::EPKPChallengeKey::RegisterProfilePrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
  guest_os::prefs::RegisterProfilePrefs(registry);
  lock_screen_apps::StateController::RegisterProfilePrefs(registry);
  plugin_vm::prefs::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogger::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogManagerWrapper::RegisterProfilePrefs(registry);
  policy::StatusCollector::RegisterProfilePrefs(registry);
  ::onc::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN)
  component_updater::RegisterProfilePrefsForSwReporter(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
  safe_browsing::SettingsResetPromptPrefsManager::RegisterProfilePrefs(
      registry);
  safe_browsing::PostCleanupSettingsResetter::RegisterProfilePrefs(registry);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  browser_switcher::BrowserSwitcherPrefs::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  accessibility_prefs::RegisterInvertBubbleUserPrefs(registry);
  RegisterBrowserViewProfilePrefs(registry);
#endif

#if defined(OS_CHROMEOS)
  RegisterChromeLauncherUserPrefs(registry);
#endif

#if !defined(OS_ANDROID)
  HistoryUI::RegisterProfilePrefs(registry);
  settings::SettingsUI::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflineMetricsCollectorImpl::RegisterPrefs(registry);
  offline_pages::prefetch_prefs::RegisterPrefs(registry);
#endif

#if defined(OS_ANDROID)
  NotificationChannelsProviderAndroid::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID)
  UnifiedAutoplayConfig::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::enterprise_reporting::RegisterProfilePrefs(registry);
#endif

#if !defined(OS_ANDROID)
  enterprise_reporting::RegisterProfilePrefs(registry);
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
void MigrateObsoleteBrowserPrefs(Profile* profile, PrefService* local_state) {
  // Added 12/2018.
  local_state->ClearPref(metrics::prefs::kStabilityExecutionPhase);

#if defined(OS_ANDROID)
  // Added 9/2018
  local_state->ClearPref(
      metrics::prefs::kStabilityCrashCountWithoutGmsCoreUpdateObsolete);
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  // Added 9/2018
  local_state->ClearPref(kLastWelcomedOSVersion);
#endif
#if defined(OS_CHROMEOS)
  // Added 12/2018
  local_state->ClearPref(prefs::kCarrierDealPromoShown);
#endif

#if defined(OS_WIN)
  // Added 6/2019.
  local_state->ClearPref(kHasSeenWin10PromoPage);
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
  // Added 7/2019.
  local_state->ClearPref(kNtpActivateHideShortcutsFieldTrial);
#endif  // !defined(OS_ANDROID)

  // Added 8/2019.
  local_state->ClearPref(kLastStartupTimestamp);
  local_state->ClearPref(kLastStartupVersion);
  local_state->ClearPref(kSameVersionStartupCount);
}

// This method should be periodically pruned of year+ old migrations.
void MigrateObsoleteProfilePrefs(Profile* profile) {
  PrefService* profile_prefs = profile->GetPrefs();

  // Added 8/2018.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(profile_prefs);

  // Added 8/2018
  profile_prefs->ClearPref(kDnsPrefetchingStartupList);
  profile_prefs->ClearPref(kDnsPrefetchingHostReferralList);

  // Added 9/2018
  profile_prefs->ClearPref(kGeolocationAccessToken);
  profile_prefs->ClearPref(kGoogleServicesPasswordHash);
  profile_prefs->ClearPref(kModuleConflictBubbleShown);
  profile_prefs->ClearPref(kOptionsWindowLastTabIndex);
  profile_prefs->ClearPref(kTrustedDownloadSources);
  profile_prefs->ClearPref(kSupervisedUserCreationAllowed);

  // Added 10/2018
  profile_prefs->ClearPref(kReverseAutologinEnabled);

  // Added 11/2018.
  profile_prefs->ClearPref(kNetworkQualities);
  profile_prefs->ClearPref(kForceSessionSync);
  profile_prefs->ClearPref(kOnboardDuringNUX);
  profile_prefs->ClearPref(kNuxOnboardGroup);
  profile_prefs->ClearPref(kHttpServerProperties);

#if defined(OS_CHROMEOS)
  // Added 12/2018.
  profile_prefs->ClearPref(prefs::kDataSaverPromptsShown);

  // Added 4/2019
  guest_os::GuestOsSharePath::MigratePersistedPathsToMultiVM(profile_prefs);
#endif

  // Added 1/2019.
  profile_prefs->ClearPref(kLastUpdateCheck);
  profile_prefs->ClearPref(kNextUpdateCheck);

  syncer::MigrateSessionsToProxyTabsPrefs(profile_prefs);
  syncer::ClearObsoleteUserTypePrefs(profile_prefs);

  // Added 2/2019.
  syncer::ClearObsoleteClearServerDataPrefs(profile_prefs);
  syncer::ClearObsoleteAuthErrorPrefs(profile_prefs);

  // Added 3/2019.
  syncer::ClearObsoleteFirstSyncTime(profile_prefs);
  syncer::ClearObsoleteSyncLongPollIntervalSeconds(profile_prefs);

  // Added 3/2019.
  profile_prefs->ClearPref(kCurrentThemeImages);
  profile_prefs->ClearPref(kCurrentThemeColors);
  profile_prefs->ClearPref(kCurrentThemeTints);
  profile_prefs->ClearPref(kCurrentThemeDisplayProperties);

#if defined(OS_ANDROID)
  // Added 4/2019.
  profile_prefs->ClearPref(kDismissedAssetDownloadSuggestions);
  profile_prefs->ClearPref(kDismissedOfflinePageDownloadSuggestions);

  // Added 4/2019.
  profile_prefs->ClearPref(kBreakingNewsSubscriptionDataToken);
  profile_prefs->ClearPref(kBreakingNewsSubscriptionDataIsAuthenticated);
  profile_prefs->ClearPref(kBreakingNewsGCMSubscriptionTokenCache);
  profile_prefs->ClearPref(kBreakingNewsGCMLastTokenValidationTime);
  profile_prefs->ClearPref(kBreakingNewsGCMLastForcedSubscriptionTime);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  // Added 4/2019.
  syncer::ClearObsoleteSyncSpareBootstrapToken(profile_prefs);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  // Added 4/2019.
  profile_prefs->ClearPref(kContentSuggestionsConsecutiveIgnoredPrefName);
  profile_prefs->ClearPref(kContentSuggestionsNotificationsSentDay);
  profile_prefs->ClearPref(kContentSuggestionsNotificationsSentCount);
  profile_prefs->ClearPref(kNotificationIDWithinCategory);

  // Added 5/2019.
  profile_prefs->ClearPref(kContentSuggestionsNotificationsEnabled);
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  // Deprecated 5/2019
  profile_prefs->ClearPref(kSignInPromoShowOnFirstRunAllowed);
  profile_prefs->ClearPref(kSignInPromoShowNTPBubble);
#endif  // !defined(OS_ANDROID)

  // Added 5/2019.
  profile_prefs->ClearPref(kBookmarkAppCreationLaunchType);

  // Added 6/2019.
  profile_prefs->ClearPref(kMediaCacheSize);
#if defined(OS_MACOSX)
  profile_prefs->ClearPref(password_manager::prefs::kKeychainMigrationStatus);
#endif  // defined(OS_MACOSX)

  // Added 7/2019.
  syncer::MigrateSyncSuppressedPref(profile_prefs);
  profile_prefs->ClearPref(kSignedInTime);
  syncer::ClearObsoleteMemoryPressurePrefs(profile_prefs);
  profile_prefs->ClearPref(kLastKnownGoogleURL);
  profile_prefs->ClearPref(kLastPromptedGoogleURL);

#if defined(OS_ANDROID)
  // Added 7/2019.
  profile_prefs->ClearPref(kWebAuthnLastTransportUsedPrefName);
  profile_prefs->ClearPref(kWebAuthnBlePairedMacAddressesPrefName);
#endif  // defined(OS_ANDROID)

  // Added 7/2019.
#if defined(USE_X11)
  profile_prefs->ClearPref(kLocalProfileId);
#endif

  // Added 8/2019
  profile_prefs->ClearPref(kInsecureExtensionUpdatesEnabled);
  profile_prefs->ClearPref(kHintLoadedCounts);

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
}
