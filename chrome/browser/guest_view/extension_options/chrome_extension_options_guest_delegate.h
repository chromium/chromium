// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_CHROME_EXTENSION_OPTIONS_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_CHROME_EXTENSION_OPTIONS_GUEST_DELEGATE_H_

#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"

namespace extensions {

class ExtensionOptionsGuest;

class ChromeExtensionOptionsGuestDelegate
    : public ExtensionOptionsGuestDelegate {
 public:
  explicit ChromeExtensionOptionsGuestDelegate(ExtensionOptionsGuest* guest);

  ChromeExtensionOptionsGuestDelegate(
      const ChromeExtensionOptionsGuestDelegate&) = delete;
  ChromeExtensionOptionsGuestDelegate& operator=(
      const ChromeExtensionOptionsGuestDelegate&) = delete;

  ~ChromeExtensionOptionsGuestDelegate() override;

  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  content::WebContents* OpenURLInNewTab(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_CHROME_EXTENSION_OPTIONS_GUEST_DELEGATE_H_
