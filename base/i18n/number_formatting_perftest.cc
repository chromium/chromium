// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"

#include <stdint.h>

#include <string>
#include <string_view>

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

#undef TEST_NUMBER_FORMAT

}  // namespace
}  // namespace base
