// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROME_CONTENT_BROWSER_CLIENT_ASH_PART_H_
#define CHROME_BROWSER_ASH_CHROME_CONTENT_BROWSER_CLIENT_ASH_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"

class GURL;

namespace ash {

class ChromeContentBrowserClientAshPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientAshPart();

  ChromeContentBrowserClientAshPart(const ChromeContentBrowserClientAshPart&) =
      delete;
  ChromeContentBrowserClientAshPart& operator=(
      const ChromeContentBrowserClientAshPart&) = delete;

  ~ChromeContentBrowserClientAshPart() override;

  // ChromeContentBrowserClientParts:
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;

  static bool UseDefaultFontSizeForTest(const GURL& url);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHROME_CONTENT_BROWSER_CLIENT_ASH_PART_H_
