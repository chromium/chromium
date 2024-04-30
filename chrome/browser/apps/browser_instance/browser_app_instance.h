// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
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
                     bool is_web_contents_active,
                     uint32_t browser_session_id,
                     uint32_t restored_browser_session_id);
  BrowserAppInstance(BrowserAppInstanceUpdate update, aura::Window* window);
  ~BrowserAppInstance();
  BrowserAppInstance(const BrowserAppInstance&) = delete;
  BrowserAppInstance& operator=(const BrowserAppInstance&) = delete;

  // Updates mutable attributes and returns true if any were updated.
  bool MaybeUpdate(aura::Window* new_window,
                   std::string new_title,
                   bool new_is_browser_active,
                   bool new_is_web_contents_active,
                   uint32_t new_browser_session_id,
                   uint32_t new_restored_browser_session_id);

  BrowserAppInstanceUpdate ToUpdate() const;

  // TODO(b/332628771): Hide this behind BUILDFLAG(IS_CHROMEOS_ASH) in M127.
  // Checks if `window` is the active window.
  bool is_browser_active() const;

  const base::UnguessableToken id;
  const Type type;
  const std::string app_id;

  // Window may change for an app tab when a window gets dragged, but stays the
  // same for an app window.
  raw_ptr<aura::Window> window;
  std::string title;
  // TODO(b/332628771): Remove this in M127.
  // Use `is_browser_activated()` instead.
  bool is_browser_active_deprecated;
  // If a tab is active in the browser's tab strip. Only applicable to instances
  // with type kAppTab. Always set to true for app instances of type kAppWindow.
  bool is_web_contents_active;
  uint32_t browser_session_id;
  uint32_t restored_browser_session_id;
};

// An instance representing a single Chrome browser window.
struct BrowserWindowInstance {
  BrowserWindowInstance(base::UnguessableToken id,
                        aura::Window* window,
                        uint32_t browser_session_id,
                        uint32_t restored_browser_session_id,
                        bool is_incognito,
                        uint64_t lacros_profile_id,
                        bool is_active);
  BrowserWindowInstance(BrowserWindowInstanceUpdate update,
                        aura::Window* window);
  ~BrowserWindowInstance();
  BrowserWindowInstance(const BrowserWindowInstance&) = delete;
  BrowserWindowInstance& operator=(const BrowserWindowInstance&) = delete;

  bool MaybeUpdate(bool new_is_active);

  BrowserWindowInstanceUpdate ToUpdate() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string GetAppId() const;
#endif

  // TODO(b/332628771): Hide this behind BUILDFLAG(IS_CHROMEOS_ASH) in M127.
  // Checks if `window` is the active window.
  bool is_active() const;

  const base::UnguessableToken id;
  const raw_ptr<aura::Window> window;
  const uint32_t browser_session_id;
  const uint32_t restored_browser_session_id;
  const bool is_incognito;
  // This value will only be non-zero when refer to a lacros browser instance.
  const uint64_t lacros_profile_id;

  // TODO(b/332628771): Remove this in M127.
  // Do not add code which uses this state but use `is_active()` instead.
  bool is_active_deprecated;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_H_
