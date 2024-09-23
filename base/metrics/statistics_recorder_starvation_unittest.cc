// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"

#include <atomic>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr TimeDelta kTestRunningTime = Seconds(5);
constexpr char kHistogramNamePrefix[] = "SRStarvationTest.";

class BaseThread : public SimpleThread {
 public:
  explicit BaseThread(const std::string& thread_name)
      : SimpleThread(thread_name, Options()), thread_name_(thread_name) {}

  BaseThread(const BaseThread&) = delete;
  BaseThread& operator=(const BaseThread&) = delete;

  ~BaseThread() override = default;

  const std::string& thread_name() { return thread_name_; }
  void SetStartTime() { start_time_ = Time::Now(); }
  bool ShouldStop() { return stop_.load(std::memory_order_relaxed); }
  void Stop() {
    stop_.store(true, std::memory_order_relaxed);
    end_time_ = Time::Now();
  }
  void IncrementIterCount() { ++iter_count_; }
  size_t iter_count() { return iter_count_; }
  TimeDelta running_time() { return end_time_ - start_time_; }
  TimeDelta average_time_per_iter() { return running_time() / iter_count_; }

 private:
  std::string thread_name_;
  std::atomic<bool> stop_ = false;
  size_t iter_count_ = 0;
  Time start_time_;
  Time end_time_;
};

class ReadThread : public BaseThread {
 public:
  explicit ReadThread(size_t id)
      : BaseThread(/*thread_name=*/StrCat({"ReadThread", NumberToString(id)})) {
  }

  void Run() override {
    SetStartTime();

    const std::string histogram_name =
        StrCat({kHistogramNamePrefix, "ReadThreadHistogram"});
    while (!ShouldStop()) {
      // Continuously emit to the same histogram. Because this histogram should
      // already exist within the StatisticsRecorder's internal map (except the
      // first time this is called), it should not cause any modifications to
      // it, only a lookup. In other words, it will call
      // StatisticsRecorder::FindHistogram().
      UmaHistogramBoolean(histogram_name, false);
      IncrementIterCount();
    }
  }
};

class WriteThread : public BaseThread {
 public:
  explicit WriteThread(size_t id)
      : BaseThread(
            /*thread_name=*/StrCat({"WriteThread", NumberToString(id)})) {}

  void Run() override {
    SetStartTime();

    const std::string base_name =
        StrCat({kHistogramNamePrefix, thread_name(), ".Iteration"});
    while (!ShouldStop()) {
      // Continuously emit to a new histogram. Because this histogram should not
      // exist within the StatisticsRecorder's internal map, it will cause an
      // insertion to it every time. In other words, it will call
      // StatisticsRecorder::RegisterOrDeleteDuplicate().
      UmaHistogramBoolean(StrCat({base_name, NumberToString(iter_count())}),
                          false);
      IncrementIterCount();
    }
  }
};

}  // namespace

// Determines the number of reader and writer threads to run in the test.
struct StarvationTestParams {
  size_t num_read_threads;
  size_t num_write_threads;
};

// Determines which threads should start running first.
enum class FirstThreadsToStart {
  kReaders,
  kWriters,
};

