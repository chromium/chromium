// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_tracker.h"

#include <optional>

#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::sequence_manager::internal {

// Verify that no sync work authorization is granted unless allowed by
// `SetRunTaskSynchronouslyAllowed()`.
TEST(SequenceManagerWorkTrackerTest, SetRunTaskSynchronouslyAllowed) {
  WorkTracker tracker;
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());

  tracker.OnBeginWork();
  tracker.OnIdle();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());

  tracker.WillRequestReloadImmediateWorkQueue();
  tracker.WillReloadImmediateWorkQueues();
  tracker.OnIdle();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());

  tracker.SetRunTaskSynchronouslyAllowed(true);
  EXPECT_TRUE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
  tracker.SetRunTaskSynchronouslyAllowed(false);
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
}

// Verify that `SetRunTaskSynchronouslyAllowed(false)` blocks until there is no
// valid sync work authorization.
TEST(SequenceManagerWorkTrackerTest, SetRunTaskSynchronouslyAllowedBlocks) {
  WorkTracker tracker;
  tracker.SetRunTaskSynchronouslyAllowed(true);

  WaitableEvent did_acquire_sync_work_auth;
  bool will_release_sync_work_auth = false;
  Thread other_thread("OtherThread");
  other_thread.Start();
  other_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        std::optional<SyncWorkAuthorization> auth =
            tracker.TryAcquireSyncWorkAuthorization();
        EXPECT_TRUE(auth->IsValid());
        did_acquire_sync_work_auth.Signal();
        PlatformThread::Sleep(TestTimeouts::tiny_timeout());
        will_release_sync_work_auth = true;
        auth.reset();
      }));

  did_acquire_sync_work_auth.Wait();

  tracker.SetRunTaskSynchronouslyAllowed(false);
  // `will_release_sync_work_auth` must be true (with no data race detected by
  // TSAN) when the call above returns.
  EXPECT_TRUE(will_release_sync_work_auth);

  other_thread.FlushForTesting();
}

// Verify that after `WillRequestReloadImmediateWorkQueue()`,
// `WillReloadImmediateWorkQueues()` and `OnIdle()` must be called in sequence
// for a sync work authorization to be granted.
TEST(SequenceManagerWorkTrackerTest, WillRequestReloadImmediateWorkQueue) {
  WorkTracker tracker;
  tracker.SetRunTaskSynchronouslyAllowed(true);
  EXPECT_TRUE(tracker.TryAcquireSyncWorkAuthorization().IsValid());

  tracker.WillRequestReloadImmediateWorkQueue();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
  tracker.WillReloadImmediateWorkQueues();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
  tracker.OnIdle();
  EXPECT_TRUE(tracker.TryAcquireSyncWorkAuthorization().IsValid());

  tracker.WillRequestReloadImmediateWorkQueue();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
  // `OnIdle()` without `WillReloadImmediateWorkQueues()` is not sufficient for
  // a sync work authorization to be granted.
  tracker.OnIdle();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
}

// Verify that after `OnBeginWork()`, `OnIdle()` must be called for a sync
// work authorization to be granted.
TEST(SequenceManagerWorkTrackerTest, OnBeginWork) {
  WorkTracker tracker;
  tracker.SetRunTaskSynchronouslyAllowed(true);
  EXPECT_TRUE(tracker.TryAcquireSyncWorkAuthorization().IsValid());

  tracker.OnBeginWork();
  EXPECT_FALSE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
  tracker.OnIdle();
  EXPECT_TRUE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
}

// Verify that its not possible to simultaneously acquire two sync work
// authorizations.
TEST(SequenceManagerWorkTrackerTest, TwoSyncWorkAuthorizations) {
  WorkTracker tracker;
  tracker.SetRunTaskSynchronouslyAllowed(true);

  std::optional<SyncWorkAuthorization> first =
      tracker.TryAcquireSyncWorkAuthorization();
  EXPECT_TRUE(first->IsValid());
  SyncWorkAuthorization second = tracker.TryAcquireSyncWorkAuthorization();
  EXPECT_FALSE(second.IsValid());

  first.reset();
  // `second` is invalid so doesn't prevent acquiring another sync work
  // authorization.
  EXPECT_TRUE(tracker.TryAcquireSyncWorkAuthorization().IsValid());
}

// Verify that `OnBeginWork()` blocks until there is no valid sync work
// authorization.
TEST(SequenceManagerWorkTrackerTest, OnBeginWorkBlocks) {
  WorkTracker tracker;
  tracker.SetRunTaskSynchronouslyAllowed(true);

  WaitableEvent did_acquire_sync_work_auth;
  bool will_release_sync_work_auth = false;
  Thread other_thread("OtherThread");
  other_thread.Start();
  other_thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        std::optional<SyncWorkAuthorization> auth =
            tracker.TryAcquireSyncWorkAuthorization();
        EXPECT_TRUE(auth->IsValid());
        did_acquire_sync_work_auth.Signal();
        PlatformThread::Sleep(TestTimeouts::tiny_timeout());
        will_release_sync_work_auth = true;
        auth.reset();
      }));

  did_acquire_sync_work_auth.Wait();

  tracker.OnBeginWork();
  // `will_release_sync_work_auth` must be true (with no data race detected by
  // TSAN) when the call above returns.
  EXPECT_TRUE(will_release_sync_work_auth);

  other_thread.FlushForTesting();
}

}  // namespace base::sequence_manager::internal
