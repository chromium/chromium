// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/ui_throughput_recorder.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

UiThroughputRecorder::UiThroughputRecorder() = default;
UiThroughputRecorder::~UiThroughputRecorder() = default;

void UiThroughputRecorder::OnUserLoggedIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // OnUserLoggedIn could be called multiple times from any states.
  // e.g.
  //   from kBeforeLogin: sign-in from the login screen and on cryptohome mount
  //   from kDuringLogin: during user profile loading after checking ownership
  //   from kInSession: adding a new user to the existing session.
  // Only set kDuringLogin on first OnUserLoggedIn call from kBeforeLogin so
  // that kDuringLogin starts from cryptohome mount.
  if (state_ == State::kBeforeLogin)
    state_ = State::kDuringLogin;
}

void UiThroughputRecorder::OnPostLoginAnimationFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This happens when adding a user to the existing session. Ignore it to
  // treat secondary user login as in session since multiple profile feature is
  // deprecating.
  if (state_ == State::kInSession)
    return;

  DCHECK_EQ(State::kDuringLogin, state_);
  state_ = State::kInSession;
}

void UiThroughputRecorder::ReportPercentDroppedFramesInOneSecoundWindow(
    double percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_PERCENTAGE("Ash.Smoothness.PercentDroppedFrames_1sWindow",
                           percentage);

  switch (state_) {
    case State::kBeforeLogin:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.Smoothness.PercentDroppedFrames_1sWindow.BeforeLogin",
          percentage);
      break;
    case State::kDuringLogin:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.Smoothness.PercentDroppedFrames_1sWindow.DuringLogin",
          percentage);
      break;
    case State::kInSession:
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.Smoothness.PercentDroppedFrames_1sWindow.InSession", percentage);
      break;
  }
}

}  // namespace ash
