// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEB_CONTENTS_THEME_CLIENT_H_
#define CHROME_BROWSER_ANDROID_WEB_CONTENTS_THEME_CLIENT_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace night_mode {

// Tracks the opted-in status for WebContents-based theme. If an instance of
// this object is attached as UserData to a WebContents on Android then theme
// from the app will be applied to the WebContents. If this object is missing
// the WebContents will not inherit themes from the app. The theme is applied
// in chrome_content_browser_client.
class WebContentsThemeClient
    : public content::WebContentsUserData<WebContentsThemeClient> {
 public:
  bool IsNightModeEnabled();
  bool IsForceDarkWebContentEnabled();

  static void SetIsNightModeEnabledForTesting(
      content::WebContents* web_contents,
      bool enabled);

 private:
  explicit WebContentsThemeClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebContentsThemeClient>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace night_mode

#endif  // CHROME_BROWSER_ANDROID_WEB_CONTENTS_THEME_CLIENT_H_
