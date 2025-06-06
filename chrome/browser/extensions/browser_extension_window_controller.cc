// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sessions/core/session_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

namespace {

constexpr char kAlwaysOnTopKey[] = "alwaysOnTop";
constexpr char kFocusedKey[] = "focused";
constexpr char kHeightKey[] = "height";
constexpr char kIncognitoKey[] = "incognito";
constexpr char kLeftKey[] = "left";
constexpr char kShowStateKey[] = "state";
constexpr char kTopKey[] = "top";
constexpr char kWidthKey[] = "width";
constexpr char kWindowTypeKey[] = "type";
constexpr char kShowStateValueNormal[] = "normal";
constexpr char kShowStateValueMinimized[] = "minimized";
constexpr char kShowStateValueMaximized[] = "maximized";
constexpr char kShowStateValueFullscreen[] = "fullscreen";
constexpr char kShowStateValueLockedFullscreen[] = "locked-fullscreen";

api::tabs::WindowType GetTabsWindowType(const BrowserWindowInterface* browser) {
  using BrowserType = BrowserWindowInterface::Type;
  const BrowserType type = browser->GetType();
  if (type == BrowserType::TYPE_DEVTOOLS) {
    return api::tabs::WindowType::kDevtools;
  }
  // Browser::TYPE_APP_POPUP is considered 'popup' rather than 'app' since
  // chrome.windows.create({type: 'popup'}) uses
  // Browser::CreateParams::CreateForAppPopup().
  if (type == BrowserType::TYPE_POPUP || type == BrowserType::TYPE_APP_POPUP) {
    return api::tabs::WindowType::kPopup;
  }
  if (type == BrowserType::TYPE_APP) {
    return api::tabs::WindowType::kApp;
  }
  return api::tabs::WindowType::kNormal;
}

BrowserWindow* GetBrowserWindow(BrowserWindowInterface* browser) {
  return browser->GetBrowserForMigrationOnly()->window();
}

}  // anonymous namespace

BrowserExtensionWindowController::BrowserExtensionWindowController(
    BrowserWindowInterface* browser)
    : WindowController(GetBrowserWindow(browser), browser->GetProfile()),
      browser_(browser),
      profile_(browser->GetProfile()),
      window_(GetBrowserWindow(browser)),
      tab_strip_model_(browser->GetTabStripModel()),
      session_id_(browser->GetSessionID()),
      window_type_(GetTabsWindowType(browser)) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

BrowserExtensionWindowController::~BrowserExtensionWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int BrowserExtensionWindowController::GetWindowId() const {
  return static_cast<int>(session_id_.id());
}

std::string BrowserExtensionWindowController::GetWindowTypeText() const {
  return api::tabs::ToString(window_type_);
}

void BrowserExtensionWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {
  if (window_->IsFullscreen() != is_fullscreen) {
    GetBrowser()->ToggleFullscreenModeWithExtension(extension_url);
  }
}

bool BrowserExtensionWindowController::CanClose(Reason* reason) const {
  // Don't let an extension remove the window if the user is dragging tabs
  // in that window.
  if (!window_->IsTabStripEditable()) {
    *reason = WindowController::REASON_NOT_EDITABLE;
    return false;
  }
  return true;
}

Browser* BrowserExtensionWindowController::GetBrowser() const {
  return browser_->GetBrowserForMigrationOnly();
}

bool BrowserExtensionWindowController::IsDeleteScheduled() const {
  return GetBrowser()->is_delete_scheduled();
}

content::WebContents* BrowserExtensionWindowController::GetActiveTab() const {
  return tab_strip_model_->GetActiveWebContents();
}

bool BrowserExtensionWindowController::HasEditableTabStrip() const {
  return window_->IsTabStripEditable();
}

int BrowserExtensionWindowController::GetTabCount() const {
  return tab_strip_model_->count();
}

content::WebContents* BrowserExtensionWindowController::GetWebContentsAt(
    int i) const {
  return tab_strip_model_->GetWebContentsAt(i);
}

