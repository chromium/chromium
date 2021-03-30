// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_waitable_event.h"

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class NoInvokeBlockingObserver : public internal::BlockingObserver {
 public:
  void BlockingStarted(BlockingType blocking_type) override { ADD_FAILURE(); }
  void BlockingTypeUpgraded() override { ADD_FAILURE(); }
  void BlockingEnded() override { ADD_FAILURE(); }
};

TEST(TestWaitableEvent, NoBlockingCall) {
  test::TaskEnvironment task_environment;

  NoInvokeBlockingObserver test_observer;
  internal::SetBlockingObserverForCurrentThread(&test_observer);

  TestWaitableEvent test_waitable_event;
  ThreadPool::PostTask(
      FROM_HERE, {},
      BindOnce(&WaitableEvent::Signal, Unretained(&test_waitable_event)));
  test_waitable_event.Wait();

  internal::ClearBlockingObserverForCurrentThread();
}

TEST(TestWaitableEvent, WaitingInPoolDoesntRequireAllowance) {
  test::TaskEnvironment task_environment;

  TestWaitableEvent test_waitable_event;
  // MayBlock()/WithBaseSyncPrimitives()/ScopedAllowBaseSyncPrimitivesForTesting
  // are required to Wait() on a TestWaitableEvent.
  ThreadPool::PostTask(
      FROM_HERE, {},
      BindOnce(&WaitableEvent::Wait, Unretained(&test_waitable_event)));
  test_waitable_event.Signal();

  task_environment.RunUntilIdle();
}

// Binding &WaitableEvent::Signal or &TestWaitableEvent::Signal is equivalent.
TEST(TestWaitableEvent, CanBindEitherType) {
  test::TaskEnvironment task_environment;
  TestWaitableEvent test_waitable_event(WaitableEvent::ResetPolicy::AUTOMATIC);

  ThreadPool::PostTask(
      FROM_HERE, {},
      BindOnce(&WaitableEvent::Signal, Unretained(&test_waitable_event)));
  test_waitable_event.Wait();

  ThreadPool::PostTask(
      FROM_HERE, {},
      BindOnce(&TestWaitableEvent::Signal, Unretained(&test_waitable_event)));
  test_waitable_event.Wait();
}

}  // namespace base
