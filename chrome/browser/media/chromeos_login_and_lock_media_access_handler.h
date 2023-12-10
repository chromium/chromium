// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CHROMEOS_LOGIN_AND_LOCK_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_CHROMEOS_LOGIN_AND_LOCK_MEDIA_ACCESS_HANDLER_H_

#include "chrome/browser/media/media_access_handler.h"

// MediaAccessHandler for media requests coming from SAML IdP pages on the
// login/lock screen on ChromeOS.
class ChromeOSLoginAndLockMediaAccessHandler : public MediaAccessHandler {
 public:
  ChromeOSLoginAndLockMediaAccessHandler();
  ~ChromeOSLoginAndLockMediaAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(content::WebContents* web_contents,
                          const blink::mojom::MediaStreamType type,
                          const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) override;
  void HandleRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     content::MediaResponseCallback callback,
                     const extensions::Extension* extension) override;
};

#endif  // CHROME_BROWSER_MEDIA_CHROMEOS_LOGIN_AND_LOCK_MEDIA_ACCESS_HANDLER_H_
