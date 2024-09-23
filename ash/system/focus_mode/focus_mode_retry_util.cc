// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_retry_util.h"

#include "base/check_op.h"

namespace ash {

FocusModeRetryState::FocusModeRetryState() = default;
FocusModeRetryState::~FocusModeRetryState() = default;

void FocusModeRetryState::Reset() {
  retry_index = 0;
  timer.Stop();
}

bool ShouldRetryHttpError(const google_apis::ApiErrorCode http_error_code) {
  return http_error_code == 408 || http_error_code == 500 ||
         http_error_code == 502 || http_error_code == 503 ||
         http_error_code == 504;
}

base::TimeDelta GetExponentialBackoffRetryWaitTime(const int retry_index) {
  CHECK_GT(retry_index, 0);
  return kWaitTimeOverall * (1 << (retry_index - 1));
}

}  // namespace ash
