// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {
class ContextualTask;
class ContextualTasksPermissionController;

class ContextualTasksPanelController {
 public:
  struct RecentThreadInfo {
    base::Uuid task_id;
    std::u16string title;
  };

  DECLARE_USER_DATA(ContextualTasksPanelController);

  virtual ~ContextualTasksPanelController() = default;

  class Observer : public base::CheckedObserver {
   public:
    virtual void ExpandToFullTabStateChanged() {}
    virtual void OnSurfaceStateChanged(
        ContextualTasksPanelHost::SurfaceState state,
        ContextualTasksPanelHost::StateChangeReason reason) {}
    virtual void OnControllerDestroyed() {}
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // The entry source for the contextual tasks panel.
  enum class EntrySource {
    kOther = 0,
    kLensOverlay = 1,
    kAiModeLinkClick = 2,
    kAioToCobr = 3,
    kAskGoogle = 4,
    kEphemeralToolbarButton = 5,
    kPermanentToolbarButton = 6,
    kMaxValue = kPermanentToolbarButton,
  };

  // Whether clicking the permanently pinned toolbar button to open the side
  // panel continues an existing thread or starts a new thread under Pinning v2.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ContextualTasksPermanentToolbarButtonThreadAction)
  enum class PermanentToolbarButtonThreadAction {
    kContinueThread = 0,
    kNewThread = 1,
    kMaxValue = kNewThread,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_tasks/enums.xml:ContextualTasksPermanentToolbarButtonThreadAction)

  // Visibility commands.
  // Show the panel. If |transition_from_tab| is true, trigger the panel content
  // to animate from the active tab content's bounds.
  virtual void Show(
      bool transition_from_tab = false,
      omnibox::ChromeAimEntryPoint entry_point =
          omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      bool use_no_animation = false,
      std::optional<base::TimeTicks> open_time_ticks = std::nullopt,
      std::optional<EntrySource> entry_source_override = std::nullopt) = 0;
  // Close the panel.
  virtual void Close() = 0;

  // Open the panel in zero state, or reset to zero state if already open.
  virtual void OpenInZeroState() = 0;

  // Moves only the active tab to a new task with a zero-state panel if other
  // tabs share its task. Returns whether the active tab was moved.
  virtual bool MoveActiveTabToNewThreadIfShared() = 0;

  // Returns the side panel state to save with `tab` when it's closed, so that
  // reopening it from recently closed tabs restores its open panel.
  virtual std::map<std::string, std::string> GetTabRestoreData(
      tabs::TabInterface* tab) const = 0;

  // Shares `tab_handles` when starting a new thread from the tab context menu.
  // If the side panel is already open on the active tab, attaches `tab_handles`
  // to the current task; otherwise, opens a fresh zero-state thread and
  // attaches both the active tab (if valid for context sharing) and
  // `tab_handles` as context.
  virtual void ShareTabsWithActiveOrNewThread(
      const std::vector<tabs::TabHandle>& tab_handles) = 0;

  // Switches the side panel to the existing cached thread `task_id`, opens the
  // side panel with `entry_point`, and attaches both the active tab (if valid
  // for context sharing) and `tab_handles` as context.
  virtual void ShareTabsWithExistingThread(
      const base::Uuid& task_id,
      const std::vector<tabs::TabHandle>& tab_handles,
      omnibox::ChromeAimEntryPoint entry_point) = 0;

  // Returns up to 5 recent non-zero-state threads cached in this browser
  // window, ordered with the active side panel thread first followed by most
  // recently active threads.
  virtual std::vector<RecentThreadInfo> GetRecentThreads() const = 0;

  // Removes `tab_handles` from every contextual task thread in this browser
  // window and clears their tab underlines.
  virtual void UnshareTabs(const std::vector<tabs::TabHandle>& tab_handles) = 0;

  // State checks.
  // Check if the panel is currently opening for ContextualTask as another
  // feature might also show panel.
  virtual bool IsPanelOpenForContextualTask() const = 0;

  // Returns true if the active tab's side panel task is showing results (i.e.
  // an AIM thread that is not in zero state, a task waiting for its search URL,
  // or a Search Results Page / Lens SRP loaded in the side panel).
  virtual bool IsShowingResults() const = 0;

