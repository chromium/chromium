// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_WEB_CONTENTS_INSTANCE_ID_UTILS_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_WEB_CONTENTS_INSTANCE_ID_UTILS_H_

#include <optional>
#include <string>

namespace content {
class WebContents;
}

namespace apps {

// Get ID of the app running in WebContents as defined by the instance registry
// and the shelf. Checks for web apps, and extension-based apps (hosted app,
// packaged v1 apps).
//
// For an app running in a tab, non-empty ID will only be returned if the app is
// configured to run in a tab. For an app running in a window, non-empty ID will
// be returned regardless of how the app is confiured to launch.
std::optional<std::string> GetInstanceAppIdForWebContents(
    content::WebContents* tab);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_WEB_CONTENTS_INSTANCE_ID_UTILS_H_
