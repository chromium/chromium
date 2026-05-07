// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_NAVIGATION_UTILS_ANDROID_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_NAVIGATION_UTILS_ANDROID_H_

class Profile;

namespace content {
class WebContents;
}

namespace glic {

// Opens the GLIC settings page on Android.
void ShowGlicSettings();

// Opens the GLIC signin activity on Android. `web_contents` is used to find the
// activity to display the sign-in sheet.
void ShowSignIn(Profile* profile, content::WebContents* web_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_NAVIGATION_UTILS_ANDROID_H_
