// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;
class PrefService;

namespace base {
class Uuid;
}

namespace views {
class View;
class WebView;
}  // namespace views

namespace contextual_tasks {

class ContextualTask;
class ContextualTasksContextController;
class ContextualTasksUiService;
class ContextualTasksWebView;

class ContextualTasksSidePanelCoordinator : public TabStripModelObserver,
                                            content::WebContentsObserver {
 public:
  // A data structure to hold the cache and state of the side panel per thread.
  struct WebContentsCacheItem {
    WebContentsCacheItem(std::unique_ptr<content::WebContents> wc, bool open);
    ~WebContentsCacheItem();
    WebContentsCacheItem(const WebContentsCacheItem&) = delete;
    WebContentsCacheItem& operator=(const WebContentsCacheItem&) = delete;

    // Own the WebContents from the side panel.
    std::unique_ptr<content::WebContents> web_contents;

    // Whether the side panel is open. Only used when FeatureParam
    // `kTaskScopedSidePanel` is set to true.
    bool is_open;
  };
  DECLARE_USER_DATA(ContextualTasksSidePanelCoordinator);

  explicit ContextualTasksSidePanelCoordinator(
      BrowserWindowInterface* browser_window);
  ContextualTasksSidePanelCoordinator(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ContextualTasksSidePanelCoordinator& operator=(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ~ContextualTasksSidePanelCoordinator() override;

  static ContextualTasksSidePanelCoordinator* From(
      BrowserWindowInterface* window);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // Show the side panel. If |transition_from_tab| is true, trigger the side
  // panel content to animate from the active tab content's bounds.
  void Show(bool transition_from_tab = false);

  // Close the side panel.
  void Close();

  // Check if the side panel is currently showing
  bool IsSidePanelOpen();

  // Check if the side panel is currently opening for ContextualTask as other
  // feature might also show side panel.
  bool IsSidePanelOpenForContextualTask() const;

  // Transfer WebContents from tab to side panel.
  // This is called before a tab is converted to the side panel.
  void TransferWebContentsFromTab(
      const base::Uuid& task_id,
      std::unique_ptr<content::WebContents> web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  content::WebContents* GetActiveWebContents();

  // Detaches the WebContents for the given task and returns it.
  std::unique_ptr<content::WebContents> DetachWebContentsForTask(
      const base::Uuid& task_id);

  // Called when the current task is changed to a new task or an existing task.
  // In both cases, the cache needs to be updated.
  void OnTaskChanged(content::WebContents* web_contents, base::Uuid task_id);

 private:
  // Get the task associated with the active tab.
  std::optional<ContextualTask> GetCurrentTask();

  // Hide or show side panel base on open state of the current task.
  void UpdateSidePanelVisibility();

  // Clean up unused WebContents.
  void CleanUpUnusedWebContents();

  int GetPreferredDefaultSidePanelWidth();

  // Update the associated WebContents for active tab. Returns whether the web
  // contents was changed.
  bool UpdateWebContentsForActiveTab();

  // Handle swapping WebContents if thread changes.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Create the side panel view.
  std::unique_ptr<views::View> CreateSidePanelView(SidePanelEntryScope& scope);

  // Create side panel contents for active tab. Return nullptr if no thread is
  // associated with the current tab.
  content::WebContents* MaybeGetOrCreateSidePanelWebContentsForActiveTab();

  // Hide/Unhide the side panel and don't update any task associated with it.
  void Hide();
  void Unhide();

  // Called before a WebContents is moved or destroyed to make sure the side
  // panel does not attach to it any more.
  void MaybeDetachWebContentsFromWebView(content::WebContents* web_contents);

  // Called when active tab has been updated.
  void ObserveWebContentsOnActiveTab();

  // Update the statucs of active tab context on the side panel.
  void UpdateContextualTaskUI();

  // Disassociate the tab from the task if it's associated with it.
  void DisassociateTabFromTask(content::WebContents* web_contents);

  // Update open state of the side panel, can be either task scoped or tab
  // scoped based on FeatureParam `kTaskScopedSidePanel`.
  void UpdateOpenState(bool is_open);

  // Get the open state of the side panel, can be either task scoped or tab
  // scoped based on FeatureParam `kTaskScopedSidePanel`.
  bool ShouldBeOpen();

  // Initialize the open state of the tab scoped side panel if the
  // active tab does not have an open state.
  void MaybeInitTabScopedOpenState();

  // Helper method to get the session handle from the side panel's web contents.
  contextual_search::ContextualSearchSessionHandle*
  GetContextualSearchSessionHandleForSidePanel();

  // Browser window of the current side panel.
  const raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;

  // Subscription to listen for tab change.
  base::CallbackListSubscription active_tab_subscription_;

  // Context controller to query task information.
  const raw_ptr<ContextualTasksContextController> context_controller_;

  const raw_ptr<ContextualTasksUiService> ui_service_;

  // Pref service for the current profile.
  const raw_ptr<PrefService> pref_service_;

  // WebView of the current side panel. It's owned by side panel framework so
  // weak pointer is needed in case it's destroyed. The WebContents in the
  // WebView is owned by the cache and can change based on active task change.
  base::WeakPtr<ContextualTasksWebView> web_view_ = nullptr;

  // WebContents cache for each task.
  // It's okay to assume there is only 1 WebContents per task per window.
  // Different windows do not share the WebContents with the same task.
  std::map<base::Uuid, std::unique_ptr<WebContentsCacheItem>>
      task_id_to_web_contents_cache_;

  // The tab scoped side panel open state map. Only used when FeatureParam
  // `kTaskScopedSidePanel` is set to false.
  std::map<SessionID, bool> tab_scoped_open_state_;

  ui::ScopedUnownedUserData<ContextualTasksSidePanelCoordinator>
      scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
