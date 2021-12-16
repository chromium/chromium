// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
#define CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

#include <memory>

class ArcIconCache;
class AutomationManagerLacros;
class BrowserServiceLacros;
class DriveFsCache;
class DownloadControllerClientLacros;
class ForceInstalledTrackerLacros;
class LacrosButterBar;
class LacrosExtensionAppsController;
class LacrosExtensionAppsPublisher;
class KioskSessionServiceLacros;
class FieldTrialObserver;
class StandaloneBrowserTestController;

namespace crosapi {
class TaskManagerLacros;
class WebPageInfoProviderLacros;
}  // namespace crosapi

namespace content {
class ScreenOrientationDelegate;
}  // namespace content

// Browser initialization for Lacros.
class ChromeBrowserMainExtraPartsLacros : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsLacros();
  ChromeBrowserMainExtraPartsLacros(const ChromeBrowserMainExtraPartsLacros&) =
      delete;
  ChromeBrowserMainExtraPartsLacros& operator=(
      const ChromeBrowserMainExtraPartsLacros&) = delete;
  ~ChromeBrowserMainExtraPartsLacros() override;

 private:
  // ChromeBrowserMainExtraParts:
  void PostBrowserStart() override;

  // Receiver and cache of arc icon info updates.
  std::unique_ptr<ArcIconCache> arc_icon_cache_;

  std::unique_ptr<AutomationManagerLacros> automation_manager_;

  // Handles browser action requests from ash-chrome.
  std::unique_ptr<BrowserServiceLacros> browser_service_;

  // Handles task manager crosapi from ash for sending lacros tasks to ash.
  std::unique_ptr<crosapi::TaskManagerLacros> task_manager_provider_;

  // Receiver and cache of drive mount point path updates.
  std::unique_ptr<DriveFsCache> drivefs_cache_;

  // Sends lacros download information to ash.
  std::unique_ptr<DownloadControllerClientLacros> download_controller_client_;

  // Sends lacros installation status of force-installed extensions to ash.
  std::unique_ptr<ForceInstalledTrackerLacros> force_installed_tracker_;

  // Manages the resources used in the web Kiosk session, and sends window
  // status changes of lacros-chrome to ash when necessary.
  std::unique_ptr<KioskSessionServiceLacros> kiosk_session_service_;

  // Handles tab property requests from ash.
  std::unique_ptr<crosapi::WebPageInfoProviderLacros> web_page_info_provider_;

  // Receives extension app events from ash.
  std::unique_ptr<LacrosExtensionAppsController> extension_apps_controller_;

  // Sends extension app events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> extension_apps_publisher_;

  // A test controller that is registered with the ash-chrome's test controller
  // service over crosapi to let tests running in ash-chrome control this Lacros
  // instance. It is only instantiated in Linux builds AND only when Ash's test
  // controller is available (practically, just test binaries), so this will
  // remain null in production builds.
  std::unique_ptr<StandaloneBrowserTestController>
      standalone_browser_test_controller_;

  // Receiver of field trial updates.
  std::unique_ptr<FieldTrialObserver> field_trial_observer_;

  // Shows a butter bar on the first window.
  std::unique_ptr<LacrosButterBar> butter_bar_;

  // Receives orientation lock data.
  std::unique_ptr<content::ScreenOrientationDelegate>
      screen_orientation_delegate_;
};

#endif  // CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
