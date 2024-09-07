// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/synchronization/waitable_event.h"

#include <stddef.h>

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
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
  EXPECT_FALSE(event.TimedWait(Milliseconds(10)));

  event.Signal();
  event.Wait();
  EXPECT_TRUE(event.TimedWait(Milliseconds(10)));
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
  EXPECT_FALSE(event.TimedWait(Milliseconds(10)));

  event.Signal();
  event.Wait();
  EXPECT_FALSE(event.TimedWait(Milliseconds(10)));

  event.Signal();
  EXPECT_TRUE(event.TimedWait(Milliseconds(10)));
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
  raw_ptr<WaitableEvent> event_;
};

// Tests that a WaitableEvent can be safely deleted when |Wait| is done without
// additional synchronization.
TEST(WaitableEventTest, WaitAndDelete) {
  WaitableEvent* ev =
      new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                        WaitableEvent::InitialState::NOT_SIGNALED);

  PlatformThreadHandle thread;
  {
    // Signaler can't outlive event.
    WaitableEventSignaler signaler(Milliseconds(10), ev);
    PlatformThread::Create(0, &signaler, &thread);
    ev->Wait();
  }
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

  PlatformThreadHandle thread;
  {
    // Signaler can't outlive event.
    WaitableEventSignaler signaler(Milliseconds(10), ev[2]);
    PlatformThread::Create(0, &signaler, &thread);
    size_t index = WaitableEvent::WaitMany(ev, 5);
    EXPECT_EQ(2u, index);
  }

  for (auto* i : ev)
    delete i;

  PlatformThread::Join(thread);
}

// Tests that using TimeDelta::Max() on TimedWait() is not the same as passing
// a timeout of 0. (crbug.com/465948)
TEST(WaitableEventTest, TimedWait) {
  WaitableEvent* ev =
      new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                        WaitableEvent::InitialState::NOT_SIGNALED);

  PlatformThreadHandle thread;
  TimeDelta thread_delay = Milliseconds(10);
  {
    // Signaler can't outlive event.
    WaitableEventSignaler signaler(thread_delay, ev);
    TimeTicks start = TimeTicks::Now();
    PlatformThread::Create(0, &signaler, &thread);
    EXPECT_TRUE(ev->TimedWait(TimeDelta::Max()));
    EXPECT_GE(TimeTicks::Now() - start, thread_delay);
  }
  delete ev;

  PlatformThread::Join(thread);
}

// Tests that a sub-ms TimedWait doesn't time out promptly.
TEST(WaitableEventTest, SubMsTimedWait) {
  WaitableEvent ev(WaitableEvent::ResetPolicy::AUTOMATIC,
                   WaitableEvent::InitialState::NOT_SIGNALED);

  TimeDelta delay = Microseconds(900);
  TimeTicks start_time = TimeTicks::Now();
  ev.TimedWait(delay);
  EXPECT_GE(TimeTicks::Now() - start_time, delay);
}

// Tests that timeouts of zero return immediately (true if already signaled,
// false otherwise).
TEST(WaitableEventTest, ZeroTimeout) {
  WaitableEvent ev;
  TimeTicks start_time = TimeTicks::Now();
  EXPECT_FALSE(ev.TimedWait(TimeDelta()));
  EXPECT_LT(TimeTicks::Now() - start_time, Milliseconds(1));

  ev.Signal();
  start_time = TimeTicks::Now();
  EXPECT_TRUE(ev.TimedWait(TimeDelta()));
  EXPECT_LT(TimeTicks::Now() - start_time, Milliseconds(1));
}

// Same as ZeroTimeout for negative timeouts.
TEST(WaitableEventTest, NegativeTimeout) {
  WaitableEvent ev;
  TimeTicks start_time = TimeTicks::Now();
  EXPECT_FALSE(ev.TimedWait(Milliseconds(-10)));
  EXPECT_LT(TimeTicks::Now() - start_time, Milliseconds(1));

  ev.Signal();
  start_time = TimeTicks::Now();
  EXPECT_TRUE(ev.TimedWait(Milliseconds(-10)));
  EXPECT_LT(TimeTicks::Now() - start_time, Milliseconds(1));
}

}  // namespace base
