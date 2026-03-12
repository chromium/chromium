// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_

#include "base/observer_list_types.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

class BrowserWindowInterface;

namespace base {
class Uuid;
}

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {
class ContextualTask;

class ContextualTasksPanelController {
 public:
  virtual ~ContextualTasksPanelController() = default;

  class Observer : public base::CheckedObserver {
   public:
    virtual void ExpandToFullTabStateChanged() {}
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Visibility commands.
  // Show the panel. If |transition_from_tab| is true, trigger the panel content
  // to animate from the active tab content's bounds.
  virtual void Show(
      bool transition_from_tab = false,
      omnibox::ChromeAimEntryPoint entry_point =
          omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT) = 0;
  // Close the panel.
  virtual void Close() = 0;

  // State checks.
  // Check if the panel is currently opening for ContextualTask as another
  // feature might also show panel.
  virtual bool IsPanelOpenForContextualTask() const = 0;

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

  // WebContents & session management.
  // Returns the currently active WebContents, or NULL if there is none.
  virtual content::WebContents* GetActiveWebContents() const = 0;
  // Returns a list of all cached panel WebContents.
  virtual std::vector<content::WebContents*> GetPanelWebContentsList()
      const = 0;
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

  // Static.
  static ContextualTasksPanelController* From(BrowserWindowInterface* browser);
};

}  // namespace contextual_tasks
#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_
