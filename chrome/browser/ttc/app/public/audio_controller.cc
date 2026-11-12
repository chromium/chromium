// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/public/audio_controller.h"

#include <cmath>

#include "base/check_op.h"

namespace ttc {

// static
base::TimeDelta AudioController::FramesToCeilingTimeDelta(int64_t frames,
                                                          int sample_rate) {
  CHECK_GE(frames, 0);
  CHECK_GT(sample_rate, 0);
  return base::Microseconds(static_cast<int64_t>(
      std::ceil(static_cast<double>(frames) *
                base::Time::kMicrosecondsPerSecond / sample_rate)));
}

}  // namespace ttc
