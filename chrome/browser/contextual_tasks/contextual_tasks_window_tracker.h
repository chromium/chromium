// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_

#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "url/gurl.h"

class TabListInterface;

namespace tabs {
class TabInterface;
}

namespace contextual_tasks {

// Tracks a window (tab) opened via HandleNavigationImpl that was
// allowed to open naturally. It associates the window with a task ID.
class ContextualTasksWindowTracker : public BrowserCollectionObserver,
                                     public TabListInterfaceObserver {
 public:
  ContextualTasksWindowTracker(
      TabListInterface* current_tab_list,
      const base::Uuid& task_id,
      const GURL& expected_url,
      base::OnceCallback<void(ContextualTasksWindowTracker*)>
          on_closed_callback);
  ~ContextualTasksWindowTracker() override;
  ContextualTasksWindowTracker(const ContextualTasksWindowTracker&) = delete;
  ContextualTasksWindowTracker& operator=(const ContextualTasksWindowTracker&) =
      delete;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override;
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  void OnWindowClosed();

  // Accessors for testing.
  const base::Uuid& task_id() const { return task_id_; }
  const GURL& expected_url() const { return expected_url_; }
  tabs::TabInterface* tracked_tab() const { return tracked_tab_; }

 private:
  base::Uuid task_id_;
  GURL expected_url_;
  raw_ptr<tabs::TabInterface> tracked_tab_ = nullptr;
  std::set<raw_ptr<TabListInterface>> observed_tab_lists_;
  base::OnceCallback<void(ContextualTasksWindowTracker*)> on_closed_callback_;
  base::OneShotTimer timeout_timer_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WINDOW_TRACKER_H_
