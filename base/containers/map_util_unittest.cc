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

}  // namespace

}  // namespace base

#endif  // BASE_CONTAINERS_MAP_UTIL_UNITTEST_CC_
