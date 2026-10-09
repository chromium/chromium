// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_features.h"

#include <memory>

#include "base/android/android_info.h"
#include "base/check_is_test.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_tab_helper.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/android/ui/actor_ui_tab_controller_android.h"
#include "chrome/browser/android/media_state_observer.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"
#include "chrome/browser/android/persisted_tab_data/language_persisted_tab_data_android.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/android/policy/policy_auditor_bridge.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/banners/android/chrome_app_banner_manager_android.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_tab_helper.h"
#include "chrome/browser/finds/finds_service_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/glic/glic_marketing_page_tab_helper.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_desktop_android.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/recent_tab_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/payments/web_payments_observer.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/plugins/plugin_observer_android.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor_tab_helper.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/site_protection/site_protection_metrics_observer.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/security_state_event_observer.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync_tab_context/tab_context_decryption_token_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "chrome/browser/ui/side_panel/internal/android/dev/side_panel_tab_scoped_dev_feature.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/actor/core/actor_features.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/popup_opener_tab_helper.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/client_hints/browser/client_hints_web_contents_observer.h"
#include "components/commerce/content/browser/commerce_tab_helper.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/enterprise/data_protection/features.h"
#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/history/content/browser/web_contents_top_sites_observer.h"
#include "components/history/core/browser/top_sites.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/metrics/content/metrics_services_web_contents_observer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/page_content_annotations/content/page_content_annotations_web_contents_observer.h"
#include "components/payments/core/features.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/security_interstitials/core/features.h"
#include "components/sensitive_content/android/android_sensitive_content_client.h"
#include "components/sensitive_content/features.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/customize_chrome/side_panel_controller_android.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#endif

#include "printing/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_tab_observer_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"
#include "chrome/browser/safe_browsing/trigger_creator.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/safe_browsing/core/common/features.h"
#endif

namespace tabs {

namespace {

// The data protection controller drives all per-navigation enterprise data
// protection work on Android: screenshot restrictions and tab title reporting
// for URL filtering events. It is only useful for managed profiles, and only
// when at least one of the features it powers is enabled.
bool ShouldCreateDataProtectionController(Profile* profile) {
  if (!enterprise_util::IsBrowserManaged(profile)) {
    return false;
  }
  return base::FeatureList::IsEnabled(
             enterprise_data_protection::
                 kEnableAndroidEnterpriseScreenshotProtection) ||
         base::FeatureList::IsEnabled(
             enterprise_data_protection::kEnterpriseTabTitleReporting);
}

}  // namespace

TabFeatures::TabFeatures(content::WebContents* web_contents, Profile* profile) {
  TabInterface* const tab = TabInterface::GetFromContents(web_contents);
  CHECK(tab);
  tab_subscription_ = webui::InitEmbeddingContext(tab);

  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          web_contents,
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(web_contents),
          favicon::ContentFaviconDriver::FromWebContents(web_contents));

  http_auth_cache_status_ = std::make_unique<HttpAuthCacheStatus>(web_contents);

  security_state_event_observer_ =
      std::make_unique<SecurityStateEventObserver>(web_contents);

  connection_help_tab_helper_ =
      GetUserDataFactory().CreateInstance<ConnectionHelpTabHelper>(
          *tab, *tab, web_contents);

  if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
    qwac_web_contents_observer_ =
        std::make_unique<QwacWebContentsObserver>(web_contents);
  }

  new_tab_page_preload_pipeline_manager_ =
      std::make_unique<NewTabPagePreloadPipelineManager>(web_contents);

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi)) {
    ask_before_http_dialog_controller_ =
        GetUserDataFactory().CreateInstance<AskBeforeHttpDialogController>(*tab,
                                                                           tab);
  }

  tab_scoped_side_panel_registry_ =
      AndroidSidePanelEnabledFn::IsEnabled()
          ? std::make_unique<SidePanelRegistry>(tab)
          : nullptr;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extensions::GetViewType(web_contents) ==
      extensions::mojom::ViewType::kInvalid) {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kTabContents);
  }
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
  extensions::TabHelper::CreateForWebContents(web_contents);
  if (tab_scoped_side_panel_registry_) {
    extension_side_panel_manager_ =
        std::make_unique<extensions::ExtensionSidePanelManager>(
            profile, tab, tab_scoped_side_panel_registry_.get());
  }
  navigation_extension_enabler_ =
      std::make_unique<extensions::NavigationExtensionEnabler>(web_contents);
