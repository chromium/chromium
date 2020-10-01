// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/browsing_data_history_observer_service.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter_factory.h"
#include "chrome/browser/history/domain_diversity_reporter_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/presentation/chrome_local_presentation_manager_factory.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service_factory.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/account_investigator_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_profile_attributes_updater_factory.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/tab/state/tab_state_db_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/android/thin_webview/chrome_thin_webview_initializer.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"
#else
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"
#include "chrome/browser/profiles/profile_theme_update_service_factory.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/storage/storage_notification_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/browser_context_keyed_service_factories.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_manager_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/apps/platform_apps/api/browser_context_keyed_service_factories.h"
#include "chrome/browser/apps/platform_apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"
#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/printing/cloud_print/privet_notifications_factory.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#endif

namespace chrome {

void AddProfilesExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(std::make_unique<ChromeBrowserMainExtraPartsProfiles>());
}

}  // namespace chrome

ChromeBrowserMainExtraPartsProfiles::ChromeBrowserMainExtraPartsProfiles() {}

ChromeBrowserMainExtraPartsProfiles::~ChromeBrowserMainExtraPartsProfiles() {}

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
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif
#if defined(OS_CHROMEOS)
  chromeos::cert_provisioning::CertProvisioningSchedulerUserServiceFactory::
      GetInstance();
  chromeos::EnsureBrowserContextKeyedServiceFactoriesBuilt();
#endif

  AboutSigninInternalsFactory::GetInstance();
#if !defined(OS_ANDROID)
  AccessContextAuditServiceFactory::GetInstance();
#endif
  AccountConsistencyModeManagerFactory::GetInstance();
  AccountInvestigatorFactory::GetInstance();
  AccountReconcilorFactory::GetInstance();
  AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetInstance();
#if defined(OS_CHROMEOS)
  app_list::AppListSyncableServiceFactory::GetInstance();
#endif
#if !defined(OS_ANDROID)
  apps::AppServiceProxyFactory::GetInstance();
#endif
  AutocompleteClassifierFactory::GetInstance();
  autofill::PersonalDataManagerFactory::GetInstance();
  autofill::AutofillOfferManagerFactory::GetInstance();
#if BUILDFLAG(ENABLE_BACKGROUND_CONTENTS)
  BackgroundContentsServiceFactory::GetInstance();
#endif
  BookmarkModelFactory::GetInstance();
  BookmarkUndoServiceFactory::GetInstance();
  browser_sync::UserEventServiceFactory::GetInstance();
  BrowsingDataHistoryObserverService::Factory::GetInstance();
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalServiceFactory::GetInstance();
#endif
  CertificateReportingServiceFactory::GetInstance();
  ChromeBrowsingDataRemoverDelegateFactory::GetInstance();
  ChromeSigninClientFactory::GetInstance();
  ClientHintsFactory::GetInstance();
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  cloud_print::PrivetNotificationServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
  CloudPrintProxyServiceFactory::GetInstance();
#endif
  ConsentAuditorFactory::GetInstance();
  CookieSettingsFactory::GetInstance();
  DomainDiversityReporterFactory::GetInstance();
  dom_distiller::DomDistillerServiceFactory::GetInstance();
  DownloadCoreServiceFactory::GetInstance();
  DownloadServiceFactory::GetInstance();
#if defined(OS_ANDROID)
  explore_sites::ExploreSitesServiceFactory::GetInstance();
#endif
  FaviconServiceFactory::GetInstance();
  feature_engagement::TrackerFactory::GetInstance();
#if !defined(OS_ANDROID)
  feedback::FeedbackUploaderFactoryChrome::GetInstance();
#endif
  FindBarStateFactory::GetInstance();
  GAIAInfoUpdateServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  GlobalErrorServiceFactory::GetInstance();
#endif
  GoogleSearchDomainMixingMetricsEmitterFactory::GetInstance();
  HistoryServiceFactory::GetInstance();
  HistoryUiFaviconRequestHandlerFactory::GetInstance();
  HostContentSettingsMapFactory::GetInstance();
  HttpsEngagementServiceFactory::GetInstance();
  IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt();
  InMemoryURLIndexFactory::GetInstance();
#if !defined(OS_ANDROID)
  InstantServiceFactory::GetInstance();
#endif
  LanguageModelManagerFactory::GetInstance();
#if !defined(OS_ANDROID)
  LoginUIServiceFactory::GetInstance();
