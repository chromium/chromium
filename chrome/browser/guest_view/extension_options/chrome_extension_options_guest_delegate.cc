// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest.h"

namespace extensions {

ChromeExtensionOptionsGuestDelegate::ChromeExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest)
    : ExtensionOptionsGuestDelegate(guest) {}

ChromeExtensionOptionsGuestDelegate::~ChromeExtensionOptionsGuestDelegate() =
    default;

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
  return extension_options_guest()->embedder_web_contents()->OpenURL(
      params, std::move(navigation_handle_callback));
}

}  // namespace extensions
