// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_CHROMEOS_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_CHROMEOS_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "url/gurl.h"

namespace apps {
enum class PickerShowState;
}

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class IntentPickerAutoDisplayService;

namespace chromeos {

// Allows navigation to be routed to an installed app on Chrome OS, and provides
// a static method for showing an intent picker for the current URL to display
// any handling apps.
class ChromeOsAppsNavigationThrottle : public apps::AppsNavigationThrottle {
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

  ChromeOsAppsNavigationThrottle(content::NavigationHandle* navigation_handle,
                                 bool arc_enabled);
  ~ChromeOsAppsNavigationThrottle() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeOsAppsNavigationThrottleTest,
                           TestGetDestinationPlatform);

  static void FindPwaForUrlAndShowIntentPickerForApps(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url,
      std::vector<apps::IntentPickerAppInfo> apps);

  void MaybeRemoveComingFromArcFlag(content::WebContents* web_contents,
                                    const GURL& previous_url,
                                    const GURL& current_url) override;

  void CancelNavigation();

  bool ShouldDeferNavigation(content::NavigationHandle* handle) override;

  // Passed as a callback to allow ARC-specific code to asynchronously inform
  // this object of the apps which can handle this URL, and optionally request
  // that the navigation be completely cancelled (e.g. if a preferred app has
  // been opened).
  void OnDeferredNavigationProcessed(
      apps::AppsNavigationAction action,
      std::vector<apps::IntentPickerAppInfo> apps);


  void CloseTab();

  // Whether or not the intent picker UI should be displayed without the user
  // clicking in the omnibox's icon.
  bool ShouldAutoDisplayUi(
      const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
      content::WebContents* web_contents,
      const GURL& url);

  // Whether or not we should launch preferred ARC apps.
  bool ShouldLaunchPreferredApp(const GURL& url);

  // Append PWA to the app list and show intent picker.
  void AddPwaAndShowIntentPicker(std::vector<apps::IntentPickerAppInfo> apps);

  apps::PickerShowState GetPickerShowState(
      const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
      content::WebContents* web_contents,
      const GURL& url);

  void ShowIntentPickerForApps(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url,
      std::vector<apps::IntentPickerAppInfo> apps,
      IntentPickerResponse callback);

  // True if ARC is enabled, false otherwise.
  const bool arc_enabled_;

  // Points to the service in charge of controlling auto-display for the related
  // UI.
  IntentPickerAutoDisplayService* ui_auto_display_service_;

  base::WeakPtrFactory<ChromeOsAppsNavigationThrottle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeOsAppsNavigationThrottle);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_CHROMEOS_APPS_NAVIGATION_THROTTLE_H_
