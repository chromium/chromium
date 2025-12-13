// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_WINDOWS_LOGS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_WINDOWS_LOGS_COLLECTOR_H_

#include <memory>
#include <unordered_map>

#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace chromeos {

// Collects and observer logs from web contents in app windows.
class KioskAppWindowsLogsCollector
    : public extensions::AppWindowRegistry::Observer {
 public:
  KioskAppWindowsLogsCollector(
      Profile* profile,
      KioskWebContentsObserver::LoggerCallback logger_callback);
  ~KioskAppWindowsLogsCollector() override;

  // `extensions::AppWindowRegistry::Observer` implementation:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

 private:
  void ObserveExistingAppWindows(extensions::AppWindowRegistry* registry);
  void ObserveAppWindow(extensions::AppWindow* app_window);

  KioskWebContentsObserver::LoggerCallback logger_callback_;

  std::unordered_map<extensions::AppWindow*,
                     std::unique_ptr<KioskWebContentsObserver>>
      app_windows_map_;

  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_registry_observer_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_WINDOWS_LOGS_COLLECTOR_H_
