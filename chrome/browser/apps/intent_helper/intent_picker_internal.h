// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_INTERNAL_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_INTERNAL_H_

#include <vector>

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace apps {

void ShowIntentPickerBubbleForApps(content::WebContents* web_contents,
                                   std::vector<IntentPickerAppInfo> apps,
                                   bool show_stay_in_chrome,
                                   bool show_remember_selection,
                                   IntentPickerResponse callback);

void CloseOrGoBack(content::WebContents* web_contents);

PickerEntryType GetPickerEntryType(AppType app_type);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_INTERNAL_H_
