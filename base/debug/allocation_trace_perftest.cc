// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>
#include <vector>

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/debug/allocation_trace.h"
#include "base/strings/stringprintf.h"
#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {
namespace debug {
namespace {
// Change kTimeLimit to something higher if you need more time to capture a
// trace.
constexpr base::TimeDelta kTimeLimit = base::Seconds(3);
constexpr int kWarmupRuns = 100;
constexpr int kTimeCheckInterval = 1000;
constexpr char kMetricStackTraceDuration[] = ".duration_per_run";
constexpr char kMetricStackTraceThroughput[] = ".throughput";

enum class HandlerFunctionSelector { OnAllocation, OnFree };

// An executor to perform the actual notification of the recorder. The correct
// handler function is selected using template specialization based on the
// HandlerFunctionSelector.
template <HandlerFunctionSelector HandlerFunction>
struct HandlerFunctionExecutor {
  void operator()(base::debug::tracer::AllocationTraceRecorder& recorder) const;
};

template <>
struct HandlerFunctionExecutor<HandlerFunctionSelector::OnAllocation> {
  void operator()(
      base::debug::tracer::AllocationTraceRecorder& recorder) const {
    // Since the recorder just stores the value, we can use any value for
    // address and size that we want.
    recorder.OnAllocation(
        base::allocator::dispatcher::AllocationNotificationData(
            &recorder, sizeof(recorder), nullptr,
            base::allocator::dispatcher::AllocationSubsystem::
                kPartitionAllocator));
  }
};

template <>
struct HandlerFunctionExecutor<HandlerFunctionSelector::OnFree> {
  void operator()(
      base::debug::tracer::AllocationTraceRecorder& recorder) const {
    recorder.OnFree(base::allocator::dispatcher::FreeNotificationData(
        &recorder,
        base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator));
  }
};
}  // namespace

class AllocationTraceRecorderPerfTest
    : public testing::TestWithParam<
          std::tuple<HandlerFunctionSelector, size_t>> {
 protected:
  // The result data of a single thread. From the results of all the single
  // threads the final results will be calculated.
  struct ResultData {
    TimeDelta time_per_lap;
    float laps_per_second = 0.0;
    int number_of_laps = 0;
  };

  // The data of a single test thread.
  struct ThreadRunnerData {
    std::thread thread;
    ResultData result_data;
  };

  // Create and setup the result reporter.
  const char* GetHandlerDescriptor(HandlerFunctionSelector handler_function);
  perf_test::PerfResultReporter SetUpReporter(
      HandlerFunctionSelector handler_function,
      size_t number_of_allocating_threads);

  // Select the correct test function which shall be used for the current test.
  using TestFunction =
      void (*)(base::debug::tracer::AllocationTraceRecorder& recorder,
               ResultData& result_data);

  static TestFunction GetTestFunction(HandlerFunctionSelector handler_function);
  template <HandlerFunctionSelector HandlerFunction>
  static void TestFunctionImplementation(
      base::debug::tracer::AllocationTraceRecorder& recorder,
      ResultData& result_data);

  // The test management function. Using the the above auxiliary functions it is
  // responsible to setup the result reporter, select the correct test function,
  // spawn the specified number of worker threads and post process the results.
  void PerformTest(HandlerFunctionSelector handler_function,
                   size_t number_of_allocating_threads);
};

const char* AllocationTraceRecorderPerfTest::GetHandlerDescriptor(
    HandlerFunctionSelector handler_function) {
  switch (handler_function) {
    case HandlerFunctionSelector::OnAllocation:
      return "OnAllocation";
    case HandlerFunctionSelector::OnFree:
      return "OnFree";
  }
}

perf_test::PerfResultReporter AllocationTraceRecorderPerfTest::SetUpReporter(
    HandlerFunctionSelector handler_function,
    size_t number_of_allocating_threads) {
  const std::string story_name = base::StringPrintf(
      "(%s;%zu-threads)", GetHandlerDescriptor(handler_function),
      number_of_allocating_threads);

  perf_test::PerfResultReporter reporter("AllocationRecorderPerf", story_name);
  reporter.RegisterImportantMetric(kMetricStackTraceDuration, "ns");
  reporter.RegisterImportantMetric(kMetricStackTraceThroughput, "runs/s");
  return reporter;
}

AllocationTraceRecorderPerfTest::TestFunction
AllocationTraceRecorderPerfTest::GetTestFunction(
    HandlerFunctionSelector handler_function) {
  switch (handler_function) {
    case HandlerFunctionSelector::OnAllocation:
      return TestFunctionImplementation<HandlerFunctionSelector::OnAllocation>;
    case HandlerFunctionSelector::OnFree:
      return TestFunctionImplementation<HandlerFunctionSelector::OnFree>;
  }
}

void AllocationTraceRecorderPerfTest::PerformTest(
    HandlerFunctionSelector handler_function,
    size_t number_of_allocating_threads) {
  perf_test::PerfResultReporter reporter =
      SetUpReporter(handler_function, number_of_allocating_threads);

  TestFunction test_function = GetTestFunction(handler_function);

  base::debug::tracer::AllocationTraceRecorder the_recorder;

  std::vector<ThreadRunnerData> notifying_threads;
  notifying_threads.reserve(number_of_allocating_threads);

  // Setup the threads. After creation, each thread immediately starts running.
  // We expect the creation of the threads to be so quick that the delay from
  // first to last thread is negligible.
  for (size_t i = 0; i < number_of_allocating_threads; ++i) {
    auto& last_item = notifying_threads.emplace_back();

    last_item.thread = std::thread{test_function, std::ref(the_recorder),
                                   std::ref(last_item.result_data)};
  }

  TimeDelta average_time_per_lap;
  float average_laps_per_second = 0;

  // Wait for each thread to finish and collect its result data.
  for (auto& item : notifying_threads) {
    item.thread.join();
    // When finishing, each threads writes its results into result_data. So,
    // from here we gather its performance statistics.
    average_time_per_lap += item.result_data.time_per_lap;
    average_laps_per_second += item.result_data.laps_per_second;
  }

  average_time_per_lap /= number_of_allocating_threads;
  average_laps_per_second /= number_of_allocating_threads;

  reporter.AddResult(kMetricStackTraceDuration, average_time_per_lap);
  reporter.AddResult(kMetricStackTraceThroughput, average_laps_per_second);
}

template <HandlerFunctionSelector HandlerFunction>
void AllocationTraceRecorderPerfTest::TestFunctionImplementation(
    base::debug::tracer::AllocationTraceRecorder& recorder,
    ResultData& result_data) {
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval,
                 LapTimer::TimerMethod::kUseTimeTicks);

  HandlerFunctionExecutor<HandlerFunction> handler_executor;

  timer.Start();
  do {
    handler_executor(recorder);

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  result_data.time_per_lap = timer.TimePerLap();
  result_data.laps_per_second = timer.LapsPerSecond();
  result_data.number_of_laps = timer.NumLaps();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AllocationTraceRecorderPerfTest,
    ::testing::Combine(::testing::Values(HandlerFunctionSelector::OnAllocation,
                                         HandlerFunctionSelector::OnFree),
                       ::testing::Values(1, 5, 10, 20, 40, 80)));

TEST_P(AllocationTraceRecorderPerfTest, TestNotification) {
  const auto parameters = GetParam();
  const HandlerFunctionSelector handler_function = std::get<0>(parameters);
  const size_t number_of_threads = std::get<1>(parameters);
  PerformTest(handler_function, number_of_threads);
}

}  // namespace debug
}  // namespace base
