// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_
#define BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_

#include "base/containers/map_util.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using testing::AllOf;
using testing::Eq;
using testing::Pointee;

constexpr char kKey[] = "key";
constexpr char kValue[] = "value";
constexpr char kMissingKey[] = "missing_key";

using StringToStringMap = base::flat_map<std::string, std::string>;
using StringToStringPtrMap = base::flat_map<std::string, std::string*>;
using StringToStringUniquePtrMap =
    base::flat_map<std::string, std::unique_ptr<std::string>>;

TEST(MapUtilTest, FindOrNull) {
  StringToStringMap mapping({{kKey, kValue}});

  EXPECT_THAT(FindOrNull(mapping, kKey), Pointee(Eq(kValue)));
  EXPECT_EQ(FindOrNull(mapping, kMissingKey), nullptr);
}

TEST(MapUtilTest, FindPtrOrNullForPointers) {
  auto val = std::make_unique<std::string>(kValue);

  StringToStringPtrMap mapping({{kKey, val.get()}});

  EXPECT_THAT(FindPtrOrNull(mapping, kKey),
              AllOf(Eq(val.get()), Pointee(Eq(kValue))));
  EXPECT_EQ(FindPtrOrNull(mapping, kMissingKey), nullptr);
}

TEST(MapUtilTest, FindPtrOrNullForPointerLikeValues) {
  StringToStringUniquePtrMap mapping;
  mapping.insert({kKey, std::make_unique<std::string>(kValue)});

  EXPECT_THAT(FindPtrOrNull(mapping, kKey), Pointee(Eq(kValue)));
  EXPECT_EQ(FindPtrOrNull(mapping, kMissingKey), nullptr);
}

struct LeftVsRightValue {
  enum RefType {
    UNKNOWN,
    EMPLACED,
    LVALUE,
    RVALUE,
  };

  explicit LeftVsRightValue(int n) : ref_type(EMPLACED), value(n) {}
  LeftVsRightValue(LeftVsRightValue&& n) : ref_type(RVALUE), value(n.value) {}
  LeftVsRightValue(const LeftVsRightValue& n)
      : ref_type(LVALUE), value(n.value) {}

  LeftVsRightValue& operator=(LeftVsRightValue&& n) {
    ref_type = RVALUE;
    value = n.value;
    return *this;
  }

  LeftVsRightValue& operator=(const LeftVsRightValue& n) {
    ref_type = LVALUE;
    value = n.value;
    return *this;
  }

  RefType ref_type = UNKNOWN;
  int value = 0;
};

TEST(MapUtilTest, InsertOrAssign) {
  using StringToValueMap = std::map<std::string, LeftVsRightValue, std::less<>>;
  StringToValueMap map;

  // Heterogenous keys - all of types comparable with std::string.
  const char key1[] = "This is key 1. It is very long";
  std::string_view key2 = "This is key 2. It is also very long";
  std::string key3 = "This is key 3. It, like keys 1 and 2, is long";

  // Insert a new key with a value that is an implicit rvalue.
  auto it = InsertOrAssign(map, key1, LeftVsRightValue(1));
  EXPECT_EQ(it->second.ref_type, LeftVsRightValue::RVALUE);
  EXPECT_EQ(it->second.value, 1);

  // Update that key with a value that is an implicit rvalue.
  it = InsertOrAssign(map, key1, LeftVsRightValue(2));
  EXPECT_EQ(it->second.ref_type, LeftVsRightValue::RVALUE);
  EXPECT_EQ(it->second.value, 2);

  // Insert new key with a value that is an explicit rvalue.
  LeftVsRightValue v3(3);
  it = InsertOrAssign(map, key2, std::move(v3));
  EXPECT_EQ(it->second.ref_type, LeftVsRightValue::RVALUE);
  EXPECT_EQ(it->second.value, 3);

  // Update that key with a value that is an explicit rvalue.
  LeftVsRightValue v4(4);
  it = InsertOrAssign(map, key2, std::move(v4));
  EXPECT_EQ(it->second.ref_type, LeftVsRightValue::RVALUE);
  EXPECT_EQ(it->second.value, 4);

  // Insert new key with a value that is an lvalue.
  LeftVsRightValue v5(5);
  it = InsertOrAssign(map, key3, v5);
  EXPECT_EQ(it->second.ref_type, LeftVsRightValue::LVALUE);
  EXPECT_EQ(it->second.value, 5);

  // Update that key with a value that is an lvalue.
  LeftVsRightValue v6(6);
  it = InsertOrAssign(map, key3, v6);
  EXPECT_EQ(it->second.ref_type, LeftVsRightValue::LVALUE);
  EXPECT_EQ(it->second.value, 6);
}

}  // namespace

}  // namespace base

#endif  // BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_
