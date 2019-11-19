// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"

#include "base/compiler_specific.h"
#include "base/debug/debugging_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// This is a typical use: taking Location::Current as a default parameter.
// So even though this looks contrived, it confirms that such usage works as
// expected.
Location WhereAmI(const Location& location = Location::Current()) {
  return location;
}

}  // namespace

TEST(LocationTest, CurrentYieldsCorrectValue) {
  int previous_line = __LINE__;
  Location here = WhereAmI();
  EXPECT_NE(here.program_counter(), WhereAmI().program_counter());
#if SUPPORTS_LOCATION_BUILTINS
  EXPECT_THAT(here.file_name(), ::testing::EndsWith("location_unittest.cc"));
#if BUILDFLAG(ENABLE_LOCATION_SOURCE)
  EXPECT_EQ(here.line_number(), previous_line + 1);
  EXPECT_STREQ("TestBody", here.function_name());
#endif
#endif
  ALLOW_UNUSED_LOCAL(previous_line);
}

}  // namespace base
