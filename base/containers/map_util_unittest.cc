// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_
#define BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_

#include "base/containers/map_util.h"

#include <memory>
#include <string>
#include <type_traits>

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
using StringToConstStringPtrMap =
    base::flat_map<std::string, const std::string*>;
using StringToStringUniquePtrMap =
    base::flat_map<std::string, std::unique_ptr<std::string>>;
using StringToConstStringUniquePtrMap =
    base::flat_map<std::string, std::unique_ptr<const std::string>>;

TEST(MapUtilTest, FindOrNull) {
  StringToStringMap mapping({{kKey, kValue}});

  EXPECT_THAT(FindOrNull(mapping, kKey), Pointee(Eq(kValue)));
  EXPECT_EQ(FindOrNull(mapping, kMissingKey), nullptr);

  // The following should be able to infer the type of the key from the map's
  // type.
  base::flat_map<std::pair<int, std::string>, std::string> pair_mapping;
  EXPECT_EQ(FindOrNull(pair_mapping, {3, "foo"}), nullptr);

  // Homogeneous keys are supported.
  std::pair<int, std::string> homogeneous_key(3, "bar");
  EXPECT_EQ(FindOrNull(pair_mapping, homogeneous_key), nullptr);

  // Heterogenous keys are supported.
  std::pair<int, const char*> heterogenous_key(3, "bar");
  EXPECT_EQ(FindOrNull(pair_mapping, heterogenous_key), nullptr);
}

TEST(MapUtilTest, FindPtrOrNullForPointers) {
  std::string val(kValue);
  StringToStringPtrMap mapping({{kKey, &val}});

  EXPECT_EQ(FindPtrOrNull(mapping, kKey), &val);
  EXPECT_EQ(FindPtrOrNull(mapping, kMissingKey), nullptr);

  // The following should be able to infer the type of the key from the map's
  // type.
  base::flat_map<std::pair<int, std::string>, std::string*> pair_mapping;
  EXPECT_EQ(FindPtrOrNull(pair_mapping, {3, "foo"}), nullptr);
}

// Homogeneous keys are supported.
static_assert(
    std::is_same_v<
        std::string*,
        std::invoke_result_t<
            decltype(FindPtrOrNull<base::flat_map<std::pair<int, std::string>,
                                                  std::string*>>),
            base::flat_map<std::pair<int, std::string>, std::string*>&,
            std::pair<int, std::string>>>);

// Heterogenous keys are supported.
static_assert(
    std::is_same_v<
        std::string*,
        std::invoke_result_t<
            decltype(FindPtrOrNull<base::flat_map<std::pair<int, std::string>,
                                                  std::string*>>),
            base::flat_map<std::pair<int, std::string>, std::string*>&,
            std::pair<int, const char*>>>);

TEST(MapUtilTest, FindPtrOrNullForPointerLikeValues) {
  StringToStringUniquePtrMap mapping;
  mapping.insert({kKey, std::make_unique<std::string>(kValue)});

  EXPECT_THAT(FindPtrOrNull(mapping, kKey), Pointee(Eq(kValue)));
  EXPECT_EQ(FindPtrOrNull(mapping, kMissingKey), nullptr);
}

// FindPtrOrNull const-correctness:
// Mutable raw pointers:
static_assert(
    std::is_same_v<
        std::string*,
        std::invoke_result_t<decltype(FindPtrOrNull<StringToStringPtrMap>),
                             StringToStringPtrMap&,
                             std::string>>);
static_assert(
    std::is_same_v<
        std::string*,
        std::invoke_result_t<decltype(FindPtrOrNull<StringToStringPtrMap>),
                             const StringToStringPtrMap&,
                             std::string>>);
// Const raw pointers:
static_assert(
    std::is_same_v<
        const std::string*,
        std::invoke_result_t<decltype(FindPtrOrNull<StringToConstStringPtrMap>),
                             StringToConstStringPtrMap&,
                             std::string>>);
static_assert(
    std::is_same_v<
        const std::string*,
        std::invoke_result_t<decltype(FindPtrOrNull<StringToConstStringPtrMap>),
                             const StringToConstStringPtrMap&,
                             std::string>>);
// Mutable smart pointers:
static_assert(
    std::is_same_v<std::string*,
                   std::invoke_result_t<
                       decltype(FindPtrOrNull<StringToStringUniquePtrMap>),
                       StringToStringUniquePtrMap&,
                       std::string>>);
static_assert(
    std::is_same_v<std::string*,
                   std::invoke_result_t<
                       decltype(FindPtrOrNull<StringToStringUniquePtrMap>),
                       const StringToStringUniquePtrMap&,
                       std::string>>);
// Const smart pointers:
static_assert(
    std::is_same_v<const std::string*,
                   std::invoke_result_t<
                       decltype(FindPtrOrNull<StringToConstStringUniquePtrMap>),
                       StringToConstStringUniquePtrMap&,
                       std::string>>);
static_assert(
    std::is_same_v<const std::string*,
                   std::invoke_result_t<
                       decltype(FindPtrOrNull<StringToConstStringUniquePtrMap>),
                       const StringToConstStringUniquePtrMap&,
                       std::string>>);

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

TEST(MapUtilTest, InsertOrAssignInferKeyType) {
  // The following should be able to infer the type of the key from the map's
  // type.
  base::flat_map<std::pair<int, std::string>, std::string> pair_mapping;
  auto it = InsertOrAssign(pair_mapping, {3, "foo"}, "bar");
  ASSERT_NE(it, pair_mapping.end());
  EXPECT_EQ(it->second, "bar");
}

}  // namespace

}  // namespace base

#endif  // BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_
