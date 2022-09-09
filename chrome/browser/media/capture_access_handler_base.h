// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAPTURE_ACCESS_HANDLER_BASE_H_
#define CHROME_BROWSER_MEDIA_CAPTURE_ACCESS_HANDLER_BASE_H_

#include <list>
#include <string>

#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_request_state.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace extensions {
class Extension;
}

// Base class for DesktopCaptureAccessHandler and TabCaptureAccessHandler. This
// class tracks active capturing sessions, and provides API to check if there is
// ongoing insecure video capturing.
class CaptureAccessHandlerBase : public MediaAccessHandler {
 public:
  CaptureAccessHandlerBase();

  CaptureAccessHandlerBase(const CaptureAccessHandlerBase&) = delete;
  CaptureAccessHandlerBase& operator=(const CaptureAccessHandlerBase&) = delete;

  ~CaptureAccessHandlerBase() override;

  // MediaAccessHandler implementation.
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               blink::mojom::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

  // Returns true if there is any ongoing insecured capturing of the frame
  // specified by |render_process_id| and |render_frame_id|. Returns false
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
  // Holds pending request information.
  struct PendingAccessRequest {
    PendingAccessRequest(std::unique_ptr<DesktopMediaPicker> picker,
                         const content::MediaStreamRequest& request,
                         content::MediaResponseCallback callback,
                         std::u16string application_title,
                         bool should_display_notification,
                         bool is_allowlisted_extension);
    PendingAccessRequest(const PendingAccessRequest& other) = delete;
    PendingAccessRequest& operator=(const PendingAccessRequest& other) = delete;
    ~PendingAccessRequest();

    std::unique_ptr<DesktopMediaPicker> picker;
    content::MediaStreamRequest request;
    content::MediaResponseCallback callback;
    std::u16string application_title;
    const bool should_display_notification;
    const bool is_allowlisted_extension;
  };

  using RequestsQueue =
      base::circular_deque<std::unique_ptr<PendingAccessRequest>>;

  using RequestsQueues = base::flat_map<content::WebContents*, RequestsQueue>;

  static bool IsExtensionAllowedForScreenCapture(
      const extensions::Extension* extension);

  static bool IsBuiltInFeedbackUI(const GURL& origin);

  void UpdateExtensionTrusted(const content::MediaStreamRequest& request,
                              bool is_allowlisted_extension);

  void UpdateTrusted(const content::MediaStreamRequest& request,
                     bool is_trusted);

  void UpdateTarget(const content::MediaStreamRequest& request,
                    const content::DesktopMediaID& target);

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

  // Returns true if the frame specified by |target_process_id| and
  // |target_frame_id| matches the target in |session|.
  static bool MatchesSession(const Session& session,
                             int target_process_id,
                             int target_frame_id);

  std::list<Session> sessions_;
};

#endif  // CHROME_BROWSER_MEDIA_CAPTURE_ACCESS_HANDLER_BASE_H_
