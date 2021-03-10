// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_

#include "base/optional.h"
#include "components/memories/core/visit_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history {
struct HistoryAddPageArgs;
}

namespace memories {
class MemoriesService;
}

class HistoryClustersTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HistoryClustersTabHelper> {
 public:
  ~HistoryClustersTabHelper() override;

  HistoryClustersTabHelper(const HistoryClustersTabHelper&) = delete;
  HistoryClustersTabHelper& operator=(const HistoryClustersTabHelper&) = delete;

  // Called by HistoryTabHelper right before, and after, submitting a new
  // navigation for |web_contents()| to HistoryService. We need close
  // coordination with History's conception of the visit lifetime.
  void WillUpdateHistoryForNavigation(
      content::NavigationHandle* navigation_handle,
      const history::HistoryAddPageArgs& add_page_args);
  void DidUpdateHistoryForNavigation(
      content::NavigationHandle* navigation_handle,
      const history::HistoryAddPageArgs& add_page_args);

 private:
  explicit HistoryClustersTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryClustersTabHelper>;

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  // Helper function to return the Memories service.  May return nullptr.
  memories::MemoriesService* GetMemoriesService();

  // Clustering signals for the currently active visit, if it exists.
  base::Optional<memories::MemoriesVisit> current_visit_data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
