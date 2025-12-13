// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_windows_logs_collector.h"

#include "base/functional/bind.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace chromeos {

KioskAppWindowsLogsCollector::KioskAppWindowsLogsCollector(
    Profile* profile,
    KioskWebContentsObserver::LoggerCallback logger_callback)
    : logger_callback_(std::move(logger_callback)) {
  auto* registry = extensions::AppWindowRegistry::Get(profile);
  if (!registry) {
    LOG(ERROR) << "AppWindowRegistry is not initialised hence, "
                  "not collecting app window logs.";
    return;
  }

  app_window_registry_observer_.Observe(registry);
  ObserveExistingAppWindows(registry);
}

KioskAppWindowsLogsCollector::~KioskAppWindowsLogsCollector() = default;

void KioskAppWindowsLogsCollector::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  ObserveAppWindow(app_window);
}

void KioskAppWindowsLogsCollector::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  if (!app_window) {
    return;
  }

  app_windows_map_.erase(app_window);
}

void KioskAppWindowsLogsCollector::ObserveExistingAppWindows(
    extensions::AppWindowRegistry* registry) {
  for (auto& app_window : registry->app_windows()) {
    ObserveAppWindow(app_window);
  }
}

void KioskAppWindowsLogsCollector::ObserveAppWindow(
    extensions::AppWindow* app_window) {
  if (!app_window || !app_window->web_contents() ||
      app_windows_map_.contains(app_window)) {
    return;
  }

  app_windows_map_.emplace(
      app_window,
      std::make_unique<KioskWebContentsObserver>(
          app_window->web_contents(), base::BindRepeating(logger_callback_)));
}

}  // namespace chromeos
