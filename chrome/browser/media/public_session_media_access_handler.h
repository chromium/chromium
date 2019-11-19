// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PUBLIC_SESSION_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_PUBLIC_SESSION_MEDIA_ACCESS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/media/extension_media_access_handler.h"
#include "chrome/browser/media/media_access_handler.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

// MediaAccessHandler for extension capturing requests in Public Sessions. This
// class is implemented as a wrapper around ExtensionMediaAccessHandler. It
// allows for finer access control to audioCapture/videoCapture manifest
// permission features inside of Public Sessions.
//
// In Public Sessions, apps and extensions are force-installed by admin policy
// so the user does not get a chance to review the permissions for these apps.
// This is not acceptable from a security/privacy standpoint, so when an app
// uses the capture APIs for the first time, we show the user a dialog where
// they can choose whether to allow the extension access to camera and/or
// microphone. Note: camera and microphone are used through audioCapture and
// videoCapture manifest permissions which are limited to platform apps only.
class PublicSessionMediaAccessHandler : public MediaAccessHandler {
 public:
  PublicSessionMediaAccessHandler();
  ~PublicSessionMediaAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(content::WebContents* web_contents,
                          const blink::mojom::MediaStreamType type,
                          const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const GURL& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) override;
  void HandleRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     content::MediaResponseCallback callback,
                     const extensions::Extension* extension) override;

 private:
  // Helper function used to chain the HandleRequest call into the original
  // inside of ExtensionMediaAccessHandler.
  void ChainHandleRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension,
      const extensions::PermissionIDSet& allowed_permissions);

  ExtensionMediaAccessHandler extension_media_access_handler_;

  DISALLOW_COPY_AND_ASSIGN(PublicSessionMediaAccessHandler);
};

#endif  // CHROME_BROWSER_MEDIA_PUBLIC_SESSION_MEDIA_ACCESS_HANDLER_H_
