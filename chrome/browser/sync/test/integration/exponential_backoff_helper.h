// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace exponential_backoff_helper {

// Helper class that checks if a sync client has successfully gone through
// exponential backoff after it encounters an error.
class ExponentialBackoffChecker : public SingleClientStatusChangeChecker {
 public:
  // Upon construction, it is expected that backoff hasn't started yet.
  explicit ExponentialBackoffChecker(syncer::SyncServiceImpl* sync_service);
  ~ExponentialBackoffChecker() override;

  ExponentialBackoffChecker(const ExponentialBackoffChecker&) = delete;
  ExponentialBackoffChecker& operator=(const ExponentialBackoffChecker&) =
      delete;

  // SingleClientStatusChangeChecker overrides.
  void OnSyncCycleCompleted(syncer::SyncService* sync_service) override;
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  // The minimum and maximum wait times for a retry. The actual retry would take
  // place somewhere in this range. The algorithm that calculates the retry wait
  // time uses rand functions.
  struct DelayRange {
    base::TimeDelta min_delay;
    base::TimeDelta max_delay;
  };

  // Helper functions to build the delay table.
  static DelayRange CalculateDelayRange(base::TimeDelta current_delay);
  static std::vector<DelayRange> BuildExpectedDelayTable();

  const std::vector<DelayRange> expected_delay_table_;
  std::vector<base::TimeDelta> actual_delays_;

  base::Time last_sync_time_;
};

}  // namespace exponential_backoff_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_
