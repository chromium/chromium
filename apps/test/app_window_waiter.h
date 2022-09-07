// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_TEST_APP_WINDOW_WAITER_H_
#define APPS_TEST_APP_WINDOW_WAITER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace extensions {
class AppWindow;
}

namespace apps {

// Helper class that monitors app windows to wait for a window to
// appear/activated. Use a new instance for each use, one instance will only
// work for one Wait.
class AppWindowWaiter : public extensions::AppWindowRegistry::Observer {
 public:
  AppWindowWaiter(extensions::AppWindowRegistry* registry,
                  const std::string& app_id);
  AppWindowWaiter(const AppWindowWaiter&) = delete;
  AppWindowWaiter& operator=(const AppWindowWaiter&) = delete;
  ~AppWindowWaiter() override;

  // Waits for an AppWindow of the app to be added.
  extensions::AppWindow* Wait();

  // Waits for an AppWindow of the app to be shown.
  extensions::AppWindow* WaitForShown();

  // Waits for an AppWindow of the app to be shown or returns nullptr if the
  // given timeout expires.
  extensions::AppWindow* WaitForShownWithTimeout(base::TimeDelta timeout);

  // Waits for an AppWindow of the app to be activated.
  extensions::AppWindow* WaitForActivated();

  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;
  void OnAppWindowActivated(extensions::AppWindow* app_window) override;

 private:
  enum WaitType {
    WAIT_FOR_NONE,
    WAIT_FOR_ADDED,
    WAIT_FOR_SHOWN,
    WAIT_FOR_ACTIVATED,
  };

  const raw_ptr<extensions::AppWindowRegistry> registry_;
  const std::string app_id_;
  std::unique_ptr<base::RunLoop> run_loop_;
  WaitType wait_type_ = WAIT_FOR_NONE;
  raw_ptr<extensions::AppWindow> window_ = nullptr;
};

}  // namespace apps

#endif  // APPS_TEST_APP_WINDOW_WAITER_H_
