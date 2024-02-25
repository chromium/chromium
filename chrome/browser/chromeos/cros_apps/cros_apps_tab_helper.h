// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_TAB_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_TAB_HELPER_H_

#include "base/types/pass_key.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

// CrosAppsTabHelper hooks up WebContents with ChromeOS Apps specific
// functionalities.
class CrosAppsTabHelper
    : public content::WebContentsUserData<CrosAppsTabHelper>,
      public content::WebContentsObserver {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  CrosAppsTabHelper(content::WebContents* web_contents,
                    base::PassKey<CrosAppsTabHelper>);
  CrosAppsTabHelper(const CrosAppsTabHelper&) = delete;
  CrosAppsTabHelper& operator=(const CrosAppsTabHelper&) = delete;
  ~CrosAppsTabHelper() override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<CrosAppsTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_TAB_HELPER_H_
