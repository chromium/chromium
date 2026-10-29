// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_THREAD_OBSERVER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_THREAD_OBSERVER_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class Uuid;
}  // namespace base

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Keeps the contextual task of a side panel AI Mode `WebContents`
// (post-rearchitecture) in sync with its AI Mode thread, like
// `ContextualTasksTabNavigationObserver` does for full-tab AI Mode:
// - The panel's task follows its thread (`mtid`): the thread is added to the
//   panel's task, and the panel moves to the task of another thread (e.g.
//   picked from history), created if needed.
// - When the AI Mode page reports zero state after a turn (e.g. "New chat",
//   via `ContextualTasksWebContentsUserData`), the panel moves to a new task so
//   that the previous conversation keeps its task. This includes signed-out AI
//   Mode, whose threads have no `mtid`.
//
// Owned by the side panel `WebContents` it observes (as `WebContentsUserData`),
// so it lives as long as that `WebContents`. The
// `ContextualTasksSidePanelCoordinator` attaches it when it creates, rebuilds
// or moves in a panel `WebContents`, and removes it when the `WebContents` is
// moved to a tab.
class ContextualTasksPanelThreadObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContextualTasksPanelThreadObserver> {
 public:
  ContextualTasksPanelThreadObserver(
      const ContextualTasksPanelThreadObserver&) = delete;
  ContextualTasksPanelThreadObserver& operator=(
      const ContextualTasksPanelThreadObserver&) = delete;
  ~ContextualTasksPanelThreadObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<ContextualTasksPanelThreadObserver>;

  explicit ContextualTasksPanelThreadObserver(
      content::WebContents* web_contents);

  // Called when the AI Mode page reports zero state, before the input state is
  // updated for it.
  void OnZeroState();

  // Returns the panel's current task, if any.
  std::optional<base::Uuid> GetCurrentTaskId();

  // Moves the panel from `old_task_id` to `new_task_id`, along with the tabs
  // associated with `old_task_id`.
  void ChangeTask(const base::Uuid& old_task_id, const base::Uuid& new_task_id);

  // The AI Mode thread (`mtid`) currently in the panel.
  std::optional<std::string> thread_id_;

  // Whether the panel's task has had a turn (see `HasTurn()`), i.e. isn't in
  // its initial zero state.
  bool has_turn_ = false;

  base::CallbackListSubscription zero_state_subscription_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_THREAD_OBSERVER_H_
