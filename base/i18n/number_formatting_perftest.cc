// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"

#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace base {
namespace {

constexpr int64_t kIntNumber = 1234567890LL;
constexpr double kDoubleNumber = 1234567.89012;
constexpr int kPercent = 42;

template <typename Function>
void RunAndRecord(std::string_view story_name, Function&& test_function) {
  size_t iterations = 0;
  ElapsedTimer timer;
  while (timer.Elapsed() < Milliseconds(100)) {
    for (size_t i = 0; i < 50; ++i) {
      auto result = test_function();
      ::benchmark::DoNotOptimize(result);
    }
    iterations += 50;
  }

  perf_test::PerfResultReporter reporter("NumberFormatting.", story_name);
  reporter.RegisterImportantMetric("wall_time", "ns");
  reporter.AddResult(
      "wall_time",
      static_cast<double>(timer.Elapsed().InNanoseconds()) / iterations);
}

#define TEST_NUMBER_FORMAT(name, expr)         \
  TEST(NumberFormattingPerfTest, name) {       \
    RunAndRecord(#name, [&] { return expr; }); \
  }

TEST_NUMBER_FORMAT(FormatNumber, FormatNumber(kIntNumber))
TEST_NUMBER_FORMAT(FormatDouble_FixedDigits, FormatDouble(kDoubleNumber, 2))
TEST_NUMBER_FORMAT(FormatDouble_MinMaxDigits, FormatDouble(kDoubleNumber, 0, 4))
TEST_NUMBER_FORMAT(FormatPercent, FormatPercent(kPercent))

// Measures FormatPercent when concurrent worker threads are also creating ICU
// NumberFormat instances and accessing UResourceBundle entries (contending on
// ICU's resource bundle cache and reference counts).
TEST(NumberFormattingPerfTest, FormatPercent_Concurrent) {
  class ContentionWorker : public SimpleThread {
   public:
    ContentionWorker(WaitableEvent* start_event, std::atomic<bool>* stop_flag)
        : SimpleThread("IcuContentionWorker"),
          start_event_(start_event),
          stop_flag_(stop_flag) {}

    void Run() override {
      start_event_->Wait();
      while (!stop_flag_->load(std::memory_order_relaxed)) {
        auto result = FormatPercent(kPercent);
        ::benchmark::DoNotOptimize(result);
      }
    }

   private:
    const raw_ptr<WaitableEvent> start_event_;
    const raw_ptr<std::atomic<bool>> stop_flag_;
  };

  WaitableEvent start_event(WaitableEvent::ResetPolicy::MANUAL,
                            WaitableEvent::InitialState::NOT_SIGNALED);
  std::atomic<bool> stop_flag{false};
  std::vector<std::unique_ptr<ContentionWorker>> workers;
  for (size_t i = 0; i < 4; ++i) {
    workers
        .emplace_back(
            std::make_unique<ContentionWorker>(&start_event, &stop_flag))
        ->Start();
  }
  start_event.Signal();

  RunAndRecord("FormatPercent_Concurrent",
               [&] { return FormatPercent(kPercent); });

  stop_flag.store(true, std::memory_order_relaxed);
  for (auto& w : workers) {
    w->Join();
  }
}

#undef TEST_NUMBER_FORMAT

}  // namespace
}  // namespace base
