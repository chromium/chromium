// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_ACCESS_HANDLER_H_

#include <map>
#include <set>

#include "chrome/browser/media/media_access_handler.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
class WebViewGuest;
}  // namespace extensions

namespace url {
class Origin;
}  // namespace url

namespace controlled_frame {

// MediaAccessHandler for Controlled Frame capturing media permission requests.
class ControlledFrameMediaAccessHandler : public MediaAccessHandler {
 public:
  struct PendingMediaAccessRequestDetails {
    PendingMediaAccessRequestDetails(const url::Origin& embedded_frame_origin,
                                     blink::mojom::MediaStreamType type);
    ~PendingMediaAccessRequestDetails() = default;

    url::Origin embedded_frame_origin;
    blink::mojom::MediaStreamType type;
  };

  ControlledFrameMediaAccessHandler();
  ~ControlledFrameMediaAccessHandler() override;

  // MediaAccessHandler implementation:
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

 private:
  bool IsAllowedByPermissionsPolicy(extensions::WebViewGuest* web_view,
                                    const url::Origin& requesting_origin,
                                    blink::mojom::MediaStreamType type);

  std::map<url::Origin, std::set<url::Origin>> requests_;
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_ACCESS_HANDLER_H_
