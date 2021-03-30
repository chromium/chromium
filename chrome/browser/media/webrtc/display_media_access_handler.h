// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DISPLAY_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DISPLAY_MEDIA_ACCESS_HANDLER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chrome/browser/media/capture_access_handler_base.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "content/public/browser/desktop_media_id.h"

namespace extensions {
class Extension;
}

// MediaAccessHandler for getDisplayMedia API, see
// https://w3c.github.io/mediacapture-screen-share.
class DisplayMediaAccessHandler : public CaptureAccessHandlerBase,
                                  public WebContentsCollection::Observer {
 public:
  DisplayMediaAccessHandler();
  DisplayMediaAccessHandler(
      std::unique_ptr<DesktopMediaPickerFactory> picker_factory,
      bool display_notification);
  ~DisplayMediaAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(content::WebContents* web_contents,
                          const blink::mojom::MediaStreamType stream_type,
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
  friend class DisplayMediaAccessHandlerTest;

  struct PendingAccessRequest;
  using RequestsQueue =
      base::circular_deque<std::unique_ptr<PendingAccessRequest>>;
  using RequestsQueues = base::flat_map<content::WebContents*, RequestsQueue>;

  void ProcessQueuedAccessRequest(const RequestsQueue& queue,
                                  content::WebContents* web_contents);

  void OnPickerDialogResults(content::WebContents* web_contents,
                             content::DesktopMediaID source);

  void DeletePendingAccessRequest(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id);

  // WebContentsCollection::Observer:
  void WebContentsDestroyed(content::WebContents* web_cotents) override;

  bool display_notification_ = true;
  std::unique_ptr<DesktopMediaPickerFactory> picker_factory_;
  RequestsQueues pending_requests_;

  WebContentsCollection web_contents_collection_;

  DISALLOW_COPY_AND_ASSIGN(DisplayMediaAccessHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DISPLAY_MEDIA_ACCESS_HANDLER_H_
