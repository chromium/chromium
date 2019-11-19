// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_ACCESS_HANDLER_H_

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chrome/browser/media/capture_access_handler_base.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {
class Extension;
}

// MediaAccessHandler for DesktopCapture API requests that originate from
// getUserMedia() calls. Note that getDisplayMedia() calls are handled in
// DisplayMediaAccessHandler.
class DesktopCaptureAccessHandler : public CaptureAccessHandlerBase,
                                    public content::NotificationObserver {
 public:
  DesktopCaptureAccessHandler();
  explicit DesktopCaptureAccessHandler(
      std::unique_ptr<DesktopMediaPickerFactory> picker_factory);
  ~DesktopCaptureAccessHandler() override;

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
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               blink::mojom::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

 private:
  friend class DesktopCaptureAccessHandlerTest;

  struct PendingAccessRequest;
  using RequestsQueue =
      base::circular_deque<std::unique_ptr<PendingAccessRequest>>;
  using RequestsQueues = base::flat_map<content::WebContents*, RequestsQueue>;

  void ProcessScreenCaptureAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension);

  // Returns whether desktop capture is always approved for |extension|.
  // Currently component extensions and some whitelisted extensions are default
  // approved.
  static bool IsDefaultApproved(const extensions::Extension* extension);

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;
  void AddNotificationObserver();

  // Methods for handling source change request, e.g. bringing up the picker to
  // select a new source within the current desktop sharing session.
  void ProcessChangeSourceRequest(content::WebContents* web_contents,
                                  const content::MediaStreamRequest& request,
                                  content::MediaResponseCallback callback,
                                  const extensions::Extension* extension);
  void ProcessQueuedAccessRequest(const RequestsQueue& queue,
                                  content::WebContents* web_contents);
  void OnPickerDialogResults(content::WebContents* web_contents,
                             content::DesktopMediaID source);
  void DeletePendingAccessRequest(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id);

  std::unique_ptr<DesktopMediaPickerFactory> picker_factory_;
  bool display_notification_;
  RequestsQueues pending_requests_;
  content::NotificationRegistrar notifications_registrar_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureAccessHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_ACCESS_HANDLER_H_
