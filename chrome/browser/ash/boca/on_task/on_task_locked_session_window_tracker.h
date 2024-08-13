// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_SESSION_WINDOW_TRACKER_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_SESSION_WINDOW_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class BrowserList;

// This class is used to track the windows and tabs that are opened in the
// user's OnTask locked session. Only one browser window is allowed at a time to
// be tracked. Attempting to track another browser while there is one already
// tracked will reset the tracker and setup for the new browser. It will be used
// to block the navigation of the tabs that are not allowed to be opened in the
// locked session. Each tab has its set of rules as defined in the
// `OnTaskBlocklist` which determines what types of urls are allowed on a per
// tab basis. See `OnTaskBlocklist` for more details about what the restrictions
// are. All of these calls should be called from the main thread.
class LockedSessionWindowTracker : public KeyedService,
                                   public TabStripModelObserver,
                                   public BrowserListObserver {
 public:
  explicit LockedSessionWindowTracker(
      std::unique_ptr<OnTaskBlocklist> on_task_blocklist);
  LockedSessionWindowTracker(const LockedSessionWindowTracker&) = delete;
  LockedSessionWindowTracker& operator=(const LockedSessionWindowTracker&) =
      delete;
  ~LockedSessionWindowTracker() override;

  // Starts tracking the `browser` for navigation changes.
  void InitializeBrowserInfoForTracking(Browser* browser);

  // Updates the current blocklist with its appropriate restriction. This should
  // rarely be explicitly called except for when we start tracking a new browser
  // window. All other calls should come from tab strip model changes (ex:
  // active tab changes).
  // TODO: b/357139784 - Remove RefreshBlockList.
  void RefreshUrlBlocklist();

  // Checks to make sure this is the first time an OAuth popup has occurred.
  // This is to make sure popup retries don't try to reopen windows while older
  // popups are still open.
  bool IsFirstTimePopup();

  OnTaskBlocklist* on_task_blocklist();
  Browser* browser();

 private:
  // TabStripModelObserver Impl
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserListObserver Implementation
  void OnBrowserClosing(Browser* browser) override;
  void OnBrowserAdded(Browser* browser) override;

  void MaybeCloseBrowser(base::WeakPtr<Browser> weak_browser_ptr);
  void CleanupWindowTracker();

  bool first_time_popup_ = true;
  const std::unique_ptr<OnTaskBlocklist> on_task_blocklist_;
  raw_ptr<Browser> browser_ = nullptr;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::WeakPtrFactory<LockedSessionWindowTracker> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_SESSION_WINDOW_TRACKER_H_
