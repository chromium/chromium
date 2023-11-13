// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/accessibility/page_colors_factory.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/provider_state_service_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/autofill_optimization_guide_factory.h"
#include "chrome/browser/autofill/iban_manager_factory.h"
#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/content_index/content_index_provider_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/dips/dips_cleanup_service_factory.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_service.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/font_pref_change_notifier_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/hid/hid_policy_allowed_devices_factory.h"
#include "chrome/browser/history/domain_diversity_reporter_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/image_service/image_service_factory.h"
#include "chrome/browser/ip_protection/ip_protection_config_provider_factory.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_factory.h"
#include "chrome/browser/language/accept_languages_service_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/login_detection/login_detection_keyed_service_factory.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/presentation/chrome_local_presentation_manager_factory.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service_factory.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/metrics/variations/google_groups_updater_service_factory.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service_factory.h"
#include "chrome/browser/net/dns_probe_service_factory.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/optimization_guide/model_validator_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_memory_tracker_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/field_info_manager_factory.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/permissions/origin_keyed_permission_action_service_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_auditing_service_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/prediction_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_link_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/privacy/privacy_metrics_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/private_network_access/private_network_device_permission_context_factory.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sessions/session_data_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/account_investigator_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/primary_account_policy_manager_factory.h"
#include "chrome/browser/signin/signin_profile_attributes_updater_factory.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "chrome/browser/tpcd/metadata/updater_service_factory.h"
#include "chrome/browser/tpcd/support/tpcd_support_service_factory.h"
#include "chrome/browser/translate/translate_model_service_factory.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/media_router/cast_notification_controller_lacros_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/browser/webid/federated_identity_api_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "components/enterprise/content/clipboard_restriction_service.h"
#include "components/media_effects/media_effects_service_factory.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/content/browser/password_requirements_service_factory.h"
#include "components/payments/content/can_make_payment_query_factory.h"
#include "components/permissions/features.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/content/safe_search_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"
#include "chrome/browser/android/metrics/android_session_durations_service_factory.h"
#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android_factory.h"
#include "chrome/browser/android/reading_list/reading_list_manager_factory.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/android/thin_webview/chrome_thin_webview_initializer.h"
#include "chrome/browser/android/webapk/webapk_install_service_factory.h"
#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager_factory.h"
#include "chrome/browser/content_creation/notes/internal/note_service_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"
#include "chrome/browser/search_resumption/start_suggest_service_factory.h"
#include "chrome/browser/signin/signin_manager_android_factory.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/merchant_signal_db_content.pb.h"
#else
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/media_galleries/gallery_watch_manager.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/new_tab_page/modules/drive/drive_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service_factory.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "chrome/browser/profile_resetter/reset_report_uploader_factory.h"
#include "chrome/browser/profiles/profile_theme_update_service_factory.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/storage/storage_notification_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service_factory.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"
#include "chrome/browser/apps/app_service/subscriber_crosapi_factory.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/browser_context_keyed_service_factories.h"
#include "chrome/browser/ash/file_manager/cloud_upload_prefs_watcher.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chromeos/constants/chromeos_features.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/cros_apps/cros_apps_key_event_handler_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_download_observer_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event_factory.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/win/jumplist_factory.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/recovery/recovery_install_global_error_factory.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/signin/signin_manager_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/apps/platform_apps/api/browser_context_keyed_service_factories.h"
#include "chrome/browser/apps/platform_apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"
#include "components/omnibox/browser/omnibox_suggestions_watcher.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_extension_api_browser_context_keyed_service_factories.h"
#include "chrome/browser/extensions/api/chromeos_api_browser_context_keyed_service_factories.h"
#endif
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/exit_type_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/hash_realtime_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros_factory.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_factory.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_shutdown_notifier_factory.h"
#include "chrome/browser/lacros/account_manager/profile_account_manager_factory.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"
#include "chrome/browser/speech/tts_client_factory_lacros.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/autocomplete/autocomplete_scoring_model_service_factory.h"
#include "chrome/browser/autocomplete/on_device_tail_model_service_factory.h"
#include "chrome/browser/autofill/autofill_ml_prediction_model_service_factory.h"
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/cocoa/screentime/history_bridge_factory.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/idle/idle_service_factory.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/policy/cloud/profile_token_policy_web_signin_service_factory.h"
#include "chrome/browser/signin/profile_token_web_signin_interceptor_factory.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker_factory.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_service_factory.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/new_tab_page/promos/promo_service_factory.h"
#include "chrome/browser/payments/payment_request_display_manager_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/performance_controls/performance_controls_hats_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "components/manta/features.h"
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/accessibility/ax_screen_ai_annotator_factory.h"
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#endif

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/browser/net/nss_service_factory.h"
#endif

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#endif

