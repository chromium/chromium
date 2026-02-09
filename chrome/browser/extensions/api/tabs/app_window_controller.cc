// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/app_window_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/extensions/api/tabs/app_base_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/common/extensions/api/tabs.h"
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
  return api::tabs::ToString(api::tabs::WindowType::kApp);
}

void AppWindowController::SetFullscreenMode(bool is_fullscreen,
                                            const GURL& extension_url) const {
  // Full screen not supported by app windows.
}

Browser* AppWindowController::GetBrowser() const {
  return nullptr;
}

content::WebContents* AppWindowController::GetActiveTab() const {
  return app_window_->web_contents();
}

int AppWindowController::GetTabCount() const {
  return 1;  // Only one "tab" in an app window.
}

content::WebContents* AppWindowController::GetWebContentsAt(int i) const {
  return i == 0 ? app_window_->web_contents() : nullptr;
}

bool AppWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool allow_dev_tools_windows) const {
  DCHECK(extension);
  return extension->id() == app_window_->extension_id();
}

base::DictValue AppWindowController::CreateWindowValueForExtension(
    const Extension* extension,
    PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) const {
  return base::DictValue();
}

base::ListValue AppWindowController::CreateTabList(
    const Extension* extension,
    mojom::ContextType context) const {
  return base::ListValue();
}

bool AppWindowController::OpenOptionsPage(const Extension* extension,
                                          const GURL& url,
                                          bool open_in_tab) {
  return false;
}

}  // namespace extensions
