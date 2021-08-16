// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_

#include <string>

class Browser;

namespace content {
class WebContents;
}

namespace apps {

typedef uint32_t WebContentsId;

// An instance of a browser-based app. Can represent either of:
// - apps running inside Browser->WebContents,
// - actual browser instances (a single browser window). In this case `contents`
//   will be null and app ID will be set to |extension_misc::kChromeAppId|.
struct BrowserAppInstance {
  BrowserAppInstance(const BrowserAppInstance&) = delete;
  BrowserAppInstance& operator=(const BrowserAppInstance&) = delete;

  std::string app_id;
  Browser* browser;
  content::WebContents* web_contents;
  WebContentsId web_contents_id;
  bool visible;
  bool active;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
