// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_
#define ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_

#include <string>
#include <string_view>

#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"
#include "ash/ash_export.h"

#include "base/timer/elapsed_timer.h"

namespace ash {

enum class ScreensaverImageDownloadResult;

constexpr char kManagedScreensaverEnabledUMA[] = "Enabled";
constexpr char kManagedScreensaverEngagementTimeSlideshowUMA[] =
    "EngagementTime.Slideshow";
constexpr char kManagedScreensaverStartupTimeSlideshowUMA[] =
    "StartupTime.Slideshow";
constexpr char kManagedScreensaverImageCountUMA[] = "ImageCount";
constexpr char kManagedScreensaverImageDownloadResultUMA[] =
    "ImageDownloadResult";

ASH_EXPORT std::string GetManagedScreensaverHistogram(
    std::string_view histogram_suffix);

ASH_EXPORT void RecordManagedScreensaverEnabled(bool enabled);

ASH_EXPORT void RecordManagedScreensaverImageCount(int image_count);

ASH_EXPORT void RecordManagedScreensaverImageDownloadResult(
    ScreensaverImageDownloadResult result);

class ManagedScreensaverMetricsDelegate
    : public AmbientSessionMetricsRecorder::Delegate {
 public:
  ManagedScreensaverMetricsDelegate();
  ~ManagedScreensaverMetricsDelegate() override;
  ManagedScreensaverMetricsDelegate(const ManagedScreensaverMetricsDelegate&) =
      delete;
  ManagedScreensaverMetricsDelegate& operator=(
      const ManagedScreensaverMetricsDelegate&) = delete;

  void RecordStartupTime(base::TimeDelta startup_time) override;
  void RecordEngagementTime(base::TimeDelta engagement_time) override;
};

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_
