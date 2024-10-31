// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#include "chrome/browser/ash/input_method/get_current_window_properties.h"

#include <optional>

#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

std::optional<GURL> GetFocusedTabUrl() {
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
