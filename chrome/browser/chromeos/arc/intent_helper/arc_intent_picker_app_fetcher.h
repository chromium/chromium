// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_INTENT_PICKER_APP_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_INTENT_PICKER_APP_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace arc {

// A class that allow us to retrieve installed ARC apps which can handle
// a particular URL.
class ArcIntentPickerAppFetcher : content::WebContentsObserver {
 public:
  // Retrieves ARC apps which can handle |url| for |web_contents|, and runs
  // |callback| when complete. Does not attempt to open preferred apps.
  static void GetArcAppsForPicker(content::WebContents* web_contents,
                                  const GURL& url,
                                  apps::GetAppsCallback callback);

  // Returns true if the navigation request represented by |handle| should be
  // deferred while ARC is queried for apps, and if so, |callback| will be run
  // asynchronously with the action for the navigation. |callback| will not be
  // run if false is returned. If |should_launch_preferred_app| is set to true,
  // the preferred app (if it exists) will be automatically launched.
  static bool WillGetArcAppsForNavigation(content::NavigationHandle* handle,
                                          apps::AppsNavigationCallback callback,
                                          bool should_launch_preferred_app);

  // Called to launch an ARC app if it was selected by the user, and persist the
  // preference to launch or stay in Chrome if |should_persist| is true. Returns
  // true if an app was launched, and false otherwise.
  static bool MaybeLaunchOrPersistArcApp(const GURL& url,
                                         const std::string& package_name,
                                         bool should_launch,
                                         bool should_persist);

  // Finds |selected_app_package| from the |app_candidates| array and returns
  // the index. If the app is not found, returns |app_candidates.size()|.
  static size_t GetAppIndex(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates,
      const std::string& selected_app_package);

  // TODO(djacobo): Remove this function and instead stop ARC from returning
  // Chrome as a valid app candidate.
  // Records true if |app_candidates| contain one or more apps. When this
  // function is called from OnAppCandidatesReceived, |app_candidates| always
  // contains Chrome (aka intent helper), but the same function doesn't treat
  // this as an app.
  static bool IsAppAvailable(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);

  static bool IsAppAvailableForTesting(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);
  static size_t FindPreferredAppForTesting(
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);

  ~ArcIntentPickerAppFetcher() override;

 private:
  explicit ArcIntentPickerAppFetcher(content::WebContents* web_contents);

  // Asychronously queries ARC for apps which can handle |url|. Runs |callback|
  // with RESUME/CANCEL for the deferred navigation and (if applicable) the list
  // of handling apps. |should_launch_preferred_app| represent whether we want
  // to automatically launch the preferred app (if exists).
  void GetArcAppsForNavigation(mojom::IntentHelperInstance* instance,
                               const GURL& url,
                               apps::AppsNavigationCallback callback,
                               bool should_launch_preferred_app);

  // Asychronously queries ARC for apps which can handle |url|. Runs |callback|
  // with the list of handling apps.
  void GetArcAppsForPicker(mojom::IntentHelperInstance* instance,
                           const GURL& url,
                           apps::GetAppsCallback callback);

  // Determines if there are apps to show the intent picker, or if we should
  // open a preferred app. Runs |callback| to RESUME/CANCEL the navigation which
  // was deferred prior to calling this method, and (if applicable) the list of
  // apps to show in the picker. The current tab is only closed if we continue
  // the navigation on ARC and the current tab was explicitly generated for this
  // navigation.
  void OnAppCandidatesReceivedForNavigation(
      const GURL& url,
      apps::AppsNavigationCallback callback,
      bool should_launch_preferred_app,
      std::vector<mojom::IntentHandlerInfoPtr> app_candidates);

  // Determines if there are apps to show the intent picker. Runs |callback|
  // with the list of apps to show in the picker.
  void OnAppCandidatesReceivedForPicker(
      const GURL& url,
      apps::GetAppsCallback callback,
      std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates);

  // Returns NONE if there is no preferred app given the potential
  // |app_candidates| or if we had an error while checking for preferred apps.
  // Otherwise return the platform where the preferred app lives (for now only
  // ARC and NATIVE_CHROME).
  apps::PreferredPlatform DidLaunchPreferredArcApp(
      const GURL& url,
      const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates);

  // Queries the ArcIntentHelperBridge for ARC app icons for the apps in
  // |app_candidates|. Calls OnAppIconsReceived() when finished.
  void GetArcAppIcons(const GURL& url,
                      std::vector<mojom::IntentHandlerInfoPtr> app_candidates,
                      apps::GetAppsCallback callback);

  void OnAppIconsReceived(
      const GURL& url,
      std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates,
      apps::GetAppsCallback callback,
      std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override;

  // This has to be the last member of the class.
  base::WeakPtrFactory<ArcIntentPickerAppFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcIntentPickerAppFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_ARC_INTENT_PICKER_APP_FETCHER_H_
