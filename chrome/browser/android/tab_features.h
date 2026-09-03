// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
#define CHROME_BROWSER_ANDROID_TAB_FEATURES_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/webui/buildflags.h"

class AskBeforeHttpDialogController;
class SidePanelTabScopedDevFeature;
class Profile;
class QwacWebContentsObserver;
class NewTabPagePreloadPipelineManager;

namespace actor {
class ActorTabData;
}  // namespace actor

namespace contextual_tasks {
class ContextualTasksTabVisitTracker;
}  // namespace contextual_tasks

namespace actor::ui {
class ActorUiTabControllerInterface;
}  // namespace actor::ui

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_data_protection {
class DataProtectionNavigationController;
}  // namespace enterprise_data_protection

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
namespace extensions {
class ExtensionSidePanelManager;
}  // namespace extensions
#endif

namespace enterprise_reporting {
class SaasUsageNavigationObserver;
}  // namespace enterprise_reporting

namespace glic {
class ContextualCueingHelper;
class GlicInstanceHelper;
class GlicSidePanelCoordinator;
}  // namespace glic

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace lens {
class TabContextualizationController;
}  // namespace lens

namespace payments {
class WebPaymentsObserver;
}  // namespace payments

class ConnectionHelpTabHelper;
class HttpAuthCacheStatus;
class SecurityStateEventObserver;

#if BUILDFLAG(ENABLE_WEBUI_NTP)
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
#endif

  std::unique_ptr<AskBeforeHttpDialogController>
      ask_before_http_dialog_controller_;

  std::unique_ptr<actor::ActorTabData> actor_tab_data_;

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
  std::unique_ptr<enterprise_reporting::SaasUsageNavigationObserver>
      saas_usage_navigation_observer_;

  std::unique_ptr<glic::ContextualCueingHelper> contextual_cueing_helper_;
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  std::unique_ptr<customize_chrome::SidePanelController>
      customize_chrome_side_panel_controller_;
#endif
  std::unique_ptr<tabs::PageContextEligibilityHelper>
      page_context_eligibility_helper_;
  std::unique_ptr<glic::GlicInstanceHelper> glic_instance_helper_;
  std::unique_ptr<glic::GlicSidePanelCoordinator> glic_side_panel_coordinator_;
  std::unique_ptr<actor::ui::ActorUiTabControllerInterface>
      actor_ui_tab_controller_;

  std::unique_ptr<payments::WebPaymentsObserver> web_payments_observer_;

  // Holds the WebUI embedding context subscription.
  base::CallbackListSubscription tab_subscription_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
