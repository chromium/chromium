// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_NAVIGATION_OBSERVER_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

// Per-tab navigation observer for contextual tasks. Currently maps the thread
// in a full-tab AI Mode URL (`mtid`) to the tab's contextual task.
// TODO(crbug.com/573687602): Also move non-AI navigation task disassociation
// out of `ActiveTaskContextProviderImpl::PrimaryPageChanged` into this class.
class ContextualTasksTabNavigationObserver
    : public content::WebContentsObserver {
 public:
  explicit ContextualTasksTabNavigationObserver(tabs::TabInterface& tab);
  ContextualTasksTabNavigationObserver(
      const ContextualTasksTabNavigationObserver&) = delete;
  ContextualTasksTabNavigationObserver& operator=(
      const ContextualTasksTabNavigationObserver&) = delete;
  ~ContextualTasksTabNavigationObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Sets this tab's task to the task of the thread in `url` (its `mtid`
  // param), creating it if needed, like `ContextualTasksUI::FrameNavObserver`
  // does for the legacy WebUI. Without a thread (e.g. zero state), the tab has
  // no task.
  void UpdateTaskForUrl(const GURL& url);

  // Disassociates `contents` from its thread task and unassigns the session
  // from it so the next task can adopt it.
  void ClearThreadTask(content::WebContents* contents);

  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);

  // The AI Mode thread (`mtid`) currently mapped to this tab's task.
  std::optional<std::string> thread_id_;
  base::CallbackListSubscription will_discard_contents_subscription_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_NAVIGATION_OBSERVER_H_
