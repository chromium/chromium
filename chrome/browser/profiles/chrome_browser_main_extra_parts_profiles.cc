// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/page_colors_factory.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter_factory.h"
#include "chrome/browser/history/domain_diversity_reporter_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/login_detection/login_detection_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/presentation/chrome_local_presentation_manager_factory.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service_factory.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/optimization_guide/model_validator_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_memory_tracker_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/last_tab_standing_tracker_factory.h"
#include "chrome/browser/permissions/permission_auditing_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_link_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/privacy/privacy_metrics_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
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
#include "chrome/browser/signin/signin_profile_attributes_updater_factory.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/permissions/features.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"
#include "chrome/browser/android/reading_list/reading_list_manager_factory.h"
#include "chrome/browser/android/reading_list/reading_list_notification_service_factory.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/android/thin_webview/chrome_thin_webview_initializer.h"
#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager_factory.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/merchant_signal_db_content.pb.h"
#else
#include "chrome/browser/accessibility/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_translate_controller_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "chrome/browser/profiles/profile_theme_update_service_factory.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/storage/storage_notification_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/browser_context_keyed_service_factories.h"
#include "chrome/browser/ash/login/security_token_session_controller_factory.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager_factory.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/policy/messaging_layer/util/manual_test_heartbeat_event_factory.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/adjustments/web_app_adjustments.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/api/chromeos_api_browser_context_keyed_service_factories.h"
#endif
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/exit_type_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros_factory.h"
#include "chrome/browser/lacros/account_manager/profile_account_manager_factory.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/autocomplete/autocomplete_scoring_model_service_factory.h"
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/cocoa/screentime/history_bridge_factory.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/idle/idle_service_factory.h"
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
// static
void ChromeBrowserMainExtraPartsProfiles::
    EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_apps::api::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  chrome_extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#if BUILDFLAG(IS_CHROMEOS)
  chromeos_extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::login::SecurityTokenSessionControllerFactory::GetInstance();
  ash::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif

  AboutSigninInternalsFactory::GetInstance();
  AboutThisSiteServiceFactory::GetInstance();
  AccessContextAuditServiceFactory::GetInstance();
  AccountConsistencyModeManagerFactory::GetInstance();
  AccountInvestigatorFactory::GetInstance();
  AccountReconcilorFactory::GetInstance();
  AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_list::AppListSyncableServiceFactory::GetInstance();
  apps::AppPreloadServiceFactory::GetInstance();
  ash::AccountAppsAvailabilityFactory::GetInstance();
  ash::SystemWebAppManagerFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  apps::AppServiceProxyFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS)
  apps::SupportedLinksInfoBarPrefsServiceFactory::GetInstance();
#endif
  AutocompleteClassifierFactory::GetInstance();
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  AutocompleteScoringModelServiceFactory::GetInstance();
#endif
  autofill::AutofillImageFetcherFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
  autofill::AutofillOfferManagerFactory::GetInstance();
  autofill::MerchantPromoCodeManagerFactory::GetInstance();
#if BUILDFLAG(ENABLE_BACKGROUND_CONTENTS)
  BackgroundContentsServiceFactory::GetInstance();
#endif
  BookmarkModelFactory::GetInstance();
  BookmarkUndoServiceFactory::GetInstance();
  if (breadcrumbs::IsEnabled())
    BreadcrumbManagerKeyedServiceFactory::GetInstance();
  browser_sync::UserEventServiceFactory::GetInstance();
  BrowsingDataHistoryObserverService::Factory::GetInstance();
  browsing_topics::BrowsingTopicsServiceFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::CleanupManagerLacrosFactory::GetInstance();
  chromeos::RemoteAppsProxyLacrosFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  CartServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(commerce::kCommerceMerchantViewer))
    MerchantViewerDataManagerFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  CertDbInitializerFactory::GetInstance();
#endif
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  CertificateReportingServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  ChromeBrowsingDataLifetimeManagerFactory::GetInstance();
#endif
  ChromeBrowsingDataRemoverDelegateFactory::GetInstance();
  ChromeSigninClientFactory::GetInstance();
  ClientHintsFactory::GetInstance();
  ConsentAuditorFactory::GetInstance();
  CookieSettingsFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  CouponServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DiceWebSigninInterceptorFactory::GetInstance();
