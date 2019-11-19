// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_GRAPHICS_JANK_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_GRAPHICS_JANK_DETECTOR_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace arc {

// Detector is used to determine janks in the sequence of periodic samples that
// have a timestamp. It conceptually has the following phases:
//   * kWarmUp - at this stage, samples are ignored, assuming the app is just
//               activated or resumed.
//   * kRateDetection - at this stage, samples are collected to determine the
//                      prevailing or normal rate. Once enough samples are
//                      collected, rate is determined as 33% cut-off of sorted
//                      sample durations.
//   * kActive - at this stage each new sample is tested against detected normal
//               rate and in case timestamp offset of the sample is longer than
//               190% of the determined period than this is considered as a
//               jank.
// In case the next sample appears after a long interval this is not considered
// as a jank and detector switches to warm-up stage.
// Detector may work in fixed mode, activated by |SetPeriodFixed| call. In this
// mode detector skips kWarmUp and kRateDetection and always watches for janks.
class ArcGraphicsJankDetector {
 public:
  using JankCallback =
      base::RepeatingCallback<void(const base::Time& timestamp)>;

  enum class Stage {
    kWarmUp,         // ignore any update
    kRateDetection,  // detect the update rate
    kActive,         // looking for janks
  };

  // Number of samples to skip during the warm up.
  static constexpr int kWarmUpSamples = 10;

  // Number of samples to collect for the rate detection.
  static constexpr size_t kSamplesForRateDetection = 15;

  // Constant to detect pause to reduce the chance of triggering jank detection
  // in case Android app just not producing the samples due to inactivity.
  static constexpr base::TimeDelta kPauseDetectionThreshold =
      base::TimeDelta::FromSecondsD(0.25);

  // Threshold relative to the normal rate to consider the current frame as a
  // jank if its duration longer than this threshold.
  static constexpr int kJankDetectionThresholdPercent = 190;

  explicit ArcGraphicsJankDetector(const JankCallback& callback);
  ~ArcGraphicsJankDetector();

  // Resets detector to its initial state, stage is set to |Stage::kWarmUp| with
  // the initial number of warm up samples. Fixed period is discarded if it was
  // set.
  void Reset();

  // Sets the expected refresh rate. This disables |Stage::kWarmUp| and
  // |Stage::kRateDetection| stages and keeps detector in |Stage::kActive|
  // stage.
  void SetPeriodFixed(const base::TimeDelta& period);

  // Notifies about the next sample. Used in runtime and timestamp is considered
  // as now time.
  void OnSample();
  // Notifies about the next sample with corresponding timestamp. Used in tests
  // off-line processing.
  void OnSample(const base::Time& timestamp);

  Stage stage() const { return stage_; }
  base::TimeDelta period() const { return period_; }

 private:
  // Once jank is detected |callback_| is fired.
  JankCallback callback_;

  Stage stage_;
  base::Time last_sample_time_;
  int warm_up_sample_cnt_;
  std::vector<base::TimeDelta> intervals_;
  // Considered as normal period.
  base::TimeDelta period_;
  // Period fixed.
  bool period_fixed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcGraphicsJankDetector);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_GRAPHICS_JANK_DETECTOR_H_
