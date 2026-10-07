// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
#define CHROME_BROWSER_ANDROID_TAB_FEATURES_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/common/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/webui/buildflags.h"

class AskBeforeHttpDialogController;
class SidePanelTabScopedDevFeature;
class Profile;
class QwacWebContentsObserver;
class NewTabPagePreloadPipelineManager;

namespace actor {
class ActorSurfaceTabHelper;
class ActorTabData;
}  // namespace actor

namespace contextual_tasks {
class ContextualTasksTabVisitTracker;
}  // namespace contextual_tasks

namespace actor::ui {
class ActorUiTabControllerInterface;
}  // namespace actor::ui

namespace blocked_content {
class PopupOpenerTabHelper;
}  // namespace blocked_content

namespace chrome_browser_net {
class NetErrorTabHelper;
}  // namespace chrome_browser_net

namespace client_hints {
class ClientHintsWebContentsObserver;
}  // namespace client_hints

namespace commerce {
class CommerceTabHelper;
}  // namespace commerce

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadNavigationObserver;
}  // namespace download

namespace enterprise_data_protection {
class DataProtectionNavigationController;
}  // namespace enterprise_data_protection

namespace enterprise_net {
class EnterpriseProxyTabHelper;
}  // namespace enterprise_net

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
namespace extensions {
class ExtensionSidePanelManager;
class NavigationExtensionEnabler;
}  // namespace extensions
#endif

namespace enterprise_reporting {
class SaasUsageNavigationObserver;
}  // namespace enterprise_reporting

namespace finds {
class FindsTabHelper;
}  // namespace finds

namespace glic {
class ContextualCueingHelper;
class GlicInstanceHelper;
class GlicMarketingPageTabHelper;
class GlicSidePanelCoordinator;
}  // namespace glic

namespace history {
class WebContentsTopSitesObserver;
}  // namespace history

namespace offline_pages {
class AutoFetchNavigationObserver;
class OfflinePageTabHelper;
class RecentTabHelper;
}  // namespace offline_pages

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace lens {
class TabContextualizationController;
}  // namespace lens

namespace metrics {
class MetricsServicesWebContentsObserver;
}  // namespace metrics

namespace page_content_annotations {
class PageContentAnnotationsWebContentsObserver;
}  // namespace page_content_annotations

namespace payments {
class WebPaymentsObserver;
}  // namespace payments

namespace prerender {
class NoStatePrefetchTabHelper;
}  // namespace prerender

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
namespace safe_browsing {
class SafeBrowsingTabObserver;
class TailoredSecurityUrlObserver;
class TriggerCreator;
}  // namespace safe_browsing
#endif

namespace site_protection {
class SiteProtectionMetricsObserver;
}  // namespace site_protection

namespace tasks {
class TaskTabHelper;
}  // namespace tasks

namespace v8_compile_hints {
class V8CompileHintsTabHelper;
}  // namespace v8_compile_hints

namespace vr {
class VrTabHelper;
}  // namespace vr

namespace webapps {
class AppBannerManagerAndroid;
class MLInstallabilityPromoter;
}  // namespace webapps

class AboutThisSiteTabHelper;
class AutoPictureInPictureTabHelper;
class ChainedBackNavigationTracker;
class ChromeFacilitatedPaymentsClient;
class ConnectionHelpTabHelper;
class CoreTabHelper;
class ExternalProtocolObserver;
class FileSystemAccessTabHelper;
class FromGWSNavigationAndKeepAliveRequestObserver;
class HistoryEmbeddingsTabHelper;
class HttpAuthCacheStatus;
class MediaStateObserver;
class MixedContentSettingsTabHelper;
class NavigationMetricsRecorder;
class NavigationPredictorPreconnectClient;
class OomInterventionTabHelper;
class PluginObserverAndroid;
class PolicyAuditorBridge;
class RecentlyAudibleHelper;
class RevokedPermissionsTabHelper;
class SearchEngineTabHelper;
class SecurityStateEventObserver;
class SoundContentSettingObserver;
class StorageAccessAPITabHelper;
class SupervisedUserNavigationObserver;
class TabContextDecryptionTokenTabHelper;

#if BUILDFLAG(ENABLE_WEBUI_NTP)
class SearchTabHelper;
namespace customize_chrome {
class SidePanelController;
}  // namespace customize_chrome
#endif