namespace chrome {

void AddProfilesExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsProfiles>());
}

}  // namespace chrome

ChromeBrowserMainExtraPartsProfiles::ChromeBrowserMainExtraPartsProfiles() =
    default;

ChromeBrowserMainExtraPartsProfiles::~ChromeBrowserMainExtraPartsProfiles() =
    default;

// This method gets the instance of each ServiceFactory. We do this so that
// each ServiceFactory initializes itself and registers its dependencies with
// the global PreferenceDependencyManager. We need to have a complete
// dependency graph when we create a profile so we can dispatch the profile
// creation message to the services that want to create their services at
// profile creation time.
//
// TODO(erg): This needs to be something else. I don't think putting every
// FooServiceFactory here will scale or is desirable long term.
//
// TODO(crbug/1414416): Check how to simplify the approach of registering every
// factory in this function.
//
// static
void ChromeBrowserMainExtraPartsProfiles::
    EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  // ---------------------------------------------------------------------------
  // Redirect to those lists for factories that are part of those modules.
  // Module specific registration functions:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_apps::api::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chromeos_extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif  // BUILDFLAG(IS_CHROMEOS)
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // ---------------------------------------------------------------------------
  // Common factory registrations (Please keep this list ordered without taking
  // into consideration buildflags, repeating buildflags is ok):
  AboutSigninInternalsFactory::GetInstance();
  AboutThisSiteServiceFactory::GetInstance();
  AcceptLanguagesServiceFactory::GetInstance();
  AccessContextAuditServiceFactory::GetInstance();
  AccessibilityLabelsServiceFactory::GetInstance();
  AccountBookmarkSyncServiceFactory::GetInstance();
  AccountConsistencyModeManagerFactory::GetInstance();
  AccountInvestigatorFactory::GetInstance();
  AccountPasswordStoreFactory::GetInstance();
  AccountReconcilorFactory::GetInstance();
  AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  AndroidSessionDurationsServiceFactory::GetInstance();
#endif
  AnnouncementNotificationServiceFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_list::AppListSyncableServiceFactory::GetInstance();
  apps::AppPreloadServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  apps::AppServiceProxyFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::deduplication::AppDeduplicationServiceFactory::GetInstance();
  apps::StandaloneBrowserExtensionAppsFactoryForApp::GetInstance();
  apps::StandaloneBrowserExtensionAppsFactoryForExtension::GetInstance();
  apps::SubscriberCrosapiFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::WebAppsCrosapiFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  AppSessionServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::features::IsOrcaEnabled()) {
    ash::input_method::EditorMediatorFactory::GetInstance();
  }
  // The 2 factories below could not be added to the ash list as they need to
  // declare dependencies.
  // TODO: fix the dependency with
  // 'chrome/browser/ash/browser_context_keyed_service_factories.cc'
  if (base::FeatureList::IsEnabled(ash::features::kSystemExtensions)) {
    ash::CrosWindowManagementContextFactory::GetInstance();
    ash::SystemExtensionsProviderFactory::GetInstance();
  }
