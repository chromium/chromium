// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker_manager.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

ContextualTasksWindowTrackerManager::ContextualTasksWindowTrackerManager() =
    default;
ContextualTasksWindowTrackerManager::~ContextualTasksWindowTrackerManager() {
  for (auto* tab_list : observed_tab_lists_) {
    tab_list->RemoveTabListInterfaceObserver(this);
  }
}

void ContextualTasksWindowTrackerManager::AddTracker(
    std::unique_ptr<ContextualTasksWindowTracker> tracker) {
  window_trackers_.push_back(std::move(tracker));
}

void ContextualTasksWindowTrackerManager::RemoveTracker(
    ContextualTasksWindowTracker* tracker) {
  std::erase_if(window_trackers_,
                [tracker](const auto& ptr) { return ptr.get() == tracker; });
}

void ContextualTasksWindowTrackerManager::RegisterWindow(
    ContextualTaskId task_id,
    const GURL& url,
    ContextualWindowId window_id) {
  for (const auto& tracker : window_trackers_) {
    if (tracker->task_id() == task_id && tracker->expected_url() == url &&
        !tracker->window_id().has_value()) {
      tracker->SetWindowId(window_id);
      break;
    }
  }
}

void ContextualTasksWindowTrackerManager::CloseTrackedWindow(
    ContextualWindowId window_id) {
  for (const auto& tracker : window_trackers_) {
    if (tracker->window_id() == window_id) {
      if (tracker->GetTabWebContents()) {
        tracker->GetTabWebContents()->Close();
      }
      break;
    }
  }
}

bool ContextualTasksWindowTrackerManager::IsTrackedWindow(
    content::WebContents* web_contents) const {
  for (const auto& tracker : window_trackers_) {
    if (tracker->GetTabWebContents() == web_contents) {
      return true;
    }
  }
  return false;
}

ContextualTasksWindowTracker*
ContextualTasksWindowTrackerManager::MatchAndAssociatePendingTracker(
    const GURL& url,
    content::WebContents* source_contents) {
  for (const auto& tracker : window_trackers_) {
    if (tracker->GetTabWebContents() == source_contents) {
      return tracker.get();
    }
    if (tracker->expected_url() == url && !tracker->GetTabWebContents() &&
        tracker->initiator_contents().get() != source_contents) {
      tracker->SetTabWebContents(source_contents);
      return tracker.get();
    }
  }
  return nullptr;
}

void ContextualTasksWindowTrackerManager::ObserveTabList(
    TabListInterface* tab_list) {
  if (observed_tab_lists_.insert(tab_list).second) {
    tab_list->AddTabListInterfaceObserver(this);
  }
}

void ContextualTasksWindowTrackerManager::OnTabAdded(TabListInterface& tab_list,
                                                     tabs::TabInterface* tab,
                                                     int index) {
  content::WebContents* inserted_contents = tab->GetContents();
  if (!inserted_contents) {
    return;
  }

  content::RenderFrameHost* opener_rfh = inserted_contents->GetOpener();

  content::WebContents* opener_contents = nullptr;
  if (opener_rfh) {
    opener_contents = content::WebContents::FromRenderFrameHost(opener_rfh);
  }

  // Try to match by opener first.
  if (opener_contents) {
    for (const auto& tracker : window_trackers_) {
      if (tracker->initiator_contents().get() == opener_contents &&
          !tracker->GetTabWebContents()) {
        tracker->SetTabWebContents(inserted_contents);
        return;
      }
    }
  }

  // If opener matching failed or no opener, try matching by URL!
  // Note: GetVisibleURL() might be empty or about:blank if navigation hasn't
  // started yet. This is a best-effort early match. HandleNavigationImpl
  // will catch it later if it fails here.
  GURL url = inserted_contents->GetVisibleURL();

  for (const auto& tracker : window_trackers_) {
    if (tracker->expected_url() == url && !tracker->GetTabWebContents()) {
      tracker->SetTabWebContents(inserted_contents);
      return;
    }
  }
}

void ContextualTasksWindowTrackerManager::OnTabListDestroyed(
    TabListInterface& tab_list) {
  observed_tab_lists_.erase(&tab_list);
}

}  // namespace contextual_tasks
