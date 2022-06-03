// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_deduplicator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace sequence_manager {
namespace internal {

using NextTask = WorkDeduplicator::NextTask;
using ShouldScheduleWork = WorkDeduplicator::ShouldScheduleWork;

TEST(WorkDeduplicatorTest, BindToCurrentThreadWithoutPriorOnWorkRequested) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());

  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.BindToCurrentThread());
}

TEST(WorkDeduplicatorTest, OnWorkRequestedUnBound) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());

  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.BindToCurrentThread());
}

TEST(WorkDeduplicatorTest, OnWorkRequestedOnWorkStarted) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
}

TEST(WorkDeduplicatorTest, TaskRequestedWorkButDidCheckForMoreWorkDelayed) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  work_deduplicator.WillCheckForMoreWork();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsDelayed));
}

TEST(
    WorkDeduplicatorTest,
    TaskRequestedWorkButDidCheckForMoreWorkDelayedAndCrossThreadWorkRequested) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  work_deduplicator.WillCheckForMoreWork();
  // Simulate cross-thread PostTask while checking for more work.
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsDelayed));
}

TEST(WorkDeduplicatorTest, TaskRequestedWorkAndDidCheckForMoreWorkImmediate) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  work_deduplicator.WillCheckForMoreWork();
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsImmediate));
}

TEST(WorkDeduplicatorTest,
     TaskRequestedWorkAndDidCheckForMoreWorkImmediateCrossThreadWorkRequested) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  work_deduplicator.WillCheckForMoreWork();
  // Simulate cross-thread PostTask while checking for more work.
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsImmediate));
}

TEST(WorkDeduplicatorTest, DidCheckForMoreWorkDelayed) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  work_deduplicator.WillCheckForMoreWork();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsDelayed));
}

TEST(WorkDeduplicatorTest,
     DidCheckForMoreWorkDelayedAndCrossThreadWorkRequested) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  work_deduplicator.WillCheckForMoreWork();
  // Simulate cross-thread PostTask while checking for more work.
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsDelayed));
}

TEST(WorkDeduplicatorTest, DidCheckForMoreWorkImmediate) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  work_deduplicator.WillCheckForMoreWork();
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsImmediate));
}

TEST(WorkDeduplicatorTest,
     DidCheckForMoreWorkImmediateCrossThreadWorkRequested) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  work_deduplicator.WillCheckForMoreWork();
  // Simulate cross-thread PostTask while checking for more work.
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.DidCheckForMoreWork(NextTask::kIsImmediate));
}

TEST(WorkDeduplicatorTest, OnDelayedWorkRequestedUnbound) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());

  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnDelayedWorkRequested());
}

TEST(WorkDeduplicatorTest, OnDelayedWorkRequestedBound) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.OnDelayedWorkRequested());

  // Unlike OnWorkRequested, calling this again doesn't change the result,
  // because we assume a different delay is being requested.
  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.OnDelayedWorkRequested());
}

TEST(WorkDeduplicatorTest, OnDelayedWorkRequestedInDoWork) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnDelayedWorkRequested());
  work_deduplicator.WillCheckForMoreWork();
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnDelayedWorkRequested());
  work_deduplicator.DidCheckForMoreWork(NextTask::kIsImmediate);
}

TEST(WorkDeduplicatorTest,
     OnDelayedWorkRequestedDidCheckForMoreWorkWithMoreWork) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  work_deduplicator.WillCheckForMoreWork();
  work_deduplicator.DidCheckForMoreWork(NextTask::kIsImmediate);

  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnDelayedWorkRequested());
}

TEST(WorkDeduplicatorTest,
     OnDelayedWorkRequestedDidCheckForMoreWorkWithNoMoreWork) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  work_deduplicator.OnWorkStarted();
  work_deduplicator.WillCheckForMoreWork();
  work_deduplicator.DidCheckForMoreWork(NextTask::kIsDelayed);

  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.OnDelayedWorkRequested());
}

TEST(WorkDeduplicatorTest, OnDelayedWorkRequestedWithDoWorkPending) {
  WorkDeduplicator work_deduplicator(AssociatedThreadId::CreateBound());
  work_deduplicator.BindToCurrentThread();

  EXPECT_EQ(ShouldScheduleWork::kScheduleImmediate,
            work_deduplicator.OnWorkRequested());
  EXPECT_EQ(ShouldScheduleWork::kNotNeeded,
            work_deduplicator.OnDelayedWorkRequested());
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