#endif

  if (tab_scoped_side_panel_registry_ &&
      base::FeatureList::IsEnabled(
          chrome::android::kEnableAndroidSidePanelDevFeature)) {
    std::string scope = base::GetFieldTrialParamValueByFeature(
        chrome::android::kEnableAndroidSidePanelDevFeature, "scope");
    if (scope == "tab") {
      tab_scoped_side_panel_dev_feature_ =
          std::make_unique<SidePanelTabScopedDevFeature>(
              tab, tab_scoped_side_panel_registry_.get());
    }
  }

  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    actor_tab_data_ =
        GetUserDataFactory().CreateInstance<actor::ActorTabData>(*tab, tab);
    actor_surface_tab_helper_ =
        GetUserDataFactory().CreateInstance<actor::ActorSurfaceTabHelper>(*tab,
                                                                          *tab);
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile);
  if (glic::GlicEnabling::IsProfileEligible(profile) && actor_service) {
    actor_ui_tab_controller_ =
        GetUserDataFactory()
            .CreateInstance<actor::ui::ActorUiTabControllerAndroid>(
                *tab, *tab, actor_service);
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasksContext)) {
    contextual_tasks_tab_visit_tracker_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::ContextualTasksTabVisitTracker>(
                *tab, *tab);
  }

  tab_contextualization_controller_ =
      GetUserDataFactory().CreateInstance<lens::TabContextualizationController>(
          *tab, tab);

  if (ShouldCreateDataProtectionController(profile)) {
    data_protection_tab_controller_ = std::make_unique<
        enterprise_data_protection::DataProtectionNavigationController>(tab);
  }

  enterprise_proxy_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<enterprise_net::EnterpriseProxyTabHelper>(
              *tab, *tab, web_contents,
              EnterpriseProxyErrorServiceFactory::GetForProfile(profile),
              std::make_unique<
                  enterprise_net::EnterpriseProxyTabHelperDelegate>());

  glic_instance_helper_ =
      GetUserDataFactory().CreateInstance<glic::GlicInstanceHelper>(*tab, tab);

  page_context_eligibility_helper_ =
      GetUserDataFactory().CreateInstance<tabs::PageContextEligibilityHelper>(
          *tab, *tab);

  bool use_glic_side_panel = false;
  if (base::FeatureList::IsEnabled(features::kGlicAndroidSidePanel) &&
      AndroidSidePanelEnabledFn::IsEnabled()) {
    const ui::DeviceFormFactor form_factor = ui::GetDeviceFormFactor();
    use_glic_side_panel =
        form_factor == ui::DEVICE_FORM_FACTOR_DESKTOP ||
        (form_factor == ui::DEVICE_FORM_FACTOR_TABLET &&
         base::FeatureList::IsEnabled(features::kGlicAndroidTablet));
  }
  if (use_glic_side_panel) {
    glic_side_panel_coordinator_ =
        GetUserDataFactory()
            .CreateInstance<glic::GlicSidePanelCoordinatorDesktopAndroid>(
                *tab, tab, tab_scoped_side_panel_registry_.get(), profile);
  } else {
    glic_side_panel_coordinator_ =
        GetUserDataFactory()
            .CreateInstance<glic::GlicSidePanelCoordinatorAndroid>(*tab, tab);
  }

  contextual_cueing_helper_ = glic::ContextualCueingHelper::MaybeCreate(tab);

  if (base::FeatureList::IsEnabled(features::kGlicMarketingAutoOpen)) {
    glic_marketing_page_tab_helper_ =
        std::make_unique<glic::GlicMarketingPageTabHelper>(web_contents);
  }

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  if (base::FeatureList::IsEnabled(ntp_features::kNtpCustomizeWebUiAndroid)) {
    customize_chrome_side_panel_controller_ =
        std::make_unique<customize_chrome::SidePanelControllerAndroid>(*tab);
  }
  if (search::IsInstantExtendedAPIEnabled()) {
    search_tab_helper_ = GetUserDataFactory().CreateInstance<SearchTabHelper>(
        *tab, *tab, web_contents);
  }
