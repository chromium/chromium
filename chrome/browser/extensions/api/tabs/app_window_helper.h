// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_HELPER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_PLATFORM_APPS));

class Profile;

namespace extensions {

class AppWindow;
class AppWindowController;
class WindowController;

// A helper class to handle creating AppWindowControllers for each new
// AppWindow, as well as notifying of window focus changes.
class AppWindowHelper : public AppWindowRegistry::Observer {
 public:
  using ActiveWindowChangedCallback =
      base::RepeatingCallback<void(WindowController*)>;

  AppWindowHelper(Profile* profile, ActiveWindowChangedCallback callback);
  ~AppWindowHelper() override;

  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* app_window) override;
  void OnAppWindowRemoved(AppWindow* app_window) override;
  void OnAppWindowActivated(AppWindow* app_window) override;

 private:
  void AddAppWindow(AppWindow* app_window);

  raw_ptr<Profile> profile_;

  ActiveWindowChangedCallback active_window_changed_callback_;

  // Map of AppWindows. The key is the unique SessionId from the AppWindow.
  using AppWindowMap = std::map<int, std::unique_ptr<AppWindowController>>;
  AppWindowMap app_windows_;

  base::ScopedObservation<AppWindowRegistry, AppWindowRegistry::Observer>
      app_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_HELPER_H_
