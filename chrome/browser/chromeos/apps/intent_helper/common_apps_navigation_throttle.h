// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_COMMON_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_COMMON_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

// Allows navigation to be routed to an installed app, and provides
// a static method for showing an intent picker for the current URL to display
// any handling apps. This is a common throttle that work with the App Service.
// This only works with Chrome OS at the moment and will work with all platforms
// after the App Service supports apps for all platforms.
// TODO(crbug.com/853604): Add metrics, add ARC auto pop up, add persistency.
class CommonAppsNavigationThrottle : public apps::AppsNavigationThrottle {
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

  // Called when the intent picker is closed for |url|, in |web_contents|, with
  // |launch_name| as the (possibly empty) action to be triggered based on
  // |entry_type|. |close_reason| gives the reason for the picker being closed,
  // and |should_persist| is true if the user indicated they wish to remember
  // the choice made. |ui_auto_display_service| keeps track of whether or not
  // the user dismissed the ui without engaging with it.
  static void OnIntentPickerClosed(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url,
      const std::string& launch_name,
      apps::PickerEntryType entry_type,
      apps::IntentPickerCloseReason close_reason,
      bool should_persist);

  explicit CommonAppsNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~CommonAppsNavigationThrottle() override;

 private:
  std::vector<IntentPickerAppInfo> FindAppsForUrl(
      content::WebContents* web_contents,
      const GURL& url,
      std::vector<IntentPickerAppInfo> apps) override;

  static std::vector<apps::IntentPickerAppInfo> FindAllAppsForUrl(
      content::WebContents* web_contents,
      const GURL& url,
      std::vector<apps::IntentPickerAppInfo> apps);

  PickerShowState GetPickerShowState(
      const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
      content::WebContents* web_contents,
      const GURL& url) override;

  IntentPickerResponse GetOnPickerClosedCallback(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url) override;

  DISALLOW_COPY_AND_ASSIGN(CommonAppsNavigationThrottle);
};

}  // namespace apps

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_COMMON_APPS_NAVIGATION_THROTTLE_H_
