// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_access_handler.h"

#include <memory>
#include <utility>

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

bool MediaAccessHandler::IsInsecureCapturingInProgress(int render_process_id,
                                                       int render_frame_id) {
  return false;
}

// static
void MediaAccessHandler::CheckDevicesAndRunCallback(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    bool audio_allowed,
    bool video_allowed) {
  // TODO(vrk): This code is largely duplicated in
  // MediaStreamDevicesController::GetDevices(). Move this code into a shared
  // method between the two classes.

  // TODO(crbug.com/40216442): Generalize to multiple streams.
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];

  // Set an initial error result. If neither audio or video is allowed, we'll
  // never try to get any device below but will just create |ui| and return an
  // empty list with a "permission denied" result. If at least one is allowed,
  // we'll try to get the device(s), and if failure, we want to return a "no
  // hardware" result.
  blink::mojom::MediaStreamRequestResult result =
      (audio_allowed || video_allowed)
          ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
          : blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;

  // Get the exact audio or video device if an id is specified.
  // We only set any error result here and before running the callback change
  // it to OK if we have any device.
  if (audio_allowed) {
    stream_devices.audio_device =
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetPreferredAudioDeviceForBrowserContext(
                web_contents->GetBrowserContext(),
                request.requested_audio_device_ids);
  }
  if (video_allowed) {
    stream_devices.video_device =
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetPreferredVideoDeviceForBrowserContext(
                web_contents->GetBrowserContext(),
                request.requested_video_device_ids);
  }

  std::unique_ptr<content::MediaStreamUI> ui;
  if (stream_devices.audio_device.has_value() ||
      stream_devices.video_device.has_value()) {
    result = blink::mojom::MediaStreamRequestResult::OK;
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, stream_devices);
  }

  if (!stream_devices.audio_device.has_value() &&
      !stream_devices.video_device.has_value()) {
    stream_devices_set.stream_devices.clear();
  }

  std::move(callback).Run(stream_devices_set, result, std::move(ui));
}
