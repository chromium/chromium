// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"

#include <utility>

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest.h"

namespace extensions {

ChromeExtensionOptionsGuestDelegate::ChromeExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest)
    : ExtensionOptionsGuestDelegate(guest) {
}

ChromeExtensionOptionsGuestDelegate::~ChromeExtensionOptionsGuestDelegate() {
}

bool ChromeExtensionOptionsGuestDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  content::WebContents* web_contents =
      extension_options_guest()->web_contents();
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  DCHECK(menu_delegate);
  DCHECK_EQ(web_contents,
            content::WebContents::FromRenderFrameHost(&render_frame_host));

  std::unique_ptr<RenderViewContextMenuBase> menu =
      menu_delegate->BuildMenu(render_frame_host, params);
  menu_delegate->ShowMenu(std::move(menu));
  return true;
}

content::WebContents* ChromeExtensionOptionsGuestDelegate::OpenURLInNewTab(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  Browser* browser = chrome::FindBrowserWithTab(
      extension_options_guest()->embedder_web_contents());
  return browser->OpenURL(params, std::move(navigation_handle_callback));
}

}  // namespace extensions
