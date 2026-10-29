// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_inactivity_detector.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ttc/core/features.h"

namespace ttc {

SessionInactivityDetector::SessionInactivityDetector(
    SessionController& controller,
    base::RepeatingClosure timeout_callback,
    base::TimeDelta timeout)
    : timeout_callback_(std::move(timeout_callback)), timeout_(timeout) {
  scoped_observation_.Observe(&controller);
}

SessionInactivityDetector::SessionInactivityDetector(
    SessionController& controller,
    base::RepeatingClosure timeout_callback)
    : SessionInactivityDetector(controller,
                                std::move(timeout_callback),
                                kSessionInactivityTimeout.Get()) {}

SessionInactivityDetector::~SessionInactivityDetector() = default;

void SessionInactivityDetector::OnSessionLifecycleChanged(
    SessionLifecycle lifecycle) {
  if (lifecycle == SessionLifecycle::kFinished) {
    inactivity_timer_.Stop();
    scoped_observation_.Reset();
  } else {
    ResetTimer();
  }
}

void SessionInactivityDetector::OnModelTurnStarted() {
  is_model_turn_in_progress_ = true;
  inactivity_timer_.Stop();
}

void SessionInactivityDetector::OnModelTurnComplete() {
  is_model_turn_in_progress_ = false;
  ResetTimer();
}

void SessionInactivityDetector::OnModelInterrupted() {
  is_model_turn_in_progress_ = false;
  ResetTimer();
}

void SessionInactivityDetector::OnInputTranscription(const std::string& text) {
  if (!is_model_turn_in_progress_) {
    ResetTimer();
  }
}

void SessionInactivityDetector::OnUserAudioLevelUpdate(float audio_level) {
  if (is_model_turn_in_progress_) {
    return;
  }
  if (audio_level >= kSessionInactivityAudioEnergyThreshold.Get()) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_audio_reset_time_ >= base::Seconds(1)) {
      last_audio_reset_time_ = now;
      ResetTimer();
    }
  }
}

void SessionInactivityDetector::OnToolExecutionCompleted(
    const std::string& tool_name,
    bool success) {
  if (!is_model_turn_in_progress_) {
    ResetTimer();
  }
}

void SessionInactivityDetector::ResetTimer() {
  if (is_model_turn_in_progress_ || !scoped_observation_.IsObserving()) {
    return;
  }
  inactivity_timer_.Start(FROM_HERE, timeout_,
                          base::BindOnce(&SessionInactivityDetector::OnTimeout,
                                         base::Unretained(this)));
}

void SessionInactivityDetector::OnTimeout() {
  if (timeout_callback_) {
    timeout_callback_.Run();
  }
}

}  // namespace ttc
