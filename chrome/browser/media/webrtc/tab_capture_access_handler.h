// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_TAB_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_TAB_CAPTURE_ACCESS_HANDLER_H_

#include "chrome/browser/media/capture_access_handler_base.h"

class MediaStreamUI;

namespace content {
class WebContents;
}  // namespace content

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
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) override;
  void HandleRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     content::MediaResponseCallback callback,
                     const extensions::Extension* extension) override;

 private:
  friend class TabCaptureAccessHandlerTest;

  // Helper method to finalize processing an approved request.
  void AcceptRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     content::MediaResponseCallback callback,
                     bool is_allowlisted_extension,
                     std::unique_ptr<MediaStreamUI> media_ui);

#if BUILDFLAG(IS_CHROMEOS)
  // Called back after checking Data Leak Prevention (DLP) restrictions.
  void OnDlpRestrictionChecked(
      base::WeakPtr<content::WebContents> web_contents,
      std::unique_ptr<PendingAccessRequest> pending_request,
      std::unique_ptr<MediaStreamUI> media_ui,
      bool is_dlp_allowed);

#endif  // BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_TAB_CAPTURE_ACCESS_HANDLER_H_