#endif
#if defined(OS_ANDROID)
  MediaDrmOriginIdManagerFactory::GetInstance();
#endif
  if (MediaEngagementService::IsEnabled())
    MediaEngagementServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  media_feeds::MediaFeedsServiceFactory::GetInstance();
  MediaGalleriesPreferencesFactory::GetInstance();
#endif
  if (base::FeatureList::IsEnabled(media::kUseMediaHistoryStore))
    media_history::MediaHistoryKeyedServiceFactory::GetInstance();
  media_router::ChromeLocalPresentationManagerFactory::GetInstance();
  media_router::ChromeMediaRouterFactory::GetInstance();
#if !defined(OS_ANDROID)
  media_router::MediaRouterUIServiceFactory::GetInstance();
#endif
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  metrics::DesktopProfileSessionDurationsServiceFactory::GetInstance();
#endif
  ModelTypeStoreServiceFactory::GetInstance();
#if defined(OS_CHROMEOS)
  NearbySharingServiceFactory::GetInstance();
#endif
  NotifierStateTrackerFactory::GetInstance();
#if !defined(OS_ANDROID)
  NTPResourceCacheFactory::GetInstance();
#endif
  PasswordStoreFactory::GetInstance();
#if !defined(OS_ANDROID)
  PinnedTabServiceFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  PluginPrefsFactory::GetInstance();
#endif
  PrefMetricsService::Factory::GetInstance();
  PrefsTabHelper::GetServiceInstance();
  policy::UserCloudPolicyInvalidatorFactory::GetInstance();
#if !defined(OS_CHROMEOS)
  policy::UserPolicySigninServiceFactory::GetInstance();
#endif
  predictors::AutocompleteActionPredictorFactory::GetInstance();
  predictors::LoadingPredictorFactory::GetInstance();
  predictors::PredictorDatabaseFactory::GetInstance();
  prerender::PrerenderLinkManagerFactory::GetInstance();
  prerender::PrerenderManagerFactory::GetInstance();
  ProfileSyncServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  ProfileThemeUpdateServiceFactory::GetInstance();
#endif
  ProtocolHandlerRegistryFactory::GetInstance();
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kReadLater))
    ReadingListModelFactory::GetInstance();
#endif
  RendererUpdaterFactory::GetInstance();
#if !defined(OS_ANDROID)
  performance_manager::SiteDataCacheFacadeFactory::GetInstance();
#endif
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance();
#endif
  SCTReportingServiceFactory::GetInstance();
#if defined(OS_ANDROID)
  SearchPermissionsService::Factory::GetInstance();
#endif
  send_tab_to_self::SendTabToSelfClientServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionServiceFactory::GetInstance();
#endif
  SharingServiceFactory::GetInstance();
  ShortcutsBackendFactory::GetInstance();
  SigninProfileAttributesUpdaterFactory::GetInstance();
  if (SiteEngagementService::IsEnabled())
    SiteEngagementServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  SigninManagerFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
  SpellcheckServiceFactory::GetInstance();
#endif
#if !defined(OS_ANDROID)
  StorageNotificationServiceFactory::GetInstance();
#endif
  suggestions::SuggestionsServiceFactory::GetInstance();
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserServiceFactory::GetInstance();
#endif
  TabRestoreServiceFactory::GetInstance();
  TabStateDBFactory::GetInstance();
  TemplateURLFetcherFactory::GetInstance();
  TemplateURLServiceFactory::GetInstance();
#if !defined(OS_ANDROID)
  ThemeServiceFactory::GetInstance();
#endif
#if defined(OS_ANDROID)
  thin_webview::android::ChromeThinWebViewInitializer::Initialize();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ToolbarActionsModelFactory::GetInstance();
#endif
  TopSitesFactory::GetInstance();
  translate::TranslateRankerFactory::GetInstance();
#if defined(OS_WIN)
  TriggeredProfileResetterFactory::GetInstance();
#endif
  UnifiedConsentServiceFactory::GetInstance();
  UrlLanguageHistogramFactory::GetInstance();
#if !defined(OS_ANDROID)
  UsbChooserContextFactory::GetInstance();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  web_app::WebAppMetricsFactory::GetInstance();
  web_app::WebAppProviderFactory::GetInstance();
#endif
  WebDataServiceFactory::GetInstance();
  webrtc_event_logging::WebRtcEventLogManagerKeyedServiceFactory::GetInstance();
}

void ChromeBrowserMainExtraPartsProfiles::PreProfileInit() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
}