#endif
  DomainDiversityReporterFactory::GetInstance();
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  DownloadCoreServiceFactory::GetInstance();
  BackgroundDownloadServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  ExitTypeServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  explore_sites::ExploreSitesServiceFactory::GetInstance();
#endif
  FaviconServiceFactory::GetInstance();
  feature_engagement::TrackerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  feedback::FeedbackUploaderFactoryChrome::GetInstance();
#endif
  FindBarStateFactory::GetInstance();
  first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  GAIAInfoUpdateServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  GlobalErrorServiceFactory::GetInstance();
#endif
  GoogleSearchDomainMixingMetricsEmitterFactory::GetInstance();
  HistoryServiceFactory::GetInstance();
  HistoryUiFaviconRequestHandlerFactory::GetInstance();
  HostContentSettingsMapFactory::GetInstance();
  HttpsEngagementServiceFactory::GetInstance();
  IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt();
  InMemoryURLIndexFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  InstantServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  FirstRunServiceFactory::GetInstance();
#endif
  LanguageModelManagerFactory::GetInstance();
  if (base::FeatureList::IsEnabled(
          permissions::features::kOneTimeGeolocationPermission)) {
    LastTabStandingTrackerFactory::GetInstance();
  }
#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  captions::LiveCaptionControllerFactory::GetInstance();
#endif
  if (base::FeatureList::IsEnabled(media::kLiveTranslate))
    captions::LiveTranslateControllerFactory::GetInstance();
#endif
  login_detection::LoginDetectionKeyedServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  LoginUIServiceFactory::GetInstance();
  ManagedConfigurationAPIFactory::GetInstance();
#endif
#if BUILDFLAG(IS_ANDROID)
  MediaDrmOriginIdManagerFactory::GetInstance();
#endif
  if (MediaEngagementService::IsEnabled())
    MediaEngagementServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  MediaGalleriesPreferencesFactory::GetInstance();
#endif
  if (base::FeatureList::IsEnabled(media::kUseMediaHistoryStore))
    media_history::MediaHistoryKeyedServiceFactory::GetInstance();
  media_router::ChromeLocalPresentationManagerFactory::GetInstance();
  media_router::ChromeMediaRouterFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastUI))
    media_router::AccessCodeCastSinkServiceFactory::GetInstance();
  media_router::MediaRouterUIServiceFactory::GetInstance();
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  metrics::DesktopProfileSessionDurationsServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
  enterprise_idle::IdleServiceFactory::GetInstance();
#endif
  ModelTypeStoreServiceFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NearbySharingServiceFactory::GetInstance();
#endif
  NotifierStateTrackerFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  NTPResourceCacheFactory::GetInstance();
#endif
  OptimizationGuideKeyedServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  PageColorsFactory::GetInstance();
#endif
  if (optimization_guide::switches::ShouldValidateModel())
    optimization_guide::ModelValidatorKeyedServiceFactory::GetInstance();
  OriginTrialsFactory::GetInstance();
  page_load_metrics::PageLoadMetricsMemoryTrackerFactory::GetInstance();
  PasswordStoreFactory::GetInstance();
  PermissionAuditingServiceFactory::GetInstance();
  SessionProtoDBFactory<
      persisted_state_db::PersistedStateContentProto>::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  PinnedTabServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  PluginPrefsFactory::GetInstance();
#endif
  PrefMetricsService::Factory::GetInstance();
  PrefsTabHelper::GetServiceInstance();
#if !BUILDFLAG(IS_ANDROID)
  SessionProtoDBFactory<cart_db::ChromeCartContentProto>::GetInstance();
  SessionProtoDBFactory<coupon_db::CouponContentProto>::GetInstance();