#endif

  if (base::FeatureList::IsEnabled(enterprise_reporting::kSaasUsageReporting)) {
    saas_usage_navigation_observer_ =
        std::make_unique<enterprise_reporting::SaasUsageNavigationObserver>(
            web_contents);
  }

  if (base::FeatureList::IsEnabled(
          payments::features::kThreeDSecureTelemetry)) {
    web_payments_observer_ =
        std::make_unique<payments::WebPaymentsObserver>(web_contents);
  }

  tab_context_decryption_token_tab_helper_ =
      TabContextDecryptionTokenTabHelper::MaybeCreate(web_contents);

  v8_compile_hints_tab_helper_ =
      v8_compile_hints::V8CompileHintsTabHelper::MaybeCreate(web_contents);

  storage_access_api_tab_helper_ = std::make_unique<StorageAccessAPITabHelper>(
      web_contents,
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile));

  if (auto* service =
          RevokedPermissionsServiceFactory::GetForProfile(profile)) {
    revoked_permissions_tab_helper_ =
        std::make_unique<RevokedPermissionsTabHelper>(web_contents, service);
  }

  external_protocol_observer_ =
      std::make_unique<ExternalProtocolObserver>(web_contents);

  no_state_prefetch_tab_helper_ =
      std::make_unique<prerender::NoStatePrefetchTabHelper>(web_contents);

  navigation_predictor_preconnect_client_ =
      std::make_unique<NavigationPredictorPreconnectClient>(web_contents);

  navigation_metrics_recorder_ =
      std::make_unique<NavigationMetricsRecorder>(web_contents);

  site_protection_metrics_observer_ =
      std::make_unique<site_protection::SiteProtectionMetricsObserver>(
          web_contents);

  permissions::PermissionRequestManager::CreateForWebContents(web_contents);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service(),
      enterprise_connectors::IsReferrerChainNeededForEnterprise(profile));
  if (autofill::ContentAutofillClient::FromWebContents(web_contents)) {
    // Attach password reuse detection client when Autofill is present.
    ChromePasswordReuseDetectionManagerClient::CreateForWebContents(
        web_contents);
    safe_browsing_tab_observer_ =
        GetUserDataFactory()
            .CreateInstance<safe_browsing::SafeBrowsingTabObserver>(
                *tab, *tab, web_contents,
                std::make_unique<
                    safe_browsing::ChromeSafeBrowsingTabObserverDelegate>());
  }
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityIntegration)) {
    tailored_security_url_observer_ =
        std::make_unique<safe_browsing::TailoredSecurityUrlObserver>(
            web_contents,
            safe_browsing::TailoredSecurityServiceFactory::GetForProfile(
                profile));
  }
  // Create Safe Browsing triggers for the tab.
  trigger_creator_ = std::make_unique<safe_browsing::TriggerCreator>(
      *tab, profile, web_contents);
