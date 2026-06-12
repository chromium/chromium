// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/omnibox/common/logger.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

ContextualTasksWindowTracker::ContextualTasksWindowTracker(
    TabListInterface* current_tab_list,
    const base::Uuid& task_id,
    const GURL& expected_url,
    base::OnceCallback<void(ContextualTasksWindowTracker*)> on_closed_callback)
    : task_id_(task_id),
      expected_url_(expected_url),
      on_closed_callback_(std::move(on_closed_callback)) {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  if (current_tab_list) {
    current_tab_list->AddTabListInterfaceObserver(this);
    observed_tab_lists_.insert(current_tab_list);
  }
  timeout_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(&ContextualTasksWindowTracker::OnWindowClosed,
                     base::Unretained(this)));
  OMNIBOX_LOG("window_tracking")
      << "ContextualTasksWindowTracker created for task: "
      << task_id_.AsLowercaseString() << ", expected URL: " << expected_url_;
}

ContextualTasksWindowTracker::~ContextualTasksWindowTracker() {
  for (TabListInterface* tab_list : observed_tab_lists_) {
    tab_list->RemoveTabListInterfaceObserver(this);
  }
}

void ContextualTasksWindowTracker::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (tracked_tab_) {
    return;
  }
  TabListInterface* tab_list = TabListInterface::From(browser);
  if (tab_list && !observed_tab_lists_.contains(tab_list)) {
    tab_list->AddTabListInterfaceObserver(this);
    observed_tab_lists_.insert(tab_list);
  }
}

void ContextualTasksWindowTracker::OnTabAdded(TabListInterface& tab_list,
                                              tabs::TabInterface* tab,
                                              int index) {
  if (tracked_tab_) {
    return;
  }

  if (tab->GetContents()->GetVisibleURL() == expected_url_) {
    tracked_tab_ = tab;

    OMNIBOX_LOG("window_tracking")
        << "ContextualTasksWindowTracker found matching tab for URL: "
        << expected_url_;
    timeout_timer_.Stop();

    // Stop observing GlobalBrowserCollection.
    browser_collection_observation_.Reset();

    // Stop observing all other tab lists except the one containing the tab.
    for (TabListInterface* observed_list : observed_tab_lists_) {
      if (observed_list != &tab_list) {
        observed_list->RemoveTabListInterfaceObserver(this);
      }
    }
    observed_tab_lists_.clear();
    observed_tab_lists_.insert(&tab_list);
  }
}

void ContextualTasksWindowTracker::OnTabRemoved(
    TabListInterface& tab_list,
    tabs::TabInterface* tab,
    TabRemovedReason removed_reason) {
  if (tab == tracked_tab_) {
    OMNIBOX_LOG("window_tracking")
        << "ContextualTasksWindowTracker::OnTabRemoved matched tracked tab";
    OnWindowClosed();
  }
}

void ContextualTasksWindowTracker::OnTabListDestroyed(
    TabListInterface& tab_list) {
  bool erased = observed_tab_lists_.erase(&tab_list) > 0;
  if (erased) {
    if (tracked_tab_ || observed_tab_lists_.empty()) {
      OMNIBOX_LOG("window_tracking")
          << "ContextualTasksWindowTracker::OnTabListDestroyed "
             "triggering closure";
      OnWindowClosed();
    }
  }
}

void ContextualTasksWindowTracker::OnWindowClosed() {
  OMNIBOX_LOG("window_tracking")
      << "ContextualTasksWindowTracker::OnWindowClosed called for task: "
      << task_id_.AsLowercaseString();

  if (on_closed_callback_) {
    // Post as a task to avoid deleting `this` synchronously if called during
    // observer notifications.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_closed_callback_), this));
  }
}

}  // namespace contextual_tasks