#endif
  AutocompleteClassifierFactory::GetInstance();
  AutocompleteControllerEmitter::EnsureFactoryBuilt();
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  AutocompleteScoringModelServiceFactory::GetInstance();
#endif
  autofill::AutocompleteHistoryManagerFactory::GetInstance();
  autofill::AutofillImageFetcherFactory::GetInstance();
  autofill::AutofillLogRouterFactory::GetInstance();
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  autofill::AutofillMlPredictionModelServiceFactory::GetInstance();
#endif
  autofill::AutofillOfferManagerFactory::GetInstance();
  autofill::AutofillOptimizationGuideFactory::GetInstance();
  autofill::IbanManagerFactory::GetInstance();
  autofill::MerchantPromoCodeManagerFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  AuxiliarySearchProvider::EnsureFactoryBuilt();
#endif
#if BUILDFLAG(ENABLE_BACKGROUND_CONTENTS)
  BackgroundContentsServiceFactory::GetInstance();
#endif
  BackgroundDownloadServiceFactory::GetInstance();
  BackgroundFetchDelegateFactory::GetInstance();
  BackgroundSyncControllerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  badging::BadgeManagerFactory::GetInstance();
#endif
  BitmapFetcherServiceFactory::GetInstance();
  BluetoothChooserContextFactory::GetInstance();
#if defined(TOOLKIT_VIEWS)
  BookmarkExpandedStateTrackerFactory::GetInstance();
#endif
  BookmarkModelFactory::GetInstance();
  BookmarkUndoServiceFactory::GetInstance();
  if (breadcrumbs::IsEnabled()) {
    BreadcrumbManagerKeyedServiceFactory::GetInstance();
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  browser_switcher::BrowserSwitcherServiceFactory::GetInstance();
#endif
  browser_sync::UserEventServiceFactory::GetInstance();
  browsing_topics::BrowsingTopicsServiceFactory::GetInstance();
  BrowsingDataHistoryObserverService::Factory::GetInstance();
#if defined(TOOLKIT_VIEWS)
  BubbleContentsWrapperServiceFactory::GetInstance();
#endif
  BulkLeakCheckServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  captions::LiveCaptionControllerFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kLiveTranslate)) {
    captions::LiveTranslateControllerFactory::GetInstance();
  }
#endif
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  CartServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  CertDbInitializerFactory::GetInstance();
#endif
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  CertificateReportingServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  ChildAccountServiceFactory::GetInstance();
#endif
  chrome_browser_net::DnsProbeServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  chrome_colors::ChromeColorsFactory::GetInstance();
#endif
  ChromeBrowsingDataLifetimeManagerFactory::GetInstance();
  ChromeBrowsingDataRemoverDelegateFactory::GetInstance();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS)
  ChromeDeviceAuthenticatorFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::CertificateProviderServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::CleanupManagerLacrosFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    chromeos::cloud_upload::CloudUploadPrefsWatcherFactory::GetInstance();
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::RemoteAppsProxyLacrosFactory::GetInstance();
#endif
  ChromeSigninClientFactory::GetInstance();
  ClientHintsFactory::GetInstance();
  ClipboardRestrictionServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  ClosedTabCacheServiceFactory::GetInstance();
  companion::visual_search::VisualSearchSuggestionsServiceFactory::
      GetInstance();
#endif
  commerce::ShoppingServiceFactory::GetInstance();
  ConsentAuditorFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  content_creation::NoteServiceFactory::GetInstance();
#endif
  ContentIndexProviderFactory::GetInstance();
  CookieControlsServiceFactory::GetInstance();
  CookieSettingsFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  CouponServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          chromeos::features::kCrosAppsBackgroundEventHandling)) {
    CrosAppsKeyEventHandlerFactory::GetInstance();
  }
#endif
#if !BUILDFLAG(IS_ANDROID)
  DevToolsAndroidBridge::Factory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DiceResponseHandler::EnsureFactoryBuilt();
  DiceWebSigninInterceptorFactory::GetInstance();
