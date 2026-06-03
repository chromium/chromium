// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_MANAGER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace contextual_tasks {

class ContextualTasksWindowTracker;

// Centralized class to track all window trackers for Contextual Tasks.
class ContextualTasksWindowTrackerManager : public TabListInterfaceObserver {
 public:
  ContextualTasksWindowTrackerManager();
  ~ContextualTasksWindowTrackerManager() override;

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  // Starts observing the given tab list.
  void ObserveTabList(TabListInterface* tab_list);

  ContextualTasksWindowTrackerManager(
      const ContextualTasksWindowTrackerManager&) = delete;
  ContextualTasksWindowTrackerManager& operator=(
      const ContextualTasksWindowTrackerManager&) = delete;

  // Adds a tracker to be managed.
  void AddTracker(std::unique_ptr<ContextualTasksWindowTracker> tracker);

  // Removes a tracker.
  void RemoveTracker(ContextualTasksWindowTracker* tracker);

  // Registers a tracked window with its ID, associated task ID, and URL.
  void RegisterWindow(ContextualTaskId task_id,
                      const GURL& url,
                      ContextualWindowId window_id);

  // Requests the browser to close a tracked window.
  void CloseTrackedWindow(ContextualWindowId window_id);

  // Returns true if the web_contents is tracked.
  bool IsTrackedWindow(content::WebContents* web_contents) const;

  // Return true if there is a pending tracker that is waiting for the window to
  // open for the given url and source_contents.
  bool IsPendingWindow(const GURL& url,
                       content::WebContents* source_contents) const;

  // Returns the pending tracker for the given URL and source_contents, or
  // nullptr if none.
  ContextualTasksWindowTracker* GetPendingTracker(
      const GURL& url,
      content::WebContents* source_contents) const;

  // Matches a pending tracker for the given URL and associates it with the
  // `source_contents` and the `message_proxy_web_contents`. Returns the matched
  // tracker, or nullptr if no match.
  ContextualTasksWindowTracker* MatchAndAssociatePendingTracker(
      const GURL& url,
      content::WebContents* source_contents,
      std::unique_ptr<content::WebContents> message_proxy_web_contents);

  // Finds the tracker that corresponds to the given message proxy WebContents.
  ContextualTasksWindowTracker* FindTrackerByMessageProxy(
      content::WebContents* proxy_contents);

  // For testing.
  const std::vector<std::unique_ptr<ContextualTasksWindowTracker>>&
  window_trackers_for_testing() const {
    return window_trackers_;
  }

 private:
  // A vector is used as a type of FIFO queue. The trackers are assigned based
  // on the first tracker that matches the navigation.
  std::vector<std::unique_ptr<ContextualTasksWindowTracker>> window_trackers_;

  std::set<TabListInterface*> observed_tab_lists_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_MANAGER_H_