namespace tabs {

class TabInterface;
class PageContextEligibilityHelper;

// This class holds state that is scoped to a tab in Android. It is constructed
// after the WebContents/tab_helpers, and destroyed before.
class TabFeatures {
 public:
  TabFeatures(content::WebContents* web_contents, Profile* profile);
  ~TabFeatures();

  NewTabPagePreloadPipelineManager* new_tab_page_preload_pipeline_manager() {
    return new_tab_page_preload_pipeline_manager_.get();
  }

  enterprise_data_protection::DataProtectionNavigationController*
  data_protection_controller() {
    return data_protection_tab_controller_.get();
  }

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  customize_chrome::SidePanelController*
  customize_chrome_side_panel_controller() {
    return customize_chrome_side_panel_controller_.get();
  }

  customize_chrome::SidePanelController*
  SetCustomizeChromeSidePanelControllerForTesting(
      std::unique_ptr<customize_chrome::SidePanelController>
          customize_chrome_side_panel_controller);
#endif

 private:
  // Returns the factory used to create owned components.
  static ui::UserDataFactoryWithOwner<TabInterface>& GetUserDataFactory();

  std::unique_ptr<SidePanelRegistry> tab_scoped_side_panel_registry_;
  std::unique_ptr<SidePanelTabScopedDevFeature>
      tab_scoped_side_panel_dev_feature_;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;
  std::unique_ptr<extensions::NavigationExtensionEnabler>
      navigation_extension_enabler_;
#endif

  std::unique_ptr<AskBeforeHttpDialogController>
      ask_before_http_dialog_controller_;

  std::unique_ptr<actor::ActorTabData> actor_tab_data_;
  std::unique_ptr<actor::ActorSurfaceTabHelper> actor_surface_tab_helper_;

  std::unique_ptr<sync_sessions::SyncSessionsRouterTabHelper>
      sync_sessions_router_;
  std::unique_ptr<ConnectionHelpTabHelper> connection_help_tab_helper_;
  std::unique_ptr<HttpAuthCacheStatus> http_auth_cache_status_;
  std::unique_ptr<SecurityStateEventObserver> security_state_event_observer_;
  std::unique_ptr<QwacWebContentsObserver> qwac_web_contents_observer_;
  std::unique_ptr<NewTabPagePreloadPipelineManager>
      new_tab_page_preload_pipeline_manager_;
  std::unique_ptr<contextual_tasks::ContextualTasksTabVisitTracker>
      contextual_tasks_tab_visit_tracker_;
  std::unique_ptr<lens::TabContextualizationController>
      tab_contextualization_controller_;

  std::unique_ptr<
      enterprise_data_protection::DataProtectionNavigationController>
      data_protection_tab_controller_;
  std::unique_ptr<enterprise_net::EnterpriseProxyTabHelper>
      enterprise_proxy_tab_helper_;
  std::unique_ptr<enterprise_reporting::SaasUsageNavigationObserver>
      saas_usage_navigation_observer_;

  std::unique_ptr<glic::ContextualCueingHelper> contextual_cueing_helper_;
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  std::unique_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;
  std::unique_ptr<SearchTabHelper> search_tab_helper_;
#endif
  std::unique_ptr<tabs::PageContextEligibilityHelper>
      page_context_eligibility_helper_;
  std::unique_ptr<glic::GlicInstanceHelper> glic_instance_helper_;
  std::unique_ptr<glic::GlicMarketingPageTabHelper>
      glic_marketing_page_tab_helper_;
  std::unique_ptr<glic::GlicSidePanelCoordinator> glic_side_panel_coordinator_;
  std::unique_ptr<actor::ui::ActorUiTabControllerInterface>
      actor_ui_tab_controller_;

