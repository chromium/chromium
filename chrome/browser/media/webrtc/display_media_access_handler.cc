// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/display_media_access_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

// Holds pending request information so that we display one picker UI at a time
// for each content::WebContents.
struct DisplayMediaAccessHandler::PendingAccessRequest {
  PendingAccessRequest(std::unique_ptr<DesktopMediaPicker> picker,
                       const content::MediaStreamRequest& request,
                       content::MediaResponseCallback callback)
      : picker(std::move(picker)),
        request(request),
        callback(std::move(callback)) {}
  ~PendingAccessRequest() = default;

  std::unique_ptr<DesktopMediaPicker> picker;
  content::MediaStreamRequest request;
  content::MediaResponseCallback callback;
};

DisplayMediaAccessHandler::DisplayMediaAccessHandler()
    : picker_factory_(new DesktopMediaPickerFactoryImpl()) {
  AddNotificationObserver();
}

DisplayMediaAccessHandler::DisplayMediaAccessHandler(
    std::unique_ptr<DesktopMediaPickerFactory> picker_factory,
    bool display_notification)
    : display_notification_(display_notification),
      picker_factory_(std::move(picker_factory)) {
  AddNotificationObserver();
}

DisplayMediaAccessHandler::~DisplayMediaAccessHandler() = default;

bool DisplayMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const content::MediaStreamType stream_type,
    const extensions::Extension* extension) {
  return stream_type == content::MEDIA_DISPLAY_VIDEO_CAPTURE;
}

bool DisplayMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DisplayMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<DesktopMediaPicker> picker = picker_factory_->CreatePicker();
  if (!picker) {
    std::move(callback).Run(content::MediaStreamDevices(),
                            content::MEDIA_DEVICE_INVALID_STATE, nullptr);
    return;
  }

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(std::make_unique<PendingAccessRequest>(
      std::move(picker), request, std::move(callback)));
  // If this is the only request then pop picker UI.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    content::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state != content::MEDIA_REQUEST_STATE_DONE &&
      state != content::MEDIA_REQUEST_STATE_CLOSING) {
    return;
  }

  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    DeletePendingAccessRequest(render_process_id, render_frame_id,
                               page_request_id);
  }
  CaptureAccessHandlerBase::UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, stream_type, state);

  // This method only gets called with the above checked states when all
  // requests are to be canceled. Therefore, we don't need to process the
  // next queued request.
}

void DisplayMediaAccessHandler::ProcessQueuedAccessRequest(
    const RequestsQueue& queue,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const PendingAccessRequest& pending_request = *queue.front();
  UpdateTrusted(pending_request.request, false /* is_trusted */);

  std::vector<content::DesktopMediaID::Type> media_types = {
      content::DesktopMediaID::TYPE_SCREEN,
      content::DesktopMediaID::TYPE_WINDOW,
      content::DesktopMediaID::TYPE_WEB_CONTENTS};
  auto source_lists = picker_factory_->CreateMediaList(media_types);

  DesktopMediaPicker::DoneCallback done_callback =
      base::BindRepeating(&DisplayMediaAccessHandler::OnPickerDialogResults,
                          base::Unretained(this), web_contents);
  DesktopMediaPicker::Params picker_params;
  picker_params.web_contents = web_contents;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = base::ASCIIToUTF16(
      web_contents->GetVisibleURL().GetWithEmptyPath().spec());
  picker_params.target_name = picker_params.app_name;
  picker_params.request_audio = false;
  pending_request.picker->Show(picker_params, std::move(source_lists),
                               done_callback);
}

void DisplayMediaAccessHandler::OnPickerDialogResults(
    content::WebContents* web_contents,
    content::DesktopMediaID media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end())
    return;
  RequestsQueue& queue = it->second;
  if (queue.empty()) {
    // UpdateMediaRequestState() called with MEDIA_REQUEST_STATE_CLOSING. Don't
    // need to do anything.
    return;
  }

  PendingAccessRequest& pending_request = *queue.front();
  content::MediaStreamDevices devices;
  content::MediaStreamRequestResult request_result =
      content::MEDIA_DEVICE_PERMISSION_DENIED;
  std::unique_ptr<content::MediaStreamUI> ui;
  if (media_id.is_null()) {
    request_result = content::MEDIA_DEVICE_PERMISSION_DENIED;
  } else {
    request_result = content::MEDIA_DEVICE_OK;
    ui = GetDevicesForDesktopCapture(
        web_contents, &devices, media_id, content::MEDIA_DISPLAY_VIDEO_CAPTURE,
        content::MEDIA_NO_SERVICE, false /* capture_audio */,
        false /* disable_local_echo */, display_notification_,
        web_contents->GetTitle(), web_contents->GetTitle());
  }

  std::move(pending_request.callback)
      .Run(devices, request_result, std::move(ui));
  queue.pop_front();

  if (!queue.empty())
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::AddNotificationObserver() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notifications_registrar_.Add(this,
                               content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                               content::NotificationService::AllSources());
}

void DisplayMediaAccessHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);

  pending_requests_.erase(content::Source<content::WebContents>(source).ptr());
}

void DisplayMediaAccessHandler::DeletePendingAccessRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id) {
  for (auto& queue_it : pending_requests_) {
    RequestsQueue& queue = queue_it.second;
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      const PendingAccessRequest& pending_request = **it;
      if (pending_request.request.render_process_id == render_process_id &&
          pending_request.request.render_frame_id == render_frame_id &&
          pending_request.request.page_request_id == page_request_id) {
        queue.erase(it);
        return;
      }
    }
  }
}
