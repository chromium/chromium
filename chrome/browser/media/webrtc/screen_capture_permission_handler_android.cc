// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/screen_capture_permission_handler_android.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace screen_capture {
void GetScreenCapturePermissionAndroid(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(request.video_type ==
             blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
         request.video_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
         request.video_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
         request.video_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET);

  // TODO(crbug.com/40160723): Implement a capture prompt.
  blink::mojom::MediaStreamRequestResult result =
      base::FeatureList::IsEnabled(features::kUserMediaScreenCapturing)
          ? blink::mojom::MediaStreamRequestResult::OK
          : blink::mojom::MediaStreamRequestResult::INVALID_STATE;

  // TODO(crbug.com/40216442): Generalize to multiple streams.
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];
  std::unique_ptr<content::MediaStreamUI> ui;
  if (result == blink::mojom::MediaStreamRequestResult::OK) {
    if (request.video_type ==
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
      content::DesktopMediaID screen_id;
      screen_id.type = content::DesktopMediaID::TYPE_WEB_CONTENTS;
      screen_id.web_contents_id = content::WebContentsMediaCaptureId(
          request.render_process_id, request.render_frame_id);
      stream_devices.video_device = blink::MediaStreamDevice(
          request.video_type, screen_id.ToString(), "Current Tab");
    } else {
      content::DesktopMediaID screen_id = content::DesktopMediaID(
          content::DesktopMediaID::TYPE_SCREEN, webrtc::kFullDesktopScreenId);
      stream_devices.video_device = blink::MediaStreamDevice(
          request.video_type, screen_id.ToString(), "Screen");
    }

    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, stream_devices);
  }

  std::move(callback).Run(stream_devices_set, result, std::move(ui));
}
}  // namespace screen_capture
