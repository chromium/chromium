// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_tab_strip_model_observer.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

namespace page_content_annotations {

PageContentExtractionTabStripModelObserver::
    PageContentExtractionTabStripModelObserver(
        Profile* profile,
        PageContentExtractionService* service)
    : profile_(CHECK_DEREF(profile)), service_(CHECK_DEREF(service)) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&*profile_);
  CHECK(collection);
  browser_collection_observation_.Observe(collection);
  // Attach to any browser windows created before this service (e.g. when
  // `features::kLazyKeyedServiceInstantiationOptimizationGuide` is enabled or
  // in tests).
  collection->ForEach(
      [this](BrowserWindowInterface* browser) {
        OnBrowserCreated(browser);
        return true;
      },
      BrowserCollection::Order::kCreation);
}

PageContentExtractionTabStripModelObserver::
    ~PageContentExtractionTabStripModelObserver() = default;

void PageContentExtractionTabStripModelObserver::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  browser->GetTabStripModel()->AddObserver(this);
}

void PageContentExtractionTabStripModelObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kRemoved:
      if (ShouldPreserveCacheOnTabClose(tab_strip_model)) {
        break;
      }
      for (const auto& removed_tab : change.GetRemove()->contents) {
        // Only evict when the tab's `WebContents` is actually being destroyed
        // (not when detaching it to move to another window or side panel).
        if (TabRemoveReasonUtils::WillDeleteWebContents(
                removed_tab.remove_reason) &&
            removed_tab.contents) {
          EvictFromCache(removed_tab.contents);
        }
      }
      break;
    case TabStripModelChange::kReplaced: {
      // When `features::kWebContentsDiscard` is disabled, Memory Saver discards
      // a tab by swapping out its `WebContents` via `DiscardWebContents()`,
      // which assigns a new `SessionID` to `new_contents`. (With
      // `kWebContentsDiscard` enabled, the `WebContents` and its `SessionID`
      // are preserved in place.) Evict the old `SessionID` so its entry is not
      // orphaned.
      // TODO(b/579119200): Use `UpdateTabId()` to transfer the cached
      // entry from `old_contents` to `new_contents` instead of evicting it.
      if (content::WebContents* old_contents =
              change.GetReplace()->old_contents) {
        EvictFromCache(old_contents);
      }
      break;
    }
    case TabStripModelChange::kInserted:
    case TabStripModelChange::kSelectionOnly:
    case TabStripModelChange::kMoved:
      break;
  }
}

void PageContentExtractionTabStripModelObserver::EvictFromCache(
    content::WebContents* contents) {
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  CHECK(tab_id.is_valid());
  // TODO(b/550510181): Consider renaming `OnTabClosed()` on
  // `PageContentExtractionService` and `PageContentCacheHandler` to better
  // reflect cache eviction (e.g. `EvictCachedContentForTab()`).
  service_->OnTabClosed(tab_id.id());
}

bool PageContentExtractionTabStripModelObserver::ShouldPreserveCacheOnTabClose(
    const TabStripModel* tab_strip_model) const {
  CHECK(tab_strip_model);
  // Quitting Chrome or closing a profile's last normal tabbed window closes all
  // of its tabs during window teardown, but `SessionService` keeps those tabs
  // in the session to be restored on next startup. Preserve their cached
  // entries for session restore.
  if (browser_shutdown::IsTryingToQuit() ||
      (g_browser_process && g_browser_process->IsShuttingDown())) {
    return true;
  }
  // `closing_all()` is true when an entire browser window is closing (whereas
  // closing an individual tab has `closing_all() == false` and should evict).
  if (!tab_strip_model->closing_all()) {
    return false;
  }
  // `ProfileBrowserCollection` includes non-tabbed windows (such as undocked
  // DevTools, apps, and popups/PiP) that `SessionService` either ignores or
  // does not trigger session restore for. Preserve the cache only if the
  // closing window is the profile's last remaining `TYPE_NORMAL` browser;
  // closing one normal window while another normal window stays open evicts the
  // closed window's tabs.
  // TODO(b/579118935): Consider preserving cached entries for closed
  // windows/groups tracked by `TabRestoreService` and re-keying them via
  // `UpdateTabId()` when reopened with Ctrl+Shift+T (`AddRestoredTab`).
  BrowserWindowInterface* closing_browser =
      tab_strip_model->delegate()->GetBrowserWindowInterface();
  if (!closing_browser ||
      closing_browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return false;
  }
  bool has_other_normal_browser = false;
  ProfileBrowserCollection::GetForProfile(&*profile_)
      ->ForEach([&](BrowserWindowInterface* browser) {
        if (browser != closing_browser &&
            browser->GetType() == BrowserWindowInterface::TYPE_NORMAL) {
          has_other_normal_browser = true;
          return false;  // Stop iterating.
        }
        return true;  // Continue iterating.
      });
  return !has_other_normal_browser;
}

}  // namespace page_content_annotations
