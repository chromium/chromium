// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_transform_case.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(PickerTransformCase, TransformsCaseCorrectlyForEnglish) {
  EXPECT_EQ(PickerTransformToUpperCase(u"abc"), u"ABC");
  EXPECT_EQ(PickerTransformToLowerCase(u"XYZ"), u"xyz");
  EXPECT_EQ(PickerTransformToTitleCase(u"how are you"), u"How Are You");
}

}  // namespace
}  // namespace ash
