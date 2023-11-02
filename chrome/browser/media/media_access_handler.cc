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
  bool get_default_audio_device = audio_allowed;
  bool get_default_video_device = video_allowed;

  // TOOD(crbug.com/1300883): Generalize to multiple streams.
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];

  // Set an initial error result. If neither audio or video is allowed, we'll
  // never try to get any device below but will just create |ui| and return an
  // empty list with "invalid state" result. If at least one is allowed, we'll
  // try to get device(s), and if failure, we want to return "no hardware"
  // result.
  // TODO(grunell): The invalid state result should be changed to a new denied
  // result + a dcheck to ensure at least one of audio or video types is
  // capture.
  blink::mojom::MediaStreamRequestResult result =
      (audio_allowed || video_allowed)
          ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
          : blink::mojom::MediaStreamRequestResult::INVALID_STATE;

  // Get the exact audio or video device if an id is specified.
  // We only set any error result here and before running the callback change
  // it to OK if we have any device.
  if (audio_allowed && !request.requested_audio_device_id.empty()) {
    const blink::MediaStreamDevice* audio_device =
        MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedAudioDevice(
            request.requested_audio_device_id);
    if (audio_device) {
      stream_devices.audio_device = *audio_device;
      get_default_audio_device = false;
    }
  }
  if (video_allowed && !request.requested_video_device_id.empty()) {
    const blink::MediaStreamDevice* video_device =
        MediaCaptureDevicesDispatcher::GetInstance()->GetRequestedVideoDevice(
            request.requested_video_device_id);
    if (video_device) {
      stream_devices.video_device = *video_device;
      get_default_video_device = false;
    }
  }

  // If either or both audio and video devices were requested but not
  // specified by id, get the default devices.
  if (get_default_audio_device || get_default_video_device) {
    MediaCaptureDevicesDispatcher::GetInstance()
        ->GetDefaultDevicesForBrowserContext(
            web_contents->GetBrowserContext(), get_default_audio_device,
            get_default_video_device, stream_devices);
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
