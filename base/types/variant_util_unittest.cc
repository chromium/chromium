// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/variant_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
namespace {

TEST(VariantUtilTest, IndexOfType) {
  using TestType = absl::variant<bool, int, double>;

  static_assert(VariantIndexOfType<TestType, bool>() == 0);
  static_assert(VariantIndexOfType<TestType, int>() == 1);
  static_assert(VariantIndexOfType<TestType, double>() == 2);
}

}  // namespace
}  // namespace base
