// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag.h"

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_util.h"
#include "base/test/perf_time_logger.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base::i18n {
namespace {

// This performance test suite measures the speed of key operations on the
// LanguageTag class (such as construction, copying, and subtag retrieval)
// and compares them against manual std::string/std::string_view counterparts
// to ensure the class is efficient enough for high-frequency code paths.

// Prefix for all recorded performance test metrics.
constexpr std::string_view kMetricPrefixLanguageTag = "LanguageTag.";

// Throughput of copying a LanguageTag object (copies/ms).
constexpr std::string_view kCopyThroughput = "copy_throughput";

// The percentage of the difference between LanguageTag copy speed to
// std::string copy speed.
// Negative means that LanguageTag implementation is better.
constexpr std::string_view kCopyStringComparison = "copy_vs_string";

// Throughput of creating/parsing a LanguageTag object (tags/ms).
constexpr std::string_view kCreateThroughput = "create_throughput";

// The percentage of the difference between LanguageTag creation speed to
// std::string creation speed.
// Negative means that LanguageTag implementation is better.
constexpr std::string_view kCreateStringComparison = "create_vs_string";

// Throughput of extracting the language subtag using
// LanguageTag::language_subtag() (subtags/ms).
constexpr std::string_view kLanuageSubtagThroughput =
    "language_subtag_throughput";

// The percentage of the difference between LanguageTag::language_subtag() speed
// to manual std::string_view parsing.
// Negative means that LanguageTag implementation is better.
constexpr std::string_view kLanuageSubtagThroughputStringComparison =
    "language_subtag_vs_string";

// Throughput of extracting the region subtag using LanguageTag::region_subtag()
// (subtags/ms).
constexpr std::string_view kRegionSubtagThroughput = "region_subtag_throughput";

// The differecence (in percentage) of LanguageTag::region_subtag() speed
// compared to the speed of manual parsing of std::string_view.
// Negative means that LanguageTag implementation is better.
constexpr std::string_view kRegionSubtagThroughputStringComparison =
    "region_subtag_vs_string";

// Forces the compiler to treat the value of `val` as being read or written to
// memory, preventing optimization away of unused variables in loops.
template <typename T>
void DoNotOptimize(const T& val) {
  asm volatile("" : : "g"(&val) : "memory");
}

// Configures and returns a PerfResultReporter for a given test scenario
// ("story_name").
perf_test::PerfResultReporter SetUpReporter(std::string_view story_name) {
  return perf_test::PerfResultReporter(kMetricPrefixLanguageTag, story_name);
}

// Measures the time required to copy a LanguageTag object `iterations` times.
base::TimeDelta CopyTime(const LanguageTag& tag, size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    LanguageTag copy(tag);
    DoNotOptimize(copy);
  }
  return timer.Elapsed();
}

// Measures the creation/parsing time using a factory lambda `create_fn` over
// `iterations`.
template <typename F>
base::TimeDelta CreateTime(F create_fn, size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    DoNotOptimize(create_fn());
  }
  return timer.Elapsed();
}

// Measures the time required to copy a std::string baseline of identical
// content.
base::TimeDelta StringCopyTime(std::string_view str, size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    std::string copy(str);
    DoNotOptimize(copy);
  }
  return timer.Elapsed();
}

// Measures the speed of extracting the language subtag from a LanguageTag
// object.
base::TimeDelta GetLanguageSubtagTime(const LanguageTag& tag,
                                      size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    DoNotOptimize(tag.language_subtag());
  }
  return timer.Elapsed();
}

// Measures manual extraction of the language subtag from a raw string_view.
base::TimeDelta GetLanguageSubtagFromStringTime(std::string_view tag,
                                                size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    std::string language_subtag = std::string(tag.substr(0, tag.find('-')));
    DoNotOptimize(language_subtag);
  }
  return timer.Elapsed();
}

// Measures manual extraction of the region subtag from a raw string_view.
// This requires parsing the string sequentially, checking for the presence of
// an optional 4-letter script subtag first, then validating whether the next
// subtag qualifies as a 2-letter alpha or 3-digit numeric region code.
base::TimeDelta GetRegionSubtagFromStringTime(std::string_view tag,
                                              size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    std::string_view region;
    size_t start_pos = tag.find('-');
    if (start_pos != std::string_view::npos) {
      ++start_pos;
      size_t end_pos = tag.find('-', start_pos);
      std::string_view next_subtag = tag.substr(start_pos, end_pos - start_pos);
      if (next_subtag.size() == 4) {  // Script subtag
        if (end_pos != std::string_view::npos) {
          start_pos = end_pos + 1;
          end_pos = tag.find('-', start_pos);
          next_subtag = tag.substr(start_pos, end_pos - start_pos);
        } else {
          next_subtag = std::string_view();
        }
      }
      if ((next_subtag.size() == 2 &&
           std::ranges::all_of(next_subtag,
                               [](char c) { return base::IsAsciiAlpha(c); })) ||
          (next_subtag.size() == 3 && next_subtag[0] >= '0' &&
           next_subtag[0] <= '9')) {
        region = next_subtag;
      }
    }
    std::string region_subtag = std::string(region);
    DoNotOptimize(region_subtag);
  }
  return timer.Elapsed();
}

