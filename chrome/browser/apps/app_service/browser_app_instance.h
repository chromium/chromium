// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_

#include <string>

#include "base/unguessable_token.h"

namespace aura {
class Window;
}

namespace content {
class WebContents;
}

namespace apps {

// An instance of an app running in WebContents, either a tab or a window (PWA,
// SWA, hosted app, packaged v1 app).
struct BrowserAppInstance {
  enum class Type {
    kAppTab,
    kAppWindow,
  };

  BrowserAppInstance(base::UnguessableToken id,
                     Type type,
                     std::string app_id,
                     aura::Window* window,
                     std::string title,
                     bool is_browser_active,
                     bool is_web_contents_active);
  ~BrowserAppInstance();
  BrowserAppInstance(const BrowserAppInstance&) = delete;
  BrowserAppInstance& operator=(const BrowserAppInstance&) = delete;
  BrowserAppInstance(BrowserAppInstance&&);
  BrowserAppInstance& operator=(BrowserAppInstance&&);

  // Updates mutable attributes and returns true if any were updated.
  bool MaybeUpdate(aura::Window* window,
                   std::string title,
                   bool is_browser_active,
                   bool is_web_contents_active);

  // Immutable attributes.
  base::UnguessableToken id;
  Type type;
  std::string app_id;

  // Mutable attributes.
  aura::Window* window;
  std::string title;
  bool is_browser_active;
  bool is_web_contents_active;
};

// An instance representing a single Chrome browser window.
struct BrowserWindowInstance {
  BrowserWindowInstance(base::UnguessableToken id,
                        aura::Window* window,
                        bool is_active);
  ~BrowserWindowInstance();
  BrowserWindowInstance(const BrowserWindowInstance&) = delete;
  BrowserWindowInstance& operator=(const BrowserWindowInstance&) = delete;
  BrowserWindowInstance(BrowserWindowInstance&&);
  BrowserWindowInstance& operator=(BrowserWindowInstance&&);

  bool MaybeUpdate(bool is_active);

  // Immutable attributes.
  base::UnguessableToken id;
  aura::Window* window;

  // Mutable attributes.
  bool is_active;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
