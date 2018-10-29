// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"

#include <stddef.h>

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(WaitableEventTest, ManualBasics) {
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_FALSE(event.IsSignaled());

  event.Signal();
  EXPECT_TRUE(event.IsSignaled());
  EXPECT_TRUE(event.IsSignaled());

  event.Reset();
  EXPECT_FALSE(event.IsSignaled());
  EXPECT_FALSE(event.TimedWait(TimeDelta::FromMilliseconds(10)));

  event.Signal();
  event.Wait();
  EXPECT_TRUE(event.TimedWait(TimeDelta::FromMilliseconds(10)));
}

TEST(WaitableEventTest, ManualInitiallySignaled) {
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::SIGNALED);

  EXPECT_TRUE(event.IsSignaled());
  EXPECT_TRUE(event.IsSignaled());

  event.Reset();

  EXPECT_FALSE(event.IsSignaled());
  EXPECT_FALSE(event.IsSignaled());

  event.Signal();

  event.Wait();
  EXPECT_TRUE(event.IsSignaled());
  EXPECT_TRUE(event.IsSignaled());
}

TEST(WaitableEventTest, AutoBasics) {
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_FALSE(event.IsSignaled());

  event.Signal();
  EXPECT_TRUE(event.IsSignaled());
  EXPECT_FALSE(event.IsSignaled());

  event.Reset();
  EXPECT_FALSE(event.IsSignaled());
  EXPECT_FALSE(event.TimedWait(TimeDelta::FromMilliseconds(10)));

  event.Signal();
  event.Wait();
  EXPECT_FALSE(event.TimedWait(TimeDelta::FromMilliseconds(10)));

  event.Signal();
  EXPECT_TRUE(event.TimedWait(TimeDelta::FromMilliseconds(10)));
}

TEST(WaitableEventTest, AutoInitiallySignaled) {
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::SIGNALED);

  EXPECT_TRUE(event.IsSignaled());
  EXPECT_FALSE(event.IsSignaled());

  event.Signal();

  EXPECT_TRUE(event.IsSignaled());
  EXPECT_FALSE(event.IsSignaled());
}

TEST(WaitableEventTest, WaitManyShortcut) {
  WaitableEvent* ev[5];
  for (auto*& i : ev) {
    i = new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                          WaitableEvent::InitialState::NOT_SIGNALED);
  }

  ev[3]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 3u);

  ev[3]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 3u);

  ev[4]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 4u);

  ev[0]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 0u);

  for (auto* i : ev)
    delete i;
}

TEST(WaitableEventTest, WaitManyLeftToRight) {
  WaitableEvent* ev[5];
  for (auto*& i : ev) {
    i = new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                          WaitableEvent::InitialState::NOT_SIGNALED);
  }

  // Test for consistent left-to-right return behavior across all permutations
  // of the input array. This is to verify that only the indices -- and not
  // the WaitableEvents' addresses -- are relevant in determining who wins when
  // multiple events are signaled.

  std::sort(ev, ev + 5);
  do {
    ev[0]->Signal();
    ev[1]->Signal();
    EXPECT_EQ(0u, WaitableEvent::WaitMany(ev, 5));

    ev[2]->Signal();
    EXPECT_EQ(1u, WaitableEvent::WaitMany(ev, 5));
    EXPECT_EQ(2u, WaitableEvent::WaitMany(ev, 5));

    ev[3]->Signal();
    ev[4]->Signal();
    ev[0]->Signal();
    EXPECT_EQ(0u, WaitableEvent::WaitMany(ev, 5));
    EXPECT_EQ(3u, WaitableEvent::WaitMany(ev, 5));
    ev[2]->Signal();
    EXPECT_EQ(2u, WaitableEvent::WaitMany(ev, 5));
    EXPECT_EQ(4u, WaitableEvent::WaitMany(ev, 5));
  } while (std::next_permutation(ev, ev + 5));

  for (auto* i : ev)
    delete i;
}

class WaitableEventSignaler : public PlatformThread::Delegate {
 public:
  WaitableEventSignaler(TimeDelta delay, WaitableEvent* event)
      : delay_(delay),
        event_(event) {
  }

  void ThreadMain() override {
    PlatformThread::Sleep(delay_);
    event_->Signal();
  }

