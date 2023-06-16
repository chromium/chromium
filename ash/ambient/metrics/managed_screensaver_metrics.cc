// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/managed_screensaver_metrics.h"

#include "ash/ambient/managed/screensaver_image_downloader.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"

namespace ash {

namespace {
// Histograms use exponential bucketing, so shorter times will have more
// buckets. This number was chosen to be consistent with other ambient mode
// metrics.
// TODO(b/287231044) Move this along with other constants UMA to a shared
// header.
constexpr int kManagedScreensaverEngagemenTimeHistogramBuckets = 144;
constexpr int kManagedScreensaverStartupTimeHistogramBuckets = 144;
constexpr base::StringPiece kManagedScreensaverHistogramPrefix =
    "Enterprise.ManagedScreensaver.";

// This limit is specified in the policy definition for the policies
// ScreensaverLockScreenImages and DeviceScreensaverLoginScreenImages.
constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

}  // namespace

std::string GetManagedScreensaverHistogram(
    const base::StringPiece& histogram_suffix) {
  return base::StrCat({kManagedScreensaverHistogramPrefix, histogram_suffix});
}

void RecordManagedScreensaverEnabled(bool enabled) {
  base::UmaHistogramBoolean(
      GetManagedScreensaverHistogram(kManagedScreensaverEnabledUMA), enabled);
}

ASH_EXPORT void RecordManagedScreensaverImageCount(int image_count) {
  base::UmaHistogramExactLinear(
      GetManagedScreensaverHistogram(kManagedScreensaverImageCountUMA),
      image_count, kMaxUrlsToProcessFromPolicy + 1);
}

ASH_EXPORT void RecordManagedScreensaverImageDownloadResult(
    ScreensaverImageDownloadResult result) {
  base::UmaHistogramEnumeration(
      GetManagedScreensaverHistogram(kManagedScreensaverImageDownloadResultUMA),
      result);
}

ManagedScreensaverMetricsRecorder::ManagedScreensaverMetricsRecorder() =
    default;
ManagedScreensaverMetricsRecorder::~ManagedScreensaverMetricsRecorder() =
    default;

void ManagedScreensaverMetricsRecorder::RecordSessionStart() {
  session_elapsed_timer_ = std::make_unique<base::ElapsedTimer>();
}

void ManagedScreensaverMetricsRecorder::RecordSessionEnd() {
  // The screensaver can transition to stopped/hidden state without ever being
  // started when chrome starts up. That is why we add an early return here to
  // make sure that we only record valid sessions.
  if (!session_elapsed_timer_) {
    return;
  }

  base::UmaHistogramCustomTimes(
      /*name=*/GetManagedScreensaverHistogram(
          kManagedScreensaverEngagementTimeSlideshowUMA),
      /*sample=*/session_elapsed_timer_->Elapsed(),
      /*min=*/base::Seconds(1),
      /*max=*/base::Hours(24),
      /*buckets=*/kManagedScreensaverEngagemenTimeHistogramBuckets);

  session_elapsed_timer_.reset();
}

void ManagedScreensaverMetricsRecorder::RecordSessionStartupTime() {
  if (!session_elapsed_timer_) {
    return;
  }

  base::UmaHistogramCustomTimes(
      /*name=*/GetManagedScreensaverHistogram(
          kManagedScreensaverStartupTimeSlideshowUMA),
      /*sample=*/session_elapsed_timer_->Elapsed(),
      /*min=*/base::Seconds(0),
      /*max=*/base::Seconds(1000),
      /*buckets=*/kManagedScreensaverStartupTimeHistogramBuckets);
}

}  // namespace ash
