// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_media_listener.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace video_conference {

VideoConferenceMediaListener::VideoConferenceMediaListener(
    base::RepeatingCallback<void()> media_usage_update_callback,
    base::RepeatingCallback<VideoConferenceWebApp*(content::WebContents*)>
        create_vc_web_app_callback)
    : media_usage_update_callback_(media_usage_update_callback),
      create_vc_web_app_callback_(create_vc_web_app_callback) {
  observation_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                           ->GetMediaStreamCaptureIndicator()
                           .get());
}

VideoConferenceMediaListener::~VideoConferenceMediaListener() = default;

void VideoConferenceMediaListener::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  VideoConferenceWebApp* vc_app =
      GetOrCreateVcWebApp(contents, is_capturing_video);

  // It is normal for `vc_app` to be a nullptr, e.g. when this method is called
  // upon the deletion of a `VideoConferenceWebApp` with an is_capturing of
  // false.
  if (vc_app) {
    vc_app->state().is_capturing_camera = is_capturing_video;
    media_usage_update_callback_.Run();
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
    vc_app->state().is_capturing_microphone = is_capturing_audio;
    media_usage_update_callback_.Run();
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
    vc_app->state().is_capturing_screen = is_capturing_screen;
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
