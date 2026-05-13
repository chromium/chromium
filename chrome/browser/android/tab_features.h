// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
#define CHROME_BROWSER_ANDROID_TAB_FEATURES_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/common/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

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

namespace glic {
class GlicInstanceHelper;
class GlicSidePanelCoordinator;
}  // namespace glic

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace lens {
class TabContextualizationController;
}  // namespace lens

namespace tabs {

class TabInterface;

// This class holds state that is scoped to a tab in Android. It is constructed
// after the WebContents/tab_helpers, and destroyed before.
class TabFeatures {
 public:
  TabFeatures(content::WebContents* web_contents, Profile* profile);
  ~TabFeatures();

  NewTabPagePreloadPipelineManager* new_tab_page_preload_pipeline_manager() {
    return new_tab_page_preload_pipeline_manager_.get();
  }

 private:
  // Returns the factory used to create owned components.
  static ui::UserDataFactoryWithOwner<TabInterface>& GetUserDataFactory();

  std::unique_ptr<SidePanelRegistry> tab_scoped_side_panel_registry_;
  std::unique_ptr<SidePanelTabScopedDevFeature>
      tab_scoped_side_panel_dev_feature_;

  std::unique_ptr<AskBeforeHttpDialogController>
      ask_before_http_dialog_controller_;

  std::unique_ptr<actor::ActorTabData> actor_tab_data_;

  std::unique_ptr<sync_sessions::SyncSessionsRouterTabHelper>
      sync_sessions_router_;
  std::unique_ptr<QwacWebContentsObserver> qwac_web_contents_observer_;
  std::unique_ptr<NewTabPagePreloadPipelineManager>
      new_tab_page_preload_pipeline_manager_;
  std::unique_ptr<contextual_tasks::ContextualTasksTabVisitTracker>
      contextual_tasks_tab_visit_tracker_;
  std::unique_ptr<lens::TabContextualizationController>
      tab_contextualization_controller_;

  std::unique_ptr<glic::GlicInstanceHelper> glic_instance_helper_;
  std::unique_ptr<glic::GlicSidePanelCoordinator> glic_side_panel_coordinator_;
  std::unique_ptr<actor::ui::ActorUiTabControllerInterface>
      actor_ui_tab_controller_;

  // Holds the WebUI embedding context subscription.
  base::CallbackListSubscription tab_subscription_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
