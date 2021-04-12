// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <string>

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

// This file contains tests to measure the cost of incrementing:
// - A non-atomic variable, no lock.
// - A non-atomic variable, with lock.
// - An atomic variable, no memory barriers.
// - An atomic variable, acquire-release barriers.
// The goal is to provide data to guide counter implementation choices.

namespace base {

namespace {

constexpr char kMetricPrefixCounter[] = "Counter.";
constexpr char kMetricOperationThroughput[] = "operation_throughput";
constexpr uint64_t kNumIterations = 100000000;

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixCounter, story_name);
  reporter.RegisterImportantMetric(kMetricOperationThroughput, "operations/ms");
  return reporter;
}

class Uint64_NoLock {
 public:
  Uint64_NoLock() = default;
  void Increment() { ++counter_; }
  uint64_t value() const { return counter_; }

 private:
  // Volatile to prevent the compiler from over-optimizing the increment.
  volatile uint64_t counter_ = 0;
};

class Uint64_Lock {
 public:
  Uint64_Lock() = default;
  void Increment() {
    AutoLock auto_lock(lock_);
    ++counter_;
  }
  uint64_t value() const {
    AutoLock auto_lock(lock_);
    return counter_;
  }

 private:
  mutable Lock lock_;
  uint64_t counter_ GUARDED_BY(lock_) = 0;
};

class AtomicUint64_NoBarrier {
 public:
  AtomicUint64_NoBarrier() = default;
  void Increment() { counter_.fetch_add(1, std::memory_order_relaxed); }
  uint64_t value() const { return counter_; }

 private:
  std::atomic<uint64_t> counter_{0};
};

class AtomicUint64_Barrier {
 public:
  AtomicUint64_Barrier() = default;
  void Increment() { counter_.fetch_add(1, std::memory_order_acq_rel); }
  uint64_t value() const { return counter_; }

 private:
  std::atomic<uint64_t> counter_{0};
};

template <typename CounterType>
class IncrementThread : public SimpleThread {
 public:
  // Upon entering its main function, the thread waits for |start_event| to be
  // signaled. Then, it increments |counter| |kNumIterations| times.
  // Finally, it invokes |done_closure|.
  explicit IncrementThread(WaitableEvent* start_event,
                           CounterType* counter,
                           OnceClosure done_closure)
      : SimpleThread("IncrementThread"),
        start_event_(start_event),
        counter_(counter),
        done_closure_(std::move(done_closure)) {}

  // SimpleThread:
  void Run() override {
    start_event_->Wait();
    for (uint64_t i = 0; i < kNumIterations; ++i)
      counter_->Increment();
    std::move(done_closure_).Run();
  }

 private:
  WaitableEvent* const start_event_;
  CounterType* const counter_;
  OnceClosure done_closure_;
};

template <typename CounterType>
void RunIncrementPerfTest(const std::string& story_name, int num_threads) {
  WaitableEvent start_event;
  WaitableEvent end_event;
  CounterType counter;
  RepeatingClosure done_closure = BarrierClosure(
      num_threads, BindOnce(&WaitableEvent::Signal, Unretained(&end_event)));

  std::vector<std::unique_ptr<IncrementThread<CounterType>>> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.push_back(std::make_unique<IncrementThread<CounterType>>(
        &start_event, &counter, done_closure));
    threads.back()->Start();
  }

  TimeTicks start_time = TimeTicks::Now();
  start_event.Signal();
  end_event.Wait();
  TimeTicks end_time = TimeTicks::Now();

  EXPECT_EQ(num_threads * kNumIterations, counter.value());

  auto reporter = SetUpReporter(story_name);
  reporter.AddResult(
      kMetricOperationThroughput,
      kNumIterations / (end_time - start_time).InMillisecondsF());

  for (auto& thread : threads)
    thread->Join();
}

}  // namespace

TEST(CounterPerfTest, Uint64_NoLock_1Thread) {
  RunIncrementPerfTest<Uint64_NoLock>("Uint64_NoLock_1Thread", 1);
}

// No Uint64_NoLock_4Threads test because it would cause data races.

TEST(CounterPerfTest, Uint64_Lock_1Thread) {
  RunIncrementPerfTest<Uint64_Lock>("Uint64_Lock_1Thread", 1);
}

TEST(CounterPerfTest, Uint64_Lock_4Threads) {
  RunIncrementPerfTest<Uint64_Lock>("Uint64_Lock_4Threads", 4);
}

TEST(CounterPerfTest, AtomicUint64_NoBarrier_1Thread) {
  RunIncrementPerfTest<AtomicUint64_NoBarrier>("AtomicUint64_NoBarrier_1Thread",
                                               1);
}

TEST(CounterPerfTest, AtomicUint64_NoBarrier_4Threads) {
  RunIncrementPerfTest<AtomicUint64_NoBarrier>(
      "AtomicUint64_NoBarrier_4Threads", 4);
}

TEST(CounterPerfTest, AtomicUint64_Barrier_1Thread) {
  RunIncrementPerfTest<AtomicUint64_Barrier>("AtomicUint64_Barrier_1Thread", 1);
}

TEST(CounterPerfTest, AtomicUint64_Barrier_4Threads) {
  RunIncrementPerfTest<AtomicUint64_Barrier>("AtomicUint64_Barrier_4Threads",
                                             4);
}

}  // namespace base
