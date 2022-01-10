// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_

#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace exponential_backoff_helper {

// The minimum and maximum wait times for a retry. The actual retry would take
// place somewhere in this range. The algorithm that calculates the retry wait
// time uses rand functions.
struct DelayInfo {
  base::TimeDelta min_delay;
  base::TimeDelta max_delay;
};

// Helper class that checks if a sync client has successfully gone through
// exponential backoff after it encounters an error.
class ExponentialBackoffChecker : public SingleClientStatusChangeChecker {
 public:
  static constexpr int kMaxRetriesToVerify = 3;

  explicit ExponentialBackoffChecker(syncer::SyncServiceImpl* sync_service);
  ~ExponentialBackoffChecker() override;

  ExponentialBackoffChecker(const ExponentialBackoffChecker&) = delete;
  ExponentialBackoffChecker& operator=(const ExponentialBackoffChecker&) =
      delete;

  // Checks if backoff is complete. Called repeatedly each time SyncServiceImpl
  // notifies observers of a state change.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  int retry_count_ = 0;
  base::Time last_sync_time_;
  DelayInfo delay_table_[kMaxRetriesToVerify];
  bool success_ = false;
  bool done_ = false;
};

}  // namespace exponential_backoff_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXPONENTIAL_BACKOFF_HELPER_H_