class StatisticsRecorderStarvationTest
    : public testing::TestWithParam<
          std::tuple<StarvationTestParams, FirstThreadsToStart>> {
 public:
  StatisticsRecorderStarvationTest() = default;

  StatisticsRecorderStarvationTest(const StatisticsRecorderStarvationTest&) =
      delete;
  StatisticsRecorderStarvationTest& operator=(
      const StatisticsRecorderStarvationTest&) = delete;

  ~StatisticsRecorderStarvationTest() override = default;

  void SetUp() override {
    // Create a new StatisticsRecorder so that this test run will not affect
    // future ones. In particular, this test relies on creating new histograms
    // and adding them to the SR.
    sr_ = StatisticsRecorder::CreateTemporaryForTesting();

    // Emit a bunch of histograms, which will add them to the SR's internal
    // histogram map. This is so that lookups and insertions (FindHistogram()
    // and RegisterOrDeleteDuplicate()) during the test don't complete (pretty
    // much) instantly.
    for (size_t i = 0; i < 10000; ++i) {
      UmaHistogramBoolean(
          StrCat({kHistogramNamePrefix, "Dummy", NumberToString(i)}), false);
    }
  }

  void TearDown() override {
    // Clean up histograms that were allocated during this test. Note that the
    // histogram objects are deleted after releasing the temporary `sr_`.
    // Otherwise, for a brief moment, the temporary `sr_` would be holding
    // dangling pointers.
    auto histograms = sr_->GetHistograms();
    sr_.reset();
    for (auto* histogram : histograms) {
      if (StartsWith(histogram->histogram_name(), kHistogramNamePrefix)) {
        delete histogram;
      }
    }
  }

  // Starts reader and writer threads.
  void StartThreads() {
    for (size_t i = 0; i < num_read_threads(); ++i) {
      read_threads_.emplace_back(std::make_unique<ReadThread>(/*id=*/i));
    }
    for (size_t i = 0; i < num_write_threads(); ++i) {
      write_threads_.emplace_back(std::make_unique<WriteThread>(/*id=*/i));
    }

    // Depending on the value of GetFirstThreadsToStart(), either start the
    // readers or the writers first. Because some implementations will give
    // priority to whatever managed to get the lock first, do this to have
    // coverage.
    span<std::unique_ptr<BaseThread>> start_first_threads;
    span<std::unique_ptr<BaseThread>> start_second_threads;
    switch (GetFirstThreadsToStart()) {
      case FirstThreadsToStart::kReaders:
        start_first_threads = read_threads_;
        start_second_threads = write_threads_;
        break;
      case FirstThreadsToStart::kWriters:
        start_first_threads = write_threads_;
        start_second_threads = read_threads_;
        break;
    }
    for (auto& thread : start_first_threads) {
      thread->Start();
    }
    PlatformThread::Sleep(Milliseconds(100));
    for (auto& thread : start_second_threads) {
      thread->Start();
    }
  }

  // Stops reader and writer threads.
  void StopThreads() {
    for (auto* thread : GetAllThreads()) {
      thread->Stop();
      thread->Join();
    }
  }

  std::vector<BaseThread*> GetAllThreads() {
    std::vector<BaseThread*> threads;
    threads.reserve(num_read_threads() + num_write_threads());
    for (auto& read_thread : read_threads_) {
      threads.push_back(read_thread.get());
    }
    for (auto& write_thread : write_threads_) {
      threads.push_back(write_thread.get());
    }
    return threads;
  }

  size_t num_read_threads() { return std::get<0>(GetParam()).num_read_threads; }

  size_t num_write_threads() {
    return std::get<0>(GetParam()).num_write_threads;
  }

  FirstThreadsToStart GetFirstThreadsToStart() {
    return std::get<1>(GetParam());
  }

 protected:
  std::vector<std::unique_ptr<BaseThread>> read_threads_;
  std::vector<std::unique_ptr<BaseThread>> write_threads_;

 private:
  std::unique_ptr<StatisticsRecorder> sr_;
};

// Verifies that there are no starvation issues when emitting histograms (since
// it may be done from any thread). In particular, emitting a histogram requires
// a lock to look up (and sometimes write to) an internal map in the
// StatisticsRecorder. When switching to a Read/Write lock (see crbug/1123627),
// we encountered such a starvation issue, where a thread trying to write to the
// internal map was starved out for 10+ seconds by readers on iOS.
// TODO(crbug.com/41489801): StatisticsRecorderNoStarvation continuously emits a
// new histogram which can cause the app memory footprint to grow unbounded and
// watchdog kill the unit test on iOS devices.
TEST_P(StatisticsRecorderStarvationTest, StatisticsRecorderNoStarvation) {
  // Make sure there is no GlobalHistogramAllocator so that histograms emitted
  // during this test are all allocated on the heap, which makes it a lot easier
  // to clean them up at the end.
  ASSERT_FALSE(GlobalHistogramAllocator::Get());

  // Start reader and writer threads.
  StartThreads();

  // Let the test run for |kTestRunningTime|.
  PlatformThread::Sleep(kTestRunningTime);

  // Stop reader and writer threads. This waits for them to complete by joining
  // the threads.
  StopThreads();

  // Verify that on average, on each thread, performing a read or write took
  // less than 1ms. There is no meaning to 50ms -- this is just to ensure that
  // there is no egregious starvation effect (for example, we've seen crash
  // reports where readers were starving out writers for >10 seconds). Note that
  // the average time it took to perform one iteration is:
  // average_time_per_iteration = running_time / iteration_count.
  static constexpr TimeDelta kStarvationThreshold = Milliseconds(50);
  std::vector<BaseThread*> threads = GetAllThreads();
  for (auto* thread : threads) {
    EXPECT_LT(thread->average_time_per_iter(), kStarvationThreshold);
  }

  // Print some useful information that could come in handy if this test fails
  // (or for diagnostic purposes).
  LOG(INFO) << "Params: num_read_threads=" << num_read_threads()
            << ", num_write_threads=" << num_write_threads()
            << ", FirstThreadsToStart="
            << static_cast<int>(GetFirstThreadsToStart());
  for (auto* thread : threads) {
    LOG(INFO) << thread->thread_name() << " iter_count=" << thread->iter_count()
              << ", running_time=" << thread->running_time()
              << ", average_time_per_iter=" << thread->average_time_per_iter();
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StatisticsRecorderStarvationTest,
    testing::Combine(
        testing::Values(StarvationTestParams{.num_read_threads = 10,
                                             .num_write_threads = 1},
                        StarvationTestParams{.num_read_threads = 1,
                                             .num_write_threads = 10},
                        StarvationTestParams{.num_read_threads = 1,
                                             .num_write_threads = 1},
                        StarvationTestParams{.num_read_threads = 5,
                                             .num_write_threads = 5}),
        testing::Values(FirstThreadsToStart::kReaders,
                        FirstThreadsToStart::kWriters)));

}  // namespace base
