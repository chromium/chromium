// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"

#include <string>

#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace base {

namespace {

class TraceWaitableEvent {
 public:
  TraceWaitableEvent() = default;
  ~TraceWaitableEvent() = default;

  void Signal() {
    ElapsedTimer timer;
    event_.Signal();
    total_signal_time_ += timer.Elapsed();
    ++signal_samples_;
  }

  void Wait() {
    ElapsedTimer timer;
    event_.Wait();
    total_wait_time_ += timer.Elapsed();
    ++wait_samples_;
  }

  bool TimedWaitUntil(const TimeTicks& end_time) {
    ElapsedTimer timer;
    const bool signaled = event_.TimedWait(end_time - timer.Begin());
    total_wait_time_ += timer.Elapsed();
    ++wait_samples_;
    return signaled;
  }

  bool IsSignaled() { return event_.IsSignaled(); }

  TimeDelta total_signal_time() const { return total_signal_time_; }
  TimeDelta total_wait_time() const { return total_wait_time_; }
  size_t signal_samples() const { return signal_samples_; }
  size_t wait_samples() const { return wait_samples_; }

 private:
  WaitableEvent event_{WaitableEvent::ResetPolicy::AUTOMATIC};

  TimeDelta total_signal_time_;
  TimeDelta total_wait_time_;

  size_t signal_samples_ = 0U;
  size_t wait_samples_ = 0U;

  DISALLOW_COPY_AND_ASSIGN(TraceWaitableEvent);
};

class SignalerThread : public SimpleThread {
 public:
  SignalerThread(TraceWaitableEvent* waiter, TraceWaitableEvent* signaler)
      : SimpleThread("WaitableEventPerfTest signaler"),
        waiter_(waiter),
        signaler_(signaler) {}

  ~SignalerThread() override = default;

  void Run() override {
    while (!stop_event_.IsSignaled()) {
      if (waiter_)
        waiter_->Wait();
      if (signaler_)
        signaler_->Signal();
    }
  }

  // Signals the thread to stop on the next iteration of its loop (which
  // will happen immediately if no |waiter_| is present or is signaled.
  void RequestStop() { stop_event_.Signal(); }

 private:
  WaitableEvent stop_event_;
  TraceWaitableEvent* waiter_;
  TraceWaitableEvent* signaler_;
  DISALLOW_COPY_AND_ASSIGN(SignalerThread);
};

void PrintPerfWaitableEvent(const TraceWaitableEvent* event,
                            const std::string& trace) {
  perf_test::PrintResult(
      "WaitableEvent_SignalTime_ns", "", trace,
      static_cast<size_t>(event->total_signal_time().InNanoseconds()) /
          event->signal_samples(),
      "ns/sample", true);
  perf_test::PrintResult(
      "WaitableEvent_WaitTime_ns", "", trace,
      static_cast<size_t>(event->total_wait_time().InNanoseconds()) /
          event->wait_samples(),
      "ns/sample", true);
}

}  // namespace

TEST(WaitableEventPerfTest, SingleThread) {
  const size_t kSamples = 1000;

  TraceWaitableEvent event;

  for (size_t i = 0; i < kSamples; ++i) {
    event.Signal();
    event.Wait();
  }

  PrintPerfWaitableEvent(&event, "singlethread-1000-samples");
}

TEST(WaitableEventPerfTest, MultipleThreads) {
  const size_t kSamples = 1000;

  TraceWaitableEvent waiter;
  TraceWaitableEvent signaler;

  // The other thread will wait and signal on the respective opposite events.
  SignalerThread thread(&signaler, &waiter);
  thread.Start();

  for (size_t i = 0; i < kSamples; ++i) {
    signaler.Signal();
    waiter.Wait();
  }

  // Signal the stop event and then make sure the signaler event it is
  // waiting on is also signaled.
  thread.RequestStop();
  signaler.Signal();

  thread.Join();

  PrintPerfWaitableEvent(&waiter, "multithread-1000-samples_waiter");
  PrintPerfWaitableEvent(&signaler, "multithread-1000-samples_signaler");
}

TEST(WaitableEventPerfTest, Throughput) {
  TraceWaitableEvent event;

  SignalerThread thread(nullptr, &event);
  thread.Start();

  const TimeTicks end_time = TimeTicks::Now() + TimeDelta::FromSeconds(1);
  size_t count = 0;
  while (event.TimedWaitUntil(end_time)) {
    ++count;
  }

  thread.RequestStop();
  thread.Join();

  perf_test::PrintResult("counts", "", "throughput", count, "signals", true);
  PrintPerfWaitableEvent(&event, "throughput");
}

}  // namespace base
