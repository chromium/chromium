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
  constexpr char kTestString[] = "string contents";
  constexpr auto kTestStringLength = std::string_view(kTestString).size();
  std::unique_ptr<std::string> input =
      std::make_unique<std::string>(kTestString);

  // Verify the contents get copied + the new local variable has the right type
  // when the array size given is exactly `input->size() + 1`.
  DEBUG_ALIAS_FOR_CSTR(copy1, input->c_str(),
                       kTestStringLength + 1 /* == input->size() + 1 */);
  static_assert(std::is_same_v<decltype(copy1), char[kTestStringLength + 1]>);
  EXPECT_TRUE(
      std::equal(std::begin(kTestString), std::end(kTestString), copy1));

  // Verify that the copy is properly null-terminated even when it is smaller
  // than the input string.
  DEBUG_ALIAS_FOR_CSTR(copy2, input->c_str(), 3 /* < input->size() */);
  static_assert(std::is_same_v<decltype(copy2), char[3]>);
  EXPECT_TRUE(std::equal(std::begin(copy2), std::end(copy2), "st"));

  // Verify that the copy is properly null-terminated even when it is larger
  // than the input string.
  DEBUG_ALIAS_FOR_CSTR(copy3, input->c_str(), 100 /* > input->size() + 1 */);
  static_assert(std::is_same_v<decltype(copy3), char[100]>);
  EXPECT_TRUE(
      std::equal(std::begin(kTestString), std::end(kTestString), copy3));
}

TEST(DebugAlias, U16String) {
  constexpr char16_t kTestString[] = u"H͟e͟l͟l͟o͟ ͟w͟o͟r͟l͟d͟!͟";
  constexpr auto kTestStringLength = std::u16string_view(kTestString).size();
  std::u16string input = kTestString;

  // Verify the contents get copied + the new local variable has the right type
  // when the array size given is exactly `input->size() + 1`.
  DEBUG_ALIAS_FOR_U16CSTR(aliased_copy, input.c_str(), kTestStringLength + 1);
  static_assert(
      std::is_same_v<decltype(aliased_copy), char16_t[kTestStringLength + 1]>);
  EXPECT_TRUE(
      std::equal(std::begin(kTestString), std::end(kTestString), aliased_copy));

  // Verify that the copy is properly null-terminated even when it is larger
  // than the input string.
  DEBUG_ALIAS_FOR_U16CSTR(aliased_copy2, input.c_str(), kTestStringLength + 1);
  static_assert(
      std::is_same_v<decltype(aliased_copy2), char16_t[kTestStringLength + 1]>);
  EXPECT_TRUE(std::equal(std::begin(kTestString), std::end(kTestString),
                         aliased_copy2));
}

TEST(DebugAlias, U16StringPartialCopy) {
  std::u16string input = u"Hello world!";
  DEBUG_ALIAS_FOR_U16CSTR(aliased_copy, input.c_str(), 5);
  // Make sure we don't write past the specified number of characters. We
  // subtract 1 to account for the null terminator.
  EXPECT_EQ(input.substr(0, 4), aliased_copy);
}
