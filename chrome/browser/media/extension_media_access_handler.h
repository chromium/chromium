// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_EXTENSION_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_EXTENSION_MEDIA_ACCESS_HANDLER_H_

#include "chrome/browser/media/media_access_handler.h"

// MediaAccessHandler for extension capturing requests.
class ExtensionMediaAccessHandler : public MediaAccessHandler {
 public:
  ExtensionMediaAccessHandler();
  ~ExtensionMediaAccessHandler() override;

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

#endif  // CHROME_BROWSER_MEDIA_EXTENSION_MEDIA_ACCESS_HANDLER_H_
