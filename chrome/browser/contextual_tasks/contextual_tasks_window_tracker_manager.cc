// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker_manager.h"

#include "base/logging.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"
#include "chrome/browser/contextual_tasks/guest_opener_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/omnibox/common/logger.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

ContextualTasksWindowTrackerManager::ContextualTasksWindowTrackerManager(
    Profile* profile)
    : profile_(profile) {
  auto* collection = GlobalBrowserCollection::GetInstance();
  if (collection) {
    browser_collection_observation_.Observe(collection);
    // Observe existing browsers for this profile.
    collection->ForEach([this](BrowserWindowInterface* browser) {
      if (browser->GetProfile()->GetOriginalProfile() ==
          profile_->GetOriginalProfile()) {
        TabListInterface* tab_list = TabListInterface::From(browser);
        if (tab_list) {
          ObserveTabList(tab_list);
        }
      }
      return true;
    });
  }
}

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
  OMNIBOX_LOG("window_tracker") << "RemoveTracker called";
  std::erase_if(window_trackers_,
                [tracker](const auto& ptr) { return ptr.get() == tracker; });
}

void ContextualTasksWindowTrackerManager::RegisterWindow(
    ContextualTaskId task_id,
    const GURL& url,
    ContextualWindowId window_id) {
  OMNIBOX_LOG("window_tracker")
      << "RegisterWindow called for task: "
      << task_id.value().AsLowercaseString() << ", URL: " << url.spec()
      << ", window_id: " << window_id.value().ToString();
  for (const auto& tracker : window_trackers_) {
    OMNIBOX_LOG("window_tracker")
        << "RegisterWindow: checking tracker for task: "
        << tracker->task_id().value().AsLowercaseString()
        << ", expected URL: " << tracker->expected_url().spec()
        << ", has window_id: " << tracker->window_id().has_value();
    if (tracker->task_id() == task_id && tracker->expected_url() == url &&
        !tracker->window_id().has_value()) {
      OMNIBOX_LOG("window_tracker") << "RegisterWindow: matched tracker!";
      tracker->SetWindowId(window_id);
      break;
    }
  }
}

void ContextualTasksWindowTrackerManager::CloseTrackedWindow(
    ContextualWindowId window_id) {
  OMNIBOX_LOG("window_tracker") << "CloseTrackedWindow called for window_id: "
                                << window_id.value().ToString();
  for (const auto& tracker : window_trackers_) {
    if (tracker->window_id() == window_id) {
      if (tracker->GetTabWebContents()) {
        OMNIBOX_LOG("window_tracker") << "CloseTrackedWindow: closing tab";
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
      content::WebContents* initiator_contents =
          tracker->initiator_contents().get();
      OMNIBOX_LOG("window_tracker")
          << "GetPendingTracker: checking pending tracker for URL: "
          << tracker->expected_url().spec() << ", current URL: " << url
          << ", initiator URL: "
          << (initiator_contents ? initiator_contents->GetVisibleURL().spec()
                                 : "null")
          << ", source URL: " << source_contents->GetVisibleURL().spec();
      if (initiator_contents &&
          source_contents == initiator_contents->GetResponsibleWebContents() &&
          url == tracker->expected_url()) {
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
    content::WebContents* source_contents,
    std::unique_ptr<content::WebContents> message_proxy_web_contents) {
  for (const auto& tracker : window_trackers_) {
    if (tracker->GetTabWebContents() == source_contents) {
      if (message_proxy_web_contents) {
        tracker->SetMessageProxyWebContents(
            std::move(message_proxy_web_contents));
      }
      return tracker.get();
    }
    if (tracker->expected_url() == url && !tracker->GetTabWebContents() &&
        tracker->initiator_contents().get() != source_contents) {
      tracker->SetTabWebContents(source_contents);
      if (message_proxy_web_contents) {
        tracker->SetMessageProxyWebContents(
            std::move(message_proxy_web_contents));
      }
      return tracker.get();
    }
  }
  return nullptr;
}

ContextualTasksWindowTracker*
ContextualTasksWindowTrackerManager::FindTrackerByMessageProxy(
    content::WebContents* proxy_contents) {
  for (const auto& tracker : window_trackers_) {
    if (tracker->message_proxy_web_contents() == proxy_contents) {
      return tracker.get();
    }
  }
  return nullptr;
}

void ContextualTasksWindowTrackerManager::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (browser->GetProfile()->GetOriginalProfile() ==
      profile_->GetOriginalProfile()) {
    TabListInterface* tab_list = TabListInterface::From(browser);
    if (tab_list) {
      ObserveTabList(tab_list);
    }
  }
}

void ContextualTasksWindowTrackerManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  // TabListInterfaceObserver::OnTabListDestroyed will handle cleanup of
  // observed tab lists when the browser is destroyed.
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
    bool is_guest_opener = GuestOpenerUserData::IsGuestOpener(opener_contents);
    for (const auto& tracker : window_trackers_) {
      if ((tracker->initiator_contents().get() == opener_contents ||
           is_guest_opener) &&
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
