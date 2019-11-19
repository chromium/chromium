// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_CONTENT_BROWSER_CLIENT_CHROMEOS_PART_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_CONTENT_BROWSER_CLIENT_CHROMEOS_PART_H_

#include "base/macros.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"

class GURL;

class ChromeContentBrowserClientChromeOsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientChromeOsPart();
  ~ChromeContentBrowserClientChromeOsPart() override;

  // ChromeContentBrowserClientParts:
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* web_prefs) override;

  static bool UseDefaultFontSizeForTest(const GURL& url);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientChromeOsPart);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_CONTENT_BROWSER_CLIENT_CHROMEOS_PART_H_
