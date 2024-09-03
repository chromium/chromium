// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_window_desktop.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"

namespace extensions {

ExtensionBrowserWindowDesktop::ExtensionBrowserWindowDesktop(Browser& browser)
    : browser_(browser) {}
ExtensionBrowserWindowDesktop::~ExtensionBrowserWindowDesktop() = default;

Browser* ExtensionBrowserWindowDesktop::GetBrowserObject() const {
  return &browser_.get();
}

int ExtensionBrowserWindowDesktop::GetWindowId() const {
  return browser_->session_id().id();
}

std::string ExtensionBrowserWindowDesktop::GetBrowserWindowTypeText() const {
  if (browser_->is_type_devtools()) {
    return tabs_constants::kWindowTypeValueDevTools;
  }
  // Browser::TYPE_APP_POPUP is considered 'popup' rather than 'app' since
  // chrome.windows.create({type: 'popup'}) uses
  // Browser::CreateParams::CreateForAppPopup().
  if (browser_->is_type_popup() || browser_->is_type_app_popup()) {
    return tabs_constants::kWindowTypeValuePopup;
  }
  if (browser_->is_type_app()) {
    return tabs_constants::kWindowTypeValueApp;
  }
  return tabs_constants::kWindowTypeValueNormal;
}

base::Value::Dict ExtensionBrowserWindowDesktop::CreateWindowValueForExtension(
    const Extension* extension,
    PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) const {
  base::Value::Dict dict;

  dict.Set(tabs_constants::kIdKey, browser_->session_id().id());
  dict.Set(tabs_constants::kWindowTypeKey, GetBrowserWindowTypeText());
  ui::BaseWindow* window = browser_->window();
  dict.Set(tabs_constants::kFocusedKey, window->IsActive());
  const Profile* profile = browser_->profile();
  dict.Set(tabs_constants::kIncognitoKey, profile->IsOffTheRecord());
  dict.Set(tabs_constants::kAlwaysOnTopKey,
           window->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow);

  std::string window_state;
  if (window->IsMinimized()) {
    window_state = tabs_constants::kShowStateValueMinimized;
  } else if (window->IsFullscreen()) {
    window_state = tabs_constants::kShowStateValueFullscreen;
    if (platform_util::IsBrowserLockedFullscreen(&browser_.get())) {
      window_state = tabs_constants::kShowStateValueLockedFullscreen;
    }
  } else if (window->IsMaximized()) {
    window_state = tabs_constants::kShowStateValueMaximized;
  } else {
    window_state = tabs_constants::kShowStateValueNormal;
  }
  dict.Set(tabs_constants::kShowStateKey, window_state);

  gfx::Rect bounds;
  if (window->IsMinimized()) {
    bounds = window->GetRestoredBounds();
  } else {
    bounds = window->GetBounds();
  }
  dict.Set(tabs_constants::kLeftKey, bounds.x());
  dict.Set(tabs_constants::kTopKey, bounds.y());
  dict.Set(tabs_constants::kWidthKey, bounds.width());
  dict.Set(tabs_constants::kHeightKey, bounds.height());

  if (populate_tab_behavior == kPopulateTabs) {
    dict.Set(tabs_constants::kTabsKey, CreateTabList(extension, context));
  }

  return dict;
}

base::Value::List ExtensionBrowserWindowDesktop::CreateTabList(
    const Extension* extension,
    mojom::ContextType context) const {
  base::Value::List tab_list;
  TabStripModel* tab_strip = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
    ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(extension, context, web_contents);
    tab_list.Append(ExtensionTabUtil::CreateTabObject(web_contents,
                                                      scrub_tab_behavior,
                                                      extension, tab_strip, i)
                        .ToValue());
  }

  return tab_list;
}

bool ExtensionBrowserWindowDesktop::GetActiveTab(
    content::WebContents** contents,
    int* optional_tab_id) const {
  DCHECK(contents);

  *contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (*contents) {
    if (optional_tab_id) {
      *optional_tab_id = ExtensionTabUtil::GetTabId(*contents);
    }
    return true;
  }

  return false;
}

}  // namespace extensions
