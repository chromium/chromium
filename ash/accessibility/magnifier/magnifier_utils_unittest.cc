// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/magnifier_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace magnifier_utils {
namespace {

TEST(MagnifierScaleUtilsTest, AdjustScaleFromScroll) {
  // Scaling by offset 0.2f followed by another 0.2f should result in the same
  // scale if scaling by offset 0.4f (given that we start from the same scale).
  const float min_scale = 1.0f;
  const float max_scale = 20.f;
  const float starting_scale = 1.0f;

  // Calculate scale if we're stepping by 0.2f twice.
  float linear_offset = 0.2f;
  const float step_1_scale =
      GetScaleFromScroll(linear_offset, starting_scale, min_scale, max_scale);
  const float step_2_scale =
      GetScaleFromScroll(linear_offset, step_1_scale, min_scale, max_scale);

  // Calculate scale if we're stepping by 0.4f from the same starting scale.
  linear_offset = 0.4f;
  const float final_scale =
      GetScaleFromScroll(linear_offset, starting_scale, min_scale, max_scale);
  EXPECT_FLOAT_EQ(final_scale, step_2_scale);

  // However, each step doesn't change the scale linearly.
  EXPECT_NE(step_1_scale - starting_scale, step_2_scale - step_1_scale);
}

TEST(MagnifierScaleUtilsTest, GetNextMagnifierScaleValue) {
  const float min = 3.0f;
  const float max = 40.0f;
  float current_scale = min;

  // A positive |delta_index| always result in an increasing scale values that
  // are less than |max|.
  int delta_index = 1;
  while (current_scale < max) {
    float new_scale =
        GetNextMagnifierScaleValue(delta_index, current_scale, min, max);
    EXPECT_GT(new_scale, current_scale);
    current_scale = new_scale;
  }

  EXPECT_FLOAT_EQ(current_scale, max);

  // And vice versa for a negative |delta_index|.
  delta_index = -1;
  while (current_scale > min) {
    float new_scale =
        GetNextMagnifierScaleValue(delta_index, current_scale, min, max);
    EXPECT_LT(new_scale, current_scale);
    current_scale = new_scale;
  }

  EXPECT_FLOAT_EQ(min, current_scale);
}

}  // namespace
}  // namespace magnifier_utils
}  // namespace ash
