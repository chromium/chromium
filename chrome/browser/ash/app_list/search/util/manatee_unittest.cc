// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/manatee.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

// Test for successful cosine similarity calculation of two positive input
// vectors.
TEST(ManateeTest, GetWordSimilarityPositiveVectors) {
  std::vector<double> v1{1.0, 2.0, 3.0};
  std::vector<double> v2{4.0, 5.0, 6.0};
  std::optional<double> result = GetEmbeddingSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_NEAR(0.987316, result.value(), 1e-5);

  v1 = {0.01, 0.02, 0.03};
  v2 = {0.04, 0.05, 0.06};
  result = GetEmbeddingSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_NEAR(0.987316, result.value(), 1e-5);
}

// Test for correct handling of input vectors with mismatching size.
TEST(ManateeTest, GetWordSimilarityMismatchingSize) {
  std::vector<double> v1{1.0, 2.0, 3.0};
  std::vector<double> v2{4.0, 5.0, 6.0, 7.0};
  std::optional<double> result = GetEmbeddingSimilarity(v1, v2);
  ASSERT_FALSE(result.has_value());
}

// Test for correct handling of empty input vectors.
TEST(ManateeTest, GetWordSimilarityEmptyVectors) {
  std::vector<double> v1{};
  std::vector<double> v2{};
  std::optional<double> result = GetEmbeddingSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(0.0, result.value());
}

// Test for correct cosine similarity with a vector of all zero values.
TEST(ManateeTest, GetWordSimilarityZeroVectors) {
  std::vector<double> v1{1.0, 2.0, 3.0};
  std::vector<double> v2{0.0, 0.0, 0.0};
  std::optional<double> result = GetEmbeddingSimilarity(v1, v2);
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(0.0, result.value());
}

}  // namespace app_list::test
