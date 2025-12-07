// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DISPLAY_MEDIA_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DISPLAY_MEDIA_ACCESS_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/media/capture_access_handler_base.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace extensions {
class Extension;
}

BASE_DECLARE_FEATURE(kDisplayMediaRejectLongDomains);

// MediaAccessHandler for getDisplayMedia API, see
// https://w3c.github.io/mediacapture-screen-share.
class DisplayMediaAccessHandler : public CaptureAccessHandlerBase,
                                  public WebContentsCollection::Observer {
 public:
  DisplayMediaAccessHandler();
  DisplayMediaAccessHandler(
      std::unique_ptr<DesktopMediaPickerFactory> picker_factory,
      bool display_notification);

  DisplayMediaAccessHandler(const DisplayMediaAccessHandler&) = delete;
  DisplayMediaAccessHandler& operator=(const DisplayMediaAccessHandler&) =
      delete;

  ~DisplayMediaAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(content::RenderFrameHost* render_frame_host,
                          const blink::mojom::MediaStreamType stream_type,
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
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               blink::mojom::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

 private:
  friend class DisplayMediaAccessHandlerTest;

  void ShowMediaSelectionDialog(content::WebContents* web_contents,
                                const content::MediaStreamRequest& request,
                                content::MediaResponseCallback callback);

  void BypassMediaSelectionDialog(content::WebContents* web_contents,
                                  const content::MediaStreamRequest& request,
                                  const content::DesktopMediaID& media_id,
                                  content::MediaResponseCallback callback);

  void ProcessChangeSourceRequest(content::WebContents* web_contents,
                                  const content::MediaStreamRequest& request,
                                  content::MediaResponseCallback callback);

  // Processes one pending request. Requests are queued so that we display one
  // picker UI at a time for each content::WebContents.
  void ProcessQueuedAccessRequest(const RequestsQueue& queue,
                                  content::WebContents* web_contents);

  void ProcessQueuedPickerRequest(const PendingAccessRequest& pending_request,
                                  content::WebContents* web_contents,
                                  AllowedScreenCaptureLevel capture_level,
                                  const GURL& request_origin);

  void ProcessQueuedChangeSourceRequest(
      const content::MediaStreamRequest& request,
      content::WebContents* web_contents);

  void RejectRequest(content::WebContents* web_contents,
                     blink::mojom::MediaStreamRequestResult result);

  // Helper to finalize processing the first queued request for |web_contents|,
  // after all checks have been performed. Calls ProcessQueuedAccessRequest() if
  // there are more requests left in the queue.
  void AcceptRequest(content::WebContents* web_contents,
                     const content::DesktopMediaID& media_id);

  void OnDesktopCaptureDevicesObtainedAfterAcceptRequest(
      base::WeakPtr<content::WebContents> web_contents,
      content::MediaStreamRequest request,
      const content::DesktopMediaID& media_id,
      blink::mojom::StreamDevices devices,
      std::unique_ptr<content::MediaStreamUI> ui);

  bool IsRequestFirstInQueue(const RequestsQueue& queue,
                             const content::MediaStreamRequest& request) const;

  // Called after the user interacts with the media-picker.
  // - If the user chooses to share a tab/window/screen, `result.value()` holds
  //   the media-ID of the chosen surface.
  // - If the user rejects the prompt, or if any error occurs, or if the system
  //   automatically rejects the request, `result.error()` holds the relevant
  //   error code.
  void OnDisplaySurfaceSelected(
      base::WeakPtr<content::WebContents> web_contents,
      base::expected<content::DesktopMediaID,
                     blink::mojom::MediaStreamRequestResult> result);

  void OnDesktopCaptureDevicesObtainedAfterBypassMediaSelectionDialog(
      base::WeakPtr<content::WebContents> web_contents,
      content::MediaStreamRequest request,
      content::MediaResponseCallback callback,
      blink::mojom::StreamDevices devices,
      std::unique_ptr<content::MediaStreamUI> ui);

#if BUILDFLAG(IS_CHROMEOS)
  // Called back after checking Data Leak Prevention (DLP) restrictions.
  void OnDlpRestrictionChecked(base::WeakPtr<content::WebContents> web_contents,
                               const content::DesktopMediaID& media_id,
                               bool is_dlp_allowed);
#endif  // BUILDFLAG(IS_CHROMEOS)

  void DeletePendingAccessRequest(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id);

  // WebContentsCollection::Observer:
  void WebContentsDestroyed(content::WebContents* web_cotents) override;

  bool display_notification_ = true;
  std::unique_ptr<DesktopMediaPickerFactory> picker_factory_;
  RequestsQueues pending_requests_;

  WebContentsCollection web_contents_collection_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DISPLAY_MEDIA_ACCESS_HANDLER_H_
