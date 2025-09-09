// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/variant_map.h"

#include "base/containers/contains.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using KeyType = int64_t;
using ValueType = std::string;

constexpr KeyType kTestKey = 4;
constexpr KeyType kUnusedKey = 8;
const ValueType kTestValue = "TEST";

namespace {}  // namespace

class VariantMapTest : public ::testing::Test,
                       public ::testing::WithParamInterface<MapType> {};

TEST_P(VariantMapTest, Construction) {
  VariantMap<KeyType, ValueType> map(GetParam());
  EXPECT_EQ(map.size(), 0);
}

TEST_P(VariantMapTest, Insertion) {
  VariantMap<KeyType, ValueType> map(GetParam());

  map[kTestKey] = kTestValue;
  EXPECT_EQ(map.size(), 1);

  EXPECT_EQ(map[kTestKey], kTestValue);
}

TEST_P(VariantMapTest, Find) {
  VariantMap<KeyType, ValueType> map(GetParam());
  map[kTestKey] = kTestValue;

  EXPECT_NE(map.find(kTestKey), map.end());
  EXPECT_EQ(map.find(kUnusedKey), map.end());
}

TEST_P(VariantMapTest, Iteration) {
  VariantMap<KeyType, ValueType> map(GetParam());

  int64_t kCount = 10;
  for (int64_t i = 0; i < kCount; ++i) {
    map[i] = kTestValue;
  }

  int64_t iteration_count = 0;
  for (auto it = map.begin(); it != map.end(); ++it) {
    iteration_count++;
  }

  EXPECT_EQ(kCount, iteration_count);
}

INSTANTIATE_TEST_SUITE_P(All,
                         VariantMapTest,
                         testing::Values(MapType::kStdMap,
                                         MapType::kFlatHashMap));

}  // namespace base
