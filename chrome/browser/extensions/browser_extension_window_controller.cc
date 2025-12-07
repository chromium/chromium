// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sessions/core/session_id.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "ui/base/base_window.h"

// TODO(http://crbug.com/453008083): Stop including
// "android/chrome_feature_list.h".
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
#if !BUILDFLAG(IS_ANDROID)
constexpr char kShowStateValueLockedFullscreen[] = "locked-fullscreen";
#endif

api::tabs::WindowType GetTabsWindowType(const BrowserWindowInterface* browser) {
  switch (browser->GetType()) {
    case BrowserWindowInterface::TYPE_APP:
      return api::tabs::WindowType::kApp;
    // Browser::TYPE_APP_POPUP is considered 'popup' rather than 'app' since
    // chrome.windows.create({type: 'popup'}) uses
    // Browser::CreateParams::CreateForAppPopup().
    case BrowserWindowInterface::TYPE_APP_POPUP:
    case BrowserWindowInterface::TYPE_POPUP:
      return api::tabs::WindowType::kPopup;
#if !BUILDFLAG(IS_ANDROID)
    case BrowserWindowInterface::TYPE_DEVTOOLS:
      return api::tabs::WindowType::kDevtools;
#endif

    // All the following are considered "normal".
    // TODO(https://crbug.com/438514981): This is almost certainly wrong, and
    // an artifact of not updating this when new types were added. PIP is
    // closer to a popup, and custom tabs might be app-like (if they can even
    // reach this point).
    case BrowserWindowInterface::TYPE_NORMAL:
#if !BUILDFLAG(IS_ANDROID)
    case BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE:
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case BrowserWindowInterface::TYPE_CUSTOM_TAB:
#endif
      return api::tabs::WindowType::kNormal;
  }
}

}  // anonymous namespace

DEFINE_USER_DATA(BrowserExtensionWindowController);

BrowserExtensionWindowController::BrowserExtensionWindowController(
    BrowserWindowInterface* browser)
    : WindowController(browser->GetWindow(), browser->GetProfile()),
      browser_(CHECK_DEREF(browser)),
#if !BUILDFLAG(IS_ANDROID)
      window_(CHECK_DEREF(browser->GetBrowserForMigrationOnly()->window())),
#endif  // !BUILDFLAG(IS_ANDROID)
      tab_list_(CHECK_DEREF(TabListInterface::From(browser))),
      session_id_(browser->GetSessionID()),
      window_type_(GetTabsWindowType(browser)),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

BrowserExtensionWindowController::~BrowserExtensionWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

BrowserExtensionWindowController* BrowserExtensionWindowController::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<BrowserExtensionWindowController>::Get(
      browser_window_interface->GetUnownedUserDataHost());
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
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  if (window_->IsFullscreen() != is_fullscreen) {
    GetBrowser()->ToggleFullscreenModeWithExtension(extension_url);
  }
#endif
}

bool BrowserExtensionWindowController::CanClose(Reason* reason) const {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  // Don't let an extension remove the window if the user is dragging tabs
  // in that window.
  if (!window_->IsTabStripEditable()) {
    *reason = WindowController::REASON_NOT_EDITABLE;
    return false;
  }
#endif
  return true;
}

BrowserWindowInterface*
BrowserExtensionWindowController::GetBrowserWindowInterface() {
  return &browser_.get();
}

#if !BUILDFLAG(IS_ANDROID)
Browser* BrowserExtensionWindowController::GetBrowser() const {
  return browser_->GetBrowserForMigrationOnly();
}
#endif

bool BrowserExtensionWindowController::IsDeleteScheduled() const {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  return false;
#else
  return GetBrowser()->is_delete_scheduled();
#endif
}

content::WebContents* BrowserExtensionWindowController::GetActiveTab() const {
  // In some situations, especially tests, there may not be an active tab.
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  return active_tab ? active_tab->GetContents() : nullptr;
}

bool BrowserExtensionWindowController::HasEditableTabStrip() const {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  return true;
#else
  return window_->IsTabStripEditable();
#endif
}

