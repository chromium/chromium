// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
#define ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/metrics/ui_throughput_recorder.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "chromeos/login/login_state/login_state.h"

namespace ash {

class ASH_EXPORT LoginUnlockThroughputRecorder
    : public SessionObserver,
      public chromeos::LoginState::Observer {
 public:
  LoginUnlockThroughputRecorder();
  LoginUnlockThroughputRecorder(const LoginUnlockThroughputRecorder&) = delete;
  LoginUnlockThroughputRecorder& operator=(
      const LoginUnlockThroughputRecorder&) = delete;
  ~LoginUnlockThroughputRecorder() override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // chromeos::LoginState::Observer:
  void LoggedInStateChanged() override;

 private:
  void OnLoginAnimationFinish(
      base::TimeTicks start,
      const cc::FrameSequenceMetrics::CustomReportData& data);

  UiThroughputRecorder ui_recorder_;
  base::WeakPtrFactory<LoginUnlockThroughputRecorder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