#endif
  DIPSCleanupServiceFactory::GetInstance();
  DIPSServiceFactory::GetInstance();
  DocumentSuggestionsServiceFactory::GetInstance();
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  DomainDiversityReporterFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  DownloadBubbleUpdateServiceFactory::GetInstance();
#endif
  DownloadCoreServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  DriveServiceFactory::GetInstance();
#endif
  enterprise::ProfileIdServiceFactory::GetInstance();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  enterprise_commands::UserRemoteCommandsServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  enterprise_connectors::DeviceTrustConnectorServiceFactory::GetInstance();
  enterprise_connectors::DeviceTrustServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  enterprise_connectors::LocalBinaryUploadServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
  enterprise_idle::IdleServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  enterprise_reporting::CloudProfileReportingServiceFactory::GetInstance();
#endif
  enterprise_reporting::LegacyTechServiceFactory::GetInstance();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  enterprise_signals::SignalsAggregatorFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  enterprise_signals::UserPermissionServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  enterprise_signin::EnterpriseSigninServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  ExitTypeServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  FastCheckoutCapabilitiesFetcherFactory::GetInstance();
#endif
  FaviconServiceFactory::GetInstance();
  feature_engagement::TrackerFactory::GetInstance();
  feature_guide::FeatureNotificationGuideServiceFactory::GetInstance();
  FederatedIdentityApiPermissionContextFactory::GetInstance();
  FederatedIdentityAutoReauthnPermissionContextFactory::GetInstance();
  FederatedIdentityPermissionContextFactory::GetInstance();
  feed::FeedServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  feedback::FeedbackUploaderFactoryChrome::GetInstance();
#endif
  FieldInfoManagerFactory::GetInstance();
  FileSystemAccessPermissionContextFactory::GetInstance();
  FindBarStateFactory::GetInstance();
  first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance();
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  FirstRunServiceFactory::GetInstance();
#endif
  FontPrefChangeNotifierFactory::GetInstance();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  GAIAInfoUpdateServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  GalleryWatchManager::EnsureFactoryBuilt();
  GlobalErrorServiceFactory::GetInstance();
#endif
  GoogleGroupsUpdaterServiceFactory::GetInstance();
  HeavyAdServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  HidChooserContextFactory::GetInstance();
  HidConnectionTrackerFactory::GetInstance();
  HidPolicyAllowedDevicesFactory::GetInstance();
  HistoryClustersModuleServiceFactory::GetInstance();
#endif
  HistoryClustersServiceFactory::EnsureFactoryBuilt();
  HistoryServiceFactory::GetInstance();
  HistoryUiFaviconRequestHandlerFactory::GetInstance();
  HostContentSettingsMapFactory::GetInstance();
  HttpsEngagementServiceFactory::GetInstance();
  HttpsFirstModeServiceFactory::GetInstance();
  IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt();
  InMemoryURLIndexFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  InstantServiceFactory::GetInstance();
#endif
  IpProtectionConfigProviderFactory::GetInstance();
#if BUILDFLAG(IS_WIN)
  JumpListFactory::GetInstance();
#endif
  KAnonymityServiceFactory::GetInstance();
  LanguageModelManagerFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  LevelDBPersistedTabDataStorageAndroidFactory::GetInstance();
#endif
  LocalOrSyncableBookmarkSyncServiceFactory::GetInstance();
  login_detection::LoginDetectionKeyedServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  LoginUIServiceFactory::GetInstance();
#endif
  LogoServiceFactory::GetInstance();
  LookalikeUrlService::EnsureFactoryBuilt();
#if !BUILDFLAG(IS_ANDROID)
  ManagedConfigurationAPIFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  if (manta::features::IsMantaServiceEnabled()) {
    manta::MantaServiceFactory::GetInstance();
  }
