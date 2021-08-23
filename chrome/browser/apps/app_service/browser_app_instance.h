// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_

#include <string>

#include "base/process/process_handle.h"
#include "base/types/id_type.h"

class Browser;

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

namespace apps {

struct WebContentsIdTypeMarker {};

typedef base::IdTypeU32<WebContentsIdTypeMarker> WebContentsId;

// An instance of a browser-based app. Can represent either of:
// - apps running inside Browser->WebContents (in a tab or in a window),
// - Chrome browser instances (a single browser window). In this case the app ID
//   will be set to |extension_misc::kChromeAppId|.
struct BrowserAppInstance {
  enum class Type {
    kAppTab,
    kAppWindow,
    kChromeWindow,
  };

  ~BrowserAppInstance();
  BrowserAppInstance(const BrowserAppInstance&) = delete;
  BrowserAppInstance& operator=(const BrowserAppInstance&) = delete;

  std::string app_id;
  Type type;
  base::ProcessId process_id;
  aura::Window* window;
  WebContentsId web_contents_id;
  bool visible;
  bool active;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
