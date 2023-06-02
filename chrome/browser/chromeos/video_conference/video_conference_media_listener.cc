// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_media_listener.h"

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace video_conference {

VideoConferenceMediaListener::VideoConferenceMediaListener(
    base::RepeatingCallback<void()> media_usage_update_callback,
    base::RepeatingCallback<VideoConferenceWebApp*(content::WebContents*)>
        create_vc_web_app_callback,
    base::RepeatingCallback<void(crosapi::mojom::VideoConferenceMediaDevice,
                                 const std::u16string&)>
        device_used_while_disabled_callback)
    : media_usage_update_callback_(std::move(media_usage_update_callback)),
      create_vc_web_app_callback_(std::move(create_vc_web_app_callback)),
      device_used_while_disabled_callback_(
          std::move(device_used_while_disabled_callback)) {
  observation_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                           ->GetMediaStreamCaptureIndicator()
                           .get());
}

VideoConferenceMediaListener::~VideoConferenceMediaListener() = default;

void VideoConferenceMediaListener::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool disabled) {
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      camera_system_disabled_ = disabled;
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      microphone_system_disabled_ = disabled;
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kUnusedDefault:
      return;
  }
}

void VideoConferenceMediaListener::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  VideoConferenceWebApp* vc_app =
      GetOrCreateVcWebApp(contents, is_capturing_video);
  // It is normal for `vc_app` to be a nullptr, e.g. when this method is called
  // upon the deletion of a `VideoConferenceWebApp` with an is_capturing of
  // false.
  if (vc_app) {
    auto& state = vc_app->state();
    bool prev_is_capturing_video = state.is_capturing_camera;
    vc_app->SetCapturingStatus(VideoConferenceMediaType::kCamera,
                               is_capturing_video);

    // Remove `vc_app` from client if it belongs to an extension which has
    // stopped capturing.
    if (vc_app->IsInactiveExtension()) {
      vc_app->WebContentsDestroyed();
      return;
    }

    media_usage_update_callback_.Run();

    // This will be an AnchoredNudge, which is only visible if the tray is
    // visible; so we have to call this after media_usage_update_callback_.
    if (camera_system_disabled_ && !prev_is_capturing_video &&
        is_capturing_video) {
      device_used_while_disabled_callback_.Run(
          crosapi::mojom::VideoConferenceMediaDevice::kCamera,
          contents->GetTitle());
    }
  }
}

void VideoConferenceMediaListener::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  VideoConferenceWebApp* vc_app =
      GetOrCreateVcWebApp(contents, is_capturing_audio);

  // It is normal for `vc_app` to be a nullptr, e.g. when this method is called
  // upon the deletion of a `VideoConferenceWebApp` with an is_capturing of
  // false.
  if (vc_app) {
    auto& state = vc_app->state();
    bool prev_is_capturing_audio = state.is_capturing_microphone;
    vc_app->SetCapturingStatus(VideoConferenceMediaType::kMicrophone,
                               is_capturing_audio);

    // Remove `vc_app` from client if it belongs to an extension which has
    // stopped capturing.
    if (vc_app->IsInactiveExtension()) {
      vc_app->WebContentsDestroyed();
      return;
    }

    media_usage_update_callback_.Run();

    // This will be an AnchoredNudge, which is only visible if the tray is
    // visible; so we have to call this after media_usage_update_callback_.
    if (microphone_system_disabled_ && !prev_is_capturing_audio &&
        is_capturing_audio) {
      device_used_while_disabled_callback_.Run(
          crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
          contents->GetTitle());
    }
  }
}

void VideoConferenceMediaListener::OnIsCapturingWindowChanged(
    content::WebContents* contents,
    bool is_capturing_window) {
  // We don't distinguish between window and display capture and put them
  // together into 'screen capturing'.
  OnIsCapturingScreenChanged(contents, is_capturing_window);
}

void VideoConferenceMediaListener::OnIsCapturingDisplayChanged(
    content::WebContents* contents,
    bool is_capturing_display) {
  // We don't distinguish between window and display capture and put them
  // together into 'screen capturing'.
  OnIsCapturingScreenChanged(contents, is_capturing_display);
}

void VideoConferenceMediaListener::OnIsCapturingScreenChanged(
    content::WebContents* contents,
    bool is_capturing_screen) {
  VideoConferenceWebApp* vc_app =
      GetOrCreateVcWebApp(contents, is_capturing_screen);

  // It is normal for `vc_app` to be a nullptr, e.g. when this method is called
  // upon the deletion of a `VideoConferenceWebApp` with an is_capturing of
  // false.
  if (vc_app) {
    vc_app->SetCapturingStatus(VideoConferenceMediaType::kScreen,
                               is_capturing_screen);

    // Remove `vc_app` from client if it belongs to an extension which has
    // stopped capturing.
    if (vc_app->IsInactiveExtension()) {
      vc_app->WebContentsDestroyed();
      return;
    }

    media_usage_update_callback_.Run();
  }
}

VideoConferenceWebApp* VideoConferenceMediaListener::GetOrCreateVcWebApp(
    content::WebContents* contents,
    bool is_capturing) {
  auto* vc_app =
      content::WebContentsUserData<VideoConferenceWebApp>::FromWebContents(
          contents);

  // Create and add a |VideoConferenceWebApp| for this webcontents on the
  // client if it doesn't exist.
  if (!vc_app) {
    // A new VcWebApp should only be created the first time an app starts
    // capturing. For example, we do not want to create a new VcWebApp if an old
    // one is closed and that causes an OnIsCapturingXChanged to trigger with a
    // capturing value of false.
    if (!is_capturing) {
      return nullptr;
    }

    if (ShouldSkipId(contents->GetURL().host())) {
      return nullptr;
    }

    vc_app = create_vc_web_app_callback_.Run(contents);
  }

  return vc_app;
}

}  // namespace video_conference
