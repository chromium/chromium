// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ScopedHistogramTimer, ThreeTimersOneScope) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("TestShortTimer0");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("TestShortTimer1");
  SCOPED_UMA_HISTOGRAM_TIMER("TestTimer0");
  SCOPED_UMA_HISTOGRAM_TIMER("TestTimer1");
  SCOPED_UMA_HISTOGRAM_LONG_TIMER("TestLongTimer0");
  SCOPED_UMA_HISTOGRAM_LONG_TIMER("TestLongTimer1");
}

// Compile tests for UMA_HISTOGRAM_ENUMERATION with the three different types it
// accepts:
// - integral types
// - unscoped enums
// - scoped enums
TEST(HistogramMacro, IntegralPseudoEnumeration) {
  UMA_HISTOGRAM_ENUMERATION("Test.FauxEnumeration", 1, 1000);
}

TEST(HistogramMacro, UnscopedEnumeration) {
  enum TestEnum : char {
    FIRST_VALUE,
    SECOND_VALUE,
    THIRD_VALUE,
    MAX_ENTRIES,
  };
  UMA_HISTOGRAM_ENUMERATION("Test.UnscopedEnumeration", SECOND_VALUE,
                            MAX_ENTRIES);
}

TEST(HistogramMacro, ScopedEnumeration) {
  enum class TestEnum {
    FIRST_VALUE,
    SECOND_VALUE,
    THIRD_VALUE,
    kMaxValue = THIRD_VALUE,
  };
  UMA_HISTOGRAM_ENUMERATION("Test.ScopedEnumeration", TestEnum::FIRST_VALUE);

  enum class TestEnum2 {
    FIRST_VALUE,
    SECOND_VALUE,
    THIRD_VALUE,
    MAX_ENTRIES,
  };
  UMA_HISTOGRAM_ENUMERATION("Test.ScopedEnumeration2", TestEnum2::SECOND_VALUE,
                            TestEnum2::MAX_ENTRIES);
}

// Compile tests for UMA_HISTOGRAM_ENUMERATION when the value type is:
// - a const reference to an enum
// - a non-const reference to an enum
TEST(HistogramMacro, EnumerationConstRef) {
  enum class TestEnum { kValue, kMaxValue = kValue };
  const TestEnum& value_ref = TestEnum::kValue;
  UMA_HISTOGRAM_ENUMERATION("Test.ScopedEnumeration3", value_ref);
}

TEST(HistogramMacro, EnumerationNonConstRef) {
  enum class TestEnum { kValue, kMaxValue = kValue };
  TestEnum value = TestEnum::kValue;
  TestEnum& value_ref = value;
  UMA_HISTOGRAM_ENUMERATION("Test.ScopedEnumeration4", value_ref);
}

TEST(HistogramMacro, SplitByProcessPriorityMacro) {
  TimeTicks mock_now = TimeTicks::Now();

  constexpr TimeDelta kMockIntervalBetweenSamples = Seconds(1);

  {
    // No BestEffort suffix by default (SetSharedLastForegroundTimeForMetrics
    // not invoked yet in this process).
    HistogramTester tester;
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(UMA_HISTOGRAM_COUNTS_1000, mock_now,
                                            Microseconds(0), "Test.MyCount",
                                            123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 1);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 0);
  }

  mock_now += kMockIntervalBetweenSamples;
  std::atomic<base::TimeTicks> shared_last_foreground_time = mock_now;
  internal::SetSharedLastForegroundTimeForMetrics(&shared_last_foreground_time);

  {
    // No BestEffort suffix while foreground.
    HistogramTester tester;
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(UMA_HISTOGRAM_COUNTS_1000, mock_now,
                                            Microseconds(0), "Test.MyCount",
                                            123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 1);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 0);
  }

  mock_now += kMockIntervalBetweenSamples;
  shared_last_foreground_time.store(TimeTicks(), std::memory_order_relaxed);

  {
    // BestEffort suffix while in BestEffort.
    HistogramTester tester;
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(UMA_HISTOGRAM_COUNTS_1000, mock_now,
                                            Microseconds(0), "Test.MyCount",
                                            123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 0);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 1);
  }

  mock_now += kMockIntervalBetweenSamples;
  shared_last_foreground_time.store(mock_now, std::memory_order_relaxed);

  {
    // No BestEffort suffix once back to normal priority.
    HistogramTester tester;
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(UMA_HISTOGRAM_COUNTS_1000, mock_now,
                                            Microseconds(0), "Test.MyCount",
                                            123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 1);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 0);
  }
  {
    // BestEffort suffix while normal priority if the sample_interval overlaps
    // into a BestEffort range.
    HistogramTester tester;
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(UMA_HISTOGRAM_COUNTS_1000, mock_now,
                                            kMockIntervalBetweenSamples * 1.5,
                                            "Test.MyCount", 123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 0);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 1);
  }
  {
    HistogramTester tester;
    // ... and also if the sample was taken at a time before the last foreground
    // time:
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(
        UMA_HISTOGRAM_COUNTS_1000, mock_now - kMockIntervalBetweenSamples * 1.5,
        Microseconds(0), "Test.MyCount", 123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 0);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 1);
  }

  // Reset the global state at the end of the test.
  internal::SetSharedLastForegroundTimeForMetrics(nullptr);

  {
    // No BestEffort suffix by default, again.
    HistogramTester tester;
    UMA_HISTOGRAM_SPLIT_BY_PROCESS_PRIORITY(UMA_HISTOGRAM_COUNTS_1000, mock_now,
                                            Microseconds(0), "Test.MyCount",
                                            123);
    tester.ExpectUniqueSample("Test.MyCount", 123, 1);
    tester.ExpectUniqueSample("Test.MyCount.BestEffort", 123, 0);
  }
}

}  // namespace base
