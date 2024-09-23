// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#include "chrome/browser/ash/input_method/get_current_window_properties.h"

#include <optional>

#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

std::optional<GURL> GetAshChromeUrl() {
  Browser* browser = chrome::FindLastActive();
  // Ash chrome will return true for browser->window()->IsActive() if the
  // user is currently typing in an ash browser tab. IsActive() will return
  // false if the user is currently typing a lacros browser tab.
  if (browser && browser->window() && browser->window()->IsActive() &&
      browser->tab_strip_model() &&
      browser->tab_strip_model()->GetActiveWebContents()) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }

  return std::nullopt;
}

void GetLacrosChromeUrl(GetFocusedTabUrlCallback callback) {
  crosapi::BrowserManager* browser_manager = crosapi::BrowserManager::Get();
  // browser_manager will exist whenever there is a lacros browser running.
  // GetActiveTabUrlSupported() will only return true if the current lacros
  // browser is being used by the user.
  if (browser_manager && browser_manager->IsRunning() &&
      browser_manager->GetActiveTabUrlSupported()) {
    browser_manager->GetActiveTabUrl(std::move(callback));
    return;
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace

void GetFocusedTabUrl(GetFocusedTabUrlCallback callback) {
  std::optional<GURL> ash_url = GetAshChromeUrl();
  if (ash_url.has_value()) {
    std::move(callback).Run(ash_url);
    return;
  }

  GetLacrosChromeUrl(std::move(callback));
}

WindowProperties GetFocusedWindowProperties() {
  WindowProperties properties = {.app_id = "", .arc_package_name = ""};
  if (!exo::WMHelper::HasInstance()) {
    return properties;
  }

  auto* wm_helper = exo::WMHelper::GetInstance();
  auto* window = wm_helper ? wm_helper->GetActiveWindow() : nullptr;
  if (!window) {
    return properties;
  }

  const std::string* arc_package_name =
      window->GetProperty(ash::kArcPackageNameKey);
  if (arc_package_name) {
    properties.arc_package_name = *arc_package_name;
  }
  const std::string* app_id = window->GetProperty(ash::kAppIDKey);
  if (app_id) {
    properties.app_id = *app_id;
  }
  return properties;
}
}  // namespace input_method
}  // namespace ash
