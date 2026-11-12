// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_standalone_window_controller.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

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

int TestStandaloneWindowController::GetWindowId() const {
  return session_id_.id();
}

std::string TestStandaloneWindowController::GetWindowTypeText() const {
  return api::tabs::ToString(api::tabs::WindowType::kPopup);
}

void TestStandaloneWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {}

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
  const gfx::Rect bounds =
      window() ? (window()->IsMinimized() ? window()->GetRestoredBounds()
                                          : window()->GetBounds())
               : gfx::Rect();
  base::DictValue dict;
  dict.Set("id", GetWindowId());
  dict.Set("focused", window() ? window()->IsActive() : false);
  dict.Set("top", bounds.y());
  dict.Set("left", bounds.x());
  dict.Set("width", bounds.width());
  dict.Set("height", bounds.height());
  dict.Set("type", GetWindowTypeText());
  dict.Set("state",
           window() && window()->IsMinimized() ? "minimized" : "normal");
  dict.Set("incognito", profile()->IsOffTheRecord());
  dict.Set("alwaysOnTop", window() ? window()->GetZOrderLevel() ==
                                         ui::ZOrderLevel::kFloatingWindow
                                   : true);
  if (populate_tab_behavior == kPopulateTabs) {
    dict.Set(ExtensionTabUtil::kTabsKey, CreateTabList(extension, context));
  }
  return dict;
}

base::ListValue TestStandaloneWindowController::CreateTabList(
    const Extension* extension,
    mojom::ContextType context) const {
  base::ListValue list;
  if (web_contents_) {
    list.Append(ExtensionTabUtil::CreateTabObject(
                    web_contents_,
                    ExtensionTabUtil::GetScrubTabBehavior(extension, context,
                                                          web_contents_),
                    extension, nullptr, 0)
                    .ToValue());
  }
  return list;
}

bool TestStandaloneWindowController::OpenOptionsPage(const Extension* extension,
                                                     const GURL& url,
                                                     bool open_in_tab) {
  return false;
}

}  // namespace extensions
