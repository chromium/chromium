// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_
#define ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string_piece.h"
#include "base/timer/elapsed_timer.h"

namespace ash {

constexpr char kManagedScreensaverEnabledUMA[] = "Enabled";
constexpr char kManagedScreensaverEngagementTimeSlideshowUMA[] =
    "EngagementTime.Slideshow";
constexpr char kManagedScreensaverStartupTimeSlideshowUMA[] =
    "StartupTime.Slideshow";

ASH_EXPORT std::string GetManagedScreensaverHistogram(
    const base::StringPiece& histogram_suffix);

ASH_EXPORT void RecordManagedScreensaverEnabled(bool enabled);

class ManagedScreensaverMetricsRecorder {
 public:
  ManagedScreensaverMetricsRecorder();
  ~ManagedScreensaverMetricsRecorder();
  ManagedScreensaverMetricsRecorder(const ManagedScreensaverMetricsRecorder&) =
      delete;
  ManagedScreensaverMetricsRecorder& operator=(
      const ManagedScreensaverMetricsRecorder&) = delete;

  // Starts the session elapsed timer. This is used to keep track of the start
  // of a session.
  void RecordSessionStart();

  // Records the amount of time it takes for the managed screensaver to start.
  void RecordSessionStartupTime();

  // Records the engagement time UMA.
  void RecordSessionEnd();

 private:
  // Timer use to keep track of ambient mode managed screensaver sessions.
  std::unique_ptr<base::ElapsedTimer> session_elapsed_timer_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_
