// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PERFORMANCE_SYNC_TIMING_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PERFORMANCE_SYNC_TIMING_HELPER_H_

#include <vector>

namespace base {
class TimeDelta;
}

class SyncServiceImplHarness;

namespace sync_timing_helper {

// Returns the time taken for |client| to complete a single sync cycle.
base::TimeDelta TimeSyncCycle(SyncServiceImplHarness* client);

// Returns the time taken for both |client| and |partner| to complete a sync
// cycle.
base::TimeDelta TimeMutualSyncCycle(SyncServiceImplHarness* client,
                                    SyncServiceImplHarness* partner);

// Returns the time taken for all clients in |clients| to complete their
// respective sync cycles.
base::TimeDelta TimeUntilQuiescence(
    const std::vector<SyncServiceImplHarness*>& clients);

}  // namespace sync_timing_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PERFORMANCE_SYNC_TIMING_HELPER_H_
