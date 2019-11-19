// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_TAB_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_TAB_CAPTURE_ACCESS_HANDLER_H_

#include "chrome/browser/media/capture_access_handler_base.h"

// MediaAccessHandler for TabCapture API.
class TabCaptureAccessHandler : public CaptureAccessHandlerBase {
 public:
  TabCaptureAccessHandler();
  ~TabCaptureAccessHandler() override;

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
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_TAB_CAPTURE_ACCESS_HANDLER_H_
