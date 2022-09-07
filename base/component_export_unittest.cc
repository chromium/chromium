// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using ComponentExportTest = testing::Test;

#define IS_TEST_COMPONENT_A_IMPL 1
#define IS_TEST_COMPONENT_B_IMPL
#define IS_TEST_COMPONENT_C_IMPL 0
#define IS_TEST_COMPONENT_D_IMPL 2
#define IS_TEST_COMPONENT_E_IMPL xyz

TEST(ComponentExportTest, ImportExport) {
  // Defined as 1. Treat as export.
  EXPECT_EQ(1, INSIDE_COMPONENT_IMPL(TEST_COMPONENT_A));

  // Defined, but empty. Treat as import.
  EXPECT_EQ(0, INSIDE_COMPONENT_IMPL(TEST_COMPONENT_B));

  // Defined, but 0. Treat as import.
  EXPECT_EQ(0, INSIDE_COMPONENT_IMPL(TEST_COMPONENT_C));

  // Defined, but some other arbitrary thing that isn't 1. Treat as import.
  EXPECT_EQ(0, INSIDE_COMPONENT_IMPL(TEST_COMPONENT_D));
  EXPECT_EQ(0, INSIDE_COMPONENT_IMPL(TEST_COMPONENT_E));

  // Undefined. Treat as import.
  EXPECT_EQ(0, INSIDE_COMPONENT_IMPL(TEST_COMPONENT_F));

  // And just for good measure, ensure that the macros evaluate properly in the
  // context of preprocessor #if blocks.
#if INSIDE_COMPONENT_IMPL(TEST_COMPONENT_A)
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(false);
#endif

#if !INSIDE_COMPONENT_IMPL(TEST_COMPONENT_B)
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(false);
#endif

#if !INSIDE_COMPONENT_IMPL(TEST_COMPONENT_C)
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(false);
#endif

#if !INSIDE_COMPONENT_IMPL(TEST_COMPONENT_D)
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(false);
#endif

#if !INSIDE_COMPONENT_IMPL(TEST_COMPONENT_E)
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(false);
#endif

#if !INSIDE_COMPONENT_IMPL(TEST_COMPONENT_F)
  EXPECT_TRUE(true);
#else
  EXPECT_TRUE(false);
#endif
}

#undef IS_TEST_COMPONENT_A_IMPL
#undef IS_TEST_COMPONENT_B_IMPL
#undef IS_TEST_COMPONENT_C_IMPL
#undef IS_TEST_COMPONENT_D_IMPL
#undef IS_TEST_COMPONENT_E_IMPL

}  // namespace
}  // namespace base