bool BrowserExtensionWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool allow_dev_tools_windows) const {
  // TODO(joelhockey): We are assuming that the caller is webui when |extension|
  // is null and allowing access to all windows. It would be better if we could
  // pass in mojom::ContextType or some way to detect caller type.
  // Platform apps can only see their own windows.
  if (extension && extension->is_platform_app()) {
    return false;
  }

  return (window_type_ != api::tabs::WindowType::kDevtools) ||
         allow_dev_tools_windows;
}

base::Value::Dict
BrowserExtensionWindowController::CreateWindowValueForExtension(
    const Extension* extension,
    PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) const {
  base::Value::Dict dict;

  dict.Set(extension_misc::kId, session_id_.id());
  dict.Set(kWindowTypeKey, GetWindowTypeText());
  ui::BaseWindow* window = window_;
  dict.Set(kFocusedKey, window->IsActive());
  const Profile* profile = profile_;
  dict.Set(kIncognitoKey, profile->IsOffTheRecord());
  dict.Set(kAlwaysOnTopKey,
           window->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow);

  const std::string_view window_state = [&]() {
    if (window->IsMinimized()) {
      return kShowStateValueMinimized;
    } else if (window->IsFullscreen()) {
      if (platform_util::IsBrowserLockedFullscreen(GetBrowser())) {
        return kShowStateValueLockedFullscreen;
      }
      return kShowStateValueFullscreen;
    } else if (window->IsMaximized()) {
      return kShowStateValueMaximized;
    }
    return kShowStateValueNormal;
  }();
  dict.Set(kShowStateKey, window_state);

  const gfx::Rect bounds = [window]() {
    if (window->IsMinimized()) {
      return window->GetRestoredBounds();
    }
    return window->GetBounds();
  }();
  dict.Set(kLeftKey, bounds.x());
  dict.Set(kTopKey, bounds.y());
  dict.Set(kWidthKey, bounds.width());
  dict.Set(kHeightKey, bounds.height());

  if (populate_tab_behavior == kPopulateTabs) {
    dict.Set(ExtensionTabUtil::kTabsKey, CreateTabList(extension, context));
  }

  return dict;
}

base::Value::List BrowserExtensionWindowController::CreateTabList(
    const Extension* extension,
    mojom::ContextType context) const {
  base::Value::List tab_list;
  for (int i = 0; i < tab_strip_model_->count(); ++i) {
    content::WebContents* web_contents = tab_strip_model_->GetWebContentsAt(i);
    const ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(extension, context, web_contents);
    tab_list.Append(
        ExtensionTabUtil::CreateTabObject(web_contents, scrub_tab_behavior,
                                          extension, tab_strip_model_, i)
            .ToValue());
  }

  return tab_list;
}

bool BrowserExtensionWindowController::OpenOptionsPage(
    const Extension* extension,
    const GURL& url,
    bool open_in_tab) {
  DCHECK(OptionsPageInfo::HasOptionsPage(extension));

  // Force the options page to open in non-OTR window if the extension is not
  // running in split mode, because it won't be able to save settings from OTR.
  // This version of OpenOptionsPage() can be called from an OTR window via e.g.
  // the action menu, since that's not initiated by the extension.
  Browser* browser_to_use = GetBrowser();
  std::optional<chrome::ScopedTabbedBrowserDisplayer> displayer;
  if (profile_->IsOffTheRecord() && !IncognitoInfo::IsSplitMode(extension)) {
    displayer.emplace(profile_->GetOriginalProfile());
    browser_to_use = displayer->browser();
  }

  // We need to respect path differences because we don't want opening the
  // options page to close a page that might be open to extension content.
  // However, if the options page opens inside the chrome://extensions page, we
  // can override an existing page.
  // Note: ref behavior is to ignore.
  ShowSingletonTabOverwritingNTP(browser_to_use, url,
                                 open_in_tab
                                     ? NavigateParams::RESPECT
                                     : NavigateParams::IGNORE_AND_NAVIGATE);
  return true;
}

bool BrowserExtensionWindowController::SupportsTabs() {
  return window_type_ != api::tabs::WindowType::kDevtools;
}

}  // namespace extensions
