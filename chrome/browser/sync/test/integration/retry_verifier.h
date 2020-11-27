// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_RETRY_VERIFIER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_RETRY_VERIFIER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"

namespace syncer {
class SyncCycleSnapshot;
}  // namespace syncer

// The minimum and maximum wait times for a retry. The actual retry would take
// place somewhere in this range. The algorithm that calculates the retry wait
// time uses rand functions.
struct DelayInfo {
  base::TimeDelta min_delay;
  base::TimeDelta max_delay;
};

// Class to verify retries take place using the exponential backoff algorithm.
class RetryVerifier {
 public:
  static const int kMaxRetry = 3;
  RetryVerifier();
  ~RetryVerifier();
  int retry_count() const { return retry_count_; }

  // Initialize with the current sync session snapshot. Using the snapshot
  // we will figure out when the first retry sync happened.
  void Initialize(const syncer::SyncCycleSnapshot& snap);
  void VerifyRetryInterval(const syncer::SyncCycleSnapshot& snap);
  bool done() const { return done_; }
  bool Succeeded() const { return done() && success_; }

 private:
  int retry_count_;
  base::Time last_sync_time_;
  DelayInfo delay_table_[kMaxRetry];
  bool success_;
  bool done_;
  DISALLOW_COPY_AND_ASSIGN(RetryVerifier);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_RETRY_VERIFIER_H_