 private:
  const TimeDelta delay_;
  WaitableEvent* event_;
};

// Tests that a WaitableEvent can be safely deleted when |Wait| is done without
// additional synchronization.
TEST(WaitableEventTest, WaitAndDelete) {
  WaitableEvent* ev =
      new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                        WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEventSignaler signaler(TimeDelta::FromMilliseconds(10), ev);
  PlatformThreadHandle thread;
  PlatformThread::Create(0, &signaler, &thread);

  ev->Wait();
  delete ev;

  PlatformThread::Join(thread);
}

// Tests that a WaitableEvent can be safely deleted when |WaitMany| is done
// without additional synchronization.
TEST(WaitableEventTest, WaitMany) {
  WaitableEvent* ev[5];
  for (auto*& i : ev) {
    i = new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                          WaitableEvent::InitialState::NOT_SIGNALED);
  }

  WaitableEventSignaler signaler(TimeDelta::FromMilliseconds(10), ev[2]);
  PlatformThreadHandle thread;
  PlatformThread::Create(0, &signaler, &thread);

  size_t index = WaitableEvent::WaitMany(ev, 5);

  for (auto* i : ev)
    delete i;

  PlatformThread::Join(thread);
  EXPECT_EQ(2u, index);
}

// Tests that using TimeDelta::Max() on TimedWait() is not the same as passing
// a timeout of 0. (crbug.com/465948)
TEST(WaitableEventTest, TimedWait) {
  WaitableEvent* ev =
      new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                        WaitableEvent::InitialState::NOT_SIGNALED);

  TimeDelta thread_delay = TimeDelta::FromMilliseconds(10);
  WaitableEventSignaler signaler(thread_delay, ev);
  PlatformThreadHandle thread;
  TimeTicks start = TimeTicks::Now();
  PlatformThread::Create(0, &signaler, &thread);

  EXPECT_TRUE(ev->TimedWait(TimeDelta::Max()));
  EXPECT_GE(TimeTicks::Now() - start, thread_delay);
  delete ev;

  PlatformThread::Join(thread);
}

// Tests that a sub-ms TimedWait doesn't time out promptly.
TEST(WaitableEventTest, SubMsTimedWait) {
  WaitableEvent ev(WaitableEvent::ResetPolicy::AUTOMATIC,
                   WaitableEvent::InitialState::NOT_SIGNALED);

  TimeDelta delay = TimeDelta::FromMicroseconds(900);
  TimeTicks start_time = TimeTicks::Now();
  ev.TimedWait(delay);
  EXPECT_GE(TimeTicks::Now() - start_time, delay);
}

// Tests that TimedWaitUntil can be safely used with various end_time deadline
// values.
TEST(WaitableEventTest, TimedWaitUntil) {
  WaitableEvent ev(WaitableEvent::ResetPolicy::AUTOMATIC,
                   WaitableEvent::InitialState::NOT_SIGNALED);

  TimeTicks start_time(TimeTicks::Now());
  TimeDelta delay = TimeDelta::FromMilliseconds(10);

  // Should be OK to wait for the current time or time in the past.
  // That should end promptly and be equivalent to IsSignalled.
  EXPECT_FALSE(ev.TimedWaitUntil(start_time));
  EXPECT_FALSE(ev.TimedWaitUntil(start_time - delay));

  // Should be OK to wait for zero TimeTicks().
  EXPECT_FALSE(ev.TimedWaitUntil(TimeTicks()));

  // Waiting for a time in the future shouldn't end before the deadline
  // if the event isn't signalled.
  EXPECT_FALSE(ev.TimedWaitUntil(start_time + delay));
  EXPECT_GE(TimeTicks::Now() - start_time, delay);

  // Test that passing TimeTicks::Max to TimedWaitUntil is valid and isn't
  // the same as passing TimeTicks(). Also verifies that signaling event
  // ends the wait promptly.
  WaitableEventSignaler signaler(delay, &ev);
  PlatformThreadHandle thread;
  start_time = TimeTicks::Now();
  PlatformThread::Create(0, &signaler, &thread);

  EXPECT_TRUE(ev.TimedWaitUntil(TimeTicks::Max()));
  EXPECT_GE(TimeTicks::Now() - start_time, delay);

  PlatformThread::Join(thread);
}

}  // namespace base
