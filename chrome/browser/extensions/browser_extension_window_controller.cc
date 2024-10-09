// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include <string>

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tabs.h"
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

}  // anonymous namespace

BrowserExtensionWindowController::BrowserExtensionWindowController(
    Browser* browser)
    : WindowController(browser->window(), browser->profile()),
      browser_(browser) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

BrowserExtensionWindowController::~BrowserExtensionWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int BrowserExtensionWindowController::GetWindowId() const {
  return static_cast<int>(browser_->session_id().id());
}

std::string BrowserExtensionWindowController::GetWindowTypeText() const {
  if (browser_->is_type_devtools()) {
    return api::tabs::ToString(api::tabs::WindowType::kDevtools);
  }
  // Browser::TYPE_APP_POPUP is considered 'popup' rather than 'app' since
  // chrome.windows.create({type: 'popup'}) uses
  // Browser::CreateParams::CreateForAppPopup().
  if (browser_->is_type_popup() || browser_->is_type_app_popup()) {
    return api::tabs::ToString(api::tabs::WindowType::kPopup);
  }
  if (browser_->is_type_app()) {
    return api::tabs::ToString(api::tabs::WindowType::kApp);
  }
  return api::tabs::ToString(api::tabs::WindowType::kNormal);
}

void BrowserExtensionWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {
  if (browser_->window()->IsFullscreen() != is_fullscreen) {
    browser_->ToggleFullscreenModeWithExtension(extension_url);
  }
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

bool BrowserExtensionWindowController::IsDeleteScheduled() const {
  return browser_->is_delete_scheduled();
}

content::WebContents* BrowserExtensionWindowController::GetActiveTab() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool BrowserExtensionWindowController::HasEditableTabStrip() const {
  return browser_->window()->IsTabStripEditable();
}

int BrowserExtensionWindowController::GetTabCount() const {
  return browser_->tab_strip_model()->count();
}

content::WebContents* BrowserExtensionWindowController::GetWebContentsAt(
    int i) const {
  return browser_->tab_strip_model()->GetWebContentsAt(i);
}

bool BrowserExtensionWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool allow_dev_tools_windows) const {
  // TODO(joelhockey): We are assuming that the caller is webui when |extension|
  // is null and allowing access to all windows. It would be better if we could
  // pass in mojom::ContextType or some way to detect caller type.
  // Platform apps can only see their own windows.
  if (extension && extension->is_platform_app())
    return false;

  return !browser_->is_type_devtools() || allow_dev_tools_windows;
}

base::Value::Dict
BrowserExtensionWindowController::CreateWindowValueForExtension(
    const Extension* extension,
    PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) const {
  base::Value::Dict dict;

  dict.Set(extension_misc::kId, browser_->session_id().id());
  dict.Set(kWindowTypeKey, GetWindowTypeText());
  ui::BaseWindow* window = browser_->window();
  dict.Set(kFocusedKey, window->IsActive());
  const Profile* profile = browser_->profile();
  dict.Set(kIncognitoKey, profile->IsOffTheRecord());
  dict.Set(kAlwaysOnTopKey,
           window->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow);

  std::string window_state;
  if (window->IsMinimized()) {
    window_state = kShowStateValueMinimized;
  } else if (window->IsFullscreen()) {
    window_state = kShowStateValueFullscreen;
    if (platform_util::IsBrowserLockedFullscreen(browser_.get())) {
      window_state = kShowStateValueLockedFullscreen;
    }
  } else if (window->IsMaximized()) {
    window_state = kShowStateValueMaximized;
  } else {
    window_state = kShowStateValueNormal;
  }
  dict.Set(kShowStateKey, window_state);

  gfx::Rect bounds;
  if (window->IsMinimized()) {
    bounds = window->GetRestoredBounds();
  } else {
    bounds = window->GetBounds();
  }
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

bool BrowserExtensionWindowController::OpenOptionsPage(
    const Extension* extension) {
  if (!OptionsPageInfo::HasOptionsPage(extension)) {
    return false;
  }

  // Force the options page to open in non-OTR window if the extension is not
  // running in split mode, because it won't be able to save settings from OTR.
  // This version of OpenOptionsPage() can be called from an OTR window via e.g.
  // the action menu, since that's not initiated by the extension.
  Browser* browser_to_use = browser_.get();
  std::unique_ptr<chrome::ScopedTabbedBrowserDisplayer> displayer;
  if (browser_->profile()->IsOffTheRecord() &&
      !IncognitoInfo::IsSplitMode(extension)) {
    displayer = std::make_unique<chrome::ScopedTabbedBrowserDisplayer>(
        browser_->profile()->GetOriginalProfile());
    browser_to_use = displayer->browser();
  }

  GURL url_to_navigate;
  bool open_in_tab = OptionsPageInfo::ShouldOpenInTab(extension);
  if (open_in_tab) {
    // Options page tab is simply e.g. chrome-extension://.../options.html.
    url_to_navigate = OptionsPageInfo::GetOptionsPage(extension);
  } else {
    // Options page tab is Extension settings pointed at that Extension's ID,
    // e.g. chrome://extensions?options=...
    url_to_navigate = GURL(chrome::kChromeUIExtensionsURL);
    GURL::Replacements replacements;
    std::string query =
        base::StringPrintf("options=%s", extension->id().c_str());
    replacements.SetQueryStr(query);
    url_to_navigate = url_to_navigate.ReplaceComponents(replacements);
  }

  // We need to respect path differences because we don't want opening the
  // options page to close a page that might be open to extension content.
  // However, if the options page opens inside the chrome://extensions page, we
  // can override an existing page.
  // Note: ref behavior is to ignore.
  ShowSingletonTabOverwritingNTP(browser_to_use, url_to_navigate,
                                 open_in_tab
                                     ? NavigateParams::RESPECT
                                     : NavigateParams::IGNORE_AND_NAVIGATE);
  return true;
}

bool BrowserExtensionWindowController::SupportsTabs() {
  return !browser_->is_type_devtools();
}

}  // namespace extensions
