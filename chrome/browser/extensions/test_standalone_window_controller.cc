// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_standalone_window_controller.h"

#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/common/extensions/api/tabs.h"

namespace extensions {

TestStandaloneWindowController::TestStandaloneWindowController(
    ui::BaseWindow* base_window,
    Profile* profile,
    SessionID session_id,
    content::WebContents* web_contents)
    : WindowController(base_window, profile),
      session_id_(session_id),
      web_contents_(web_contents) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

TestStandaloneWindowController::~TestStandaloneWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

void TestStandaloneWindowController::SetBrowserWindowInterfaceForLookup(
    BrowserWindowInterface* bwi) {
  lookup_bwi_ = bwi;
}

int TestStandaloneWindowController::GetWindowId() const {
  return session_id_.id();
}

std::string TestStandaloneWindowController::GetWindowTypeText() const {
  return api::tabs::ToString(api::tabs::WindowType::kPopup);
}

void TestStandaloneWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {}

BrowserWindowInterface*
TestStandaloneWindowController::GetBrowserWindowInterface() {
  return lookup_bwi_;
}

content::WebContents* TestStandaloneWindowController::GetActiveTab() const {
  return web_contents_;
}

int TestStandaloneWindowController::GetTabCount() const {
  return web_contents_ ? 1 : 0;
}

content::WebContents* TestStandaloneWindowController::GetWebContentsAt(
    int i) const {
  return i == 0 ? web_contents_ : nullptr;
}

bool TestStandaloneWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool include_dev_tools_windows) const {
  return true;
}

base::DictValue TestStandaloneWindowController::CreateWindowValueForExtension(
    const Extension* extension,
    PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) const {
  return base::DictValue();
}

base::ListValue TestStandaloneWindowController::CreateTabList(
    const Extension* extension,
    mojom::ContextType context) const {
  return base::ListValue();
}

bool TestStandaloneWindowController::OpenOptionsPage(const Extension* extension,
                                                     const GURL& url,
                                                     bool open_in_tab) {
  return false;
}

}  // namespace extensions
