// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/geometry_test_utils.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {

// NOTE: even though transform data types use double precision, we only check
// for equality within single-precision error bounds because many transforms
// originate from single-precision data types such as quads/rects/etc.

void ExpectTransformationMatrixEq(const gfx::Transform& expected,
                                  const gfx::Transform& actual) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_FLOAT_EQ(expected.matrix().get(row, col),
                      actual.matrix().get(row, col))
          << "row: " << row << " col: " << col;
    }
  }
}

void ExpectTransformationMatrixNear(const gfx::Transform& expected,
                                    const gfx::Transform& actual,
                                    float abs_error) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_NEAR(expected.matrix().get(row, col),
                  actual.matrix().get(row, col), abs_error)
          << "row: " << row << " col: " << col;
    }
  }
}

gfx::Transform Inverse(const gfx::Transform& transform) {
  gfx::Transform result(gfx::Transform::kSkipInitialization);
  bool inverted_successfully = transform.GetInverse(&result);
  DCHECK(inverted_successfully);
  return result;
}

}  // namespace cc
