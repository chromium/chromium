// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_features.h"

#include <memory>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/android/ui/actor_ui_tab_controller_android.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"
#include "chrome/browser/android/policy/policy_auditor_bridge.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
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
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/net/http_auth_cache_status.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/offline_pages/android/auto_fetch_page_load_watcher.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_info/about_this_site_tab_helper.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/payments/web_payments_observer.h"
#include "chrome/browser/plugins/plugin_observer_android.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/site_protection/site_protection_metrics_observer.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/ssl/connection_help_tab_helper.h"
#include "chrome/browser/ssl/security_state_event_observer.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/sync_tab_context/tab_context_decryption_token_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "chrome/browser/ui/side_panel/internal/android/dev/side_panel_tab_scoped_dev_feature.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/enterprise/data_protection/features.h"
#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/payments/core/features.h"
#include "components/search/ntp_features.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/features.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/customize_chrome/side_panel_controller_android.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"
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

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityIntegration)) {
    tailored_security_url_observer_ =
        std::make_unique<safe_browsing::TailoredSecurityUrlObserver>(
            web_contents,
            safe_browsing::TailoredSecurityServiceFactory::GetForProfile(
                profile));
  }
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
