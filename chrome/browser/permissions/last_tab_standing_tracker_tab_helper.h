// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_TAB_HELPER_H_
#define CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// This class informs LastTabStandingTracker of pages being loaded, navigated or
// destroyed in each tab. This information is then used by the
// OneTimeGeolocationPermissionProvider to revoke permissions.
class LastTabStandingTrackerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LastTabStandingTrackerTabHelper> {
 public:
  ~LastTabStandingTrackerTabHelper() override;

  LastTabStandingTrackerTabHelper(const LastTabStandingTrackerTabHelper&) =
      delete;
  LastTabStandingTrackerTabHelper& operator=(
      const LastTabStandingTrackerTabHelper&) = delete;

  // content::WebContentObserver
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

 private:
  explicit LastTabStandingTrackerTabHelper(content::WebContents* webContents);
  friend class content::WebContentsUserData<LastTabStandingTrackerTabHelper>;
  absl::optional<url::Origin> last_committed_origin_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PERMISSIONS_LAST_TAB_STANDING_TRACKER_TAB_HELPER_H_
