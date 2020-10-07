// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_SETTINGS_LAUNCHER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_SETTINGS_LAUNCHER_ANDROID_H_

namespace content {
class WebContents;
}

namespace safe_browsing {

// Opens the Safe Browsing settings page on Android.
void ShowSafeBrowsingSettings(content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_SAFE_BROWSING_SETTINGS_LAUNCHER_ANDROID_H_
