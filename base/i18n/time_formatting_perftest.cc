// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include <string>
#include <string_view>

#include "base/i18n/timezone.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "third_party/icu/source/i18n/unicode/timezone.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace base {
namespace {

constexpr Time kBenchmarkTime = Time::FromSecondsSinceUnixEpoch(1700000000);

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

  perf_test::PerfResultReporter reporter("TimeFormatting.", story_name);
  reporter.RegisterImportantMetric("wall_time", "ns");
  reporter.AddResult(
      "wall_time",
      static_cast<double>(timer.Elapsed().InNanoseconds()) / iterations);
}

#define TEST_TIME_FORMAT(func)                                 \
  TEST(TimeFormattingPerfTest, func) {                         \
    RunAndRecord(#func, [&] { return func(kBenchmarkTime); }); \
  }

TEST_TIME_FORMAT(TimeFormatHTTP)
TEST_TIME_FORMAT(TimeFormatShortDateAndTime)
TEST_TIME_FORMAT(TimeFormatAsIso8601)
TEST_TIME_FORMAT(TimeFormatTimeOfDay)
TEST_TIME_FORMAT(TimeFormatTimeOfDayWithMilliseconds)
TEST_TIME_FORMAT(TimeFormatShortDate)
TEST_TIME_FORMAT(TimeFormatShortDateNumeric)
TEST_TIME_FORMAT(TimeFormatShortDateAndTimeWithTimeZone)
TEST_TIME_FORMAT(TimeFormatMonthAndYear)
TEST_TIME_FORMAT(TimeFormatFriendlyDateAndTime)
TEST_TIME_FORMAT(TimeFormatFriendlyDate)
TEST_TIME_FORMAT(TimeFormatUnix)

TEST(TimeFormattingPerfTest, TimeFormatTimeOfDayWithHourClockType) {
  RunAndRecord("TimeFormatTimeOfDayWithHourClockType", [&] {
    return TimeFormatTimeOfDayWithHourClockType(kBenchmarkTime, k12HourClock,
                                                kKeepAmPm);
  });
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(TimeFormattingPerfTest, TimeFormatMonthAndYearForTimeZone) {
  const icu::TimeZone* tz = icu::TimeZone::getGMT();
  RunAndRecord("TimeFormatMonthAndYearForTimeZone", [&] {
    return TimeFormatMonthAndYearForTimeZone(kBenchmarkTime, tz);
  });
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST(TimeFormattingPerfTest, TimeFormatAsIso8601WithTimeZone) {
  const i18n::TimeZone tz = i18n::TimeZone::FromString("America/Los_Angeles");
  RunAndRecord("TimeFormatAsIso8601WithTimeZone", [&] {
    return TimeFormatAsIso8601WithTimeZone(kBenchmarkTime, tz);
  });
}

#define TEST_TIME_DURATION_FORMAT(name, func, delta)       \
  TEST(TimeFormattingPerfTest, name) {                     \
    RunAndRecord(#name, [&] {                              \
      std::u16string result;                               \
      bool ok = func(delta, DURATION_WIDTH_WIDE, &result); \
      ::benchmark::DoNotOptimize(ok);                      \
      return result;                                       \
    });                                                    \
  }

TEST_TIME_DURATION_FORMAT(TimeDurationFormat,
                          TimeDurationFormat,
                          Minutes(15 * 60 + 42))
TEST_TIME_DURATION_FORMAT(TimeDurationFormatWithSeconds,
                          TimeDurationFormatWithSeconds,
                          Seconds(15 * 3600 + 42 * 60 + 30))
TEST_TIME_DURATION_FORMAT(TimeDurationCompactFormatWithSeconds,
                          TimeDurationCompactFormatWithSeconds,
                          Seconds(15 * 3600 + 42 * 60 + 30))

TEST(TimeFormattingPerfTest, GetHourClockType) {
  RunAndRecord("GetHourClockType", [&] { return GetHourClockType(); });
}

#undef TEST_TIME_DURATION_FORMAT
#undef TEST_TIME_FORMAT

}  // namespace
}  // namespace base
