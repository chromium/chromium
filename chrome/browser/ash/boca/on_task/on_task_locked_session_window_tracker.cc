// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

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
  if (!browser) {
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
  Browser* browser = weak_browser_ptr.get();
  if (!browser) {
    return;
  }
  if (browser != browser_) {
    browser->window()->Close();
  }
}

OnTaskBlocklist* LockedSessionWindowTracker::on_task_blocklist() {
  return on_task_blocklist_.get();
}

Browser* LockedSessionWindowTracker::browser() {
  return browser_;
}

bool LockedSessionWindowTracker::IsFirstTimePopup() {
  return first_time_popup_;
}

void LockedSessionWindowTracker::CleanupWindowTracker() {
  if (browser_) {
    browser_->tab_strip_model()->RemoveObserver(this);
    browser_list_observation_.Reset();
  }
  on_task_blocklist_->CleanupBlocklist();
  browser_ = nullptr;
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
  }
}

// BrowserListObserver Implementation
// TODO: b/355049175 - Allow screen pinning controller to stack popups on top of
// pinned windows.
void LockedSessionWindowTracker::OnBrowserClosing(Browser* browser) {
  if (browser == browser_) {
    CleanupWindowTracker();
  }
}

void LockedSessionWindowTracker::OnBrowserAdded(Browser* browser) {
  if (browser->type() == Browser::Type::TYPE_APP_POPUP) {
    // TODO: b/355049175 - Allow screen pinning controller to stack popups on
    // top of pinned windows.
    if (!first_time_popup_) {
      first_time_popup_ = true;
    }
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseBrowser,
                       weak_pointer_factory_.GetWeakPtr(),
                       browser->AsWeakPtr()));
  }
}