  // Returns true if the active tab's side panel has results loaded or is
  // currently trying to load results, whether the side panel is currently open
  // or was closed and can be reopened to resume those results.
  virtual bool HasResults() const = 0;

  // Returns the entry source of the currently active panel.
  virtual EntrySource GetActiveEntrySource() const = 0;

  // Returns true if the onboarding or Ask Google tooltip bubble is visible.
  virtual bool IsOnboardingTooltipVisible() const;

  // Context management.
  // Returns the tab handle of the auto suggested tab if the auto suggested tab
  // chip is shown in the compose box.
  virtual std::optional<tabs::TabHandle> GetAutoSuggestedTabHandle() = 0;
  // Called when the current task is changed to a new task or an existing task.
  // In both cases, the cache needs to be updated.
  virtual void OnTaskChanged(content::WebContents* web_contents,
                             base::Uuid task_id) = 0;
  // Called when there is an AI interaction in the panel.
  virtual void OnAiInteraction() = 0;

  // Sets a task to be used once the side panel opens. Used to open a specific
  // task without associating a web contents (which would inherently get added
  // as context).
  virtual void SetPendingTaskForTab(tabs::TabInterface* tab,
                                    const base::Uuid& task_id) = 0;

  // WebContents & session management.
  // Returns the currently active WebContents, or NULL if there is none.
  virtual content::WebContents* GetActiveWebContents() const = 0;
  // Returns the WebContents hosting the top WebUI toolbar, or nullptr if none.
  virtual content::WebContents* GetToolbarWebContents() const = 0;
  // Returns a list of all cached panel WebContents.
  virtual std::vector<content::WebContents*> GetPanelWebContentsList()
      const = 0;
  // Cleans up any WebContents that are no longer associated with active tabs or
  // have expired from the cache.
  virtual void CleanUpUnusedWebContents() = 0;
  // Detaches the WebContents for the given task and returns it.
  virtual std::unique_ptr<content::WebContents> DetachWebContentsForTask(
      const base::Uuid& task_id) = 0;
  // Helper method to get the session handle from the side panel's WebUI.
  virtual contextual_search::ContextualSearchSessionHandle*
  GetContextualSearchSessionHandleForPanel() = 0;
  // Transfer WebContents from tab to panel.
  // This is called before a tab is converted to the panel.
  virtual void TransferWebContentsFromTab(
      const base::Uuid& task_id,
      std::unique_ptr<content::WebContents> web_contents) = 0;
  // Get the task associated with the active tab.
  virtual std::optional<ContextualTask> GetCurrentTask() = 0;

  // Returns currently showing task's task ID and session handle.
  virtual std::pair<std::optional<base::Uuid>,
                    contextual_search::ContextualSearchSessionHandle*>
  GetSessionHandleForActiveTabOrPanel() = 0;

  // Metrics.
  // Returns the number of active tasks tracked by `this`.
  virtual size_t GetNumberOfActiveTasks() const = 0;

  // Move the side panel WebContents to new tab.
  virtual void MoveTaskUiToNewTab() = 0;

  // Notify expand to full tab state changed for showing/hiding
  // ContextualTasksCloseTabButton.
  virtual void NotifyExpandToFullTabStateChanged() = 0;

  // Whether the side panel can expand to full tab.
  virtual bool CanExpandToFullTab() const = 0;

  // Shows the native PageInfo bubble.
  virtual void ShowPageInfoBubble(bool is_pointer_interaction) = 0;
  virtual void OnLogoPointerDown() = 0;
  virtual ContextualTasksPermissionController* permission_controller() = 0;

  // Records the time to handshake complete metric for the given WebContents.
  virtual void RecordTimeToHandshakeComplete(
      content::WebContents* web_contents) {}

  // Static.
  static ContextualTasksPanelController* From(BrowserWindowInterface* browser);
  // Returns whether the given WebContents is hosted within the Contextual Tasks
  // side panel.
  static bool IsWebContentsInPanel(content::WebContents* web_contents);
};

}  // namespace contextual_tasks
#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_