// Measures the speed of extracting the region subtag from a LanguageTag object.
base::TimeDelta GetRegionSubtagTime(const LanguageTag& tag, size_t iterations) {
  base::ElapsedTimer timer;
  for (size_t i = 0; i < iterations; ++i) {
    DoNotOptimize(tag.region_subtag());
  }
  return timer.Elapsed();
}

// Calculates the percentage of the difference between `t1` and `t2`: (t1-t2)/t2
float GetDeltaPercentage(base::TimeDelta t1, base::TimeDelta t2) {
  return 100 * (t1 - t2).InMillisecondsF() / t2.InMillisecondsF();
}

// Runs all individual benchmarks for a given tag structure, computes
// comparisons, and records the findings using the performance reporting
// infrastructure.
template <typename FLanguageTag, typename FString>
void RecordPerfMetrics(FLanguageTag create_tag,
                       FString create_string,
                       size_t iterations,
                       std::string_view story_name) {
  LanguageTag tag = create_tag();
  TimeDelta copy_time = CopyTime(tag, iterations);
  TimeDelta string_copy_time = StringCopyTime(tag.tag_string(), iterations);
  TimeDelta create_time = CreateTime(create_tag, iterations);
  TimeDelta create_string_time = CreateTime(create_string, iterations);
  TimeDelta language_subtag_time = GetLanguageSubtagTime(tag, iterations);
  TimeDelta language_subtag_string_time =
      GetLanguageSubtagFromStringTime(tag.tag_string(), iterations);
  TimeDelta region_subtag_time = GetRegionSubtagTime(tag, iterations);
  TimeDelta region_subtag_string_time =
      GetRegionSubtagFromStringTime(tag.tag_string(), iterations);

  perf_test::PerfResultReporter reporter = SetUpReporter(story_name);
  reporter.RegisterImportantMetric(kCopyThroughput, "copies/ms");
  reporter.RegisterImportantMetric(kCopyStringComparison, "%");
  reporter.RegisterImportantMetric(kCreateThroughput, "tags/ms");
  reporter.RegisterImportantMetric(kCreateStringComparison, "%");
  reporter.RegisterImportantMetric(kLanuageSubtagThroughput, "subtags/ms");
  reporter.RegisterImportantMetric(kRegionSubtagThroughput, "subtags/ms");
  reporter.RegisterImportantMetric(kLanuageSubtagThroughputStringComparison,
                                   "%");
  reporter.RegisterImportantMetric(kRegionSubtagThroughputStringComparison,
                                   "%");

  reporter.AddResult(kCopyThroughput, iterations / copy_time.InMillisecondsF());
  reporter.AddResult(kCopyStringComparison,
                     GetDeltaPercentage(copy_time, string_copy_time));
  reporter.AddResult(kCreateThroughput,
                     iterations / create_time.InMillisecondsF());
  reporter.AddResult(kCreateStringComparison,
                     GetDeltaPercentage(create_time, create_string_time));
  reporter.AddResult(kLanuageSubtagThroughput,
                     iterations / language_subtag_time.InMillisecondsF());
  reporter.AddResult(
      kLanuageSubtagThroughputStringComparison,
      GetDeltaPercentage(language_subtag_time, language_subtag_string_time));
  reporter.AddResult(kRegionSubtagThroughput,
                     iterations / region_subtag_time.InMillisecondsF());
  reporter.AddResult(
      kRegionSubtagThroughputStringComparison,
      GetDeltaPercentage(region_subtag_time, region_subtag_string_time));
}

// Performance benchmark for a simple, highly-optimized, and extremely common
// locale/language tag format ("en-US").
TEST(LanguageTagPerfTest, SmallTag) {
  constexpr int kIterations = 10'000'000;
  RecordPerfMetrics([] { return GetKnownLanguageTag("en-US"); },
                    [] { return std::string("en-US"); }, kIterations,
                    "small_tag");
}

// Performance benchmark for a complex, non-trivial BCP 47 language tag
// with script, variant, and multiple unicode extensions, exercising deep
// parsing logic in LanguageTag creation.
TEST(LanguageTagPerfTest, LargeTag) {
  constexpr int kIterations = 1'000'000;
  auto create_language_tag = [] {
    return *GetLanguageTagFromString("en-US-fonipa-u-ca-gregory-co-phonebk");
  };
  auto create_string = [] {
    return std::string("en-US-fonipa-u-ca-gregory-co-phonebk");
  };
  RecordPerfMetrics(create_language_tag, create_string, kIterations,
                    "large_tag");
}

}  // namespace
}  // namespace base::i18n
