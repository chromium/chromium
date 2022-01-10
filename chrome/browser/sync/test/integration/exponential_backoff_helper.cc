// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/exponential_backoff_helper.h"

#include <string.h>

#include <algorithm>
#include <ostream>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/polling_constants.h"

namespace exponential_backoff_helper {

namespace {

constexpr size_t kMaxRetriesToVerify = 3;

base::TimeDelta ClampBackoffDelay(base::TimeDelta delay) {
  return base::clamp(delay, syncer::kMinBackoffTime, syncer::kMaxBackoffTime);
}

}  // namespace

// static
ExponentialBackoffChecker::DelayRange
ExponentialBackoffChecker::CalculateDelayRange(base::TimeDelta current_delay) {
  // Given the current delay calculate the minimum and maximum wait times for
  // each retry. This is analogous to the production logic in
  // BackoffDelayProvider::GetDelay().
  const base::TimeDelta backoff = std::max(
      base::Seconds(1), current_delay * syncer::kBackoffMultiplyFactor);

  DelayRange delay_range;
  delay_range.min_delay =
      ClampBackoffDelay(backoff - current_delay * syncer::kBackoffJitterFactor);
  delay_range.max_delay =
      ClampBackoffDelay(backoff + current_delay * syncer::kBackoffJitterFactor);
  return delay_range;
}

// static
std::vector<ExponentialBackoffChecker::DelayRange>
ExponentialBackoffChecker::BuildExpectedDelayTable() {
  std::vector<DelayRange> delay_table;

  // Start off with the initial value used for tests, where SyncTest forces a
  // short retry time via command-line kSyncShortInitialRetryOverride.
  delay_table.push_back(
      CalculateDelayRange(syncer::kInitialBackoffShortRetryTime));

  for (size_t i = 1; i < kMaxRetriesToVerify; ++i) {
    DelayRange range;
    range.min_delay =
        CalculateDelayRange(delay_table.back().min_delay).min_delay;
    range.max_delay =
        CalculateDelayRange(delay_table.back().max_delay).max_delay;
    delay_table.push_back(range);
  }

  return delay_table;
}

ExponentialBackoffChecker::ExponentialBackoffChecker(
    syncer::SyncServiceImpl* sync_service)
    : SingleClientStatusChangeChecker(sync_service),
      expected_delay_table_(BuildExpectedDelayTable()) {
  const syncer::SyncCycleSnapshot& snap =
      service()->GetLastCycleSnapshotForDebugging();
  last_sync_time_ = snap.sync_start_time();
}

ExponentialBackoffChecker::~ExponentialBackoffChecker() = default;

bool ExponentialBackoffChecker::IsExitConditionSatisfied(std::ostream* os) {
  DCHECK(retry_count_ < kMaxRetriesToVerify);

  *os << "Verifying backoff intervals (" << retry_count_ << "/"
      << kMaxRetriesToVerify << ")";

  const syncer::SyncCycleSnapshot& snap =
      service()->GetLastCycleSnapshotForDebugging();

  if (retry_count_ == 0) {
    if (snap.sync_start_time() != last_sync_time_) {
      retry_count_++;
      last_sync_time_ = snap.sync_start_time();
    }
    success_ = true;
    return false;
  }

  // Check if the sync start time has changed. If so indicates a new sync
  // has taken place.
  if (snap.sync_start_time() != last_sync_time_) {
    const base::TimeDelta actual_time_elapsed =
        snap.sync_start_time() - last_sync_time_;
    *os << "Retry Count : " << (retry_count_ - 1)
        << " Time elapsed : " << actual_time_elapsed << " Retry table min: "
        << expected_delay_table_[retry_count_ - 1].min_delay
        << " Retry table max: "
        << expected_delay_table_[retry_count_ - 1].max_delay;

    // Verifies if the current retry is on time. Note that we dont use the
    // maximum value of the retry range in verifying, only the minimum. Reason
    // being there is no guarantee that the retry will be on the dot. However in
    // practice it is on the dot. But making that assumption for all the
    // platforms would make the test flaky.
    success_ = (actual_time_elapsed >=
                expected_delay_table_[retry_count_ - 1].min_delay);
    last_sync_time_ = snap.sync_start_time();
    ++retry_count_;
    done_ = (retry_count_ >= kMaxRetriesToVerify);
  }

  return done_ && success_;
}

}  // namespace exponential_backoff_helper
