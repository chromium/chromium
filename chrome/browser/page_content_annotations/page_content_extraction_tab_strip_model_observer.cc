// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_tab_strip_model_observer.h"

#include <stdint.h>

#include <set>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

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

  // `PageContentExtractionService` is created during `Profile` initialization,
  // before `StartupBrowserCreator` runs later in the same turn to either start
  // `SessionRestore` or open initial startup tabs. Post
  // `RunCleanUpTasksIfNotRestoring()` so it runs after `StartupBrowserCreator`,
  // and subscribe to `SessionRestore` in case a restore is still in progress
  // when the posted task executes.
  session_restore_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
          &PageContentExtractionTabStripModelObserver::OnSessionRestored,
          weak_ptr_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PageContentExtractionTabStripModelObserver::
                                    RunCleanUpTasksIfNotRestoring,
                                weak_ptr_factory_.GetWeakPtr()));
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
      // are preserved in place.) Migrate any cached entry from `old_contents`
      // to `new_contents`.
      // TODO(b/550510181): Content is only flushed to `PageContentCache` on
      // `Visibility::HIDDEN`, and `TabFeatures::WillDiscardContents()` resets
      // `AnnotatedPageContentRequest` before `kReplaced` fires. Flush any
      // in-memory content for occluded or visible tabs before discard.
      const TabStripModelChange::Replace* replace = change.GetReplace();
      SessionID old_tab_id =
          sessions::SessionTabHelper::IdForTab(replace->old_contents);
      SessionID new_tab_id =
          sessions::SessionTabHelper::IdForTab(replace->new_contents);
      CHECK(old_tab_id.is_valid());
      CHECK(new_tab_id.is_valid());
      service_->UpdateTabId(old_tab_id.id(), new_tab_id.id());
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

void PageContentExtractionTabStripModelObserver::OnSessionRestored(
    Profile* profile,
    int num_tabs_restored) {
  if (profile != &*profile_) {
    return;
  }
  RunCleanUpTasks();
}

void PageContentExtractionTabStripModelObserver::
    RunCleanUpTasksIfNotRestoring() {
  // TODO(b/579119725): Consider also deferring startup cleanup while
  // `ExitTypeService::waiting_for_user_to_ack_crash()` is true so post-crash
  // launches showing the "Restore pages?" bubble do not evict pre-crash cache
  // entries before the user clicks "Restore".
  if (SessionRestore::IsRestoring(&*profile_)) {
    // `OnSessionRestored()` will call `RunCleanUpTasks()` once session restore
    // finishes creating tabs and migrating their tab IDs via `UpdateTabId()`.
    return;
  }
  RunCleanUpTasks();
}

void PageContentExtractionTabStripModelObserver::RunCleanUpTasks() {
  if (has_run_cleanup_tasks_) {
    return;
  }
  has_run_cleanup_tasks_ = true;
  session_restore_subscription_ = {};

  std::set<int64_t> active_tab_ids;
  const base::Time now = base::Time::Now();
  const base::TimeDelta max_cache_age =
      base::Days(features::kPageContentCacheMaxCacheAgeInDays.Get());
  ProfileBrowserCollection::GetForProfile(&*profile_)
      ->ForEach([&active_tab_ids, now,
                 max_cache_age](BrowserWindowInterface* browser) {
        for (tabs::TabInterface* tab : *browser->GetTabStripModel()) {
          content::WebContents* contents = tab->GetContents();
          SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
          CHECK(tab_id.is_valid());
          const GURL& url = contents->GetVisibleURL();
          // This can overcount some open tabs that wouldn't pass
          // `IsPageContextEligible()` checks, slightly understating the
          // startup cache-hit rate metric. Since those tabs are never cached,
          // keeping their IDs in `active_tab_ids` has no effect on which
          // cached entries are evicted.
          if (now - contents->GetLastActiveTime() > max_cache_age ||
              !url.SchemeIsHTTPOrHTTPS()) {
            continue;
          }
          active_tab_ids.insert(tab_id.id());
        }
        return true;
      });

  service_->RunCleanUpTasksWithActiveTabs(active_tab_ids);
}

}  // namespace page_content_annotations
