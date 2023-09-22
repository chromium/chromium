// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_

#include <string>

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace apps {

// Returns true if persistence for PWA entries in the Intent Picker is enabled.
bool IntentPickerPwaPersistenceEnabled();

// Returns the size, in dp, of app icons shown in the intent picker bubble.
int GetIntentPickerBubbleIconSize();

// Returns all of the apps that can be used for the given url. Can includes
// platform-specific apps like mac native apps.
void FindAllAppsForUrl(
    Profile* profile,
    const GURL& url,
    base::OnceCallback<void(std::vector<apps::IntentPickerAppInfo>)> callback);

void LaunchAppFromIntentPicker(content::WebContents* web_contents,
                               const GURL& url,
                               const std::string& launch_name,
                               apps::PickerEntryType app_type);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_HELPERS_H_
