// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/chrome_browser_main_extra_parts_lacros.h"

#include "chrome/browser/lacros/automation_manager_lacros.h"
#include "chrome/browser/lacros/browser_service_lacros.h"
#include "chrome/browser/lacros/download_controller_client_lacros.h"
#include "chrome/browser/lacros/lacros_extension_apps_controller.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_memory_pressure_evaluator.h"
#include "chrome/browser/lacros/task_manager_lacros.h"
#include "chrome/browser/lacros/web_page_info_lacros.h"

ChromeBrowserMainExtraPartsLacros::ChromeBrowserMainExtraPartsLacros() =
    default;
ChromeBrowserMainExtraPartsLacros::~ChromeBrowserMainExtraPartsLacros() =
    default;

void ChromeBrowserMainExtraPartsLacros::PostBrowserStart() {
  automation_manager_ = std::make_unique<AutomationManagerLacros>();
  browser_service_ = std::make_unique<BrowserServiceLacros>();
  download_controller_client_ =
      std::make_unique<DownloadControllerClientLacros>();
  task_manager_provider_ = std::make_unique<crosapi::TaskManagerLacros>();
  web_page_info_provider_ =
      std::make_unique<crosapi::WebPageInfoProviderLacros>();

  util::MultiSourceMemoryPressureMonitor* monitor =
      static_cast<util::MultiSourceMemoryPressureMonitor*>(
          base::MemoryPressureMonitor::Get());
  if (monitor) {
    monitor->SetSystemEvaluator(std::make_unique<LacrosMemoryPressureEvaluator>(
        monitor->CreateVoter()));
  }

  extension_apps_publisher_ = std::make_unique<LacrosExtensionAppsPublisher>();
  extension_apps_publisher_->Initialize();
  extension_apps_controller_ =
      std::make_unique<LacrosExtensionAppsController>();
  extension_apps_controller_->Initialize(
      extension_apps_publisher_->publisher());
}
