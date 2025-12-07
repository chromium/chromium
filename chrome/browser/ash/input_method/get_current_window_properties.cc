// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#include "chrome/browser/ash/input_method/get_current_window_properties.h"

#include <optional>

#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

std::optional<GURL> GetFocusedTabUrl() {
  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetLastUsedBrowser();
  if (browser && browser->GetActiveWebContents()) {
    return browser->GetActiveWebContents()->GetLastCommittedURL();
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
