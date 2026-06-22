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

// Represents different settings pages/fragments within GLIC settings.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.glic
enum class GlicSettingsPage {
  // The main GLIC settings page.
  kMain = 0,
  // The GLIC actor login permissions settings subpage.
  kActorLoginPermissions = 1,
  kMaxValue = kActorLoginPermissions,
};

// Opens the GLIC `settings_page` on Android.
void ShowGlicSettings(GlicSettingsPage settings_page);

// Opens the GLIC signin activity on Android. `web_contents` is used to find the
// activity to display the sign-in sheet.
void ShowSignIn(Profile* profile, content::WebContents* web_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_NAVIGATION_UTILS_ANDROID_H_
