// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_ACCESS_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/capture_access_handler_base.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"

#if BUILDFLAG(IS_CHROMEOS)
namespace aura {
class Window;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {
class Extension;
}

namespace content {
class WebContents;
}  // namespace content

// MediaAccessHandler for DesktopCapture API requests that originate from
// getUserMedia() calls. Note that getDisplayMedia() calls are handled in
// DisplayMediaAccessHandler.
class DesktopCaptureAccessHandler : public CaptureAccessHandlerBase,
                                    public WebContentsCollection::Observer {
 public:
  DesktopCaptureAccessHandler();
  explicit DesktopCaptureAccessHandler(
      std::unique_ptr<DesktopMediaPickerFactory> picker_factory);

  DesktopCaptureAccessHandler(const DesktopCaptureAccessHandler&) = delete;
  DesktopCaptureAccessHandler& operator=(const DesktopCaptureAccessHandler&) =
      delete;

  ~DesktopCaptureAccessHandler() override;

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
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               blink::mojom::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

 private:
  friend class DesktopCaptureAccessHandlerTest;

  void ProcessScreenCaptureAccessRequest(
      content::WebContents* web_contents,
      const extensions::Extension* extension,
      std::unique_ptr<PendingAccessRequest> pending_request);

  // WebContentsCollection::Observer:
  void WebContentsDestroyed(content::WebContents* web_contents) override;

  // Methods for handling source change request, e.g. bringing up the picker to
  // select a new source within the current desktop sharing session.
  void ProcessChangeSourceRequest(
      content::WebContents* web_contents,
      std::unique_ptr<PendingAccessRequest> pending_request);
  void ProcessQueuedAccessRequest(const RequestsQueue& queue,
                                  content::WebContents* web_contents);
  void OnPickerDialogResults(base::WeakPtr<content::WebContents> web_contents,
                             const std::u16string& application_title,
                             content::DesktopMediaID source);
  void DeletePendingAccessRequest(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id);

  // Helper method to finalize processing an approved request.
  void AcceptRequest(content::WebContents* web_contents,
                     std::unique_ptr<PendingAccessRequest> pending_request,
                     const content::DesktopMediaID& media_id,
                     bool capture_audio);

  std::unique_ptr<DesktopMediaPickerFactory> picker_factory_;
  bool display_notification_;
  RequestsQueues pending_requests_;

  WebContentsCollection web_contents_collection_;

#if BUILDFLAG(IS_CHROMEOS)
  // Called back after checking Data Leak Prevention (DLP) restrictions.
  void OnDlpRestrictionChecked(
      base::WeakPtr<content::WebContents> web_contents,
      std::unique_ptr<PendingAccessRequest> pending_request,
      const content::DesktopMediaID& media_id,
      bool capture_audio,
      bool is_dlp_allowed);

  raw_ptr<aura::Window, DanglingUntriaged> primary_root_window_for_testing_ =
      nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_ACCESS_HANDLER_H_
