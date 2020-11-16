// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_MAC_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_MAC_INTENT_PICKER_HELPERS_H_

#include <string>
#include <vector>

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace apps {

void LaunchMacApp(const GURL& url, const std::string& launch_name);

std::vector<IntentPickerAppInfo> FindMacAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_MAC_INTENT_PICKER_HELPERS_H_
