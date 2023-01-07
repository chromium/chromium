// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TABLET_MODE_CHROME_CONTENT_BROWSER_CLIENT_TABLET_MODE_PART_H_
#define CHROME_BROWSER_CHROMEOS_TABLET_MODE_CHROME_CONTENT_BROWSER_CLIENT_TABLET_MODE_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"

class GURL;

class ChromeContentBrowserClientTabletModePart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientTabletModePart();

  ChromeContentBrowserClientTabletModePart(
      const ChromeContentBrowserClientTabletModePart&) = delete;
  ChromeContentBrowserClientTabletModePart& operator=(
      const ChromeContentBrowserClientTabletModePart&) = delete;

  ~ChromeContentBrowserClientTabletModePart() override;

  // ChromeContentBrowserClientParts:
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static bool UseDefaultFontSizeForTest(const GURL& url);
#endif
};

#endif  // CHROME_BROWSER_CHROMEOS_TABLET_MODE_CHROME_CONTENT_BROWSER_CLIENT_TABLET_MODE_PART_H_
