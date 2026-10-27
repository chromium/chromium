// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/views/bubble/bubble_anchor.h"
#endif

class BrowserWindowInterface;
class PrefService;
#if !BUILDFLAG(IS_ANDROID)
class ThemeService;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
namespace contextual_tasks {
class ContextualTasksExtensionsContainer;
}  // namespace contextual_tasks
#endif

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
class ContextualTasksPermissionController;
class ContextualTasksService;
class ContextualTasksUiService;
class ActiveTaskContextProvider;
class EntryPointEligibilityManager;
#if !BUILDFLAG(IS_ANDROID)
class ContextualTasksOnboardingTooltipController;
#endif

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ContextualTasksTabCloseState)
enum class ContextualTasksTabCloseState {
  kActiveTab = 0,
  kBackgroundTab = 1,
  kMaxValue = kBackgroundTab,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_tasks/enums.xml:ContextualTasksTabCloseState)

class ContextualTasksSidePanelCoordinator
    : public ContextualTasksPanelController,
      public ContextualTasksPanelHost::Observer,
      public TabListInterfaceObserver,
      content::WebContentsObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
      public ThemeServiceObserver,
      public ui::NativeThemeObserver
#endif
{
 public:
  using PaintCallback = base::OnceCallback<void(content::WebContents*)>;

  class SidePanelWebContentsObserver;

  struct WebContentsCacheItem {
    WebContentsCacheItem(
        std::unique_ptr<content::WebContents> wc,
        bool open,
        PaintCallback on_fcp_callback = base::NullCallback(),
        base::RepeatingClosure on_navigation_callback = base::NullCallback());
    ~WebContentsCacheItem();
    WebContentsCacheItem(const WebContentsCacheItem&) = delete;
    WebContentsCacheItem& operator=(const WebContentsCacheItem&) = delete;

    // Own the WebContents from the panel.
    std::unique_ptr<content::WebContents> web_contents;

    // Whether the panel is open.
    bool is_open;

    // The time when the WebContents becomes inactive.
    base::TimeTicks last_active_time_ticks;

    // The entry source that triggered this task's panel.
    ContextualTasksPanelController::EntrySource entry_source =
        ContextualTasksPanelController::EntrySource::kOther;

    // The entry point that triggered this task's panel.
    omnibox::ChromeAimEntryPoint entry_point =
        omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT;

    // The time when this task's panel was opened.
    base::TimeTicks open_time_ticks;

    // Whether the first contentful paint metric has been recorded for this
    // open session.
    bool first_contentful_paint_recorded = false;

    // Whether the time to handshake complete metric has been recorded for this
    // open session.
    bool time_to_handshake_complete_recorded = false;

    // Observer for the embedded WebContents in the side panel.
    std::unique_ptr<SidePanelWebContentsObserver> web_contents_observer;

    // For a panel recreated for a reopened tab, the URL of the page that it was
    // recreated to show. Saved with the tab if it's closed again before the
    // panel loads the page.
    GURL restored_url;
  };

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
            omnibox::ChromeAimEntryPoint entry_point,
            bool use_no_animation = false,
            std::optional<base::TimeTicks> open_time_ticks = std::nullopt,
            std::optional<EntrySource> entry_source_override =
                std::nullopt) override;
  void Close() override;
  void OpenInZeroState() override;
  bool MoveActiveTabToNewThreadIfShared() override;
  std::map<std::string, std::string> GetTabRestoreData(
      tabs::TabInterface* tab) const override;
  void ShareTabsWithActiveOrNewThread(
      const std::vector<tabs::TabHandle>& tab_handles) override;
  void ShareTabsWithExistingThread(
      const base::Uuid& task_id,
      const std::vector<tabs::TabHandle>& tab_handles,
      omnibox::ChromeAimEntryPoint entry_point) override;
  std::vector<RecentThreadInfo> GetRecentThreads() const override;
  void UnshareTabs(const std::vector<tabs::TabHandle>& tab_handles) override;
  bool IsPanelOpenForContextualTask() const override;
  bool IsShowingResults() const override;
  bool HasResults() const override;
  ContextualTasksPanelController::EntrySource GetActiveEntrySource()
      const override;
  bool IsOnboardingTooltipVisible() const override;
  std::optional<tabs::TabHandle> GetAutoSuggestedTabHandle() override;
  void OnTaskChanged(content::WebContents* web_contents,
                     base::Uuid task_id) override;
  void OnAiInteraction() override;
  void SetPendingTaskForTab(tabs::TabInterface* tab,
                            const base::Uuid& task_id) override;
  content::WebContents* GetActiveWebContents() const override;
  content::WebContents* GetToolbarWebContents() const override;
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
  void ShowPageInfoBubble(bool is_pointer_interaction) override;
  void OnLogoPointerDown() override;
  ContextualTasksPermissionController* permission_controller() override;
