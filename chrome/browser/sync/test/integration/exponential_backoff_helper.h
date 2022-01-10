// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace syncer {
class SyncCycleSnapshot;
}  // namespace syncer

namespace exponential_backoff_helper {

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

  RetryVerifier(const RetryVerifier&) = delete;
  RetryVerifier& operator=(const RetryVerifier&) = delete;

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
};

// Helper class that checks if a sync client has successfully gone through
// exponential backoff after it encounters an error.
class ExponentialBackoffChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ExponentialBackoffChecker(syncer::SyncServiceImpl* sync_service);

  ExponentialBackoffChecker(const ExponentialBackoffChecker&) = delete;
  ExponentialBackoffChecker& operator=(const ExponentialBackoffChecker&) =
      delete;

  // Checks if backoff is complete. Called repeatedly each time SyncServiceImpl
  // notifies observers of a state change.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  // Keeps track of the number of attempts at exponential backoff and its
  // related bookkeeping information for verification.
  RetryVerifier retry_verifier_;
};

}  // namespace exponential_backoff_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_
