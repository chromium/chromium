// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_ukm_helper.h"

#include <cstdint>
#include <optional>

#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace video_conference {

VideoConferenceUkmHelper::VideoConferenceUkmHelper(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id)
    : ukm_recorder_(ukm_recorder),
      source_id_(source_id),
      start_time_(base::Time::Now()) {
  CHECK(ukm_recorder_);
}

VideoConferenceUkmHelper::~VideoConferenceUkmHelper() {
  // Finish recording durations for any remaining captures.
  if (prev_camera_capture_time_) {
    UpdateCameraCaptureDuration();
  }

  if (prev_microphone_capture_time_) {
    UpdateMicrophoneCaptureDuration();
  }

  if (prev_screen_capture_time_) {
    UpdateScreenCaptureDuration();
  }

  auto total_duration = (base::Time::Now() - start_time_).InMilliseconds();

  // A `VideoConferenceWebApp` (which owns `this`) is only created if a
  // webcontents captures camera/microphone/screen at least once so we do not
  // need to check for that.
  ukm::builders::VideoConferencingEvent(source_id_)
      .SetDidCaptureCamera(did_capture_camera_)
      .SetDidCaptureMicrophone(did_capture_microphone_)
      .SetDidCaptureScreen(did_capture_screen_)
      .SetCameraCaptureDuration(
          ukm::GetSemanticBucketMinForDurationTiming(camera_capture_duration_))
      .SetMicrophoneCaptureDuration(ukm::GetSemanticBucketMinForDurationTiming(
          microphone_capture_duration_))
      .SetScreenCaptureDuration(
          ukm::GetSemanticBucketMinForDurationTiming(screen_capture_duration_))
      .SetTotalDuration(
          ukm::GetSemanticBucketMinForDurationTiming(total_duration))
      .Record(ukm_recorder_);
}

void VideoConferenceUkmHelper::RegisterCapturingUpdate(
    VideoConferenceMediaType device,
    bool is_capturing) {
  switch (device) {
    case VideoConferenceMediaType::kCamera: {
      if (!prev_camera_capture_time_.has_value() && is_capturing) {
        // Camera changed from not capturing to capturing.
        did_capture_camera_ = true;
        prev_camera_capture_time_ = base::Time::Now();
      } else if (prev_camera_capture_time_.has_value() && !is_capturing) {
        // Camera changed from capturing to not capturing.
        UpdateCameraCaptureDuration();
      }
      break;
    }
    case VideoConferenceMediaType::kMicrophone: {
      if (!prev_microphone_capture_time_.has_value() && is_capturing) {
        // Microphone changed from not capturing to capturing.
        did_capture_microphone_ = true;
        prev_microphone_capture_time_ = base::Time::Now();
      } else if (prev_microphone_capture_time_.has_value() && !is_capturing) {
        // Microphone changed from capturing to not capturing.
        UpdateMicrophoneCaptureDuration();
      }
      break;
    }
    case VideoConferenceMediaType::kScreen: {
      if (!prev_screen_capture_time_.has_value() && is_capturing) {
        // Screen changed from not capturing to capturing.
        did_capture_screen_ = true;
        prev_screen_capture_time_ = base::Time::Now();
      } else if (prev_screen_capture_time_.has_value() && !is_capturing) {
        // Screen changed from capturing to not capturing.
        UpdateScreenCaptureDuration();
      }
      break;
    }
  }
}

void VideoConferenceUkmHelper::UpdateCameraCaptureDuration() {
  DCHECK(prev_camera_capture_time_) << "VideoConferenceUkmHelper update cannot "
                                       "compute duration without start time.";
  auto duration =
      (base::Time::Now() - *prev_camera_capture_time_).InMilliseconds();
  camera_capture_duration_ += duration;
  prev_camera_capture_time_ = std::nullopt;
}

void VideoConferenceUkmHelper::UpdateMicrophoneCaptureDuration() {
  DCHECK(prev_microphone_capture_time_)
      << "VideoConferenceUkmHelper update cannot compute duration without "
         "start time.";
  auto duration =
      (base::Time::Now() - *prev_microphone_capture_time_).InMilliseconds();
  microphone_capture_duration_ += duration;
  prev_microphone_capture_time_ = std::nullopt;
}

void VideoConferenceUkmHelper::UpdateScreenCaptureDuration() {
  DCHECK(prev_screen_capture_time_)
      << "VideoConferenceUkmHelper update cannot compute duration without "
         "start time.";
  auto duration =
      (base::Time::Now() - *prev_screen_capture_time_).InMilliseconds();
  screen_capture_duration_ += duration;
  prev_screen_capture_time_ = std::nullopt;
}

}  // namespace video_conference
