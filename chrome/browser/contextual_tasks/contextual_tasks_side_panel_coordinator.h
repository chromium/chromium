// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_

#include <map>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;
class SidePanelUI;
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

namespace views {
class View;
class WebView;
}  // namespace views

namespace contextual_tasks {

class ContextualTask;
class ContextualTasksService;
class ContextualTasksUiService;
class ContextualTasksWebView;
class ActiveTaskContextProvider;
class EntryPointEligibilityManager;

class ContextualTasksSidePanelCoordinator
    : public ContextualTasksPanelController,
      public TabListInterfaceObserver,
      public SidePanelEntryObserver,
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
      SidePanelUI* side_panel_ui,
      ActiveTaskContextProvider* active_task_context_provider,
      EntryPointEligibilityManager* eligibility_manager);
  ContextualTasksSidePanelCoordinator(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ContextualTasksSidePanelCoordinator& operator=(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ~ContextualTasksSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // ContextualTasksPanelController overrides:
  void Show(bool transition_from_tab,
            omnibox::ChromeAimEntryPoint entry_point) override;
  void Close() override;
  bool IsPanelOpenForContextualTask() const override;
  std::optional<tabs::TabHandle> GetAutoSuggestedTabHandle() override;
  void OnTaskChanged(content::WebContents* web_contents,
                     base::Uuid task_id) override;
  void OnAiInteraction() override;
  content::WebContents* GetActiveWebContents() override;
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
  GetSessionHandleForActiveTabOrSidePanel() override;
  size_t GetNumberOfActiveTasks() const override;

  // Check if the side panel is currently showing
  bool IsSidePanelOpen();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Get the WebContentsCacheItem for web_contents, return nullptr if not found.
  ContextualTasksSidePanelCoordinator::WebContentsCacheItem*
  GetWebContentsCacheItemForWebContents(content::WebContents* web_contents);

  void SetSidePanelIdNotToOverrideForTesting(SidePanelEntry::Id side_panel_id);

  // TabListInterfaceObserver overrides:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override;
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override;

 private:
  friend class ContextualTasksSidePanelCoordinatorInteractiveUiTest;
  friend class ContextualTasksSidePanelCoordinatorTest;

  // Hide or show side panel base on open state of the current task.
  void UpdateSidePanelVisibility();

  // Clean up unused WebContents.
  void CleanUpUnusedWebContents();

  int GetPreferredDefaultSidePanelWidth();

  // Update the associated WebContents for active tab. Returns whether the web
  // contents was changed.
  bool UpdateWebContentsForActiveTab();

  // Create the side panel view.
  std::unique_ptr<views::View> CreateSidePanelView(SidePanelEntryScope& scope);

  // Get the side panel contents for active tab. Return nullptr if no thread is
  // associated with the current tab.
  content::WebContents* GetSidePanelWebContentsForActiveTab();

  // Create a cached WebContents if one does not exist for the current task.
  void MaybeCreateCachedWebContents(omnibox::ChromeAimEntryPoint entry_point);

  // Create a cached WebContents for a task. For tests only.
  void CreateCachedWebContentsForTesting(base::Uuid task_id, bool is_open);

  // Hide/Unhide the side panel and don't update any task associated with it.
  void Hide();
  void Unhide();

  // Called before a WebContents is moved or destroyed to make sure the side
  // panel does not attach to it any more.
  void MaybeDetachWebContentsFromWebView(content::WebContents* web_contents);

  // Called when active tab has been updated.
  void ObserveWebContentsOnActiveTab();

  // Update the status of active tab context on the side panel.
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

  // Closes any active Lens sessions for tabs associated with the given task.
  void CloseLensSessionsForTask(const ContextualTask& task);

  // Notifies the ActiveTaskContextProvider about the current session state.
  // This checks both the side panel and the active tab for a valid session
  // handle.
  void NotifyActiveTaskContextProvider();

  void RecordSessionEndMetrics();

  void OnEligibilityChange(bool is_eligible);

  // Browser window of the current side panel.
  const raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;

  // Context controller to query task information.
  const raw_ptr<ContextualTasksService> contextual_tasks_service_;

  const raw_ptr<contextual_search::ContextualSearchService>
      contextual_search_service_;

  const raw_ptr<ContextualTasksUiService> ui_service_;

  // Pref service for the current profile.
  const raw_ptr<PrefService> pref_service_;

  const raw_ptr<SidePanelUI> side_panel_ui_;

  const raw_ptr<ActiveTaskContextProvider> active_task_context_provider_;

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

  // If the side panel with this specific id is open, do not override it with
  // the contextual tasks side panel when active tab is changed.
  SidePanelEntry::Id side_panel_id_not_to_override_ = SidePanelEntry::Id::kGlic;

  base::CallbackListSubscription eligibility_change_subscription_;

  ui::ScopedUnownedUserData<ContextualTasksSidePanelCoordinator>
      scoped_unowned_user_data_;

  bool in_cobrowsing_session_ = false;

  base::WeakPtrFactory<ContextualTasksSidePanelCoordinator> weak_ptr_factory_{
      this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