  std::unique_ptr<payments::WebPaymentsObserver> web_payments_observer_;
  std::unique_ptr<TabContextDecryptionTokenTabHelper>
      tab_context_decryption_token_tab_helper_;
  std::unique_ptr<v8_compile_hints::V8CompileHintsTabHelper>
      v8_compile_hints_tab_helper_;
  std::unique_ptr<StorageAccessAPITabHelper> storage_access_api_tab_helper_;
  std::unique_ptr<RevokedPermissionsTabHelper> revoked_permissions_tab_helper_;
  std::unique_ptr<ExternalProtocolObserver> external_protocol_observer_;
  std::unique_ptr<prerender::NoStatePrefetchTabHelper>
      no_state_prefetch_tab_helper_;
  std::unique_ptr<NavigationPredictorPreconnectClient>
      navigation_predictor_preconnect_client_;
  std::unique_ptr<NavigationMetricsRecorder> navigation_metrics_recorder_;
  std::unique_ptr<site_protection::SiteProtectionMetricsObserver>
      site_protection_metrics_observer_;
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  std::unique_ptr<safe_browsing::SafeBrowsingTabObserver>
      safe_browsing_tab_observer_;
  std::unique_ptr<safe_browsing::TailoredSecurityUrlObserver>
      tailored_security_url_observer_;
  std::unique_ptr<safe_browsing::TriggerCreator> trigger_creator_;
#endif
  std::unique_ptr<OomInterventionTabHelper> oom_intervention_tab_helper_;
  std::unique_ptr<PolicyAuditorBridge> policy_auditor_bridge_;
  std::unique_ptr<finds::FindsTabHelper> finds_tab_helper_;
  std::unique_ptr<FromGWSNavigationAndKeepAliveRequestObserver>
      from_gws_navigation_and_keep_alive_request_observer_;
  std::unique_ptr<offline_pages::AutoFetchNavigationObserver>
      auto_fetch_navigation_observer_;
  std::unique_ptr<PluginObserverAndroid> plugin_observer_android_;
  std::unique_ptr<AboutThisSiteTabHelper> about_this_site_tab_helper_;
  std::unique_ptr<RecentlyAudibleHelper> recently_audible_helper_;
  std::unique_ptr<SoundContentSettingObserver> sound_content_setting_observer_;
  std::unique_ptr<tasks::TaskTabHelper> task_tab_helper_;
  std::unique_ptr<HistoryEmbeddingsTabHelper> history_embeddings_tab_helper_;
  std::unique_ptr<download::DownloadNavigationObserver>
      download_navigation_observer_;
  std::unique_ptr<history::WebContentsTopSitesObserver>
      web_contents_top_sites_observer_;
  std::unique_ptr<client_hints::ClientHintsWebContentsObserver>
      client_hints_web_contents_observer_;
  std::unique_ptr<ChainedBackNavigationTracker>
      chained_back_navigation_tracker_;
  std::unique_ptr<MediaStateObserver> media_state_observer_;
  std::unique_ptr<FileSystemAccessTabHelper> file_system_access_tab_helper_;
  std::unique_ptr<SearchEngineTabHelper> search_engine_tab_helper_;
  std::unique_ptr<chrome_browser_net::NetErrorTabHelper> net_error_tab_helper_;
  std::unique_ptr<SupervisedUserNavigationObserver>
      supervised_user_navigation_observer_;
  std::unique_ptr<offline_pages::OfflinePageTabHelper> offline_page_tab_helper_;
  std::unique_ptr<offline_pages::RecentTabHelper> recent_tab_helper_;
  std::unique_ptr<AutoPictureInPictureTabHelper>
      auto_picture_in_picture_tab_helper_;
  std::unique_ptr<webapps::AppBannerManagerAndroid> app_banner_manager_;
  std::unique_ptr<webapps::MLInstallabilityPromoter>
      ml_installability_promoter_;
  std::unique_ptr<ChromeFacilitatedPaymentsClient>
      chrome_facilitated_payments_client_;
  std::unique_ptr<commerce::CommerceTabHelper> commerce_tab_helper_;
  std::unique_ptr<metrics::MetricsServicesWebContentsObserver>
      metrics_services_web_contents_observer_;
  std::unique_ptr<blocked_content::PopupOpenerTabHelper>
      popup_opener_tab_helper_;
  std::unique_ptr<
      page_content_annotations::PageContentAnnotationsWebContentsObserver>
      page_content_annotations_web_contents_observer_;
  std::unique_ptr<CoreTabHelper> core_tab_helper_;
  std::unique_ptr<vr::VrTabHelper> vr_tab_helper_;
  std::unique_ptr<MixedContentSettingsTabHelper>
      mixed_content_settings_tab_helper_;

  // Holds the WebUI embedding context subscription.
  base::CallbackListSubscription tab_subscription_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
