// Copyright 2022 The Chromium Authors
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
  SkottieTextPropertyValue val("test-1", /*box=*/gfx::RectF());
  EXPECT_THAT(val.text(), Eq("test-1"));
  val.SetText("test-2");
  EXPECT_THAT(val.text(), Eq("test-2"));
}

TEST(SkottieTextPropertyValueTest, ComparisonOperator) {
  SkottieTextPropertyValue val("test-1", gfx::RectF(10, 20, 100, 200));
  SkottieTextPropertyValue val_2("test-1", gfx::RectF(10, 20, 100, 200));
  EXPECT_THAT(val, Eq(val_2));

  SkottieTextPropertyValue val_copy(val);
  ASSERT_THAT(val_copy, Eq(val));

  {
    SkottieTextPropertyValue tmp(val);
    tmp.SetText("test-2");
    EXPECT_THAT(val, Ne(tmp));
  }
  {
    SkottieTextPropertyValue tmp(val);
    tmp.set_box(gfx::RectF(20, 10, 200, 100));
    EXPECT_THAT(val, Ne(tmp));
  }
}

}  // namespace
}  // namespace cc
