// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/substring_set_matcher/substring_set_matcher.h"

#include <limits>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_usage_estimator.h"  // no-presubmit-check
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {

namespace {

// Returns a random string of the given length using characters from 'a' to 'z'.
std::string GetRandomString(size_t len) {
  std::vector<char> random_chars;
  random_chars.reserve(len);
  for (size_t i = 0; i < len; i++)
    random_chars.push_back(base::RandInt('a', 'z'));

  return std::string(random_chars.begin(), random_chars.end());
}

// Tests performance of SubstringSetMatcher for 20000 random patterns of length
// 30.
TEST(SubstringSetMatcherPerfTest, RandomKeys) {
  std::vector<MatcherStringPattern> patterns;
  std::set<std::string> pattern_strings;

  // Create patterns.
  const size_t kNumPatterns = 20000;
  const size_t kPatternLen = 30;
  for (size_t i = 0; i < kNumPatterns; i++) {
    std::string str = GetRandomString(kPatternLen);

    // Ensure we don't have any duplicate pattern strings.
    if (base::Contains(pattern_strings, str))
      continue;

    pattern_strings.insert(str);
    patterns.emplace_back(str, i);
  }

  base::ElapsedTimer init_timer;

  // Allocate SubstringSetMatcher on the heap so that EstimateMemoryUsage below
  // also includes its stack allocated memory.
  auto matcher = std::make_unique<SubstringSetMatcher>();
  ASSERT_TRUE(matcher->Build(patterns));
  base::TimeDelta init_time = init_timer.Elapsed();

  // Match patterns against a random string of 500 characters.
  const size_t kTextLen = 500;
  base::ElapsedTimer match_timer;
  std::set<MatcherStringPattern::ID> matches;
  matcher->Match(GetRandomString(kTextLen), &matches);
  base::TimeDelta match_time = match_timer.Elapsed();

  const char* kInitializationTime = ".init_time";
  const char* kMatchTime = ".match_time";
  const char* kMemoryUsage = ".memory_usage";
  auto reporter =
      perf_test::PerfResultReporter("SubstringSetMatcher", "RandomKeys");
  reporter.RegisterImportantMetric(kInitializationTime, "us");
  reporter.RegisterImportantMetric(kMatchTime, "us");
  reporter.RegisterImportantMetric(kMemoryUsage, "Mb");

  reporter.AddResult(kInitializationTime, init_time);
  reporter.AddResult(kMatchTime, match_time);
  reporter.AddResult(
      kMemoryUsage,
      (base::trace_event::EstimateMemoryUsage(matcher) * 1.0 / (1 << 20)));
}

}  // namespace

}  // namespace base
