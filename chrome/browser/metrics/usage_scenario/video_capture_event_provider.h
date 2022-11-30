// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_VIDEO_CAPTURE_EVENT_PROVIDER_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_VIDEO_CAPTURE_EVENT_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

class UsageScenarioDataStoreImpl;

// Provides events related to video capture to the data store.
class VideoCaptureEventProvider : public MediaStreamCaptureIndicator::Observer {
 public:
  explicit VideoCaptureEventProvider(UsageScenarioDataStoreImpl* data_store);
  ~VideoCaptureEventProvider() override;

  VideoCaptureEventProvider(const VideoCaptureEventProvider& rhs) = delete;
  VideoCaptureEventProvider& operator=(const VideoCaptureEventProvider& rhs) =
      delete;

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // The data store for the video capture events. Must outlive |this|.
  const raw_ptr<UsageScenarioDataStoreImpl> data_store_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_stream_capture_indicator_observation_{this};
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_VIDEO_CAPTURE_EVENT_PROVIDER_H_
