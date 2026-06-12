// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker_manager.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/omnibox/common/logger.h"
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
  OMNIBOX_LOG("window_tracker") << "IsTrackedWindow, searching "
                                << window_trackers_.size() << " trackers";
  for (const auto& tracker : window_trackers_) {
    if (tracker->GetTabWebContents() == web_contents) {
      OMNIBOX_LOG("window_tracker")
          << "IsTrackedWindow: matched by WebContents pointer";
      return true;
    }
  }
  OMNIBOX_LOG("window_tracker") << "IsTrackedWindow: not matched";
  return false;
}

bool ContextualTasksWindowTrackerManager::IsPendingWindow(
    const GURL& url,
    content::WebContents* source_contents) const {
  return GetPendingTracker(url, source_contents) != nullptr;
}

ContextualTasksWindowTracker*
ContextualTasksWindowTrackerManager::GetPendingTracker(
    const GURL& url,
    content::WebContents* source_contents) const {
  OMNIBOX_LOG("window_tracker") << "GetPendingTracker, searching "
                                << window_trackers_.size() << " trackers";
  for (const auto& tracker : window_trackers_) {
    // Check pending trackers.
    if (!tracker->GetTabWebContents()) {
      // Note: Matching by URL alone is a heuristic for pending trackers when
      // no opener relationship is available (e.g., when the navigation did not
      // originate from a tracked window). This is sufficient here to enable
      // scripts to close the window, but should be made more robust if
      // possible.
      content::WebContents* initiator = tracker->initiator_contents().get();
      OMNIBOX_LOG("window_tracker")
          << "GetPendingTracker: checking pending tracker for URL: "
          << tracker->expected_url().spec() << ", current URL: " << url
          << ", initiator URL: "
          << (initiator ? initiator->GetVisibleURL().spec() : "null")
          << ", source URL: " << source_contents->GetVisibleURL().spec();
      if (source_contents == initiator && url == tracker->expected_url()) {
        OMNIBOX_LOG("window_tracker")
            << "GetPendingTracker: matched pending tracker by URL: "
            << url.spec();
        return tracker.get();
      }
    }
  }
  return nullptr;
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

  // Try to match by WebContents pointer first, if it was already associated
  // but didn't have a TabInterface yet.
  for (const auto& tracker : window_trackers_) {
    if (tracker->GetTabWebContents() == inserted_contents) {
      tracker->OnTabInterfaceAvailable(tab);
      return;
    }
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
