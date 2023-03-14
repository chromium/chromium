// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MEDIA_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MEDIA_LISTENER_H_

#include <string>
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "content/public/browser/web_contents.h"

namespace video_conference {

class VideoConferenceWebApp;

// This class listens for changes in the capturing status of pages. It is
// responsible for creating `VideoConferenceWebApp` for webcontents with any
// video/audio/screen capturing activity if one doesn't already exist. This
// class is also responsible for notifying the `VideoConferenceManagerClient`
// owning it of any observed capturing changes.
class VideoConferenceMediaListener
    : public MediaStreamCaptureIndicator::Observer {
 public:
  VideoConferenceMediaListener(
      base::RepeatingCallback<void()> media_usage_update_callback,
      base::RepeatingCallback<VideoConferenceWebApp*(content::WebContents*)>
          create_vc_web_app_callback,
      base::RepeatingCallback<void(crosapi::mojom::VideoConferenceMediaDevice,
                                   const std::u16string&)>
          device_used_while_disabled_callback);

  VideoConferenceMediaListener(const VideoConferenceMediaListener&) = delete;
  VideoConferenceMediaListener& operator=(const VideoConferenceMediaListener&) =
      delete;

  ~VideoConferenceMediaListener() override;

  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled);

  // MediaStreamCaptureIndicator::Observer overrides
  void OnIsCapturingVideoChanged(content::WebContents* contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* contents,
                                 bool is_capturing_audio) override;
  void OnIsCapturingWindowChanged(content::WebContents* contents,
                                  bool is_capturing_window) override;
  void OnIsCapturingDisplayChanged(content::WebContents* contents,
                                   bool is_capturing_display) override;

 private:
  friend class FakeVideoConferenceManagerClient;
  friend class FakeVideoConferenceMediaListener;

  // Returns the `VideoConferenceWebApp` corresponding to this
  // webcontents. If it doesn't exist, also first creates it if `is_capturing`
  // is true.
  VideoConferenceWebApp* GetOrCreateVcWebApp(content::WebContents* contents,
                                             bool is_capturing);

  // Sets `is_capturing_screen` on the `VideoConferenceWebApp` associated with
  // |contents| and notifies the client to handle updates.
  void OnIsCapturingScreenChanged(content::WebContents* contents,
                                  bool is_capturing_screen);

  // The following two fields are true if the camera/microphone is system-wide
  // software disabled OR disabled via a hardware switch.
  bool camera_system_disabled_{false};
  bool microphone_system_disabled_{false};

  base::RepeatingCallback<void()> media_usage_update_callback_;
  base::RepeatingCallback<VideoConferenceWebApp*(content::WebContents*)>
      create_vc_web_app_callback_;
  base::RepeatingCallback<void(crosapi::mojom::VideoConferenceMediaDevice,
                               const std::u16string&)>
      device_used_while_disabled_callback_;

  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      observation_{this};
};

}  // namespace video_conference

#endif  // CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MEDIA_LISTENER_H_
