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
using ValueType = const char*;

constexpr KeyType kTestKey = 4;
constexpr ValueType kTestValue = "TEST";
constexpr KeyType kTestKey2 = 8;
constexpr ValueType kTestValue2 = "OTHER";
static_assert(kTestKey != kTestKey2, "Would not exercise maps correctly");
static_assert(kTestValue != kTestValue2, "Would not exercise maps correctly");

constexpr KeyType kUnusedKey = 8;

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

  map.insert({kTestKey2, kTestValue2});
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map[kTestKey2], kTestValue2);
}

TEST_P(VariantMapTest, At) {
  VariantMap<KeyType, ValueType> map(GetParam());

  // at() returns a reference to the value, but only if it already exists in the
  // map.
  map[kTestKey] = kTestValue;
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.at(kTestKey), kTestValue);
  map.at(kTestKey) = kTestValue2;

  // Force the const override of at().
  const auto& const_map = map;
  EXPECT_EQ(const_map.at(kTestKey), kTestValue2);
}

TEST_P(VariantMapTest, Empty) {
  VariantMap<KeyType, ValueType> map(GetParam());
  EXPECT_TRUE(map.empty());
  map[kTestKey] = kTestValue;
  EXPECT_FALSE(map.empty());
}

TEST_P(VariantMapTest, Clear) {
  VariantMap<KeyType, ValueType> map(GetParam());
  map[kTestKey] = kTestValue;
  EXPECT_FALSE(map.empty());
  map.clear();
  EXPECT_TRUE(map.empty());
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
