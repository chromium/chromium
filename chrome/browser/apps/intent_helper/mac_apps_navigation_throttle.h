// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_MAC_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_MAC_APPS_NAVIGATION_THROTTLE_H_

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"

// This file implements support for the macOS feature of Universal Links,
// introduced in macOS 10.15. See:
// - https://developer.apple.com/ios/universal-links/
// - https://developer.apple.com/documentation/safariservices/sfuniversallink
// - https://crbug.com/981337

namespace apps {

class MacAppsNavigationThrottle : public apps::AppsNavigationThrottle {
 public:
  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to. The user is prompted if they wish to
  // open the app or remain in the browser.
  static std::unique_ptr<apps::AppsNavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // Queries for installed apps which can handle |url|, and displays the intent
  // picker bubble for |web_contents|.
  static void ShowIntentPickerBubble(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url);

  explicit MacAppsNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~MacAppsNavigationThrottle() override;

 private:
  std::vector<IntentPickerAppInfo> FindAppsForUrl(
      content::WebContents* web_contents,
      const GURL& url,
      std::vector<IntentPickerAppInfo> apps) override;

  // Called when the intent picker is closed for |url|, in |web_contents|, with
  // |launch_name| as the (possibly empty) action to be triggered based on
  // |app_type|. |close_reason| gives the reason for the picker being closed,
  // and |should_persist| is true if the user indicated they wish to remember
  // the choice made. |ui_auto_display_service| keeps track of whether or not
  // the user dismissed the ui without engaging with it.
  static void OnIntentPickerClosed(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url,
      const std::string& launch_name,
      PickerEntryType entry_type,
      apps::IntentPickerCloseReason close_reason,
      bool should_persist);

  PickerShowState GetPickerShowState(
      const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
      content::WebContents* web_contents,
      const GURL& url) override;

  IntentPickerResponse GetOnPickerClosedCallback(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url) override;

  DISALLOW_COPY_AND_ASSIGN(MacAppsNavigationThrottle);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_MAC_APPS_NAVIGATION_THROTTLE_H_
