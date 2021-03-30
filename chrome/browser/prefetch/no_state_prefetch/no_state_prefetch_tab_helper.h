// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_TAB_HELPER_H_
#define CHROME_BROWSER_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace prerender {

class NoStatePrefetchManager;

// Notifies the NoStatePrefetchManager with the events happening in the
// WebContents for NoStatePrefetch.
class NoStatePrefetchTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NoStatePrefetchTabHelper> {
 public:
  ~NoStatePrefetchTabHelper() override;
  NoStatePrefetchTabHelper(const NoStatePrefetchTabHelper&) = delete;
  NoStatePrefetchTabHelper& operator=(const NoStatePrefetchTabHelper&) = delete;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit NoStatePrefetchTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NoStatePrefetchTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_TAB_HELPER_H_
