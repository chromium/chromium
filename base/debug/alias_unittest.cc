// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>

#include "base/debug/alias.h"
#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DebugAlias, Test) {
  const char kTestString[] = "string contents";
  std::unique_ptr<std::string> input =
      std::make_unique<std::string>("string contents");

  // Verify the contents get copied + the new local variable has the right type.
  DEBUG_ALIAS_FOR_CSTR(copy1, input->c_str(), 100 /* > input->size() */);
  static_assert(std::is_same_v<decltype(copy1), char[100]>);
  EXPECT_TRUE(
      std::equal(std::begin(kTestString), std::end(kTestString), copy1));

  // Verify that the copy is properly null-terminated even when it is smaller
  // than the input string.
  DEBUG_ALIAS_FOR_CSTR(copy2, input->c_str(), 3 /* < input->size() */);
  static_assert(std::is_same_v<decltype(copy2), char[3]>);
  EXPECT_TRUE(std::equal(std::begin(copy2), std::end(copy2), "st"));
}

TEST(DebugAlias, U16String) {
  const char16_t kTestString[] = u"H͟e͟l͟l͟o͟ ͟w͟o͟r͟l͟d͟!͟";
  std::u16string input = kTestString;

  DEBUG_ALIAS_FOR_U16CSTR(aliased_copy, input.c_str(), 100);
  static_assert(std::is_same_v<decltype(aliased_copy), char16_t[100]>);
  EXPECT_TRUE(
      std::equal(std::begin(kTestString), std::end(kTestString), aliased_copy));
}
