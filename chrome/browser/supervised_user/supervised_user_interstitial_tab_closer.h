// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_TAB_CLOSER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_TAB_CLOSER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// Helper class for closing an open tab when we go back from a Supervised User
// Intersitial.
class TabCloser : public content::WebContentsUserData<TabCloser> {
 public:
  TabCloser(const TabCloser&) = delete;
  TabCloser& operator=(const TabCloser&) = delete;
  ~TabCloser() override;

  // Closes the tab linked to the present web contents. On non-Android
  // platforms, the tab closes only if it is displayed on a browser.
  static void CheckIfInBrowserThenCloseTab(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<TabCloser>;

  explicit TabCloser(content::WebContents* web_contents);

  void CloseTabImpl();

  base::WeakPtrFactory<TabCloser> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_TAB_CLOSER_H_
