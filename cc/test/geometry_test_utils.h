// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_GEOMETRY_TEST_UTILS_H_
#define CC_TEST_GEOMETRY_TEST_UTILS_H_

#include "cc/cc_export.h"

namespace gfx {
class Transform;
}

namespace cc {

// These are macros instead of functions so that we get useful line numbers
// where a test failed.
#define EXPECT_FLOAT_RECT_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x());           \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y());           \
    EXPECT_FLOAT_EQ((expected).width(), (actual).width());   \
    EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
  } while (false)

#define EXPECT_RECT_EQ(expected, actual)               \
  do {                                                 \
    const gfx::Rect& actualRect = actual;              \
    EXPECT_EQ(expected.x(), actualRect.x());           \
    EXPECT_EQ(expected.y(), actualRect.y());           \
    EXPECT_EQ(expected.width(), actualRect.width());   \
    EXPECT_EQ(expected.height(), actualRect.height()); \
  } while (false)

#define EXPECT_RECT_NEAR(expected, actual, abs_error)                 \
  do {                                                                \
    EXPECT_NEAR((expected).x(), (actual).x(), (abs_error));           \
    EXPECT_NEAR((expected).y(), (actual).y(), (abs_error));           \
    EXPECT_NEAR((expected).width(), (actual).width(), (abs_error));   \
    EXPECT_NEAR((expected).height(), (actual).height(), (abs_error)); \
  } while (false)

#define EXPECT_POINT3F_EQ(expected, actual)        \
  do {                                             \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    EXPECT_FLOAT_EQ((expected).z(), (actual).z()); \
  } while (false)

#define EXPECT_VECTOR_EQ(expected, actual)   \
  do {                                       \
    EXPECT_EQ((expected).x(), (actual).x()); \
    EXPECT_EQ((expected).y(), (actual).y()); \
  } while (false)

#define EXPECT_VECTOR2DF_EQ(expected, actual)      \
  do {                                             \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
  } while (false)

#define EXPECT_VECTOR2DF_NEAR(expected, actual, abs_error)  \
  do {                                                      \
    EXPECT_NEAR((expected).x(), (actual).x(), (abs_error)); \
    EXPECT_NEAR((expected).y(), (actual).y(), (abs_error)); \
  } while (false)

#define EXPECT_VECTOR3DF_EQ(expected, actual)      \
  do {                                             \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    EXPECT_FLOAT_EQ((expected).z(), (actual).z()); \
  } while (false)

#define EXPECT_VECTOR3DF_NEAR(expected, actual, abs_error)  \
  do {                                                      \
    EXPECT_NEAR((expected).x(), (actual).x(), (abs_error)); \
    EXPECT_NEAR((expected).y(), (actual).y(), (abs_error)); \
    EXPECT_NEAR((expected).z(), (actual).z(), (abs_error)); \
  } while (false)

#define EXPECT_FLOAT_ARRAY_EQ(expected, actual, count) \
  do {                                                 \
    for (int i = 0; i < count; i++) {                  \
      EXPECT_FLOAT_EQ((expected)[i], (actual)[i]);     \
    }                                                  \
  } while (false)

#define EXPECT_FLOAT_SIZE_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).width(), (actual).width());   \
    EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
  } while (false)

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

// This is a function rather than a macro because when this is included as a
// macro in bulk, it causes a significant slow-down in compilation time. This
// problem exists with both gcc and clang, and bugs have been filed at
// http://llvm.org/bugs/show_bug.cgi?id=13651
// and http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54337
void ExpectTransformationMatrixEq(const gfx::Transform& expected,
                                  const gfx::Transform& actual);

#define EXPECT_TRANSFORMATION_MATRIX_EQ(expected, actual) \
  do {                                                    \
    ExpectTransformationMatrixEq(expected, actual);       \
  } while (false)

void ExpectTransformationMatrixNear(const gfx::Transform& expected,
                                    const gfx::Transform& actual,
                                    float abs_error);

#define EXPECT_TRANSFORMATION_MATRIX_NEAR(expected, actual, abs_error) \
  do {                                                                 \
    ExpectTransformationMatrixNear(expected, actual, abs_error);       \
  } while (false)

// Should be used in test code only, for convenience. Production code should use
// the gfx::Transform::GetInverse() API.
gfx::Transform Inverse(const gfx::Transform& transform);

}  // namespace cc

#endif  // CC_TEST_GEOMETRY_TEST_UTILS_H_
