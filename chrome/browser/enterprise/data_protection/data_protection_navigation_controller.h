// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace enterprise_data_protection {

// A WebContentsObserver subclass that is instanstiated once per tab. It
// observes navigations in order to correctly set that tab's Data Protection
// settings based on the SafeBrowsing verdict for said navigation.
class DataProtectionNavigationController : public base::SupportsUserData::Data,
                                           public content::WebContentsObserver {
 public:
  explicit DataProtectionNavigationController(
      content::WebContents* web_contents);

  static void MaybeCreateForWebContents(content::WebContents* web_contents);

 private:
  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_
