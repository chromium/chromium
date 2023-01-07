// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/video_capture_event_provider.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"

VideoCaptureEventProvider::VideoCaptureEventProvider(
    UsageScenarioDataStoreImpl* data_store)
    : data_store_(data_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_stream_capture_indicator_observation_.Observe(
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get());
}

VideoCaptureEventProvider::~VideoCaptureEventProvider() = default;

void VideoCaptureEventProvider::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_capturing_video) {
    data_store_->OnIsCapturingVideoStarted();
  } else {
    data_store_->OnIsCapturingVideoEnded();
  }
}
