// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_APP_VIEW_CHROME_APP_VIEW_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_APP_VIEW_CHROME_APP_VIEW_GUEST_DELEGATE_H_

#include "content/public/browser/context_menu_params.h"
#include "extensions/browser/guest_view/app_view/app_view_guest_delegate.h"

namespace extensions {

class ChromeAppViewGuestDelegate : public AppViewGuestDelegate {
 public:
  ChromeAppViewGuestDelegate();

  ChromeAppViewGuestDelegate(const ChromeAppViewGuestDelegate&) = delete;
  ChromeAppViewGuestDelegate& operator=(const ChromeAppViewGuestDelegate&) =
      delete;

  ~ChromeAppViewGuestDelegate() override;

  // AppViewGuestDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  AppDelegate* CreateAppDelegate(
      content::BrowserContext* browser_context) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_APP_VIEW_CHROME_APP_VIEW_GUEST_DELEGATE_H_
