// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TAB_HELPER_H_
#define CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TAB_HELPER_H_

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// FromGWSNavigationAndKeepAliveRequestTabHelper observes eligible navigations
// and fetch keepalive requests made from Google search result pages (SRP).
// See parent class for more details.
//
// This should only be used on Android where TabFeatures is not available.
class FromGWSNavigationAndKeepAliveRequestTabHelper
    : public FromGWSNavigationAndKeepAliveRequestObserver,
      public content::WebContentsUserData<
          FromGWSNavigationAndKeepAliveRequestTabHelper> {
 public:
  // Not copyable or movable.
  FromGWSNavigationAndKeepAliveRequestTabHelper(
      const FromGWSNavigationAndKeepAliveRequestTabHelper&) = delete;
  FromGWSNavigationAndKeepAliveRequestTabHelper& operator=(
      const FromGWSNavigationAndKeepAliveRequestTabHelper&) = delete;
  ~FromGWSNavigationAndKeepAliveRequestTabHelper() override;

 private:
  friend class content::WebContentsUserData<
      FromGWSNavigationAndKeepAliveRequestTabHelper>;

  explicit FromGWSNavigationAndKeepAliveRequestTabHelper(
      content::WebContents* tab);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TAB_HELPER_H_
