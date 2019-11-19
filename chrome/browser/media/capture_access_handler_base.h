// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAPTURE_ACCESS_HANDLER_BASE_H_
#define CHROME_BROWSER_MEDIA_CAPTURE_ACCESS_HANDLER_BASE_H_

#include <list>

#include "chrome/browser/media/media_access_handler.h"
#include "content/public/browser/media_request_state.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

// Base class for DesktopCaptureAccessHandler and TabCaptureAccessHandler. This
// class tracks active capturing sessions, and provides API to check if there is
// ongoing insecure video capturing.
class CaptureAccessHandlerBase : public MediaAccessHandler {
 public:
  CaptureAccessHandlerBase();
  ~CaptureAccessHandlerBase() override;

  // MediaAccessHandler implementation.
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               blink::mojom::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

  // Returns true if there is any ongoing insecured capturing. Returns false
  // otherwise, e.g. there is no capturing, or all capturing are secure. A
  // capturing is deemed secure if all connected video sinks are reported secure
  // and the connections to the sinks are also secure, e.g. being managed by a
  // trusted extension.
  bool IsInsecureCapturingInProgress(int render_process_id,
                                     int render_frame_id) override;

  // Updates video screen capture status with whether it |is_secure| or not.
  void UpdateVideoScreenCaptureStatus(int render_process_id,
                                      int render_frame_id,
                                      int page_request_id,
                                      bool is_secure) override;

 protected:
  static bool IsExtensionWhitelistedForScreenCapture(
      const extensions::Extension* extension);

  static bool IsBuiltInExtension(const GURL& origin);

  void UpdateExtensionTrusted(const content::MediaStreamRequest& request,
                              const extensions::Extension* extension);

  void UpdateTrusted(const content::MediaStreamRequest& request,
                     bool is_trusted);

 private:
  struct Session;

  void AddCaptureSession(int render_process_id,
                         int render_frame_id,
                         int page_request_id,
                         bool is_trusted);

  void RemoveCaptureSession(int render_process_id,
                            int render_frame_id,
                            int page_request_id);

  std::list<Session>::iterator FindSession(int render_process_id,
                                           int render_frame_id,
                                           int page_request_id);

  std::list<Session> sessions_;

  DISALLOW_COPY_AND_ASSIGN(CaptureAccessHandlerBase);
};

#endif  // CHROME_BROWSER_MEDIA_CAPTURE_ACCESS_HANDLER_BASE_H_
