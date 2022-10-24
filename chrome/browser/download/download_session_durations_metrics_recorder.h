// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SESSION_DURATIONS_METRICS_RECORDER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SESSION_DURATIONS_METRICS_RECORDER_H_

#include "base/time/time.h"

class DownloadSessionDurationsMetricsRecorder {
 public:
  DownloadSessionDurationsMetricsRecorder();
  ~DownloadSessionDurationsMetricsRecorder() = default;
  DownloadSessionDurationsMetricsRecorder(
      const DownloadSessionDurationsMetricsRecorder&) = delete;

  // Called from DesktopProfileSessionDurationsService
  void OnSessionStarted(base::TimeTicks session_start);
  void OnSessionEnded(base::TimeTicks session_end);

 private:
  base::TimeTicks session_start_;
  bool is_bubble_showing_when_session_end_ = false;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SESSION_DURATIONS_METRICS_RECORDER_H_
