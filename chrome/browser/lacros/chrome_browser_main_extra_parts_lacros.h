// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
#define CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

#include <memory>

class AutomationManagerLacros;
class BrowserServiceLacros;
class DownloadControllerClientLacros;
class LacrosExtensionAppsController;
class LacrosExtensionAppsPublisher;

namespace crosapi {
class TaskManagerLacros;
class WebPageInfoProviderLacros;
}  // namespace crosapi

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

  std::unique_ptr<AutomationManagerLacros> automation_manager_;

  // Handles browser action requests from ash-chrome.
  std::unique_ptr<BrowserServiceLacros> browser_service_;

  // Handles task manager crosapi from ash for sending lacros tasks to ash.
  std::unique_ptr<crosapi::TaskManagerLacros> task_manager_provider_;

  // Sends lacros download information to ash.
  std::unique_ptr<DownloadControllerClientLacros> download_controller_client_;

  // Handles tab property requests from ash.
  std::unique_ptr<crosapi::WebPageInfoProviderLacros> web_page_info_provider_;

  // Receives extension app events from ash.
  std::unique_ptr<LacrosExtensionAppsController> extension_apps_controller_;

  // Sends extension app events to ash.
  std::unique_ptr<LacrosExtensionAppsPublisher> extension_apps_publisher_;
};

#endif  // CHROME_BROWSER_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_LACROS_H_