#if !BUILDFLAG(IS_ANDROID)
  ContextualTasksOnboardingTooltipController*
  onboarding_controller_for_testing() {
    return onboarding_controller_.get();
  }
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  ContextualTasksExtensionsContainer* GetExtensionsContainerForTesting() {
    return extensions_container_.get();
  }
#endif
  // Records the time to handshake complete metric for the given WebContents.
  void RecordTimeToHandshakeComplete(
      content::WebContents* web_contents) override;

  // Records the time to first contentful paint (FCP) metric for the given
  // WebContents.
  void RecordTimeToFirstContentfulPaint(content::WebContents* web_contents);

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

#if !BUILDFLAG(IS_ANDROID)
  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;
#endif

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

  // Returns the task associated with the active side panel without
  // mutating pending task state.
  std::optional<ContextualTask> GetCurrentTask() const;

  // Clean up unused WebContents.
  void CleanUpUnusedWebContents() override;

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

  // Removes any zero-state implicit tab context on `web_contents` (or the
  // active panel `WebContents` when null).
  void MaybeRemoveImplicitTabContext(
      content::WebContents* web_contents = nullptr);

  // Disassociate the tab from the task if it's associated with it.
  void DisassociateTabFromTask(content::WebContents* web_contents);

  // Returns whether the current task and its tab associations should be
  // preserved when the side panel is closed so the user can resume it from the
  // ephemeral toolbar button or the pinned toolbar button (under Pinning V2).
  bool ShouldPreserveTaskOnClose() const;

  // Disassociate all tabs associated with the current task, or the active tab
  // if no current task exists. Under kToolbarEphemeralBranded mode, active
  // tasks with conversation threads are preserved on panel close to allow
  // ephemeral button resume.
  void DisassociateAllTabsFromCurrentTask();

  // Disassociates only the active tab from its task when the panel is closed.
  // Used instead of `DisassociateAllTabsFromCurrentTask()` when
  // `ShouldScopeSidePanelStateToTab()` is true. Under kToolbarEphemeralBranded
  // mode, the tab stays associated to allow ephemeral button resume.
  void DisassociateActiveTabOnClose();

  // Returns whether a tab in this window other than the active one is
  // associated with `task_id` and still shows the panel.
  bool IsPanelOpenOnOtherTabOfTask(const base::Uuid& task_id);

  // Returns whether any tab in this window is associated with `task_id`.
  bool IsTaskAssociatedWithTabInWindow(const base::Uuid& task_id) const;

  // Returns whether a tab in this window that is associated with `task_id`
  // shows the panel, i.e. didn't close it.
  bool IsPanelOpenOnTabOfTask(const base::Uuid& task_id) const;

  // Associates `tab` with the task saved when it was closed, if it was reopened
  // from recently closed tabs with the panel open. The panel is recreated when
  // the tab becomes active.
  void MaybeRestorePanelForTab(tabs::TabInterface* tab);

  // Recreates the cached WebContents for the task of `active_tab` if the task
  // is in `tasks_pending_panel_rebuild_`.
  void MaybeRebuildPanelForActiveTab(tabs::TabInterface* active_tab);

  // Recreates the cached WebContents for `task_id` and loads the task's thread.
  // Takes `task_id` by value, since it erases the task's entry from
  // `tasks_pending_panel_rebuild_`.
  void RebuildCachedWebContentsForTask(base::Uuid task_id);

  // Loads `url` into the rebuilt `web_contents` if it is still cached for
  // `task_id`.
  void OnThreadUrlForRebuiltWebContents(
      const base::Uuid& task_id,
      base::WeakPtr<content::WebContents> web_contents,
      GURL url);

  // Update open state of the panel.
  void UpdateOpenState(bool is_open);

  // Get the open state of the panel.
  bool ShouldBeOpen();

  // Returns the active task's HTTP/HTTPS results URL, agnostic to whether side
  // panel rearchitecture is enabled, or an empty `GURL` if no results URL is
  // loaded.
  GURL GetActiveResultsUrl() const;

  // Returns true if the active tab's side panel has Lens results loaded (e.g.
  // a Lens Overlay entry or a non-AIM Search Results Page / Lens SRP).
  bool IsShowingLensResults() const;

  // Returns true if the active tab's side panel has AIM results loaded (i.e.
  // an AIM page that is not in zero state).
  bool IsShowingAimResults() const;

  // Closes any active Lens sessions for tabs associated with the given task.
  void CloseLensSessionsForTask(const ContextualTask& task);

  // Closes any active Lens session on the given tab.
  void CloseLensSessionForTab(tabs::TabInterface* tab);

  // Closes any active Lens session on the given tab if it matches an
  // eligibility criteria.
  void CloseLensSessionIfActive(
      tabs::TabInterface* tab_interface,
      omnibox::ChromeAimEntryPoint entry_point,
      ContextualTasksPanelController::EntrySource entry_source);

  // Notifies the ActiveTaskContextProvider about the current session state.
  // This checks both the panel and the active tab for a valid session handle.
  void NotifyActiveTaskContextProvider();

  // Attaches `tab_handles` as context to `task_id` and updates the side panel
  // session and tab underlines.
  void AttachTabsToTask(const base::Uuid& task_id,
                        const std::vector<tabs::TabHandle>& tab_handles);

  void RecordSessionEndMetrics();

  void OnEligibilityChange(bool is_eligible);

  // ContextualTasksUiService is a ProfileKeyedService whose lifetime is bound
  // to the Profile. In contrast, ContextualTasksSidePanelCoordinator is a
  // window-scoped object whose teardown occurs asynchronously via the message
  // loop during window closure. Because of this mismatched lifetime, the UI
  // service can be destroyed before the coordinator, leading to use-after-free
  // dangling raw pointer crashes if a raw pointer is cached. To prevent this,
  // the coordinator must fetch the service dynamically via GetUiService() and
  // check for null.
  ContextualTasksUiService* GetUiService() const;

  // Browser window of the current panel.
  const raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;

  // Single permission controller and location bar for the side panel.
  std::unique_ptr<ContextualTasksPermissionController> permission_controller_;

  // WebContents cache for each task.
  // Must be declared before contextual_tasks_panel_host_ so that in automated
  // C++ reverse destruction order, the panel host is destroyed before cached
  // WebContents objects are deleted.
  // It's okay to assume there is only 1 WebContents per task per window.
  // Different windows do not share the WebContents with the same task.
  std::map<base::Uuid, std::unique_ptr<WebContentsCacheItem>>
      task_id_to_web_contents_cache_;

  // Interface to interact with/get state about the panel UI. Own the unique_ptr
  // so that its lifetime is tied to `this`.
  const std::unique_ptr<ContextualTasksPanelHost> contextual_tasks_panel_host_;

  // Context controller to query task information.
  const raw_ptr<ContextualTasksService> contextual_tasks_service_;

  const raw_ptr<contextual_search::ContextualSearchService>
      contextual_search_service_;

  // Pref service for the current profile.
  const raw_ptr<PrefService> pref_service_;

  const raw_ptr<ActiveTaskContextProvider> active_task_context_provider_;

  base::CallbackListSubscription eligibility_change_subscription_;

  ui::ScopedUnownedUserData<ContextualTasksPanelController>
      scoped_unowned_user_data_;

  bool in_cobrowsing_session_ = false;
  bool is_opening_in_zero_state_ = false;

  // When true, PrimaryPageChanged() will not auto-hide the panel on a
  // contextual tasks URL navigation. Set only by interactive tests that drive
  // in-panel webview navigation.
  bool suppress_hide_on_contextual_tasks_url_for_testing_ = false;

  // Used to save the entry source that triggered a task's panel when the panel
  // is being closed so that it can be logged.
  std::optional<ContextualTasksPanelController::EntrySource>
      closing_entry_source_;

  // Tabs on which the panel was closed while the tab stays associated with its
  // task, so that the panel stays open on other tabs of the same task. Only
  // used when `ShouldScopeSidePanelStateToTab()` is true.
  std::set<tabs::TabHandle> tabs_with_closed_panel_;

  // Tasks whose panel WebContents is recreated when one of their tabs becomes
  // active, because a tab was reopened after being closed with the panel open,
  // or because the task's WebContents moved to a new task after the thread was
  // changed on another tab. After a thread change, it's recreated right away if
  // one of the tabs shows the panel. Only used when
  // `ShouldRestoreSidePanelOnTabRestore()` or
  // `ShouldScopeSidePanelStateToTab()` is true.
  std::map<base::Uuid, ContextualTasksPanelController::EntrySource>
      tasks_pending_panel_rebuild_;

  base::ObserverList<ContextualTasksPanelController::Observer> observers_;

#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/536100150): Support this on Android Desktop
  WebUIBubbleReopenSuppressor page_info_bubble_suppressor_;

  // Returns an anchor for the side panel toolbar's super G button, or a null
  // anchor if that element is not currently available. The underlying
  // `ui::TrackedElement` is owned by the side panel WebUI document and dies
  // with it, so callers must re-resolve the anchor on every use instead of
  // caching it.
  views::BubbleAnchor GetSuperGButtonAnchor();
#endif

  // Returns the browser window hosting this side panel.
  BrowserWindowInterface* GetBrowserWindow() const;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  void OnSeeExtensionsClicked();
  std::unique_ptr<ContextualTasksExtensionsContainer> extensions_container_;
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Refreshes cached side panel `WebContents` when the profile's effective dark
  // mode changes so embedded pages stay in sync with the shared toolbar.
  void MaybeRefreshWebContentsForDarkModeChange();

  bool is_dark_mode_ = false;
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  std::unique_ptr<ContextualTasksOnboardingTooltipController>
      onboarding_controller_;
#endif

  base::WeakPtrFactory<ContextualTasksSidePanelCoordinator> weak_ptr_factory_{
      this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
