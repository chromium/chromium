// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/manatee.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

TEST(ManateeTest, GetWordSimilarityPositiveVectors) {
  // Test for successful dot product calculation of two
  // positive input vectors

  std::vector<double> v1{1.0, 2.0, 3.0};
  std::vector<double> v2{4.0, 5.0, 6.0};
  absl::optional<double> result = GetWordSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(32.0, result.value());
}

TEST(ManateeTest, GetWordSimilarityMismatchingSize) {
  // Test for correct handling of input vectors
  // with mismatching size

  std::vector<double> v1{1.0, 2.0, 3.0};
  std::vector<double> v2{4.0, 5.0, 6.0, 7.0};
  absl::optional<double> result = GetWordSimilarity(v1, v2);
  ASSERT_FALSE(result.has_value());
}

TEST(ManateeTest, GetWordSimilarityEmptyVectors) {
  // Test for correct handling of empty input vectors

  std::vector<double> v1{};
  std::vector<double> v2{};
  absl::optional<double> result = GetWordSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(0.0, result.value());
}

TEST(ManateeTest, GetWordSimilarityZeroVectors) {
  // Test for correct dot product with a vector of
  // all zero values

  std::vector<double> v1{1.0, 2.0, 3.0};
  std::vector<double> v2{0.0, 0.0, 0.0};
  absl::optional<double> result = GetWordSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(0.0, result.value());
}

}  // namespace app_list::test
