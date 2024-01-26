// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_UKM_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_UKM_HELPER_H_

#include <cstdint>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace video_conference {

// A helper class for UKM logging and recording for VC web apps. An instance
// should be created on each `VideoConferenceWebApp` and used to register media
// device capturing updates. When the VC web app is destroyed it in turn calls
// the destructor on `VideoConferenceUkmHelper` thus recording the metrics
// relevant to that VC app.
class VideoConferenceUkmHelper {
 public:
  VideoConferenceUkmHelper(ukm::UkmRecorder* ukm_recorder,
                           ukm::SourceId source_id);

  VideoConferenceUkmHelper(const VideoConferenceUkmHelper&) = delete;
  VideoConferenceUkmHelper& operator=(const VideoConferenceUkmHelper&) = delete;

  ~VideoConferenceUkmHelper();

  // Register a camera/microphone/screen update on the VC app. This UKM helper
  // class will then record it and update the capturing duration of the media
  // device.
  void RegisterCapturingUpdate(VideoConferenceMediaType device,
                               bool is_capturing);

 private:
  friend class FakeVideoConferenceUkmHelper;

  // Increments `camera_capture_duration_` by the time delta between time of
  // call and `prev_camera_capture_time_`. Expects `prev_camera_capture_time_`
  // to not be nullopt when called.
  void UpdateCameraCaptureDuration();
  // Same as `UpdateCameraCaptureDuration` but for
  // `microphone_capture_duration_`.
  void UpdateMicrophoneCaptureDuration();
  // Same as `UpdateCameraCaptureDuration` but for `screen_capture_duration_`.
  void UpdateScreenCaptureDuration();

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;

  // UKM source id associated with the webcontents at the time for first media
  // device capture.
  ukm::SourceId source_id_;
  // Time when this VC app was created. Used for recording the total time in ms
  // that this app was running.
  base::Time start_time_;
  // Booleans indicating if the app captured a media device. This is explicit
  // (rather than implicit from the capture durations) to account for situations
  // where capture durations are sub-milisecond so get recorded as 0 ms.
  bool did_capture_camera_ = false;
  bool did_capture_microphone_ = false;
  bool did_capture_screen_ = false;
  // Sum of capture durations in ms for camera, microphone, and screen.
  uint64_t camera_capture_duration_ = 0;
  uint64_t microphone_capture_duration_ = 0;
  uint64_t screen_capture_duration_ = 0;

  std::optional<base::Time> prev_camera_capture_time_ = std::nullopt;
  std::optional<base::Time> prev_microphone_capture_time_ = std::nullopt;
  std::optional<base::Time> prev_screen_capture_time_ = std::nullopt;
};

}  // namespace video_conference

#endif  // CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_UKM_HELPER_H_
