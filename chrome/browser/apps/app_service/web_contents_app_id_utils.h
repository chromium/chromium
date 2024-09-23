// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEB_CONTENTS_APP_ID_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEB_CONTENTS_APP_ID_UTILS_H_

#include <optional>
#include <string>

class Profile;

namespace content {
class WebContents;
}

// TODO(crbug.com/40193527)
// The two functions to get the app ID from a WebContents do it in slightly
// different ways and have different use cases.
//
// For example, GetInstanceAppIdForWebContents (used in the shelf and the app
// service instance registry) will filter out app instances running in tabs if
// they are configured to run in a window (current shelf and instance registry
// logic). GetAppIdForWebContents will only report an extension app in a tab
// opened using certain flows (see bug for details).
//
// Eventually they need to be merged, but in the meantime if you need guidance
// on which one you should use, please reach out to alexbn@chromium.org.

namespace apps {

// Gets the ID of the app via one of either of:
// - |web_app::WebAppTabHelper| associated with this WebContents
// - |extensions::TabHelper| associated with this WebContents
//
// An empty string is returned if the WebContents has neither tab helper
// associated.
std::string GetAppIdForWebContents(content::WebContents* web_contents);

// Sets the ID of the app with the WebContents by either of:
// - for web apps: associating the app ID with WebContents'
//   |web_app::WebAppTabHelper|.
// - for extension apps: associating the app ID with the WebContents'
//   |extensions::TabHelper|.
//
// If the app is neither an extension or a web app, no ID is set.
void SetAppIdForWebContents(Profile* profile,
                            content::WebContents* web_contents,
                            const std::string& app_id);

bool IsInstalledApp(Profile* profile, const std::string& app_id);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEB_CONTENTS_APP_ID_UTILS_H_
