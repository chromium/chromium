// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers_helper.h"
#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/navigation_handle.h"

namespace privacy_sandbox {

constexpr int kMinRequiredDialogHeight = 100;

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

  // Avoid showing the prompt on popups, pip, anything that isn't a normal
  // browser.
  if (browser_window_interface->GetType() !=
      BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }

  // If the windows height is too small, it is difficult to read or interact
  // with the dialog. The dialog is blocking modal, that is why we want to
  // prevent it from showing if there isn't enough space. The PrivacySandbox
  // prompt can always fit inside a normal tabbed window due to its minimum
  // width, so checking the height is enough here.
  auto* web_contents =
      browser_window_interface->GetWebContentsModalDialogHostForWindow();
  if (!web_contents || web_contents->GetMaximumDialogSize().height() <
                           kMinRequiredDialogHeight) {
    return;
  }

  // If a sign-in dialog is being currently displayed or is about to be
  // displayed, the prompt should not be shown to avoid conflict.
  // TODO(crbug.com/370806609): When we add sign in notice to queue, put this
  // behind flag / remove.
  auto* browser = browser_window_interface->GetBrowserForMigrationOnly();
  bool signin_dialog_showing =
      browser->signin_view_controller()->ShowsModalDialog();
#if !BUILDFLAG(IS_CHROMEOS)
  signin_dialog_showing =
      signin_dialog_showing ||
      IsProfileCustomizationBubbleSyncControllerRunning(browser);
#endif  // !BUILDFLAG(IS_CHROMEOS)
  if (signin_dialog_showing) {
    return;
  }

  // TODO(crbug.com/408016824):  Add error-event histograms.

  HandleEntryPoint(browser_window_interface);
}

}  // namespace privacy_sandbox
