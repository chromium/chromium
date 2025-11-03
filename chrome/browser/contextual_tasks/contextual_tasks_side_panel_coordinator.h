// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelCoordinator;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace base {
class Uuid;
}

namespace views {
class View;
class WebView;
}  // namespace views

namespace contextual_tasks {

class ContextualTasksContextController;
class ContextualTasksWebView;

class ContextualTasksSidePanelCoordinator {
 public:
  DECLARE_USER_DATA(ContextualTasksSidePanelCoordinator);

  ContextualTasksSidePanelCoordinator(
      BrowserWindowInterface* browser_window,
      SidePanelCoordinator* side_panel_coordinator);
  ContextualTasksSidePanelCoordinator(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ContextualTasksSidePanelCoordinator& operator=(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ~ContextualTasksSidePanelCoordinator();

  static ContextualTasksSidePanelCoordinator* From(
      BrowserWindowInterface* window);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // Show the side panel.
  void Show();

  // Close the side panel.
  void Close();

  // Check if the side panel is currently showing
  bool IsSidePanelOpen();

  // Check if the side panel is currently opening for ContextualTask as other
  // feature might also show side panel.
  bool IsSidePanelOpenForContextualTask();

  // Transfer WebContents from tab to side panel.
  // This is called before a tab is converted to the side panel.
  void TransferWebContentsFromTab(
      const base::Uuid& task_id,
      std::unique_ptr<content::WebContents> web_contents);

  content::WebContents* GetActiveWebContentsForTesting();

 private:
  int GetPreferredDefaultSidePanelWidth();

  // Update the associated WebContents for active tab.
  void UpdateWebContentsForActiveTab();

  // Handle swapping WebContents if thread changes.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Create the side panel view.
  std::unique_ptr<views::View> CreateSidePanelView(SidePanelEntryScope& scope);

  // Create side panel contents for active tab. Return nullptr if no thread is
  // associated with the current tab.
  content::WebContents* MaybeGetOrCreateSidePanelWebContentsForActiveTab();

  // Browser window of the current side panel.
  const raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;

  // Subscription to listen for tab change.
  base::CallbackListSubscription active_tab_subscription_;

  // `side_panel_coordinator_` is expected to outlife this class.
  const raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;

  // Context controller to query task information.
  const raw_ptr<ContextualTasksContextController> context_controller_;

  // WebView of the current side panel. It's owned by side panel framework so
  // weak pointer is needed in case it's destroyed. The WebContents in the
  // WebView is owned by the cache and can change based on active task change.
  base::WeakPtr<ContextualTasksWebView> web_view_ = nullptr;

  // WebContents cache for each task.
  // It's okay to assume there is only 1 WebContents per task per window.
  // Different windows do not share the WebContents with the same task.
  std::map<base::Uuid, std::unique_ptr<content::WebContents>>
      task_id_to_web_contents_cache_;

  ui::ScopedUnownedUserData<ContextualTasksSidePanelCoordinator>
      scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
