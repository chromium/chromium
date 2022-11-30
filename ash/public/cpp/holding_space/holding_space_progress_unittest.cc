// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_progress.h"

#include <limits>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using HoldingSpaceProgressTest = testing::Test;

// Verifies that `HoldingSpaceProgress()` is WAI.
TEST_F(HoldingSpaceProgressTest, DefaultConstructor) {
  HoldingSpaceProgress progress;
  EXPECT_EQ(progress.GetValue(), 1.f);
  EXPECT_TRUE(progress.IsComplete());
  EXPECT_FALSE(progress.IsIndeterminate());
}

// Verifies that `HoldingSpaceProgress(...)` is WAI.
TEST_F(HoldingSpaceProgressTest, ExplicitConstructor) {
  {
    HoldingSpaceProgress progress(/*current_bytes=*/0, /*total_bytes=*/0);
    EXPECT_EQ(progress.GetValue(), 1.f);
    EXPECT_TRUE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/0, /*total_bytes=*/0,
                                  /*complete=*/false);
    EXPECT_EQ(progress.GetValue(), 1.f - std::numeric_limits<float>::epsilon());
    EXPECT_FALSE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/0, /*total_bytes=*/0,
                                  /*complete=*/true);
    EXPECT_EQ(progress.GetValue(), 1.f);
    EXPECT_TRUE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/50, /*total_bytes=*/100);
    EXPECT_EQ(progress.GetValue(), 0.5f);
    EXPECT_FALSE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/50, /*total_bytes=*/100,
                                  /*complete=*/false);
    EXPECT_EQ(progress.GetValue(), 0.5f);
    EXPECT_FALSE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100);
    EXPECT_EQ(progress.GetValue(), 1.f);
    EXPECT_TRUE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100,
                                  /*complete=*/false);
    EXPECT_EQ(progress.GetValue(), 1.f - std::numeric_limits<float>::epsilon());
    EXPECT_FALSE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100,
                                  /*complete=*/true);
    EXPECT_EQ(progress.GetValue(), 1.f);
    EXPECT_TRUE(progress.IsComplete());
    EXPECT_FALSE(progress.IsIndeterminate());
  }
}

// Verifies that `HoldingSpaceProgress(const HoldingSpaceProgress&)` is WAI.
TEST_F(HoldingSpaceProgressTest, CopyConstructor) {
  {
    HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100);

    HoldingSpaceProgress copy(progress);
    EXPECT_EQ(copy.GetValue(), 1.f);
    EXPECT_TRUE(copy.IsComplete());
    EXPECT_FALSE(copy.IsIndeterminate());

    EXPECT_EQ(progress, copy);
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100,
                                  /*complete=*/false);

    HoldingSpaceProgress copy(progress);
    EXPECT_EQ(copy.GetValue(), 1.f - std::numeric_limits<float>::epsilon());
    EXPECT_FALSE(copy.IsComplete());
    EXPECT_FALSE(copy.IsIndeterminate());

    EXPECT_EQ(progress, copy);
  }
  {
    HoldingSpaceProgress progress(/*current_bytes=*/100, /*total_bytes=*/100,
                                  /*complete=*/true);

    HoldingSpaceProgress copy(progress);
    EXPECT_EQ(copy.GetValue(), 1.f);
    EXPECT_TRUE(copy.IsComplete());
    EXPECT_FALSE(copy.IsIndeterminate());

    EXPECT_EQ(progress, copy);
  }
}

