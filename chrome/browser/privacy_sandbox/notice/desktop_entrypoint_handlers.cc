// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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
  // TODO(crbug.com/408016824): Implement to perform checks needed before
  // showing notice.
  HandleEntryPoint(browser_window_interface);
}

}  // namespace privacy_sandbox
