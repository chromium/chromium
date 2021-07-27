// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "extensions/common/extension.h"

LacrosExtensionAppsController::LacrosExtensionAppsController() = default;
LacrosExtensionAppsController::~LacrosExtensionAppsController() = default;

void LacrosExtensionAppsController::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::PauseApp(const std::string& app_id) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::UnpauseApp(const std::string& app_id) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::GetMenuModel(
    const std::string& app_id,
    GetMenuModelCallback callback) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::LoadIcon(const std::string& app_id,
                                             apps::mojom::IconKeyPtr icon_key,
                                             apps::mojom::IconType icon_type,
                                             int32_t size_hint_in_dip,
                                             LoadIconCallback callback) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void LacrosExtensionAppsController::OpenNativeSettings(
    const std::string& app_id) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  bool success =
      lacros_extension_apps_utility::DemuxId(app_id, &profile, &extension);
  if (!success)
    return;

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser) {
    browser =
        Browser::Create(Browser::CreateParams(profile, /*user_gesture=*/true));
  }

  chrome::ShowExtensions(browser, extension->id());
}

void LacrosExtensionAppsController::SetWindowMode(
    const std::string& app_id,
    apps::mojom::WindowMode window_mode) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}
