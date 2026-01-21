// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"

#include <utility>

#include "base/notimplemented.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

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
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(
      extension_options_guest()->embedder_web_contents());
  return browser->OpenURL(params, std::move(navigation_handle_callback));
#else  // TODO(b/476468383): NEEDS_ANDROID_IMPL
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

}  // namespace extensions