#endif
  MediaDeviceSaltServiceFactory::GetInstance();
  if (base::FeatureList::IsEnabled(media::kUseMediaHistoryStore)) {
    media_history::MediaHistoryKeyedServiceFactory::GetInstance();
  }
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastUI)) {
    media_router::AccessCodeCastSinkServiceFactory::GetInstance();
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  media_router::CastNotificationControllerLacrosFactory::GetInstance();
#endif
  media_router::ChromeLocalPresentationManagerFactory::GetInstance();
  media_router::ChromeMediaRouterFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  media_router::MediaRouterUIServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  MediaDrmOriginIdManagerFactory::GetInstance();
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
    MediaEffectsServiceFactory::GetInstance();
  }
#endif
  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementServiceFactory::GetInstance();
  }
#if !BUILDFLAG(IS_ANDROID)
  MediaFileSystemRegistry::GetFactoryInstance();
  MediaGalleriesPreferencesFactory::GetInstance();
  MediaNotificationServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  MerchantViewerDataManagerFactory::GetInstance();
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  metrics::DesktopProfileSessionDurationsServiceFactory::GetInstance();
#endif
  ModelTypeStoreServiceFactory::GetInstance();
  NavigationPredictorKeyedServiceFactory::GetInstance();
  PreloadingModelKeyedServiceFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NearbySharingServiceFactory::GetInstance();
#endif
  NotificationDisplayServiceFactory::GetInstance();
  NotificationMetricsLoggerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  NotificationPermissionsReviewServiceFactory::GetInstance();
#endif
  NotificationsEngagementServiceFactory::GetInstance();
  NotifierStateTrackerFactory::GetInstance();
#if BUILDFLAG(USE_NSS_CERTS)
  NssServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  NtpBackgroundServiceFactory::GetInstance();
  NtpCustomBackgroundServiceFactory::GetInstance();
  NTPResourceCacheFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageAutoFetcherServiceFactory::GetInstance();
  offline_pages::RequestCoordinatorFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  OfflineItemModelManagerFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_EXTENSIONS)
  OmniboxInputWatcher::EnsureFactoryBuilt();
  OmniboxSuggestionsWatcher::EnsureFactoryBuilt();
#endif
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  OnDeviceTailModelServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  OneGoogleBarServiceFactory::GetInstance();
  if (base::FeatureList::IsEnabled(permissions::features::kOneTimePermission)) {
    OneTimePermissionsTrackerFactory::GetInstance();
  }
#endif
  if (optimization_guide::switches::ShouldValidateModel()) {
    optimization_guide::ModelValidatorKeyedServiceFactory::GetInstance();
  }
  OptimizationGuideKeyedServiceFactory::GetInstance();
  OriginKeyedPermissionActionServiceFactory::GetInstance();
  OriginTrialsFactory::GetInstance();
  page_image_service::ImageServiceFactory::EnsureFactoryBuilt();
  page_load_metrics::PageLoadMetricsMemoryTrackerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  PageColorsFactory::GetInstance();
#endif
  password_manager::PasswordChangeSuccessTrackerFactory::GetInstance();
  password_manager::PasswordManagerLogRouterFactory::GetInstance();
  password_manager::PasswordRequirementsServiceFactory::GetInstance();
  PasswordManagerSettingsServiceFactory::GetInstance();
  PasswordReuseManagerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  PasswordStatusCheckServiceFactory::GetInstance();
#endif
  ProfilePasswordStoreFactory::GetInstance();
  payments::CanMakePaymentQueryFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  payments::PaymentRequestDisplayManagerFactory::GetInstance();
  performance_manager::SiteDataCacheFacadeFactory::GetInstance();
  PerformanceControlsHatsServiceFactory::GetInstance();
#endif
  PermissionActionsHistoryFactory::GetInstance();
  PermissionAuditingServiceFactory::GetInstance();
  PermissionDecisionAutoBlockerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  PhotosServiceFactory::GetInstance();
  PinnedTabServiceFactory::GetInstance();
  if (base::FeatureList::IsEnabled(features::kSidePanelPinning)) {
    PinnedToolbarActionsModelFactory::GetInstance();
  }
#endif
  PlatformNotificationServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_PLUGINS)
  PluginInfoHostImpl::EnsureFactoryBuilt();
  PluginPrefsFactory::GetInstance();
