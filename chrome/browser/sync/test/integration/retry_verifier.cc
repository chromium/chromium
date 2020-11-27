// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/retry_verifier.h"

#include <string.h>

#include <algorithm>

#include "base/logging.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/polling_constants.h"

namespace {

// Given the current delay calculate the minimum and maximum wait times for
// the next retry. This is analogous to the production logic in
// BackoffDelayProvider::GetDelay().
DelayInfo CalculateDelay(base::TimeDelta current_delay) {
  base::TimeDelta backoff =
      std::max(base::TimeDelta::FromSeconds(1),
               current_delay * syncer::kBackoffMultiplyFactor);

  DelayInfo delay_info;
  delay_info.min_delay = backoff - current_delay * syncer::kBackoffJitterFactor;
  delay_info.max_delay = backoff + current_delay * syncer::kBackoffJitterFactor;

  delay_info.min_delay =
      std::max(base::TimeDelta::FromSeconds(1),
               std::min(delay_info.min_delay, syncer::kMaxBackoffTime));

  delay_info.max_delay =
      std::max(base::TimeDelta::FromSeconds(1),
               std::min(delay_info.max_delay, syncer::kMaxBackoffTime));

  return delay_info;
}

// Fills the table with the maximum and minimum values for each retry, upto
// |count| number of retries.
void FillDelayTable(DelayInfo* delay_table, int count) {
  DCHECK_GT(count, 1);

  // Start off with the initial value used for tests, where SyncTest forces a
  // short retry time via command-line kSyncShortInitialRetryOverride.
  delay_table[0] = CalculateDelay(syncer::kInitialBackoffShortRetryTime);

  for (int i = 1 ; i < count ; ++i) {
    delay_table[i].min_delay = CalculateDelay(delay_table[i-1].min_delay).
                               min_delay;
    delay_table[i].max_delay = CalculateDelay(delay_table[i-1].max_delay).
                               max_delay;
  }
}
}  // namespace

// Verifies if the current retry is on time. Note that we dont use the
// maximum value of the retry range in verifying, only the minimum. Reason
// being there is no guarantee that the retry will be on the dot. However in
// practice it is on the dot. But making that assumption for all the platforms
// would make the test flaky.
bool IsRetryOnTime(DelayInfo* delay_table, int retry_count,
                   const base::TimeDelta& time_elapsed) {
  DVLOG(1) << "Retry Count : " << retry_count
           << " Time elapsed : " << time_elapsed
           << " Retry table min: " << delay_table[retry_count].min_delay
           << " Retry table max: " << delay_table[retry_count].max_delay;
  return time_elapsed >= delay_table[retry_count].min_delay;
}

RetryVerifier::RetryVerifier() : retry_count_(0),
                                 success_(false),
                                 done_(false) {
  memset(&delay_table_, 0, sizeof(delay_table_));
}

RetryVerifier::~RetryVerifier() {
}

// Initializes the state for verification.
void RetryVerifier::Initialize(const syncer::SyncCycleSnapshot& snap) {
  retry_count_ = 0;
  last_sync_time_ = snap.sync_start_time();
  FillDelayTable(delay_table_, kMaxRetry);
  done_ = false;
  success_ = false;
}

void RetryVerifier::VerifyRetryInterval(const syncer::SyncCycleSnapshot& snap) {
  DCHECK(retry_count_ < kMaxRetry);
  if (retry_count_ == 0) {
    if (snap.sync_start_time() != last_sync_time_) {
      retry_count_++;
      last_sync_time_ = snap.sync_start_time();
    }
    success_ = true;
    return;
  }

  // Check if the sync start time has changed. If so indicates a new sync
  // has taken place.
  if (snap.sync_start_time() != last_sync_time_) {
    base::TimeDelta delta = snap.sync_start_time() - last_sync_time_;
    success_ = IsRetryOnTime(delay_table_, retry_count_ - 1, delta);
    last_sync_time_ = snap.sync_start_time();
    ++retry_count_;
    done_ = (retry_count_ >= kMaxRetry);
    return;
  }
}
