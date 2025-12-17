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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace extensions {
namespace {

BrowserWindowInterface* CreateAndShowBrowser(Profile* profile,
                                             bool user_gesture,
                                             std::string* error) {
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    *error = ExtensionTabUtil::kBrowserWindowNotAllowed;
    return nullptr;
  }

  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                   *profile, user_gesture);

  // TODO(https://crbug.com/430344931): When this is ported to android
  // platforms, this window isn't guaranteed to be fully initialized.
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    *error = ExtensionTabUtil::kBrowserWindowNotAllowed;
    return nullptr;
  }

  browser->GetWindow()->Show();
  return browser;
}

}  // namespace

OpenTabHelper::Params::Params() = default;
OpenTabHelper::Params::~Params() = default;

// static
base::expected<base::Value::Dict, std::string> OpenTabHelper::OpenTab(
    ExtensionFunction* function,
    const Params& params,
    bool user_gesture) {
  // First, do as much validation as we can. This helps limit the user-visible
  // side effects (like opening a new tab or browser) that might happen in a
  // case that the API call fails.

  auto* const extension = function->extension();
  GURL url(chrome::kChromeUINewTabURL);
  if (params.url) {
    ASSIGN_OR_RETURN(url,
                     ExtensionTabUtil::PrepareURLForNavigation(
                         *params.url, extension, function->browser_context()));
  }

  Profile* profile = Profile::FromBrowserContext(function->browser_context());

  // Try to find a suitable browser.
  // TODO(https://crbug.com/468223125): This is a wild set of tangled
  // conditions, most of which are inconsistent.

  // windowId defaults to "current" window.
  int window_id = params.window_id.value_or(extension_misc::kCurrentWindowId);
  BrowserWindowInterface* browser = nullptr;
  std::string error;
  if (WindowController* controller =
          ExtensionTabUtil::GetControllerFromWindowID(
              ChromeExtensionFunctionDetails(function), window_id, &error)) {
    browser = controller->GetBrowserWindowInterface();
  }

  // We didn't find a browser and shouldn't create a new one, according to the
  // params. Bail.
  //
  // TODO(https://crbug.com/468223125): This isn't consistent, since sometimes
  // we *will* create a new browser below.
  if (!browser && !params.create_browser_if_needed) {
    return base::unexpected(error);
  }

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window or, if
  // needed, create one.
  bool needs_original_profile =
      url.SchemeIs(kExtensionScheme) &&
      (!extension || !IncognitoInfo::IsSplitMode(extension));

  bool fallback_to_tabbed_browser = false;
  bool create_new_if_none_found = false;

  if (params.create_browser_if_needed) {
    create_new_if_none_found = true;
  }

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
    create_new_if_none_found = true;
  }

  // This check (for the opener) comes last. It will fail (by design) if
  // we're intending to create a new browser; that's good, because the new
  // browser would never match the one with the opener.
  if (params.opener_tab) {
    BrowserWindowInterface* opener_browser =
        browser_window_util::GetBrowserForTabContents(*params.opener_tab);
    if (!opener_browser || opener_browser != browser) {
      return base::unexpected(
          "Tab opener must be in the same window as the updated tab.");
    }
  }

  Profile* profile_to_use =
      needs_original_profile ? profile->GetOriginalProfile() : profile;

  if (!browser && fallback_to_tabbed_browser) {
    // Don't include incognito information if we need the original profile,
    // since the goal is to find a non-incognito browser.
    bool include_incognito_information =
        function->include_incognito_information() && !needs_original_profile;
    browser = chrome::FindTabbedBrowser(profile_to_use,
                                        include_incognito_information);
  }

  if (!browser && create_new_if_none_found) {
    browser = CreateAndShowBrowser(profile_to_use, user_gesture, &error);
  }

  if (!browser || !browser->GetWindow()) {
    return base::unexpected(ExtensionTabUtil::kNoCurrentWindowError);
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = params.active.value_or(true);

  // Default to unsplit for the new tab. The presence of the 'split' property
  // will override this default.
  bool split = params.split.value_or(false);

  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = params.pinned.value_or(false);

  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = params.index.value_or(-1);
  index = std::clamp(
      index, -1,
      browser->GetBrowserForMigrationOnly()->tab_strip_model()->count());

  int add_types = active ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE;
  add_types |= AddTabTypes::ADD_FORCE_INDEX;
  if (pinned) {
    add_types |= AddTabTypes::ADD_PINNED;
  }
  NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.user_gesture = false;
  navigate_params.tabstrip_add_types = add_types;
  // Ensure that this navigation will not get 'captured' into PWA windows, as
  // this means that `browser` could be ignored. It may be useful/desired in
  // the future to allow this behavior, but this may require an API change, and
  // likely a re-write of how this navigation is called to be compatible with
  // the navigation capturing behavior.
  navigate_params.pwa_navigation_capturing_force_off = true;

  // Treat PDF open-in-new-window navigations consistently with other PDF
  // navigations, as done in TabsUpdateFunction::UpdateURL().
  if (extension && extension->id() == extension_misc::kPdfExtensionId) {
    navigate_params.is_renderer_initiated = true;
    navigate_params.initiator_origin = extension->origin();
    navigate_params.source_site_instance = content::SiteInstance::CreateForURL(
        function->browser_context(),
        navigate_params.initiator_origin->GetURL());
  }

  base::WeakPtr<content::NavigationHandle> handle = Navigate(&navigate_params);
  if (handle && params.bookmark_id) {
    ChromeNavigationUIData* ui_data =
        static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
    ui_data->set_bookmark_id(*params.bookmark_id);
  }

  // This happens in locked fullscreen mode.
  if (!navigate_params.navigated_or_inserted_contents) {
    return base::unexpected(ExtensionTabUtil::kLockedFullscreenModeNewTabError);
  }

  // The tab may have been created in a different window, so make sure we look
  // at the right tab strip.
  TabStripModel* const tab_strip =
      navigate_params.browser->GetBrowserForMigrationOnly()->tab_strip_model();
  const int new_index = tab_strip->GetIndexOfWebContents(
      navigate_params.navigated_or_inserted_contents);
  if (params.opener_tab) {
    // Only set the opener if the opener tab is in the same tab strip as the
    // new tab.
    if (tab_strip->GetIndexOfWebContents(params.opener_tab) !=
        TabStripModel::kNoTab) {
      tab_strip->SetOpenerOfWebContentsAt(new_index, params.opener_tab);
    }
  }

  if (active) {
    navigate_params.navigated_or_inserted_contents->SetInitialFocus();
  }

  if (split) {
    tab_strip->AddToNewSplit({new_index}, split_tabs::SplitTabVisualData(),
                             split_tabs::SplitTabCreatedSource::kExtensionsApi);
  }

  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          function->extension(), function->source_context_type(),
          navigate_params.navigated_or_inserted_contents);

  // Return data about the newly created tab.
  return ExtensionTabUtil::CreateTabObject(
             navigate_params.navigated_or_inserted_contents, scrub_tab_behavior,
             function->extension(),
             TabListInterface::From(navigate_params.browser), new_index)
      .ToValue();
}

}  // namespace extensions
