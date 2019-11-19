// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PUBLIC_SESSION_TAB_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_PUBLIC_SESSION_TAB_CAPTURE_ACCESS_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/media/capture_access_handler_base.h"
#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

// MediaAccessHandler for TabCapture API in Public Sessions. This class is
// implemented as a wrapper around TabCaptureAccessHandler. It allows for finer
// access control to the TabCapture manifest permission feature inside of Public
// Sessions.
//
// In Public Sessions, extensions (and apps) are force-installed by admin policy
// so the user does not get a chance to review the permissions for these
// extensions. This is not acceptable from a security/privacy standpoint, so
// when an extension uses the TabCapture API for the first time, we show the
// user a dialog where they can choose whether to allow the extension access to
// the API.
class PublicSessionTabCaptureAccessHandler : public CaptureAccessHandlerBase {
 public:
  PublicSessionTabCaptureAccessHandler();
  ~PublicSessionTabCaptureAccessHandler() override;

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
  // inside of TabCaptureAccessHandler.
  void ChainHandleRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension,
      const extensions::PermissionIDSet& allowed_permissions);

  TabCaptureAccessHandler tab_capture_access_handler_;

  DISALLOW_COPY_AND_ASSIGN(PublicSessionTabCaptureAccessHandler);
};

#endif  // CHROME_BROWSER_MEDIA_PUBLIC_SESSION_TAB_CAPTURE_ACCESS_HANDLER_H_
