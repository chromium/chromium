// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/app_window_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"

namespace extensions {

AppWindowController::AppWindowController(
    AppWindow* app_window,
    std::unique_ptr<AppBaseWindow> base_window,
    Profile* profile)
    : WindowController(base_window.get(), profile),
      app_window_(app_window),
      base_window_(std::move(base_window)) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

AppWindowController::~AppWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int AppWindowController::GetWindowId() const {
  return static_cast<int>(app_window_->session_id().id());
}

std::string AppWindowController::GetWindowTypeText() const {
  return tabs_constants::kWindowTypeValueApp;
}

bool AppWindowController::CanClose(Reason* reason) const {
  return true;
}

Browser* AppWindowController::GetBrowser() const {
  return nullptr;
}

bool AppWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool allow_dev_tools_windows) const {
  DCHECK(extension);
  return extension->id() == app_window_->extension_id();
}

}  // namespace extensions
