// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_INTERNAL_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_INTERNAL_H_

#include <vector>

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class GURL;

namespace apps {

bool ShouldCheckAppsForUrl(content::WebContents* web_contents);

std::vector<IntentPickerAppInfo> FindPwaForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps);

void ShowIntentPickerBubbleForApps(content::WebContents* web_contents,
                                   std::vector<IntentPickerAppInfo> apps,
                                   bool show_stay_in_chrome,
                                   bool show_remember_selection,
                                   IntentPickerResponse callback);

bool InAppBrowser(content::WebContents* web_contents);

bool ShouldOverrideUrlLoading(const GURL& previous_url,
                              const GURL& current_url);

GURL GetStartingGURL(content::NavigationHandle* navigation_handle);

bool IsNavigateFromLink(content::NavigationHandle* navigation_handle);

void CloseOrGoBack(content::WebContents* web_contents);

bool IsGoogleRedirectorUrlForTesting(const GURL& url);

PickerEntryType GetPickerEntryType(AppType app_type);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_INTERNAL_H_
