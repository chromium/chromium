// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_INACTIVITY_DETECTOR_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_INACTIVITY_DETECTOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ttc/core/session_controller.h"

namespace ttc {

// Observes a SessionController and invokes a callback if no user activity
// occurs within a specified timeout duration. User activity includes input
// transcriptions, audio energy exceeding the threshold (debounced), and tool
// completion (when not waiting on a model turn). Countdown is suspended during
// active model turns.
class SessionInactivityDetector : public SessionController::Observer {
 public:
  SessionInactivityDetector(SessionController& controller,
                            base::RepeatingClosure timeout_callback,
                            base::TimeDelta timeout);
  SessionInactivityDetector(SessionController& controller,
                            base::RepeatingClosure timeout_callback);
  SessionInactivityDetector(const SessionInactivityDetector&) = delete;
  SessionInactivityDetector& operator=(const SessionInactivityDetector&) =
      delete;
  ~SessionInactivityDetector() override;

  // SessionController::Observer:
  void OnSessionLifecycleChanged(SessionLifecycle lifecycle) override;
  void OnModelTurnStarted() override;
  void OnModelTurnComplete() override;
  void OnModelInterrupted() override;
  void OnInputTranscription(const std::string& text) override;
  void OnUserAudioLevelUpdate(float audio_level) override;
  void OnToolExecutionCompleted(const std::string& tool_name,
                                bool success) override;

  void ResetTimer();

 private:
  void OnTimeout();

  base::ScopedObservation<SessionController, SessionController::Observer>
      scoped_observation_{this};
  base::RepeatingClosure timeout_callback_;
  base::TimeDelta timeout_;
  base::OneShotTimer inactivity_timer_;
  base::TimeTicks last_audio_reset_time_;
  bool is_model_turn_in_progress_ = false;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_INACTIVITY_DETECTOR_H_
