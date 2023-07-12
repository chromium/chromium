// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"

#include <utility>

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

ChromeAppViewGuestDelegate::ChromeAppViewGuestDelegate() = default;

ChromeAppViewGuestDelegate::~ChromeAppViewGuestDelegate() = default;

bool ChromeAppViewGuestDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate = ContextMenuDelegate::FromWebContents(
      content::WebContents::FromRenderFrameHost(&render_frame_host));
  DCHECK(menu_delegate);

  std::unique_ptr<RenderViewContextMenuBase> menu =
      menu_delegate->BuildMenu(render_frame_host, params);
  menu_delegate->ShowMenu(std::move(menu));
  return true;
}

AppDelegate* ChromeAppViewGuestDelegate::CreateAppDelegate(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  return new ChromeAppDelegate(profile, true);
}

}  // namespace extensions
