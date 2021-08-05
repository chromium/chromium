// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_LOCKOUT_STRATEGY_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_LOCKOUT_STRATEGY_H_

#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_connection_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// Helper class that decides when the admin is locked out from attempting to
// start a remote CRD session.
// This prevents the admin from spamming the host user.
class CrdLockoutStrategy : public CrdConnectionObserver {
 public:
  CrdLockoutStrategy() = default;
  CrdLockoutStrategy(const CrdLockoutStrategy&) = delete;
  CrdLockoutStrategy& operator=(const CrdLockoutStrategy&) = delete;
  ~CrdLockoutStrategy() override = default;

  // Returns true if the admin can attempt a CRD command, or false if a timeout
  // is in effect.
  virtual bool CanAttemptConnection() const = 0;
};

// Simple lockout strategy that blocks attempts for a fixed time after a
// given number of rejected attempts.
class CrdFixedTimeoutLockoutStrategy : public CrdLockoutStrategy {
 public:
  constexpr static base::TimeDelta kLockoutDuration =
      base::TimeDelta::FromMinutes(5);
  constexpr static int kNumRejectedAttemptsAllowed = 3;

  CrdFixedTimeoutLockoutStrategy() = default;
  CrdFixedTimeoutLockoutStrategy(const CrdFixedTimeoutLockoutStrategy&) =
      delete;
  CrdFixedTimeoutLockoutStrategy& operator=(
      const CrdFixedTimeoutLockoutStrategy&) = delete;
  ~CrdFixedTimeoutLockoutStrategy() override = default;

  // CrdLockoutStrategy implementation:
  void OnConnectionRejected() override;
  void OnConnectionEstablished() override;
  bool CanAttemptConnection() const override;

 private:
  int num_rejected_connections_ = 0;

  base::Time time_until_which_attempts_are_blocked_ = base::Time::Min();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_LOCKOUT_STRATEGY_H_