#endif
  PlusAddressServiceFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS)
  policy::DlpDownloadObserverFactory::GetInstance();
  policy::DlpRulesManagerFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FilesPolicyNotificationManagerFactory::GetInstance();
#endif
  policy::ManagementServiceFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS)
  policy::PolicyCertServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  policy::ProfileTokenPolicyWebSigninServiceFactory::GetInstance();
#endif
  policy::UserCloudPolicyInvalidatorFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS)
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::UserPolicySigninServiceFactory::GetInstance();
#endif
  PolicyBlocklistFactory::GetInstance();
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceNotificationPredictions) ||
      base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceGeolocationPredictions)) {
    PredictionModelHandlerProviderFactory::GetInstance();
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  PredictionServiceFactory::GetInstance();
  predictors::AutocompleteActionPredictorFactory::GetInstance();
  predictors::LoadingPredictorFactory::GetInstance();
  predictors::PredictorDatabaseFactory::GetInstance();
  PrefMetricsService::Factory::GetInstance();
  PrefsTabHelper::GetServiceInstance();
  prerender::NoStatePrefetchLinkManagerFactory::GetInstance();
  prerender::NoStatePrefetchManagerFactory::GetInstance();
  PrimaryAccountPolicyManagerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  PrivateNetworkDevicePermissionContextFactory::GetInstance();
#endif
  PrivacyMetricsServiceFactory::GetInstance();
  PrivacySandboxServiceFactory::GetInstance();
  PrivacySandboxSettingsFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ProfileAccountManagerFactory::GetInstance();
#endif
  ProfileNetworkContextServiceFactory::GetInstance();
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  ProfileStatisticsFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  ProfileThemeUpdateServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ProfileTokenWebSigninInterceptorFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  PromoServiceFactory::GetInstance();
#endif
  ProtocolHandlerRegistryFactory::GetInstance();
  ProviderStateServiceFactory::GetInstance();
  PushMessagingServiceFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  ReadingListManagerFactory::GetInstance();
#endif
  ReadingListModelFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  RecipesServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  RecoveryInstallGlobalErrorFactory::GetInstance();
#endif
  ReduceAcceptLanguageFactory::GetInstance();
  RendererUpdaterFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS)
  reporting::ManualTestHeartbeatEventFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  reporting::metrics::MetricReportingManagerLacrosFactory::GetInstance();
  reporting::metrics::MetricReportingManagerLacrosShutdownNotifierFactory::
      GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  ResetReportUploaderFactory::GetInstance();
#endif
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance();
  safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetInstance();
#endif
  safe_browsing::ChromePasswordProtectionServiceFactory::GetInstance();
  safe_browsing::ChromePingManagerFactory::GetInstance();
  safe_browsing::ClientSideDetectionServiceFactory::GetInstance();
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::CloudBinaryUploadServiceFactory::GetInstance();
  safe_browsing::ExtensionTelemetryServiceFactory::GetInstance();
  safe_browsing::HashRealTimeServiceFactory::GetInstance();
#endif
  safe_browsing::RealTimeUrlLookupServiceFactory::GetInstance();
  safe_browsing::SafeBrowsingMetricsCollectorFactory::GetInstance();
  safe_browsing::SafeBrowsingNavigationObserverManagerFactory::GetInstance();
  safe_browsing::TailoredSecurityServiceFactory::GetInstance();
  safe_browsing::VerdictCacheManagerFactory::GetInstance();
  SafeSearchFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  SafetyHubMenuNotificationServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  screen_ai::AXScreenAIAnnotatorFactory::EnsureFactoryBuilt();
  screen_ai::PdfOcrControllerFactory::GetInstance();
  screen_ai::ScreenAIServiceRouterFactory::EnsureFactoryBuilt();
#endif
#if BUILDFLAG(IS_MAC)
  if (screentime::IsScreenTimeEnabled()) {
    screentime::HistoryBridgeFactory::GetInstance();
  }
