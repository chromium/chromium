// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ACCESS_HANDLER_H_

#include "base/callback.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace extensions {
class Extension;
}

// Interface for handling media access requests that are propagated from
// MediaCaptureDevicesDispatcher.
class MediaAccessHandler {
 public:
  MediaAccessHandler() {}
  virtual ~MediaAccessHandler() {}

  // Check if the media stream type is supported by MediaAccessHandler.
  virtual bool SupportsStreamType(content::WebContents* web_contents,
                                  const blink::mojom::MediaStreamType type,
                                  const extensions::Extension* extension) = 0;

  // Check media access permission. |extension| is set to NULL if request was
  // made from a drive-by page.
  virtual bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const GURL& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) = 0;

  // Process media access requests. |extension| is set to NULL if request was
  // made from a drive-by page.
  virtual void HandleRequest(content::WebContents* web_contents,
                             const content::MediaStreamRequest& request,
                             content::MediaResponseCallback callback,
                             const extensions::Extension* extension) = 0;

  // Update media request state. Called on UI thread.
  virtual void UpdateMediaRequestState(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      blink::mojom::MediaStreamType stream_type,
      content::MediaRequestState state) {}

  // Return true if there is any ongoing insecured capturing. The capturing is
  // deemed secure if all connected video sinks are reported secure and the
  // connections to the sinks are being managed by a trusted source.
  virtual bool IsInsecureCapturingInProgress(int render_process_id,
                                             int render_frame_id);

  //  Update any ongoing insecured capturing state.
  virtual void UpdateVideoScreenCaptureStatus(int render_process_id,
                                              int render_frame_id,
                                              int page_request_id,
                                              bool is_secure) {}

 protected:
  // Helper function for derived classes which takes in whether audio/video
  // permissions are allowed and queries for the requested devices, running the
  // callback with the appropriate device list and status.
  static void CheckDevicesAndRunCallback(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      bool audio_allowed,
      bool video_allowed);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ACCESS_HANDLER_H_
