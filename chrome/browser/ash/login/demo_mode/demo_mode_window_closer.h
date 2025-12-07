// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_WINDOW_CLOSER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_WINDOW_CLOSER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/widget/widget_observer.h"

// Used to close apps for demo mode when device is idle or to close the GMSCore
// dialog window immediately which disrupts the attract loop during demo mode
// sessions.
class DemoModeWindowCloser : public apps::InstanceRegistry::Observer,
                             public ash::BrowserController::Observer {
 public:
  using LaunchDemoAppCallback = base::RepeatingCallback<void()>;

  explicit DemoModeWindowCloser(LaunchDemoAppCallback launch_demo_app_callback);
  DemoModeWindowCloser(const DemoModeWindowCloser&) = delete;
  DemoModeWindowCloser& operator=(const DemoModeWindowCloser&) = delete;
  ~DemoModeWindowCloser() override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  // ash::BrowserController::Observer:
  void OnLastBrowserClosed() override;

  // Trigger closing apps async.
  void StartClosingApps();

 private:
  // Return true if the GMS core window presented and processed it successfully.
  bool CloseGMSCoreWindowIfPresent(const apps::InstanceUpdate& update);

  void StartClosingWidgets();

  // `True` when start closing browsers triggered by `StartClosingApps` and flip
  // to `false` when all browsers are closed.
  bool is_closing_browsers_ = false;

  // Triggered when all browser are closed and `is_closing_apps_` is true.
  LaunchDemoAppCallback launch_demo_app_callback_;
  std::string gms_core_app_id_;
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      scoped_observation_{this};

  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::Observer>
      browser_observation_{this};

  // Keep track list of open apps with widget:
  std::set<base::UnguessableToken> opened_apps_with_widget_;
};

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_WINDOW_CLOSER_H_