#endif

  if (OomInterventionTabHelper::IsEnabled()) {
    oom_intervention_tab_helper_ =
        std::make_unique<OomInterventionTabHelper>(web_contents);
  }

  policy_auditor_bridge_ =
      PolicyAuditorBridge::MaybeCreateForWebContents(web_contents);

  if (base::FeatureList::IsEnabled(finds::features::kChromeFinds)) {
    if (auto* finds_service =
            finds::FindsServiceFactory::GetForProfile(profile)) {
      finds_tab_helper_ = std::make_unique<finds::FindsTabHelper>(
          web_contents, finds_service,
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
          TemplateURLServiceFactory::GetForProfile(profile),
          profile->GetPrefs());
    }
  }

  from_gws_navigation_and_keep_alive_request_observer_ =
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreate(web_contents);

  auto_fetch_navigation_observer_ =
      offline_pages::AutoFetchPageLoadWatcher::MaybeCreateNavigationObserver(
          web_contents);

  plugin_observer_android_ =
      GetUserDataFactory().CreateInstance<PluginObserverAndroid>(*tab, *tab,
                                                                 web_contents);

  if (page_info::IsAboutThisSiteFeatureEnabled()) {
    if (auto* optimization_guide_decider =
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
      about_this_site_tab_helper_ =
          GetUserDataFactory().CreateInstance<AboutThisSiteTabHelper>(
              *tab, *tab, web_contents, optimization_guide_decider);
    }
  }

  recently_audible_helper_ =
      GetUserDataFactory().CreateInstance<RecentlyAudibleHelper>(*tab, *tab,
                                                                 web_contents);

  sound_content_setting_observer_ =
      GetUserDataFactory().CreateInstance<SoundContentSettingObserver>(
          *tab, *tab, web_contents);

  task_tab_helper_ = GetUserDataFactory().CreateInstance<tasks::TaskTabHelper>(
      *tab, *tab, web_contents);

  if (!profile->IsOffTheRecord() &&
      HistoryEmbeddingsServiceFactory::GetForProfile(profile)) {
    history_embeddings_tab_helper_ =
        std::make_unique<HistoryEmbeddingsTabHelper>(web_contents);
  }

  download_navigation_observer_ =
      std::make_unique<download::DownloadNavigationObserver>(
          web_contents, download::NavigationMonitorFactory::GetForKey(
                            profile->GetProfileKey()));

  web_contents_top_sites_observer_ =
      std::make_unique<history::WebContentsTopSitesObserver>(
          web_contents, TopSitesFactory::GetForProfile(profile).get());

  client_hints_web_contents_observer_ =
      std::make_unique<client_hints::ClientHintsWebContentsObserver>(
          web_contents);

  chained_back_navigation_tracker_ =
      GetUserDataFactory().CreateInstance<ChainedBackNavigationTracker>(
          *tab, *tab, web_contents);

  task_manager::WebContentsTags::CreateForTabContents(web_contents);

  media_state_observer_ = std::make_unique<MediaStateObserver>(web_contents);

  // Attach FileSystemAccessPermissionRequestManager to the tab.
  FileSystemAccessPermissionRequestManager::CreateForWebContents(web_contents);

  file_system_access_tab_helper_ =
      std::make_unique<FileSystemAccessTabHelper>(web_contents);

  search_engine_tab_helper_ =
      GetUserDataFactory().CreateInstance<SearchEngineTabHelper>(*tab, *tab,
                                                                 web_contents);

  net_error_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<chrome_browser_net::NetErrorTabHelper>(*tab, *tab,
                                                                 web_contents);

  if (!profile->IsOffTheRecord()) {
    supervised_user_navigation_observer_ =
        GetUserDataFactory().CreateInstance<SupervisedUserNavigationObserver>(
            *tab, *tab, web_contents);
  }

  offline_page_tab_helper_ =
      GetUserDataFactory().CreateInstance<offline_pages::OfflinePageTabHelper>(
          *tab, *tab, web_contents);

  recent_tab_helper_ =
      GetUserDataFactory().CreateInstance<offline_pages::RecentTabHelper>(
          *tab, *tab, web_contents);

  if (base::FeatureList::IsEnabled(media::kAutoPictureInPictureAndroid)) {
    auto_picture_in_picture_tab_helper_ =
        GetUserDataFactory().CreateInstance<AutoPictureInPictureTabHelper>(
            *tab, *tab, web_contents);
  }

  if (webapps::InstallableManager::FromWebContents(web_contents)) {
    // Create AppBannerManagerAndroid; remove trace after
    // https://crbug.com/41426655
    TRACE_EVENT0("browser", "AppBannerManagerAndroid::AppBannerManagerAndroid");
    app_banner_manager_ =
        GetUserDataFactory().CreateInstance<webapps::AppBannerManagerAndroid>(
            *tab, *tab, web_contents,
            std::make_unique<webapps::ChromeAppBannerManagerAndroid>(
                *web_contents));
  } else {
    // Unit tests using `TabAndroid::CreateForTesting` construct `TabFeatures`
    // without running `TabHelpers::AttachTabHelpers`.
    CHECK_IS_TEST();
  }

  ml_installability_promoter_ =
      GetUserDataFactory().CreateInstance<webapps::MLInstallabilityPromoter>(
          *tab, *tab, web_contents);

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (autofill::ContentAutofillClient::FromWebContents(web_contents) &&
      optimization_guide_decider) {
    chrome_facilitated_payments_client_ =
        GetUserDataFactory().CreateInstance<ChromeFacilitatedPaymentsClient>(
            *tab, *tab, web_contents, optimization_guide_decider,
            base::BindRepeating([](content::WebContents* web_contents) {
              auto* tab_android = TabAndroid::FromWebContents(web_contents);
              return tab_android && tab_android->IsCustomTab();
            }));
  }

  commerce_tab_helper_ = std::make_unique<commerce::CommerceTabHelper>(
      web_contents, profile->IsOffTheRecord(),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);

  if (auto* metrics_services_manager =
          g_browser_process->GetMetricsServicesManager()) {
    metrics_services_web_contents_observer_ =
        std::make_unique<metrics::MetricsServicesWebContentsObserver>(
            web_contents, metrics_services_manager->GetOnDidStartLoadingCb(),
            metrics_services_manager->GetOnDidStopLoadingCb(),
            metrics_services_manager->GetOnRendererUnresponsiveCb());
  }

  popup_opener_tab_helper_ =
      GetUserDataFactory()
          .CreateInstance<blocked_content::PopupOpenerTabHelper>(
              *tab, *tab, web_contents, base::DefaultTickClock::GetInstance(),
              HostContentSettingsMapFactory::GetForProfile(profile));

  if (auto* page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile)) {
    page_content_annotations_web_contents_observer_ =
        GetUserDataFactory()
            .CreateInstance<page_content_annotations::
                                PageContentAnnotationsWebContentsObserver>(
                *tab, *tab, web_contents, *page_content_annotations_service);
  }

  core_tab_helper_ = GetUserDataFactory().CreateInstance<CoreTabHelper>(
      *tab, *tab, web_contents);

  vr_tab_helper_ =
      GetUserDataFactory().CreateInstance<vr::VrTabHelper>(*tab, *tab);

  // Configure find bar state for the tab.
  FindBarState::ConfigureWebContents(web_contents);

  // Attach MediaEngagementService observer when enabled.
  if (MediaEngagementService::IsEnabled()) {
    MediaEngagementService::CreateWebContentsObserver(web_contents);
  }

