// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_

#include <string>

#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/cpp/browser_app_instance_update.h"
#include "components/services/app_service/public/cpp/browser_window_instance_update.h"

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
  using Type = BrowserAppInstanceUpdate::Type;

  // Update message from Lacros.

  BrowserAppInstance(base::UnguessableToken id,
                     Type type,
                     std::string app_id,
                     aura::Window* window,
                     std::string title,
                     bool is_browser_active,
                     bool is_web_contents_active);
  BrowserAppInstance(BrowserAppInstanceUpdate update, aura::Window* window);
  ~BrowserAppInstance();
  BrowserAppInstance(const BrowserAppInstance&) = delete;
  BrowserAppInstance& operator=(const BrowserAppInstance&) = delete;

  // Updates mutable attributes and returns true if any were updated.
  bool MaybeUpdate(aura::Window* window,
                   std::string title,
                   bool is_browser_active,
                   bool is_web_contents_active);

  BrowserAppInstanceUpdate ToUpdate() const;

  // Immutable attributes.
  const base::UnguessableToken id;
  const Type type;
  const std::string app_id;

  // Mutable attributes.
  // Window may change for an app tab when a window gets dragged, but stays the
  // same for an app window.
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
  BrowserWindowInstance(BrowserWindowInstanceUpdate update,
                        aura::Window* window);
  ~BrowserWindowInstance();
  BrowserWindowInstance(const BrowserWindowInstance&) = delete;
  BrowserWindowInstance& operator=(const BrowserWindowInstance&) = delete;

  bool MaybeUpdate(bool is_active);

  BrowserWindowInstanceUpdate ToUpdate() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string GetAppId() const;
#endif

  // Immutable attributes.
  const base::UnguessableToken id;
  aura::Window* const window;

  // Mutable attributes.
  bool is_active;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_H_
