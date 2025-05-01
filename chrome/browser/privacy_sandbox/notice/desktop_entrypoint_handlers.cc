// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"

namespace privacy_sandbox {
//-----------------------------------------------------------------------------
// EntryPointHandler
//-----------------------------------------------------------------------------
EntryPointHandler::EntryPointHandler(
    base::RepeatingCallback<void(BrowserWindowInterface*)> entry_point_callback)
    : entry_point_callback_(std::move(entry_point_callback)) {}
EntryPointHandler::~EntryPointHandler() = default;

void EntryPointHandler::HandleEntryPoint(
    BrowserWindowInterface* browser_interface) {
  entry_point_callback_.Run(browser_interface);
}

//-----------------------------------------------------------------------------
// NavigationHandler
//-----------------------------------------------------------------------------
NavigationHandler::NavigationHandler(
    base::RepeatingCallback<void(BrowserWindowInterface*)> entry_point_callback)
    : EntryPointHandler(std::move(entry_point_callback)) {}

// static
bool NavigationHandler::IsUrlSuitableForPrompt(const GURL& url) {
  // The prompt should be shown on a limited list of pages:

  // about:blank is valid.
  if (url.IsAboutBlank()) {
    return true;
  }

  // Chrome settings page is valid. The subpages aren't as most of them are not
  // related to the prompt.
  if (url == GURL(chrome::kChromeUISettingsURL)) {
    return true;
  }

  // Chrome history is valid as the prompt mentions history.
  if (url == GURL(chrome::kChromeUIHistoryURL)) {
    return true;
  }

  // Only a Chrome controlled New Tab Page is valid. Third party NTP is still
  // Chrome controlled, but is without Google branding.
  if (url == GURL(chrome::kChromeUINewTabPageURL) ||
      url == GURL(chrome::kChromeUINewTabPageThirdPartyURL)) {
    return true;
  }

  return false;
}

void NavigationHandler::HandleNewNavigation(
    content::NavigationHandle* navigation_handle,
    Profile* profile) {
  auto* tab_interface =
      tabs::TabInterface::GetFromContents(navigation_handle->GetWebContents());
  if (!tab_interface) {
    return;
  }

  auto* browser_window_interface = tab_interface->GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return;
  }

  // Check whether the navigation target is a suitable prompt location. The
  // navigation URL, rather than the visible or committed URL, is required to
  // distinguish between different types of NTPs.
  if (!IsUrlSuitableForPrompt(navigation_handle->GetURL())) {
    return;
  }

  // TODO(crbug.com/408016824):  Finish implementing checks needed before
  // showing notice.
  // TODO(crbug.com/408016824):  Add error-event histograms.

  HandleEntryPoint(browser_window_interface);
}

}  // namespace privacy_sandbox
