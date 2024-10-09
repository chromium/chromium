// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"
#include "content/public/browser/browser_thread.h"

// static
Browser* LockedSessionWindowTracker::GetBrowserWithTab(
    content::WebContents* tab) {
  BrowserList* const browser_list = BrowserList::GetInstance();
  for (auto browser_iterator =
           browser_list->begin_browsers_ordered_by_activation();
       browser_iterator != browser_list->end_browsers_ordered_by_activation();
       ++browser_iterator) {
    Browser* const browser = *browser_iterator;
    if (browser && browser->tab_strip_model()->GetIndexOfWebContents(tab) !=
                       TabStripModel::kNoTab) {
      return browser;
    }
  }
  return nullptr;
}

LockedSessionWindowTracker::LockedSessionWindowTracker(
    std::unique_ptr<OnTaskBlocklist> on_task_blocklist)
    : on_task_blocklist_(std::move(on_task_blocklist)) {}

LockedSessionWindowTracker::~LockedSessionWindowTracker() {
  CleanupWindowTracker();
}

void LockedSessionWindowTracker::InitializeBrowserInfoForTracking(
    Browser* browser) {
  if (browser_ && browser_ != browser) {
    CleanupWindowTracker();
  }
  if (!browser || browser == browser_) {
    return;
  }
  browser_ = browser;
  browser_->tab_strip_model()->AddObserver(this);
  if (!browser_list_observation_.IsObserving()) {
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }
}

void LockedSessionWindowTracker::RefreshUrlBlocklist() {
  if (!browser_ || !browser_->tab_strip_model()->GetActiveWebContents() ||
      !browser_->tab_strip_model()
           ->GetActiveWebContents()
           ->GetLastCommittedURL()
           .is_valid()) {
    return;
  }

  on_task_blocklist_->RefreshForUrlBlocklist(
      browser_->tab_strip_model()->GetActiveWebContents());
}

void LockedSessionWindowTracker::MaybeCloseBrowser(
    base::WeakPtr<Browser> weak_browser_ptr) {
  Browser* const browser = weak_browser_ptr.get();
  // If tracking browser is in locked fullscreen mode, we may need to explicitly
  // close a browser when either a new window is opened from the OnTask SWA that
  // is blocked, but is not closed or when an OAuth is completed, but since
  // OnTask prevents windows from closing, we need to manually close that window
  // when the OAuth is completed.
  if (!browser || browser == browser_ ||
      (browser_ && !platform_util::IsBrowserLockedFullscreen(browser_)) ||
      (browser->is_type_app_popup() && oauth_in_progress_)) {
    return;
  }
  browser->window()->Close();
}

void LockedSessionWindowTracker::MaybeCloseWebContents(
    base::WeakPtr<content::WebContents> weak_tab_ptr) {
  content::WebContents* const tab = weak_tab_ptr.get();
  if (browser_->tab_strip_model()->count() > 1) {
    int index = browser_->tab_strip_model()->GetIndexOfWebContents(tab);
    if (index == TabStripModel::kNoTab) {
      return;
    }
    on_task_blocklist()->RemoveChildFilter(tab);
    browser_->tab_strip_model()->CloseWebContentsAt(index,
                                                    TabCloseTypes::CLOSE_NONE);
  }
}

void LockedSessionWindowTracker::ObserveWebContents(
    content::WebContents* web_content) {
  Observe(web_content);
}

void LockedSessionWindowTracker::set_can_start_navigation_throttle(
    bool is_ready) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  can_start_navigation_throttle_ = is_ready;
}

void LockedSessionWindowTracker::SetActiveTabTracker(
    ash::boca::ActiveTabTracker* active_tab_tracker) {
  active_tab_tracker_ = active_tab_tracker;
}

OnTaskBlocklist* LockedSessionWindowTracker::on_task_blocklist() {
  return on_task_blocklist_.get();
}

Browser* LockedSessionWindowTracker::browser() {
  return browser_;
}

bool LockedSessionWindowTracker::CanOpenNewPopup() {
  return can_open_new_popup_;
}

void LockedSessionWindowTracker::CleanupWindowTracker() {
  if (browser_) {
    browser_->tab_strip_model()->RemoveObserver(this);
    browser_list_observation_.Reset();
  }
  if (on_task_blocklist_) {
    on_task_blocklist_->CleanupBlocklist();
  }
  browser_ = nullptr;
  can_open_new_popup_ = true;
  oauth_in_progress_ = false;
  active_tab_tracker_ = nullptr;
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(false);
  }
}

// TabStripModel Implementation
void LockedSessionWindowTracker::TabChangedAt(content::WebContents* contents,
                                              int index,
                                              TabChangeType change_type) {
  if (change_type == TabChangeType::kAll) {
    RefreshUrlBlocklist();
  }
}

void LockedSessionWindowTracker::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    RefreshUrlBlocklist();
    if (active_tab_tracker_ && selection.new_contents) {
      active_tab_tracker_->OnActiveTabChanged(
          selection.new_contents->GetTitle());
    }
  }
}

void LockedSessionWindowTracker::OnTabWillBeRemoved(
    content::WebContents* contents,
    int index) {
  on_task_blocklist()->RemoveParentFilter(contents);
}

void LockedSessionWindowTracker::WillCloseAllTabs(
    TabStripModel* tab_strip_model) {
  CHECK(tab_strip_model);

  // We block tab unload when the browser instance is locked for OnTask. We need
  // to unset the relevant boolean to unblock tab unload so it can proceed with
  // the close operation.
  Browser* const browser = static_cast<Browser*>(
      tab_strip_model->delegate()->GetBrowserWindowInterface());
  browser->SetLockedForOnTask(false);
}

// BrowserListObserver Implementation
void LockedSessionWindowTracker::OnBrowserClosing(Browser* browser) {
  if (browser == browser_) {
    CleanupWindowTracker();
  }
  if (browser->type() == Browser::Type::TYPE_APP_POPUP) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(false);
    can_open_new_popup_ = true;
    oauth_in_progress_ = false;
  }
}

void LockedSessionWindowTracker::OnBrowserAdded(Browser* browser) {
  if (browser->type() == Browser::Type::TYPE_APP_POPUP) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(true);
    // Since this is called after the window is created, but before we set the
    // pinning controller to allow the popup window to be on top of the
    // pinned window, we need to explicitly move this `browser` to be on top.
    // Otherwise, the popup window would still be beneath the pinned window.
    aura::Window* const top_container =
        ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                 ash::kShellWindowId_AlwaysOnTopContainer);
    top_container->StackChildAtTop(browser->window()->GetNativeWindow());
    can_open_new_popup_ = false;
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseBrowser,
                       weak_pointer_factory_.GetWeakPtr(),
                       browser->AsWeakPtr()));
  }
}

// content::WebContentsObserver Impl
void LockedSessionWindowTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  Browser* const browser =
      GetBrowserWithTab(navigation_handle->GetWebContents());
  if (!browser || !browser_) {
    return;
  }
  if (browser != browser_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseBrowser,
                       weak_pointer_factory_.GetWeakPtr(),
                       browser->AsWeakPtr()));
  } else {
    content::WebContents* const tab = navigation_handle->GetWebContents();
    if (!tab || tab->GetLastCommittedURL().is_valid() ||
        on_task_blocklist()->IsParentTab(tab)) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseWebContents,
                       weak_pointer_factory_.GetWeakPtr(), tab->GetWeakPtr()));
  }
}