int BrowserExtensionWindowController::GetTabCount() const {
  return tab_list_->GetTabCount();
}

content::WebContents* BrowserExtensionWindowController::GetWebContentsAt(
    int i) const {
  return tab_list_->GetTab(i)->GetContents();
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
  dict.Set(kFocusedKey, window()->IsActive());
  dict.Set(kIncognitoKey, profile()->IsOffTheRecord());
  dict.Set(kAlwaysOnTopKey,
           window()->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow);

  const std::string_view window_state = [&]() {
    if (window()->IsMinimized()) {
      return kShowStateValueMinimized;
    } else if (window()->IsFullscreen()) {
#if !BUILDFLAG(IS_ANDROID)
      if (platform_util::IsBrowserLockedFullscreen(GetBrowser())) {
        return kShowStateValueLockedFullscreen;
      }
#endif
      return kShowStateValueFullscreen;
    } else if (window()->IsMaximized()) {
      return kShowStateValueMaximized;
    }
    return kShowStateValueNormal;
  }();
  dict.Set(kShowStateKey, window_state);

  const gfx::Rect bounds = window()->IsMinimized()
                               ? window()->GetRestoredBounds()
                               : window()->GetBounds();
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
  const int tab_count = tab_list_->GetTabCount();

  for (int i = 0; i < tab_count; ++i) {
    content::WebContents* web_contents = tab_list_->GetTab(i)->GetContents();

#if BUILDFLAG(IS_ANDROID)
    // TODO(http://crbug.com/453008083): Remove feature flags
    // kLoadAllTabsAtStartup, kTabFreezingUsesDiscard, and kWebContentsDiscard,
    // when all of them are enabled by default.
    //
    // On Android, it was possible for tabs to have null WebContents, so we
    // implemented a temporary workaround that ignored such tabs to avoid
    // crashes. The workaround introduced a bug: tabs with null WebContents were
    // visible on the tab strip, but they couldn't be seen by extensions.
    //
    // When feature flags kLoadAllTabsAtStartup, kTabFreezingUsesDiscard, and
    // kWebContentsDiscard are all enabled, all tabs will create a WebContents
    // without a renderer during initialization, which will properly fix the
    // issue above. As of Oct 22, 2025, the feature flags weren't enabled by
    // default, so we needed to keep the workaround.
    bool is_non_null_web_contents_guaranteed =
        base::FeatureList::IsEnabled(chrome::android::kLoadAllTabsAtStartup) &&
        base::FeatureList::IsEnabled(
            chrome::android::kTabFreezingUsesDiscard) &&
        base::FeatureList::IsEnabled(features::kWebContentsDiscard);

    if (!is_non_null_web_contents_guaranteed && web_contents == nullptr) {
      continue;
    }
#else
    CHECK(web_contents);
#endif

    const ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(extension, context, web_contents);
    tab_list.Append(
        ExtensionTabUtil::CreateTabObject(web_contents, scrub_tab_behavior,
                                          extension, &tab_list_.get(), i)
            .ToValue());
  }

  return tab_list;
}

bool BrowserExtensionWindowController::OpenOptionsPage(
    const Extension* extension,
    const GURL& url,
    bool open_in_tab) {
  DCHECK(OptionsPageInfo::HasOptionsPage(extension));

#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  // Force the options page to open in non-OTR window if the extension is not
  // running in split mode, because it won't be able to save settings from OTR.
  // This version of OpenOptionsPage() can be called from an OTR window via e.g.
  // the action menu, since that's not initiated by the extension.
  Browser* browser_to_use = GetBrowser();
  std::optional<chrome::ScopedTabbedBrowserDisplayer> displayer;
  if (profile()->IsOffTheRecord() && !IncognitoInfo::IsSplitMode(extension)) {
    displayer.emplace(profile()->GetOriginalProfile());
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
#endif

  return true;
}

bool BrowserExtensionWindowController::SupportsTabs() {
  return window_type_ != api::tabs::WindowType::kDevtools;
}

}  // namespace extensions
