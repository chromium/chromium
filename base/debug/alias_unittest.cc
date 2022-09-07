// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/debug/alias.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DebugAlias, Test) {
  std::unique_ptr<std::string> input =
      std::make_unique<std::string>("string contents");

  // Verify the contents get copied + the new local variable has the right type.
  DEBUG_ALIAS_FOR_CSTR(copy1, input->c_str(), 100 /* > input->size() */);
  static_assert(sizeof(copy1) == 100,
                "Verification that copy1 has expected size");
  EXPECT_STREQ("string contents", copy1);

  // Verify that the copy is properly null-terminated even when it is smaller
  // than the input string.
  DEBUG_ALIAS_FOR_CSTR(copy2, input->c_str(), 3 /* < input->size() */);
  static_assert(sizeof(copy2) == 3,
                "Verification that copy2 has expected size");
  EXPECT_STREQ("st", copy2);
  EXPECT_EQ('\0', copy2[2]);
}
