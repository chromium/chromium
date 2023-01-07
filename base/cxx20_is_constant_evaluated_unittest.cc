// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cxx20_is_constant_evaluated.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(Cxx20IsConstantEvaluated, Basic) {
  static_assert(is_constant_evaluated(), "");
  EXPECT_FALSE(is_constant_evaluated());
}

}  // namespace base
