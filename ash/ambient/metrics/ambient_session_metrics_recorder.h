// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_
#define ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

// AmbientSessionMetricsRecorder's lifetime is meant to match that of a
// single ambient session:
// * Construction     - Ambient session starts by preparing any assets needed
//                      for rendering.
// * RegisterScreen() - Ambient session is rendering. There is one call for each
//                      screen (display).
// * Destruction      - Ambient session ends.
//
// Metrics recorded apply to all `AmbientUiSettings`.
class ASH_EXPORT AmbientSessionMetricsRecorder {
 public:
  // A custom `tick_clock` may be provided for testing purposes.
  explicit AmbientSessionMetricsRecorder(
      AmbientUiSettings ui_settings,
      const base::TickClock* tick_clock = nullptr);
  AmbientSessionMetricsRecorder(const AmbientSessionMetricsRecorder&) = delete;
  AmbientSessionMetricsRecorder& operator=(
      const AmbientSessionMetricsRecorder&) = delete;
  ~AmbientSessionMetricsRecorder();

  // Should be called once per each screen rendering the UI during an ambient
  // session.
  void RegisterScreen();

 private:
  const AmbientUiSettings ui_settings_;
  const raw_ptr<const base::TickClock> clock_;
  const base::TimeTicks session_start_time_;
  int num_registered_screens_ = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_SESSION_METRICS_RECORDER_H_
