// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/capture_access_handler_base.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"

#if defined(OS_CHROMEOS)
#include "base/hash/sha1.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

// Tracks MEDIA_DESKTOP/TAB_VIDEO_CAPTURE sessions. Sessions are removed when
// MEDIA_REQUEST_STATE_CLOSING is encountered.
struct CaptureAccessHandlerBase::Session {
  int render_process_id;
  int render_frame_id;
  int page_request_id;
  // Extensions control the routing of the captured MediaStream content.
  // Therefore, only built-in extensions (and certain whitelisted ones) can be
  // trusted to set-up secure links.
  bool is_trusted;

  // This is true only if all connected video sinks are reported secure.
  bool is_capturing_link_secure;
};

CaptureAccessHandlerBase::CaptureAccessHandlerBase() {}

CaptureAccessHandlerBase::~CaptureAccessHandlerBase() {}

void CaptureAccessHandlerBase::AddCaptureSession(int render_process_id,
                                                 int render_frame_id,
                                                 int page_request_id,
                                                 bool is_trusted) {
  Session session = {render_process_id, render_frame_id, page_request_id,
                     is_trusted, true};
  sessions_.push_back(session);
}

void CaptureAccessHandlerBase::RemoveCaptureSession(int render_process_id,
                                                    int render_frame_id,
                                                    int page_request_id) {
  auto it = FindSession(render_process_id, render_frame_id, page_request_id);
  if (it != sessions_.end())
    sessions_.erase(it);
}

std::list<CaptureAccessHandlerBase::Session>::iterator
CaptureAccessHandlerBase::FindSession(int render_process_id,
                                      int render_frame_id,
                                      int page_request_id) {
  return std::find_if(sessions_.begin(), sessions_.end(),
                      [render_process_id, render_frame_id,
                       page_request_id](const Session& session) {
                        return session.render_process_id == render_process_id &&
                               session.render_frame_id == render_frame_id &&
                               session.page_request_id == page_request_id;
                      });
}

void CaptureAccessHandlerBase::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((stream_type !=
       blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) &&
      (stream_type != blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE) &&
      (stream_type != blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE) &&
      (stream_type != blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE)) {
    return;
  }

  if (state == content::MEDIA_REQUEST_STATE_DONE) {
    if (FindSession(render_process_id, render_frame_id, page_request_id) ==
        sessions_.end()) {
      AddCaptureSession(render_process_id, render_frame_id, page_request_id,
                        false);
      DVLOG(2) << "Add new session while UpdateMediaRequestState"
               << " render_process_id: " << render_process_id
               << " render_frame_id: " << render_frame_id
               << " page_request_id: " << page_request_id;
    }
  } else if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    RemoveCaptureSession(render_process_id, render_frame_id, page_request_id);
    DVLOG(2) << "Remove session: "
             << " render_process_id: " << render_process_id
             << " render_frame_id: " << render_frame_id
             << " page_request_id: " << page_request_id;
  }
}

void CaptureAccessHandlerBase::UpdateExtensionTrusted(
    const content::MediaStreamRequest& request,
    const extensions::Extension* extension) {
  const bool is_trusted = MediaCaptureDevicesDispatcher::IsOriginForCasting(
                              request.security_origin) ||
                          IsExtensionWhitelistedForScreenCapture(extension) ||
                          IsBuiltInExtension(request.security_origin);
  UpdateTrusted(request, is_trusted);
}

void CaptureAccessHandlerBase::UpdateTrusted(
    const content::MediaStreamRequest& request,
    bool is_trusted) {
  auto it = FindSession(request.render_process_id, request.render_frame_id,
                        request.page_request_id);
  if (it != sessions_.end()) {
    it->is_trusted = is_trusted;
    DVLOG(2) << "CaptureAccessHandlerBase::UpdateTrusted"
             << " render_process_id: " << request.render_process_id
             << " render_frame_id: " << request.render_frame_id
             << " page_request_id: " << request.page_request_id
             << " is_trusted: " << is_trusted;
    return;
  }

  AddCaptureSession(request.render_process_id, request.render_frame_id,
                    request.page_request_id, is_trusted);
  DVLOG(2) << "Add new session while UpdateTrusted"
           << " render_process_id: " << request.render_process_id
           << " render_frame_id: " << request.render_frame_id
           << " page_request_id: " << request.page_request_id
           << " is_trusted: " << is_trusted;
}

bool CaptureAccessHandlerBase::IsInsecureCapturingInProgress(
    int render_process_id,
    int render_frame_id) {
  if (sessions_.empty())
    return false;
  for (const Session& session : sessions_) {
    if (session.render_process_id != render_process_id ||
        session.render_frame_id != render_frame_id)
      continue;
    if (!session.is_trusted || !session.is_capturing_link_secure)
      return true;
  }
  return false;
}

void CaptureAccessHandlerBase::UpdateVideoScreenCaptureStatus(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    bool is_secure) {
  auto it = FindSession(render_process_id, render_frame_id, page_request_id);
  if (it != sessions_.end()) {
    it->is_capturing_link_secure = is_secure;
    DVLOG(2) << "UpdateVideoScreenCaptureStatus:"
             << " render_process_id: " << render_process_id
             << " render_frame_id: " << render_frame_id
             << " page_request_id: " << page_request_id
             << " is_capturing_link_secure: " << is_secure;
  }
}

bool CaptureAccessHandlerBase::IsExtensionWhitelistedForScreenCapture(
    const extensions::Extension* extension) {
  if (!extension)
    return false;

#if defined(OS_CHROMEOS)
  std::string hash = base::SHA1HashString(extension->id());
  std::string hex_hash = base::HexEncode(hash.c_str(), hash.length());

  // crbug.com/446688
  return hex_hash == "4F25792AF1AA7483936DE29C07806F203C7170A0" ||
         hex_hash == "BD8781D757D830FC2E85470A1B6E8A718B7EE0D9" ||
         hex_hash == "4AC2B6C63C6480D150DFDA13E4A5956EB1D0DDBB" ||
         hex_hash == "81986D4F846CEDDDB962643FA501D1780DD441BB";
#else
  return false;
#endif  // defined(OS_CHROMEOS)
}

bool CaptureAccessHandlerBase::IsBuiltInExtension(const GURL& origin) {
  return
      // Feedback Extension.
      origin.spec() == "chrome-extension://gfdkimpbcpahaombhbimeihdjnejgicl/";
}
