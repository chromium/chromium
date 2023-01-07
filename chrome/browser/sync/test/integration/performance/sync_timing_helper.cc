// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_timing_helper {

base::TimeDelta TimeSyncCycle(SyncServiceImplHarness* client) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(UpdatedProgressMarkerChecker(client->service()).Wait());
  return base::Time::Now() - start;
}

base::TimeDelta TimeMutualSyncCycle(SyncServiceImplHarness* client,
                                    SyncServiceImplHarness* partner) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(client->AwaitMutualSyncCycleCompletion(partner));
  return base::Time::Now() - start;
}

base::TimeDelta TimeUntilQuiescence(
    const std::vector<SyncServiceImplHarness*>& clients) {
  base::Time start = base::Time::Now();
  EXPECT_TRUE(SyncServiceImplHarness::AwaitQuiescence(clients));
  return base::Time::Now() - start;
}

}  // namespace sync_timing_helper