#endif
  SessionProtoDBFactory<commerce_subscription_db::
                            CommerceSubscriptionContentProto>::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  SessionProtoDBFactory<
      merchant_signal_db::MerchantSignalContentProto>::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS)
  policy::DlpRulesManagerFactory::GetInstance();
  policy::PolicyCertServiceFactory::GetInstance();
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
#endif
  policy::UserCloudPolicyInvalidatorFactory::GetInstance();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::UserPolicySigninServiceFactory::GetInstance();
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  enterprise_reporting::CloudProfileReportingServiceFactory::GetInstance();
#endif
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceNotificationPredictions) ||
      base::FeatureList::IsEnabled(
          permissions::features::kPermissionOnDeviceGeolocationPredictions)) {
    PredictionModelHandlerProviderFactory::GetInstance();
  }
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  predictors::AutocompleteActionPredictorFactory::GetInstance();
  predictors::LoadingPredictorFactory::GetInstance();
  predictors::PredictorDatabaseFactory::GetInstance();
  prerender::NoStatePrefetchLinkManagerFactory::GetInstance();
  prerender::NoStatePrefetchManagerFactory::GetInstance();
  PrivacyMetricsServiceFactory::GetInstance();
  PrivacySandboxServiceFactory::GetInstance();
  PrivacySandboxSettingsFactory::GetInstance();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ProfileAccountManagerFactory::GetInstance();
#endif
  ProfileNetworkContextServiceFactory::GetInstance();
  SyncServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  ProfileThemeUpdateServiceFactory::GetInstance();
#endif
  ProtocolHandlerRegistryFactory::GetInstance();

  ReadingListModelFactory::GetInstance();

#if BUILDFLAG(IS_ANDROID)
  ReadingListManagerFactory::GetInstance();
  ReadingListNotificationServiceFactory::GetInstance();
#endif

  if (base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage))
    ReduceAcceptLanguageFactory::GetInstance();

  RendererUpdaterFactory::GetInstance();

#if BUILDFLAG(IS_CHROMEOS)
  reporting::ManualTestHeartbeatEventFactory::GetInstance();
#endif

#if !BUILDFLAG(IS_ANDROID)
  performance_manager::SiteDataCacheFacadeFactory::GetInstance();
#endif
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance();
  safe_browsing::ExtensionTelemetryServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_MAC)
  if (screentime::IsScreenTimeEnabled())
    screentime::HistoryBridgeFactory::GetInstance();
#endif
  SCTReportingServiceFactory::GetInstance();
#if BUILDFLAG(IS_ANDROID)
  SearchPermissionsService::Factory::GetInstance();
#endif
  segmentation_platform::SegmentationPlatformServiceFactory::GetInstance();
  send_tab_to_self::SendTabToSelfClientServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetInstance();
  SessionDataServiceFactory::GetInstance();
#endif
  SharingServiceFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  sharing_hub::SharingHubServiceFactory::GetInstance();
#endif
  commerce::ShoppingServiceFactory::GetInstance();
  ShortcutsBackendFactory::GetInstance();
  SigninProfileAttributesUpdaterFactory::GetInstance();
  if (site_engagement::SiteEngagementService::IsEnabled())
    site_engagement::SiteEngagementServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  SigninManagerFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
  SpellcheckServiceFactory::GetInstance();
#endif
#if !BUILDFLAG(IS_ANDROID)
  StorageNotificationServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserMetricsServiceFactory::GetInstance();
  SupervisedUserServiceFactory::GetInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(ash::features::kSystemExtensions)) {
    ash::SystemExtensionsProviderFactory::GetInstance();
    ash::CrosWindowManagementContextFactory::GetInstance();
  }
#endif
  TabRestoreServiceFactory::GetInstance();
  safe_browsing::TailoredSecurityServiceFactory::GetInstance();
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
  translate::TranslateRankerFactory::GetInstance();
#if BUILDFLAG(IS_WIN)
  TriggeredProfileResetterFactory::GetInstance();
#endif
  UnifiedConsentServiceFactory::GetInstance();
  UrlLanguageHistogramFactory::GetInstance();
#if !BUILDFLAG(IS_ANDROID)
  UsbChooserContextFactory::GetInstance();
  UserEducationServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  web_app::WebAppMetricsFactory::GetInstance();
  web_app::WebAppProviderFactory::GetInstance();
  web_app::WebAppAdjustmentsFactory::GetInstance();
  web_app::IsolatedWebAppReaderRegistryFactory::GetInstance();
#endif
  WebDataServiceFactory::GetInstance();
  webrtc_event_logging::WebRtcEventLogManagerKeyedServiceFactory::GetInstance();
}

void ChromeBrowserMainExtraPartsProfiles::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}
