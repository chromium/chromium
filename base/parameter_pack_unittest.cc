// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/parameter_pack.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ParameterPack, AnyOf) {
  static_assert(any_of({true, true, true}), "");
  static_assert(any_of({false, false, true, false}), "");
  static_assert(!any_of({false}), "");
  static_assert(!any_of({false, false, false}), "");
}

TEST(ParameterPack, AllOf) {
  static_assert(all_of({true, true, true}), "");
  static_assert(!all_of({true, true, true, false}), "");
  static_assert(!all_of({false}), "");
  static_assert(!all_of({false, false}), "");
}

TEST(ParameterPack, Count) {
  static_assert(count({1, 2, 2, 2, 2, 2, 3}, 2) == 5u, "");
}

TEST(ParameterPack, HasType) {
  static_assert(ParameterPack<int, float, bool>::HasType<int>(), "");
  static_assert(ParameterPack<int, float, bool>::HasType<bool>(), "");
  static_assert(ParameterPack<int, float, bool>::HasType<bool>(), "");
  static_assert(!ParameterPack<int, float, bool>::HasType<void*>(), "");
}

TEST(ParameterPack, OnlyHasType) {
  static_assert(ParameterPack<int, int>::OnlyHasType<int>(), "");
  static_assert(ParameterPack<int, int, int, int>::OnlyHasType<int>(), "");
  static_assert(!ParameterPack<int, bool>::OnlyHasType<int>(), "");
  static_assert(!ParameterPack<int, int, bool, int>::OnlyHasType<int>(), "");
  static_assert(!ParameterPack<int, int, int>::OnlyHasType<bool>(), "");
}

TEST(ParameterPack, IsUniqueInPack) {
  static_assert(ParameterPack<int, float, bool>::IsUniqueInPack<int>(), "");
  static_assert(!ParameterPack<int, int, bool>::IsUniqueInPack<int>(), "");
}

TEST(ParameterPack, IndexInPack) {
  static_assert(ParameterPack<int, float, bool>::IndexInPack<int>() == 0u, "");
  static_assert(ParameterPack<int, float, bool>::IndexInPack<float>() == 1u,
                "");
  static_assert(ParameterPack<int, float, bool>::IndexInPack<bool>() == 2u, "");
  static_assert(
      ParameterPack<int, float, bool>::IndexInPack<void*>() == pack_npos, "");
}

TEST(ParameterPack, NthType) {
  static_assert(
      std::is_same_v<int, ParameterPack<int, float, bool>::NthType<0>>, "");
  static_assert(
      std::is_same_v<float, ParameterPack<int, float, bool>::NthType<1>>, "");
  static_assert(
      std::is_same_v<bool, ParameterPack<int, float, bool>::NthType<2>>, "");
}

TEST(ParameterPack, IsAllSameType) {
  static_assert(ParameterPack<int>::IsAllSameType(), "");
  static_assert(ParameterPack<int, int, int>::IsAllSameType(), "");
  static_assert(!ParameterPack<int, int, int, int, bool>::IsAllSameType(), "");
}

}  // namespace base
