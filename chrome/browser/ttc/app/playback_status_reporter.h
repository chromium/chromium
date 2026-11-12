// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PLAYBACK_STATUS_REPORTER_H_
#define CHROME_BROWSER_TTC_APP_PLAYBACK_STATUS_REPORTER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ttc {

// Tracks cumulative audio playback progress for a single model generation turn
// and emits periodic and completion playback status reports.
class PlaybackStatusReporter {
 public:
  using SendReportCallback =
      base::RepeatingCallback<void(base::TimeDelta playback_progress,
                                   bool playback_complete,
                                   int64_t generation_id)>;

  PlaybackStatusReporter(int64_t generation_id,
                         SendReportCallback send_report_callback);
  ~PlaybackStatusReporter();

  PlaybackStatusReporter(const PlaybackStatusReporter&) = delete;
  PlaybackStatusReporter& operator=(const PlaybackStatusReporter&) = delete;

  void OnPlaybackProgress(base::TimeDelta played_duration,
                          bool did_fully_drain_queue);
  void OnGenerationCompleted(bool is_playing);
  void OnInterrupted();
  void OnPlaybackError();
  void CompleteIfNeeded();

 private:
  enum class State {
    kPlaybackStarted,
    kGenerationCompleted,
    kPlaybackCompletedSent,
  };

  void OnReportTimerFired();
  void CompletePlayback();
  void SendReport(bool playback_complete);

  const int64_t generation_id_;
  SendReportCallback send_report_callback_;
  State state_ = State::kPlaybackStarted;
  base::TimeDelta playback_progress_;
  base::TimeDelta last_reported_progress_;
  base::RepeatingTimer report_timer_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PLAYBACK_STATUS_REPORTER_H_