// Verifies that the `+` operator is WAI.
TEST_F(HoldingSpaceProgressTest, PlusOperator) {
  struct TestCase {
    HoldingSpaceProgress lhs;
    HoldingSpaceProgress rhs;
    HoldingSpaceProgress expected_result;
  };

  std::vector<TestCase> test_cases(
      {{HoldingSpaceProgress(), HoldingSpaceProgress(), HoldingSpaceProgress()},
       {HoldingSpaceProgress(), HoldingSpaceProgress(absl::nullopt, 0),
        HoldingSpaceProgress(absl::nullopt, 0)},
       {HoldingSpaceProgress(), HoldingSpaceProgress(0, absl::nullopt),
        HoldingSpaceProgress(0, absl::nullopt)},
       {HoldingSpaceProgress(absl::nullopt, 1),
        HoldingSpaceProgress(absl::nullopt, 1),
        HoldingSpaceProgress(absl::nullopt, 2)},
       {HoldingSpaceProgress(1, absl::nullopt),
        HoldingSpaceProgress(1, absl::nullopt),
        HoldingSpaceProgress(2, absl::nullopt)},
       {HoldingSpaceProgress(50, 100), HoldingSpaceProgress(50, 100),
        HoldingSpaceProgress(100, 200)},
       {HoldingSpaceProgress(100, 100), HoldingSpaceProgress(100, 100),
        HoldingSpaceProgress(200, 200)},
       {HoldingSpaceProgress(100, 100, true),
        HoldingSpaceProgress(100, 100, false),
        HoldingSpaceProgress(200, 200, false)}});

  for (const auto& test_case : test_cases)
    EXPECT_EQ(test_case.lhs + test_case.rhs, test_case.expected_result);
}

// Wraps a test case that asserts an `expected_result` for a given `progress`.
template <typename T>
struct TestCase {
  HoldingSpaceProgress progress;
  T expected_result;
};

// Verifies that `HoldingSpaceProgress::GetValue()` is WAI.
TEST_F(HoldingSpaceProgressTest, GetValue) {
  std::vector<TestCase<absl::optional<float>>> test_cases(
      {{HoldingSpaceProgress(), 1.f},
       {HoldingSpaceProgress(0, 0), 1.f},
       {HoldingSpaceProgress(absl::nullopt, absl::nullopt), absl::nullopt},
       {HoldingSpaceProgress(absl::nullopt, 0), absl::nullopt},
       {HoldingSpaceProgress(0, absl::nullopt), absl::nullopt},
       {HoldingSpaceProgress(50, 100), 0.5f},
       {HoldingSpaceProgress(100, 100), 1.f},
       {HoldingSpaceProgress(100, 100, true), 1.f},
       {HoldingSpaceProgress(100, 100, false),
        1.f - std::numeric_limits<float>::epsilon()}});

  for (const auto& test_case : test_cases)
    EXPECT_EQ(test_case.progress.GetValue(), test_case.expected_result);
}

// Verifies that `HoldingSpaceProgress::IsComplete()` is WAI.
TEST_F(HoldingSpaceProgressTest, IsComplete) {
  std::vector<TestCase<bool>> test_cases({
      {HoldingSpaceProgress(), true},
      {HoldingSpaceProgress(0, 0), true},
      {HoldingSpaceProgress(absl::nullopt, absl::nullopt), false},
      {HoldingSpaceProgress(absl::nullopt, 0), false},
      {HoldingSpaceProgress(0, absl::nullopt), false},
      {HoldingSpaceProgress(50, 100), false},
      {HoldingSpaceProgress(100, 100), true},
      {HoldingSpaceProgress(100, 100, true), true},
      {HoldingSpaceProgress(100, 100, false), false},
  });

  for (const auto& test_case : test_cases)
    EXPECT_EQ(test_case.progress.IsComplete(), test_case.expected_result);
}

// Verifies that `HoldingSpaceProgress::IsIndeterminate()` is WAI.
TEST_F(HoldingSpaceProgressTest, IsIndeterminate) {
  std::vector<TestCase<bool>> test_cases({
      {HoldingSpaceProgress(), false},
      {HoldingSpaceProgress(0, 0), false},
      {HoldingSpaceProgress(absl::nullopt, absl::nullopt), true},
      {HoldingSpaceProgress(absl::nullopt, 0), true},
      {HoldingSpaceProgress(0, absl::nullopt), true},
      {HoldingSpaceProgress(50, 100), false},
      {HoldingSpaceProgress(100, 100), false},
      {HoldingSpaceProgress(100, 100, true), false},
      {HoldingSpaceProgress(100, 100, false), false},
  });

  for (const auto& test_case : test_cases)
    EXPECT_EQ(test_case.progress.IsIndeterminate(), test_case.expected_result);
}

}  // namespace ash
