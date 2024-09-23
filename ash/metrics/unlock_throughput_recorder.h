// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_UNLOCK_THROUGHPUT_RECORDER_H_
#define ASH_METRICS_UNLOCK_THROUGHPUT_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"

namespace ash {

class ASH_EXPORT UnlockThroughputRecorder : public SessionObserver {
 public:
  UnlockThroughputRecorder();
  UnlockThroughputRecorder(const UnlockThroughputRecorder&) = delete;
  UnlockThroughputRecorder& operator=(const UnlockThroughputRecorder&) = delete;
  ~UnlockThroughputRecorder() override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
};

}  // namespace ash

#endif  // ASH_METRICS_UNLOCK_THROUGHPUT_RECORDER_H_
