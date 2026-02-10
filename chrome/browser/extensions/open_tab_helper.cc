// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/open_tab_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#endif

namespace extensions {
namespace {

#if !BUILDFLAG(IS_ANDROID)
BrowserWindowInterface* CreateAndShowBrowser(Profile* profile,
                                             bool user_gesture) {
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                   *profile, user_gesture);

  // TODO(https://crbug.com/430344931): When this is ported to android
  // platforms, this window isn't guaranteed to be fully initialized.
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    return nullptr;
  }

  browser->GetWindow()->Show();
  return browser;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

OpenTabHelper::Params::Params() = default;
OpenTabHelper::Params::~Params() = default;

#if !BUILDFLAG(IS_ANDROID)
// static
base::expected<BrowserWindowInterface*, std::string>
OpenTabHelper::FindOrCreateBrowser(const GURL& validated_url,
                                   ExtensionFunction& function,
                                   bool create_if_needed) {
  // Try to find a suitable browser.
  // TODO(https://crbug.com/468223125): This is a wild set of tangled
  // conditions, most of which are inconsistent.

  WindowController* controller =
      ChromeExtensionFunctionDetails(&function).GetCurrentWindowController();

  // We didn't find a browser and shouldn't create a new one, according to the
  // params. Bail.
  //
  // TODO(https://crbug.com/468223125): This isn't consistent, since sometimes
  // we *will* create a new browser below.
  if (!controller && !create_if_needed) {
    return base::unexpected(ExtensionTabUtil::kNoCurrentWindowError);
  }

  BrowserWindowInterface* browser = controller->GetBrowserWindowInterface();

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window or, if
  // needed, create one.
  bool needs_original_profile =
      validated_url.SchemeIs(kExtensionScheme) &&
      (!function.extension() ||
       !IncognitoInfo::IsSplitMode(function.extension()));

  bool fallback_to_tabbed_browser = false;

  // Check if the browser is valid. If it isn't, reset `browser` and possibly
  // find a replacement.

  // TODO(https://crbug.com/468223125): Why do we check if it's not a normal
  // browser *and* it's attempting to close? Should that be *or*? This goes
  // back to the dawn of time, AKA the initial implementation in 2014:
  // https://codereview.chromium.org/245933002.
  if (browser && browser->GetType() != BrowserWindowInterface::TYPE_NORMAL &&
      browser->GetBrowserForMigrationOnly()->IsAttemptingToCloseBrowser()) {
    browser = nullptr;
    fallback_to_tabbed_browser = true;
  }

  if (browser && needs_original_profile &&
      browser->GetProfile()->IsOffTheRecord()) {
    browser = nullptr;
    fallback_to_tabbed_browser = true;
    create_if_needed = true;
  }

  Profile* profile = Profile::FromBrowserContext(function.browser_context());
  CHECK(profile);
  Profile* profile_to_use =
      needs_original_profile ? profile->GetOriginalProfile() : profile;

  if (!browser && fallback_to_tabbed_browser) {
    // Don't include incognito information if we need the original profile,
    // since the goal is to find a non-incognito browser.
    bool include_incognito_information =
        function.include_incognito_information() && !needs_original_profile;
    browser = browser_window_util::GetLastActiveNormalBrowserWithProfile(
        *profile_to_use, include_incognito_information);
  }

  if (!browser && create_if_needed) {
    browser = CreateAndShowBrowser(profile_to_use, function.user_gesture());
  }

  if (!browser || !browser->GetWindow()) {
    return base::unexpected(ExtensionTabUtil::kNoCurrentWindowError);
  }

  return browser;
}
#endif  // !BUILDFLAG(IS_ANDROID)

// static
base::expected<content::WebContents*, std::string> OpenTabHelper::OpenTab(
    const GURL& validated_url,
    BrowserWindowInterface& browser,
    const ExtensionFunction& function,
    const Params& params) {
  auto* const extension = function.extension();

  // DCHECK because the input should already have been validated, and this is
  // a somewhat costly function.
  DCHECK(ExtensionTabUtil::PrepareURLForNavigation(
             validated_url.spec(), extension,
             Profile::FromBrowserContext(function.browser_context()))
             .has_value())
      << "Invalid URL: " << validated_url;

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = params.active.value_or(true);

  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = params.index.value_or(-1);
  TabListInterface* tab_list = TabListInterface::From(&browser);
  CHECK(tab_list);
  index = std::clamp(index, -1, tab_list->GetTabCount());

  NavigateParams navigate_params(&browser, validated_url,
                                 ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.user_gesture = false;

  // TODO(https://crbug.com/430344931): `NavigateParams::tabstrip_add_types`
  // isn't supported on android builds yet.
#if !BUILDFLAG(IS_ANDROID)
  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = params.pinned.value_or(false);

  int add_types = active ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE;
  add_types |= AddTabTypes::ADD_FORCE_INDEX;
  if (pinned) {
    add_types |= AddTabTypes::ADD_PINNED;
  }
  navigate_params.tabstrip_add_types = add_types;
#endif

  // Ensure that this navigation will not get 'captured' into PWA windows, as
  // this means that `browser` could be ignored. It may be useful/desired in
  // the future to allow this behavior, but this may require an API change, and
  // likely a re-write of how this navigation is called to be compatible with
  // the navigation capturing behavior.
  navigate_params.pwa_navigation_capturing_force_off = true;

  MaybeSetPdfNavigateParams(function, navigate_params);

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&navigate_params);
  if (handle && params.bookmark_id) {
    ChromeNavigationUIData* ui_data =
        static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
    ui_data->set_bookmark_id(*params.bookmark_id);
  }

  content::WebContents* new_contents =
      navigate_params.navigated_or_inserted_contents;

  // This happens in locked fullscreen mode.
  if (!new_contents) {
    return base::unexpected(ExtensionTabUtil::kLockedFullscreenModeNewTabError);
  }

  if (active) {
    new_contents->SetInitialFocus();
  }

  return new_contents;
}

// static
bool OpenTabHelper::MaybeSetPdfNavigateParams(const ExtensionFunction& function,
                                              NavigateParams& navigate_params) {
  auto* const extension = function.extension();
  if (!extension || extension->id() != extension_misc::kPdfExtensionId) {
    return false;
  }

  navigate_params.is_renderer_initiated = true;
  navigate_params.initiator_origin = extension->origin();
  navigate_params.source_site_instance = content::SiteInstance::CreateForURL(
      function.browser_context(), navigate_params.initiator_origin->GetURL());
  return true;
}

}  // namespace extensions
