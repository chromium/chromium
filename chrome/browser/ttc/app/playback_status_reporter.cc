// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/playback_status_reporter.h"

#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "chrome/browser/ttc/core/features.h"

namespace ttc {

PlaybackStatusReporter::PlaybackStatusReporter(
    int64_t generation_id,
    SendReportCallback send_report_callback)
    : generation_id_(generation_id),
      send_report_callback_(std::move(send_report_callback)) {
  CHECK(send_report_callback_);
  report_timer_.Start(FROM_HERE, kPlaybackStatusReportInterval.Get(), this,
                      &PlaybackStatusReporter::OnReportTimerFired);
}

PlaybackStatusReporter::~PlaybackStatusReporter() = default;

void PlaybackStatusReporter::CompleteIfNeeded() {
  if (state_ != State::kPlaybackCompletedSent) {
    CompletePlayback();
  }
}

void PlaybackStatusReporter::OnPlaybackProgress(base::TimeDelta played_duration,
                                                bool did_fully_drain_queue) {
  if (state_ == State::kPlaybackCompletedSent) {
    return;
  }
  playback_progress_ += played_duration;
  if (state_ == State::kGenerationCompleted && did_fully_drain_queue) {
    CompletePlayback();
  }
}

void PlaybackStatusReporter::OnGenerationCompleted(bool is_playing) {
  if (state_ == State::kPlaybackCompletedSent) {
    return;
  }
  if (!is_playing) {
    CompletePlayback();
    return;
  }
  state_ = State::kGenerationCompleted;
}

void PlaybackStatusReporter::OnInterrupted() {
  CompleteIfNeeded();
}

void PlaybackStatusReporter::OnPlaybackError() {
  CompleteIfNeeded();
}

void PlaybackStatusReporter::OnReportTimerFired() {
  if (state_ == State::kPlaybackCompletedSent) {
    report_timer_.Stop();
    return;
  }
  if (playback_progress_.is_zero() ||
      playback_progress_ == last_reported_progress_) {
    return;
  }
  SendReport(/*playback_complete=*/false);
}

void PlaybackStatusReporter::CompletePlayback() {
  CHECK_NE(state_, State::kPlaybackCompletedSent);
  state_ = State::kPlaybackCompletedSent;
  report_timer_.Stop();
  SendReport(/*playback_complete=*/true);
}

void PlaybackStatusReporter::SendReport(bool playback_complete) {
  last_reported_progress_ = playback_progress_;
  send_report_callback_.Run(playback_progress_, playback_complete,
                            generation_id_);
}

}  // namespace ttc
