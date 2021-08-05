// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd_lockout_strategy.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"

namespace policy {

// constexpr static
const base::TimeDelta CrdFixedTimeoutLockoutStrategy::kLockoutDuration;
const int CrdFixedTimeoutLockoutStrategy::kNumRejectedAttemptsAllowed;

void CrdFixedTimeoutLockoutStrategy::OnConnectionRejected() {
  num_rejected_connections_++;

  CRD_DVLOG(3) << "There are " << num_rejected_connections_
               << " consecutive rejected attempts";

  DCHECK(num_rejected_connections_ <= kNumRejectedAttemptsAllowed);

  if (num_rejected_connections_ == kNumRejectedAttemptsAllowed) {
    time_until_which_attempts_are_blocked_ =
        base::Time::Now() + kLockoutDuration;
    num_rejected_connections_ = 0;

    CRD_DVLOG(3) << "All further attempts are locked until "
                 << time_until_which_attempts_are_blocked_;
  }
}

void CrdFixedTimeoutLockoutStrategy::OnConnectionEstablished() {
  num_rejected_connections_ = 0;
}

bool CrdFixedTimeoutLockoutStrategy::CanAttemptConnection() const {
  return base::Time::Now() > time_until_which_attempts_are_blocked_;
}

}  // namespace policy
