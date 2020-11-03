// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history {
struct HistoryAddPageArgs;
class HistoryService;
}

class HistoryTabHelper : public content::WebContentsObserver,
                         public content::WebContentsUserData<HistoryTabHelper> {
 public:
  ~HistoryTabHelper() override;

  // If true, visits that do not increment the typed count (see
  // HistoryBackend::IsTypedIncrement()) are marked as hidden. More
  // specifically, this does two things:
  //
  // . |HistoryAddPageArgs::hidden| supplied to HistoryService::AddPage() is set
  //   to true.
  // . The transition type PAGE_TRANSITION_FROM_API_3 is added.
  //
  // This results in the visit not directly influencing the omnibox and not
  // being shown in history ui.
  void set_hide_all_navigations(bool value) { hide_all_navigations_ = value; }

  // Updates history with the specified navigation. This is called by
  // DidFinishNavigation to update history state.
  void UpdateHistoryForNavigation(
      const history::HistoryAddPageArgs& add_page_args);

  // Returns the history::HistoryAddPageArgs to use for adding a page to
  // history.
  history::HistoryAddPageArgs CreateHistoryAddPageArgs(
      const GURL& virtual_url,
      base::Time timestamp,
      int nav_entry_id,
      content::NavigationHandle* navigation_handle);

 private:
  explicit HistoryTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryTabHelper>;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidActivatePortal(content::WebContents* predecessor_contents,
                         base::TimeTicks activation_time) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void WebContentsDestroyed() override;

  // Helper function to return the history service.  May return null.
  history::HistoryService* GetHistoryService();

  // True after navigation to a page is complete and the page is currently
  // loading. Only applies to the main frame of the page.
  bool is_loading_ = false;

  // Number of title changes since the loading of the navigation started.
  int num_title_changes_ = 0;

  // The time that the current page finished loading. Only title changes within
  // a certain time period after the page load is complete will be saved to the
  // history system. Only applies to the main frame of the page.
  base::TimeTicks last_load_completion_;

  // See comment above setter for details.
  bool hide_all_navigations_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelper);
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
