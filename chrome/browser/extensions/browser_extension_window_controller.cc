// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include <string>

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/session_id.h"
#include "extensions/common/extension.h"

namespace extensions {

BrowserExtensionWindowController::BrowserExtensionWindowController(
    Browser* browser)
    : WindowController(browser->window(), browser->profile()),
      browser_(browser) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

BrowserExtensionWindowController::~BrowserExtensionWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

void BrowserExtensionWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {
  if (browser_->window()->IsFullscreen() != is_fullscreen)
    browser_->ToggleFullscreenModeWithExtension(extension_url);
}

int BrowserExtensionWindowController::GetWindowId() const {
  return static_cast<int>(browser_->session_id().id());
}

std::string BrowserExtensionWindowController::GetWindowTypeText() const {
  return ExtensionTabUtil::GetBrowserWindowTypeText(*browser_);
}

bool BrowserExtensionWindowController::CanClose(Reason* reason) const {
  // Don't let an extension remove the window if the user is dragging tabs
  // in that window.
  if (!browser_->window()->IsTabStripEditable()) {
    *reason = WindowController::REASON_NOT_EDITABLE;
    return false;
  }
  return true;
}

Browser* BrowserExtensionWindowController::GetBrowser() const {
  return browser_;
}

bool BrowserExtensionWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool allow_dev_tools_windows) const {
  DCHECK(extension);
  // Platform apps can only see their own windows.
  if (extension->is_platform_app())
    return false;

  return !browser_->is_type_devtools() || allow_dev_tools_windows;
}

}  // namespace extensions
