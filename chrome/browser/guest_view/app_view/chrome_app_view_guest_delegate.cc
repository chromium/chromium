// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"

#include <utility>

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "components/renderer_context_menu/context_menu_delegate.h"

namespace extensions {

ChromeAppViewGuestDelegate::ChromeAppViewGuestDelegate() {
}

ChromeAppViewGuestDelegate::~ChromeAppViewGuestDelegate() {
}

bool ChromeAppViewGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  DCHECK(menu_delegate);

  std::unique_ptr<RenderViewContextMenuBase> menu =
      menu_delegate->BuildMenu(web_contents, params);
  menu_delegate->ShowMenu(std::move(menu));
  return true;
}

AppDelegate* ChromeAppViewGuestDelegate::CreateAppDelegate(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return new ChromeAppDelegate(profile, true);
}

}  // namespace extensions
