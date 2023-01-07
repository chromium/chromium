// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_TEST_SUPPORT_MACROS_H_
#define ASH_ASSISTANT_UTIL_TEST_SUPPORT_MACROS_H_

namespace ash {

// TODO(yawano): Migrate usage to gfx::test::AreBitmapsEqual.
// Asserts |img_a_| and |img_b_| pixel equivalence.
#define ASSERT_PIXELS_EQ(img_a_, img_b_)            \
  {                                                 \
    ASSERT_EQ(img_a_.width(), img_b_.width());      \
    ASSERT_EQ(img_a_.height(), img_b_.height());    \
    for (int x = 0; x < img_a_.width(); ++x) {      \
      for (int y = 0; y < img_a_.height(); ++y) {   \
        ASSERT_EQ(img_a_.bitmap()->getColor(x, y),  \
                  img_b_.bitmap()->getColor(x, y)); \
      }                                             \
    }                                               \
  }

}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_TEST_SUPPORT_MACROS_H_
