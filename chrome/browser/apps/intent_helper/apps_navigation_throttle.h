// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace chromeos {
FORWARD_DECLARE_TEST(ChromeOsAppsNavigationThrottleTest,
                     TestGetDestinationPlatform);
}  // namespace chromeos

class IntentPickerAutoDisplayService;

namespace apps {

// Allows navigation to be routed to an installed app on Chrome OS, and provides
// a static method for showing an intent picker for the current URL to display
// any handling apps.
class AppsNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;
  // Restricts the amount of apps displayed to the user without the need of a
  // ScrollView.
  enum { kMaxAppResults = 3 };

  static const char kUseBrowserForLink[];

  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to. The user is prompted if they wish to
  // open the app or remain in the browser.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
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
      PickerEntryType entry_type,
      IntentPickerCloseReason close_reason,
      bool should_persist);

  static bool IsGoogleRedirectorUrlForTesting(const GURL& url);

  static bool ShouldOverrideUrlLoadingForTesting(const GURL& previous_url,
                                                 const GURL& current_url);

  static void ShowIntentPickerBubbleForApps(
      content::WebContents* web_contents,
      std::vector<IntentPickerAppInfo> apps,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      IntentPickerResponse callback);

  explicit AppsNavigationThrottle(content::NavigationHandle* navigation_handle);
  ~AppsNavigationThrottle() override;

  // content::NavigationHandle overrides
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  // These enums are used to define the buckets for an enumerated UMA histogram
  // and need to be synced with the ArcIntentHandlerAction enum in enums.xml.
  // This enum class should also be treated as append-only.
  enum class PickerAction : int {
    // Picker errors occurring after the picker is shown.
    ERROR_AFTER_PICKER = 0,
    // DIALOG_DEACTIVATED keeps track of the user dismissing the UI via clicking
    // the close button or clicking outside of the IntentPickerBubbleView
    // surface. As with CHROME_PRESSED, the user stays in Chrome, however we
    // keep both options since CHROME_PRESSED is tied to an explicit intent of
    // staying in Chrome, not only just getting rid of the
    // IntentPickerBubbleView UI.
    DIALOG_DEACTIVATED = 1,
    OBSOLETE_ALWAYS_PRESSED = 2,
    OBSOLETE_JUST_ONCE_PRESSED = 3,
    PREFERRED_ACTIVITY_FOUND = 4,
    // The prefix "CHROME"/"ARC_APP"/"PWA_APP" determines whether the user
    // pressed [Stay in Chrome] or [Use app] at IntentPickerBubbleView.
    // "PREFERRED" denotes when the user decides to save this selection, whether
    // an app or Chrome was selected.
    CHROME_PRESSED = 5,
    CHROME_PREFERRED_PRESSED = 6,
    ARC_APP_PRESSED = 7,
    ARC_APP_PREFERRED_PRESSED = 8,
    PWA_APP_PRESSED = 9,
    // Picker errors occurring before the picker is shown.
    ERROR_BEFORE_PICKER = 10,
    INVALID = 11,
    DEVICE_PRESSED = 12,
    MAC_NATIVE_APP_PRESSED = 13,
    kMaxValue = MAC_NATIVE_APP_PRESSED,
  };

  // As for PickerAction, these define the buckets for an UMA histogram, so this
  // must be treated in an append-only fashion. This helps specify where a
  // navigation will continue. Must be kept in sync with the
  // ArcIntentHandlerDestinationPlatform enum in enums.xml.
  enum class Platform : int {
    ARC = 0,
    CHROME = 1,
    PWA = 2,
    DEVICE = 3,
    MAC_NATIVE = 4,
    kMaxValue = MAC_NATIVE,
  };

  // TODO(ajlinker): move these two functions below to IntentHandlingMetrics.
  // Determines the destination of the current navigation. We know that if the
  // |picker_action| is either ERROR or DIALOG_DEACTIVATED the navigation MUST
  // stay in Chrome, and when |picker_action| is PWA_APP_PRESSED the navigation
  // goes to a PWA. Otherwise we can assume the navigation goes to ARC with the
  // exception of the |selected_launch_name| being Chrome.
  static Platform GetDestinationPlatform(
      const std::string& selected_launch_name,
      PickerAction picker_action);

  // Converts the provided |entry_type|, |close_reason| and |should_persist|
  // boolean to a PickerAction value for recording in UMA.
  static PickerAction GetPickerAction(PickerEntryType entry_type,
                                      IntentPickerCloseReason close_reason,
                                      bool should_persist);

 protected:
  // These enums are used to define the intent picker show state, whether the
  // picker is popped out or just displayed as a clickable omnibox icon.
  enum class PickerShowState {
    kOmnibox = 1,  // Only show the intent icon in the omnibox
    kPopOut = 2,   // show the intent picker icon and pop out bubble
  };

  // Checks whether we can create the apps_navigation_throttle.
  static bool CanCreate(content::WebContents* web_contents);

  // This is a wrapper method for querying apps for a URL. Normally this
  // method will simply querying PWAs that can handle the URL from. If we are
  // using App Service Intent Handling to support intent picker (currently not
  // feature complete), this method will query all types of apps that that could
  // handle the URL from CommonAppsNavigationThrottle.
  virtual std::vector<IntentPickerAppInfo> FindAppsForUrl(
      content::WebContents* web_contents,
      const GURL& url,
      std::vector<IntentPickerAppInfo> apps);

  // If an installed PWA exists that can handle |url|, prepends it to |apps| and
  // returns the new list.
  static std::vector<IntentPickerAppInfo> FindPwaForUrl(
      content::WebContents* web_contents,
      const GURL& url,
      std::vector<IntentPickerAppInfo> apps);

  static void CloseOrGoBack(content::WebContents* web_contents);

  static bool ContainsOnlyPwasAndMacApps(
      const std::vector<apps::IntentPickerAppInfo>& apps);

  static bool ShouldShowPersistenceOptions(
      std::vector<apps::IntentPickerAppInfo>& apps);

  // Overrides for Chrome OS to allow ARC handling.
  virtual void MaybeRemoveComingFromArcFlag(content::WebContents* web_contents,
                                            const GURL& previous_url,
                                            const GURL& current_url) {}

  virtual bool ShouldDeferNavigation(content::NavigationHandle* handle);

  virtual bool ShouldCancelNavigation(content::NavigationHandle* handle);

  virtual void ShowIntentPickerForApps(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url,
      std::vector<IntentPickerAppInfo> apps,
      IntentPickerResponse callback);

  virtual PickerShowState GetPickerShowState(
      const std::vector<IntentPickerAppInfo>& apps_for_picker,
      content::WebContents* web_contents,
      const GURL& url);

  virtual IntentPickerResponse GetOnPickerClosedCallback(
      content::WebContents* web_contents,
      IntentPickerAutoDisplayService* ui_auto_display_service,
      const GURL& url);

  bool navigate_from_link() const;

  bool ShouldAutoDisplayUi(
      const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
      content::WebContents* web_contents,
      const GURL& url);

  // Keeps track of whether we already shown the UI or preferred app. Since
  // AppsNavigationThrottle cannot wait for the user (due to the non-blocking
  // nature of the feature) the best we can do is check if we launched a
  // preferred app or asked the UI to be shown, this flag ensures we never
  // trigger the UI twice for the same throttle.
  bool ui_displayed_;

  // Points to the service in charge of controlling auto-display for the related
  // UI.
  IntentPickerAutoDisplayService* ui_auto_display_service_;

 private:
  FRIEND_TEST_ALL_PREFIXES(AppsNavigationThrottleTest, TestGetPickerAction);
  FRIEND_TEST_ALL_PREFIXES(AppsNavigationThrottleTest,
                           TestGetDestinationPlatform);
  FRIEND_TEST_ALL_PREFIXES(chromeos::ChromeOsAppsNavigationThrottleTest,
                           TestGetDestinationPlatform);

  // Returns whether navigation to |url| was captured by a web app and what to
  // do next if so.
  base::Optional<ThrottleCheckResult>
  CaptureExperimentalTabStripWebAppScopeNavigations(
      content::WebContents* web_contents,
      content::NavigationHandle* handle) const;

  ThrottleCheckResult HandleRequest();

  // A reference to the starting GURL.
  GURL starting_url_;

  // Keeps track of whether the navigation is coming from a link or not. If the
  // navigation is not from a link, we will not show the pop up for the intent
  // picker bubble.
  bool navigate_from_link_;

  DISALLOW_COPY_AND_ASSIGN(AppsNavigationThrottle);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
