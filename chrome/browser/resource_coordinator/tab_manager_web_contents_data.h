// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_WEB_CONTENTS_DATA_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// Internal class used by TabManager to record the needed data for
// WebContentses.
// TODO(michaelpg): Merge implementation into
// TabActivityWatcher::WebContentsData and expose necessary properties publicly.
class TabManager::WebContentsData
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabManager::WebContentsData> {
 public:
  using LoadingState = resource_coordinator::TabLoadTracker::LoadingState;

  explicit WebContentsData(content::WebContents* web_contents);
  ~WebContentsData() override;

  // WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // Copies the discard state from |old_contents| to |new_contents|.
  static void CopyState(content::WebContents* old_contents,
                        content::WebContents* new_contents);

  // Sets the tab loading state.
  void SetTabLoadingState(LoadingState state) {
    tab_data_.tab_loading_state = state;
  }

  // Returns the loading state of the tab.
  LoadingState tab_loading_state() const { return tab_data_.tab_loading_state; }

  void SetIsInSessionRestore(bool is_in_session_restore) {
    tab_data_.is_in_session_restore = is_in_session_restore;
  }

  bool is_in_session_restore() const { return tab_data_.is_in_session_restore; }

  void SetIsRestoredInForeground(bool is_restored_in_foreground) {
    tab_data_.is_restored_in_foreground = is_restored_in_foreground;
  }

  bool is_restored_in_foreground() const {
    return tab_data_.is_restored_in_foreground;
  }

 private:
  friend class content::WebContentsUserData<TabManager::WebContentsData>;
  // Needed to access tab_data_.
  FRIEND_TEST_ALL_PREFIXES(TabManagerWebContentsDataTest, CopyState);
  FRIEND_TEST_ALL_PREFIXES(TabManagerWebContentsDataTest, TabLoadingState);

  struct Data {
    Data();
    bool operator==(const Data& right) const;
    bool operator!=(const Data& right) const;

    // Current loading state of this tab.
    LoadingState tab_loading_state;
    // True if the tab was created by session restore. Remains true until the
    // end of the first navigation or the tab is closed.
    bool is_in_session_restore;
    // True if the tab was created by session restore and initially foreground.
    bool is_restored_in_foreground;
  };

  // Contains all the needed data for the tab.
  Data tab_data_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_WEB_CONTENTS_DATA_H_