#if BUILDFLAG(ENABLE_PRINTING)
  // Initialize printing helpers for the tab.
  printing::InitializePrintingForWebContents(web_contents);
#endif

  if (autofill::ContentAutofillClient::FromWebContents(web_contents) &&
      base::android::android_info::sdk_int() >=
          base::android::android_info::SdkVersion::SDK_VERSION_V &&
      base::FeatureList::IsEnabled(
          sensitive_content::features::kSensitiveContent)) {
    // Attach AndroidSensitiveContentClient on Android V+.
    sensitive_content::AndroidSensitiveContentClient::CreateForWebContents(
        web_contents, "SensitiveContent.Chrome.");
  }

  // Attach MixedContentSettingsTabHelper to the tab.
  mixed_content_settings_tab_helper_ =
      GetUserDataFactory().CreateInstance<MixedContentSettingsTabHelper>(
          *tab, *tab, web_contents);

  // Track permission recovery success rate for the tab.
  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      web_contents);

  ContextMenuHelper::CreateForWebContents(web_contents);

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          web_contents));

  // Attach TrustedVaultEncryptionKeysTabHelper to the tab.
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(web_contents);

  // Track one-time permissions for the tab.
  one_time_permissions_tracker_helper_ =
      std::make_unique<OneTimePermissionsTrackerHelper>(web_contents);

  if (predictors::LoadingPredictorFactory::GetForProfile(profile)) {
    loading_predictor_tab_helper_ =
        GetUserDataFactory()
            .CreateInstance<predictors::LoadingPredictorTabHelper>(
                *tab, *tab, web_contents);
  }

  if (breadcrumbs::IsEnabled(g_browser_process->local_state()) &&
      infobars::ContentInfoBarManager::FromWebContents(web_contents)) {
    BreadcrumbManagerTabHelper::CreateForWebContents(web_contents);
  }

  PrefsTabHelper::CreateForWebContents(web_contents);

  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement::SiteEngagementService::Helper::CreateForWebContents(
        web_contents,
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile));
  }

  SafetyTipWebContentsObserver::CreateForWebContents(web_contents);

  HttpsOnlyModeTabHelper::CreateForWebContents(web_contents);

  login_detection::LoginDetectionTabHelper::MaybeCreateForWebContents(
      web_contents);

  if (!profile->IsOffTheRecord()) {
    HistoryClustersTabHelper::CreateForWebContents(
        web_contents, HistoryTabHelper::FromWebContents(web_contents));
  }

  blocked_content::PopupBlockerTabHelper::CreateForWebContents(web_contents);

  RequestDesktopSiteWebContentsObserverAndroid::CreateForWebContents(
      web_contents);

  resource_coordinator::ResourceCoordinatorTabHelper::CreateForWebContents(
      web_contents);

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents);

  // Register LanguagePersistedTabDataAndroid for non-incognito Android tabs to
  // persist language details.
  if (!profile->IsOffTheRecord() &&
      content_capture::features::ShouldSendMetadataForDataShare()) {
    if (auto* tab_android = TabAndroid::FromWebContents(web_contents);
        tab_android) {
      LanguagePersistedTabDataAndroid::From(
          tab_android,
          base::BindOnce(
              [](base::WeakPtr<content::WebContents> web_contents,
                 PersistedTabDataAndroid* persisted_tab_data) {
                if (!web_contents) {
                  return;
                }
                ChromeTranslateClient* chrome_translate_client =
                    ChromeTranslateClient::FromWebContents(web_contents.get());

                if (!chrome_translate_client) {
                  return;
                }

                auto* language_persisted_tab_data_android =
                    static_cast<LanguagePersistedTabDataAndroid*>(
                        persisted_tab_data);
                language_persisted_tab_data_android->RegisterTranslateDriver(
                    chrome_translate_client->translate_driver());
              },
              web_contents->GetWeakPtr()));
    }
  }

  // If enabled, save sensitivity data for each non-incognito Android tab.
  // TODO(crbug.com/40276584): Consider moving check conditions or the
  // registration logic to sensitivity_persisted_tab_data_android.*
  if (!profile->IsOffTheRecord()) {
    if (auto* page_content_annotations_service =
            PageContentAnnotationsServiceFactory::GetForProfile(profile)) {
      if (auto* tab_android = TabAndroid::FromWebContents(web_contents);
          tab_android) {
        SensitivityPersistedTabDataAndroid::From(
            tab_android,
            base::BindOnce(
                [](page_content_annotations::PageContentAnnotationsService*
                       page_content_annotations_service,
                   PersistedTabDataAndroid* persisted_tab_data) {
                  auto* sensitivity_persisted_tab_data_android =
                      static_cast<SensitivityPersistedTabDataAndroid*>(
                          persisted_tab_data);
                  sensitivity_persisted_tab_data_android->RegisterPCAService(
                      page_content_annotations_service);
                },
                page_content_annotations_service));
      }
    }
  }
}

TabFeatures::~TabFeatures() = default;

#if BUILDFLAG(ENABLE_WEBUI_NTP)
customize_chrome::SidePanelController*
TabFeatures::SetCustomizeChromeSidePanelControllerForTesting(  // IN-TEST
    std::unique_ptr<customize_chrome::SidePanelController>
        customize_chrome_side_panel_controller) {
  customize_chrome_side_panel_controller_ =
      std::move(customize_chrome_side_panel_controller);
  return customize_chrome_side_panel_controller_.get();
}
#endif

// static
ui::UserDataFactoryWithOwner<TabInterface>& TabFeatures::GetUserDataFactory() {
  static base::NoDestructor<ui::UserDataFactoryWithOwner<TabInterface>> factory;
  return *factory;
}

}  // namespace tabs