#endif
  SCTReportingServiceFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  search_resumption_module::StartSuggestServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
  SearchEngineChoiceServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  SearchPermissionsService::Factory::GetInstance();
#endif
  SearchPrefetchServiceFactory::GetInstance();
  segmentation_platform::SegmentationPlatformServiceFactory::GetInstance();
  send_tab_to_self::SendTabToSelfClientServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  SerialChooserContextFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionDataServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  SessionProtoDBFactory<cart_db::ChromeCartContentProto>::GetInstance();
#endif
  SessionProtoDBFactory<commerce_subscription_db::
                            CommerceSubscriptionContentProto>::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  SessionProtoDBFactory<coupon_db::CouponContentProto>::GetInstance();
  SessionProtoDBFactory<discounts_db::DiscountsContentProto>::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  SessionProtoDBFactory<
      merchant_signal_db::MerchantSignalContentProto>::GetInstance();
#endif
  SessionProtoDBFactory<
      persisted_state_db::PersistedStateContentProto>::GetInstance();
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  sharing_hub::SharingHubServiceFactory::GetInstance();
#endif
  SharingServiceFactory::GetInstance();
  ShortcutsBackendFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  SigninManagerAndroidFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  SigninManagerFactory::GetInstance();
#endif
  SigninProfileAttributesUpdaterFactory::GetInstance();
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementServiceFactory::GetInstance();
  }
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  SpeechRecognitionClientBrowserInterfaceFactory::EnsureFactoryBuilt();
#endif
#if !BUILDFLAG(IS_ANDROID)
  SpeechRecognitionServiceFactory::EnsureFactoryBuilt();
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
  SpellcheckServiceFactory::GetInstance();
#endif
  StatefulSSLHostStateDelegateFactory::GetInstance();
  StorageAccessAPIServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  StorageNotificationServiceFactory::GetInstance();
#endif
  SubresourceFilterProfileContextFactory::GetInstance();
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserMetricsServiceFactory::GetInstance();
  SupervisedUserServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  sync_file_system::SyncFileSystemServiceFactory::GetInstance();
#endif
  SyncServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  if (features::IsTabOrganization()) {
    TabOrganizationServiceFactory::GetInstance();
  }
#endif
  TabRestoreServiceFactory::GetInstance();
  TemplateURLFetcherFactory::GetInstance();
  TemplateURLServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  ThemeServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  thin_webview::android::ChromeThinWebViewInitializer::Initialize();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ToolbarActionsModelFactory::GetInstance();
#endif
  TopSitesFactory::GetInstance();
  tpcd::experiment::EligibilityServiceFactory::GetInstance();
  tpcd::metadata::UpdaterServiceFactory::GetInstance();
  tpcd::support::TpcdSupportServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  TrackingProtectionNoticeFactory::GetInstance();
#endif
  TrackingProtectionOnboardingFactory::GetInstance();
  TrackingProtectionSettingsFactory::GetInstance();
  translate::TranslateRankerFactory::GetInstance();
  TranslateModelServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  TriggeredProfileResetterFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  TtsClientFactoryLacros::GetInstance();
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  TurnSyncOnHelper::EnsureFactoryBuilt();
#endif
  UnifiedConsentServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  UnusedSitePermissionsServiceFactory::GetInstance();
#endif
  UrlLanguageHistogramFactory::GetInstance();
  UsbChooserContextFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  UsbConnectionTrackerFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  user_notes::UserNoteServiceFactory::EnsureFactoryBuilt();
  UserEducationServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  web_app::IsolatedWebAppReaderRegistryFactory::GetInstance();
  web_app::WebAppMetricsFactory::GetInstance();
  web_app::WebAppProviderFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  WebApkInstallServiceFactory::GetInstance();
#endif
  WebDataServiceFactory::GetInstance();
  webrtc_event_logging::WebRtcEventLogManagerKeyedServiceFactory::GetInstance();
}

void ChromeBrowserMainExtraPartsProfiles::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}
