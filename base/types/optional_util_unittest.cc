// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/optional_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(OptionalUtilTest, OptionalToPtr) {
  std::optional<float> optional;
  EXPECT_EQ(nullptr, OptionalToPtr(optional));

  optional = 0.1f;
  EXPECT_EQ(&optional.value(), OptionalToPtr(optional));
  EXPECT_NE(nullptr, OptionalToPtr(optional));
}

TEST(OptionalUtilTest, OptionalFromPtr) {
  float* f_ptr = nullptr;
  EXPECT_EQ(std::nullopt, OptionalFromPtr(f_ptr));

  float f = 0.1f;
  std::optional<float> optional_f(f);
  EXPECT_EQ(optional_f, OptionalFromPtr(&f));
}

}  // namespace
}  // namespace base
