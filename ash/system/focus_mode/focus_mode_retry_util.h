// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_RETRY_UTIL_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_RETRY_UTIL_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

// Max number of retries.
inline constexpr int kMaxRetryOverall = 5;

// Max number of retries for too many requests error.
inline constexpr int kMaxRetryTooManyRequests = 3;

// Max number of retries for explicit track.
inline constexpr int kMaxRetryExplicitTrack = 10;

// Initial wait time of the exponential backoff policy for overall errors.
inline constexpr base::TimeDelta kWaitTimeOverall = base::Seconds(1);

// Wait time when the client runs out of quota.
inline constexpr base::TimeDelta kWaitTimeTooManyRequests = base::Seconds(30);

// Wait time when the track is explicit.
inline constexpr base::TimeDelta kWaitTimeExplicitTrack = base::Seconds(1);

// Struct that keeps track of focus mode request retry state.
struct ASH_EXPORT FocusModeRetryState {
  FocusModeRetryState();
  ~FocusModeRetryState();

  void Reset();

  int retry_index = 0;
  base::OneShotTimer timer;
};

// Returns if it should retry the given http error.
ASH_EXPORT bool ShouldRetryHttpError(
    const google_apis::ApiErrorCode http_error);

// Returns wait time for the exponential backoff retry. For each new attempt,
// the wait time doubles, e.g. 1s, 2s, 4s, 8s, 16s.
ASH_EXPORT base::TimeDelta GetExponentialBackoffRetryWaitTime(
    const int retry_index);

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_RETRY_UTIL_H_
