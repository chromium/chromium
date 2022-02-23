// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_text_property_value.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using ::testing::Eq;
using ::testing::Ne;

TEST(SkottieTextPropertyValueTest, SetsText) {
  SkottieTextPropertyValue val("test-1");
  EXPECT_THAT(val.text(), Eq("test-1"));
  val.SetText("test-2");
  EXPECT_THAT(val.text(), Eq("test-2"));
}

TEST(SkottieTextPropertyValueTest, ComparisonOperatorComparesText) {
  SkottieTextPropertyValue val("test-1");
  SkottieTextPropertyValue val_2("test-1");
  EXPECT_THAT(val, Eq(val_2));
  val_2.SetText("test-2");
  EXPECT_THAT(val, Ne(val_2));
  SkottieTextPropertyValue copy(val);
  EXPECT_THAT(copy, Eq(val));
}

}  // namespace
}  // namespace cc
