// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/as_const.h"

#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(AsConstTest, AsConst) {
  int i = 123;
  EXPECT_EQ(&i, &base::as_const(i));
  static_assert(std::is_same<const int&, decltype(base::as_const(i))>::value,
                "Error: base::as_const() returns an unexpected type");

  const int ci = 456;
  static_assert(&ci == &base::as_const(ci),
                "Error: base::as_const() returns an unexpected reference");
  static_assert(std::is_same<const int&, decltype(base::as_const(ci))>::value,
                "Error: base::as_const() returns an unexpected type");
}

}  // namespace
}  // namespace base
