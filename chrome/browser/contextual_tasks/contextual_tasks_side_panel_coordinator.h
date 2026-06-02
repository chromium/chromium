// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_

#include <map>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class PrefService;

namespace base {
class Uuid;
}

namespace contextual_search {
class ContextualSearchService;
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace content {
class NavigationHandle;
}  // namespace content

namespace contextual_tasks {

class ContextualTask;
class ContextualTasksService;
class ContextualTasksUiService;
class ActiveTaskContextProvider;
class EntryPointEligibilityManager;

class ContextualTasksSidePanelCoordinator
    : public ContextualTasksPanelController,
      public ContextualTasksPanelHost::Observer,
      public TabListInterfaceObserver,
      content::WebContentsObserver {
 public:
  // A data structure to hold the cache and state of the panel per thread.
  struct WebContentsCacheItem {
    WebContentsCacheItem(std::unique_ptr<content::WebContents> wc, bool open);
    ~WebContentsCacheItem();
    WebContentsCacheItem(const WebContentsCacheItem&) = delete;
    WebContentsCacheItem& operator=(const WebContentsCacheItem&) = delete;

    // Own the WebContents from the panel.
    std::unique_ptr<content::WebContents> web_contents;

    // Whether the panel is open.
    bool is_open;

    // The time when the WebContents becomes inactive.
    base::TimeTicks last_active_time_ticks;
  };

  DECLARE_USER_DATA(ContextualTasksSidePanelCoordinator);

  explicit ContextualTasksSidePanelCoordinator(
      BrowserWindowInterface* browser_window,
      ActiveTaskContextProvider* active_task_context_provider,
      EntryPointEligibilityManager* eligibility_manager);

  // For testing only.
  ContextualTasksSidePanelCoordinator(
      BrowserWindowInterface* browser_window,
      std::unique_ptr<ContextualTasksPanelHost> contextual_tasks_panel_host,
      ActiveTaskContextProvider* active_task_context_provider,
      EntryPointEligibilityManager* eligibility_manager);

  ContextualTasksSidePanelCoordinator(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ContextualTasksSidePanelCoordinator& operator=(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ~ContextualTasksSidePanelCoordinator() override;

  // ContextualTasksPanelController:
  void AddObserver(ContextualTasksPanelController::Observer* observer) override;
  void RemoveObserver(
      ContextualTasksPanelController::Observer* observer) override;
  void Show(bool transition_from_tab,
            omnibox::ChromeAimEntryPoint entry_point) override;
  void Close() override;
  bool IsPanelOpenForContextualTask() const override;
  std::optional<tabs::TabHandle> GetAutoSuggestedTabHandle() override;
  void OnTaskChanged(content::WebContents* web_contents,
                     base::Uuid task_id) override;
  void OnAiInteraction() override;
  content::WebContents* GetActiveWebContents() const override;
  std::vector<content::WebContents*> GetPanelWebContentsList() const override;
  std::unique_ptr<content::WebContents> DetachWebContentsForTask(
      const base::Uuid& task_id) override;
  contextual_search::ContextualSearchSessionHandle*
  GetContextualSearchSessionHandleForPanel() override;
  void TransferWebContentsFromTab(
      const base::Uuid& task_id,
      std::unique_ptr<content::WebContents> web_contents) override;
  std::optional<ContextualTask> GetCurrentTask() override;
  std::pair<std::optional<base::Uuid>,
            contextual_search::ContextualSearchSessionHandle*>
  GetSessionHandleForActiveTabOrPanel() override;
  size_t GetNumberOfActiveTasks() const override;
  void MoveTaskUiToNewTab() override;
  void NotifyExpandToFullTabStateChanged() override;
  bool CanExpandToFullTab() const override;

  // ContextualTasksPanelHost::Observer:
  void OnSurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState state,
      ContextualTasksPanelHost::StateChangeReason reason) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override;
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override;

  // Get the WebContentsCacheItem for web_contents, return nullptr if not found.
  ContextualTasksSidePanelCoordinator::WebContentsCacheItem*
  GetWebContentsCacheItemForWebContents(content::WebContents* web_contents);

 private:
  friend class ContextualTasksSidePanelCoordinatorInteractiveUiTest;
  friend class ContextualTasksSidePanelCoordinatorTest;
  friend class ContextualTasksInteractiveUiTest;

  void SetPanelSuppressedForTesting(bool suppressed) {
    contextual_tasks_panel_host_->SetPanelSuppressedForTesting(suppressed);
  }

  void SetSuppressHideOnContextualTasksUrlForTesting(bool suppress) {
    suppress_hide_on_contextual_tasks_url_for_testing_ = suppress;
  }

  // Hide or show panel base on open state of the current task.
  void UpdatePanelVisibility();

  // Clean up unused WebContents.
  void CleanUpUnusedWebContents();

  // Update the associated WebContents for active tab. Returns whether the web
  // contents was changed.
  bool UpdateWebContentsForActiveTab();

  // Get the panel contents for active tab. Return nullptr if no thread is
  // associated with the current tab.
  content::WebContents* GetPanelWebContentsForActiveTab();

  // Create a cached WebContents if one does not exist for the current task.
  void MaybeCreateCachedWebContents(omnibox::ChromeAimEntryPoint entry_point);

  // Create a cached WebContents for a task. For tests only.
  void CreateCachedWebContentsForTesting(base::Uuid task_id, bool is_open);

  // Hide/Unhide the panel and don't update any task associated with it.
  void Hide();
  void Unhide();

  // Called before a WebContents is moved or destroyed to make sure the panel
  // does not attach to it any more, e.g. when AI Mode is transitioned from the
  // current tab to the panel. Only detaches if the passed-in WebContents
  // matches the currently active WebContents.
  void MaybeDetachWebContents(content::WebContents* web_contents);

  // Called when active tab has been updated.
  void ObserveWebContentsOnActiveTab();

  // Update the status of active tab context on the panel.
  void UpdateContextualTaskUI();

  // Disassociate the tab from the task if it's associated with it.
  void DisassociateTabFromTask(content::WebContents* web_contents);

  // Update open state of the panel.
  void UpdateOpenState(bool is_open);

  // Get the open state of the panel.
  bool ShouldBeOpen();

  // Closes any active Lens sessions for tabs associated with the given task.
  void CloseLensSessionsForTask(const ContextualTask& task);

  // Notifies the ActiveTaskContextProvider about the current session state.
  // This checks both the panel and the active tab for a valid session handle.
  void NotifyActiveTaskContextProvider();

  void RecordSessionEndMetrics();

  void OnEligibilityChange(bool is_eligible);

  // Browser window of the current panel.
  const raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;

  // Interface to interact with/get state about the panel UI. Own the unique_ptr
  // so that its lifetime is tied to `this`.
  const std::unique_ptr<ContextualTasksPanelHost> contextual_tasks_panel_host_;

  // Context controller to query task information.
  const raw_ptr<ContextualTasksService> contextual_tasks_service_;

  const raw_ptr<contextual_search::ContextualSearchService>
      contextual_search_service_;

  const raw_ptr<ContextualTasksUiService> ui_service_;

  // Pref service for the current profile.
  const raw_ptr<PrefService> pref_service_;

  const raw_ptr<ActiveTaskContextProvider> active_task_context_provider_;

  // WebContents cache for each task.
  // It's okay to assume there is only 1 WebContents per task per window.
  // Different windows do not share the WebContents with the same task.
  std::map<base::Uuid, std::unique_ptr<WebContentsCacheItem>>
      task_id_to_web_contents_cache_;

  base::CallbackListSubscription eligibility_change_subscription_;

  ui::ScopedUnownedUserData<ContextualTasksSidePanelCoordinator>
      scoped_unowned_user_data_;

  bool in_cobrowsing_session_ = false;

  // When true, PrimaryPageChanged() will not auto-hide the panel on a
  // contextual tasks URL navigation. Set only by interactive tests that drive
  // in-panel webview navigation.
  bool suppress_hide_on_contextual_tasks_url_for_testing_ = false;

  base::ObserverList<ContextualTasksPanelController::Observer> observers_;

  base::WeakPtrFactory<ContextualTasksSidePanelCoordinator> weak_ptr_factory_{
      this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
