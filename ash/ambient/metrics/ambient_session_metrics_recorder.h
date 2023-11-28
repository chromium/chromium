// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_
#define ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_

#include <memory>
#include <optional>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

// AmbientSessionMetricsRecorder's lifetime is meant to match that of a
// single ambient session:
// * Construction     - Ambient session starts by preparing any assets needed
//                      for rendering.
// * SetInitStatus    - Ambient session is initialized either successfully or
//                      unsuccessfully. If successful, it can start rendering
//                      and RegisterScreen() calls can be made.
// * RegisterScreen() - Ambient session is rendering. There is one call for each
//                      screen (display).
// * Destruction      - Ambient session ends.
//
// Metrics recorded apply to all `AmbientUiSettings`.
class ASH_EXPORT AmbientSessionMetricsRecorder {
 public:
  // `AmbientSessionMetricsRecorder` dictates when to record a metric and what
  // value to record, whereas the `Delegate` does the actual recording.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Session has started.
    virtual void RecordActivation() {}
    // Session has finished initialization and will start rendering if
    // successful.
    virtual void RecordInitStatus(bool success) {}
    // Amount of time initialization took.
    virtual void RecordStartupTime(base::TimeDelta startup_time) {}
    // Total duration of the session (initialization + rendering) before it
    // was closed.
    virtual void RecordEngagementTime(base::TimeDelta engagement_time) {}
    // Total number of screens that were rendering the UI during this session.
    // Note this may be 0 if the session never started rendering (initialization
    // was pending or failed when the session closed).
    virtual void RecordScreenCount(int num_screens) {}
  };

  explicit AmbientSessionMetricsRecorder(std::unique_ptr<Delegate> delegate);
  AmbientSessionMetricsRecorder(const AmbientSessionMetricsRecorder&) = delete;
  AmbientSessionMetricsRecorder& operator=(
      const AmbientSessionMetricsRecorder&) = delete;
  ~AmbientSessionMetricsRecorder();

  // `init_status` should be the result of `AmbientUiLauncher::Initialize()`.
  // Must only be called once in `AmbientSessionMetricsRecorder`'s lifetime.
  void SetInitStatus(bool init_status);

  // Should be called once per each screen rendering the UI during an ambient
  // session.
  void RegisterScreen();

 private:
  const std::unique_ptr<Delegate> delegate_;
  const base::TimeTicks session_start_time_;
  int num_registered_screens_ = 0;
  std::optional<bool> session_init_status_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_
