// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_

#include <string>

#include "base/process/process_handle.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

namespace apps {

typedef base::UnguessableToken BrowserAppInstanceId;

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

  BrowserAppInstanceId id;
  Type type;
  std::string app_id;
  aura::Window* window;
  // Set for apps of type kAppTab or kAppWindow, nil for kChromeWindow.
  absl::optional<std::string> title;
  bool is_browser_visible;
  bool is_browser_active;
  // Set for apps of type kAppTab or kAppWindow, nil for kChromeWindow.
  absl::optional<bool> is_web_contents_active;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
