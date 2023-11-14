// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/exponential_backoff_helper.h"

#include <algorithm>
#include <ostream>

#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/polling_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exponential_backoff_helper {

namespace {

constexpr size_t kMaxRetriesToVerify = 3;
constexpr base::TimeDelta kMinExtraUnexpectedDelayForWarning = base::Seconds(1);

bool DidLastSyncCycleFail(syncer::SyncService* sync_service) {
  // Note that SyncCycleSnapshot::is_silenced() is avoided here because,
  // unfortunately, it's one cycle "behind". is_silenced() continues to be
  // be false upon completion of the first cycle leading to backoff, because the
  // sync cycle snapshot is taken *before* the |is_silenced| bit is set to true
  // by SyncSchedulerImpl::HandleFailure().
  return syncer::HasSyncerError(
      sync_service->GetLastCycleSnapshotForDebugging().model_neutral_state());
}

base::TimeDelta ClampBackoffDelay(base::TimeDelta delay) {
  return std::clamp(delay, syncer::kMinBackoffTime, syncer::kMaxBackoffTime);
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

  return {.min_delay = ClampBackoffDelay(
              backoff - current_delay * syncer::kBackoffJitterFactor),
          .max_delay = ClampBackoffDelay(
              backoff + current_delay * syncer::kBackoffJitterFactor)};
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
  // Upon construction, backoff must not have started, since it's otherwise
  // impossible to determine the precise timestamp corresponding to the first
  // backed-off sync cycle, required to predict the exponential behavior.
  if (DidLastSyncCycleFail(sync_service)) {
    ADD_FAILURE() << "Last sync cycle already failed upon construction of "
                  << "ExponentialBackoffChecker.";
  }
}

ExponentialBackoffChecker::~ExponentialBackoffChecker() = default;

void ExponentialBackoffChecker::OnSyncCycleCompleted(
    syncer::SyncService* sync_service) {
  const syncer::SyncCycleSnapshot& snap =
      sync_service->GetLastCycleSnapshotForDebugging();

  if (!DidLastSyncCycleFail(sync_service)) {
    return;
  }

  // The very first backed-off cycle has itself no delay to verify, but only
  // acts as reference point.
  if (!last_sync_time_.is_null()) {
    // Note that this measures the delay between the *start* time of two cycles,
    // instead of measuring the time between one cycle ending and the next one
    // starting. However, the difference is negligible when using a fake server,
    // because sync cycles are very fast, and either way this checker cannot be
    // strict about upper bounds (there may be extra delays for various reasons
    // under high CPU load).
    actual_delays_.push_back(snap.sync_start_time() - last_sync_time_);
  }

  last_sync_time_ = snap.sync_start_time();
  CheckExitCondition();
}

bool ExponentialBackoffChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Verifying backoff intervals " << actual_delays_.size() << " out of "
      << kMaxRetriesToVerify << "\n";

  for (size_t i = 0; i < std::min(actual_delays_.size(), kMaxRetriesToVerify);
       ++i) {
    *os << "Delay for retry " << (i + 1) << "/" << kMaxRetriesToVerify
        << " expected between " << expected_delay_table_[i].min_delay
        << " and (approximately) " << expected_delay_table_[i].max_delay
        << "; actual = " << actual_delays_[i] << "\n";
    if (actual_delays_[i] < expected_delay_table_[i].min_delay) {
      *os << "ERROR: Delay " << i << " is too short\n";
      return false;
    }
    if (actual_delays_[i] > expected_delay_table_[i].max_delay +
                                kMinExtraUnexpectedDelayForWarning) {
      // Although the delay is usually before the max delay, there is nothing
      // providing strong guarantees about when precisely the sync thread is
      // able to issue a request to the server. Hence, to avoid test flakiness,
      // this is treated as a warning only.
      *os << "WARNING: delay " << i
          << " is longer than expected, but this may be due to high CPU load\n";
    }
  }

  return actual_delays_.size() >= kMaxRetriesToVerify;
}

}  // namespace exponential_backoff_helper
