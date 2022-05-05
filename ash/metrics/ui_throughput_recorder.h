// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_UI_THROUGHPUT_RECORDER_H_
#define ASH_METRICS_UI_THROUGHPUT_RECORDER_H_

#include "ash/ash_export.h"
#include "base/sequence_checker.h"
#include "cc/metrics/custom_metrics_recorder.h"

namespace ash {

// Records throughput metrics for ash UI. Note this class is not thread-safe.
class ASH_EXPORT UiThroughputRecorder : public cc::CustomMetricRecorder {
 public:
  UiThroughputRecorder();
  ~UiThroughputRecorder() override;

  // Invoked on a user login. This is expected to be called after cryptohome
  // mount but before user profile loading.
  void OnUserLoggedIn();

  // Invoked after post-login animation finishes.
  void OnPostLoginAnimationFinish();

  // cc::CustomMetricRecorder:
  void ReportPercentDroppedFramesInOneSecoundWindow(double percentage) override;

 private:
  // State to split "Ash.Smoothness.PercentDroppedFrames_1sWindow".
  enum class State {
    kBeforeLogin,
    kDuringLogin,
    kInSession,
  };

  State state_ = State::kBeforeLogin;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // ASH_METRICS_UI_THROUGHPUT_RECORDER_H_
