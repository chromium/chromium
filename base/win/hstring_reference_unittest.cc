// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hstring_reference.h"

#include <string>

#include "base/strings/string_piece.h"
#include "base/win/scoped_hstring.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kTestString[] = L"123";
constexpr wchar_t kEmptyString[] = L"";

void VerifyHSTRINGEquals(HSTRING hstring, const wchar_t* test_string) {
  const ScopedHString scoped_hstring(hstring);
  const WStringPiece hstring_contents = scoped_hstring.Get();
  EXPECT_EQ(hstring_contents.compare(test_string), 0);
}

}  // namespace

TEST(HStringReferenceTest, Init) {
  const HStringReference string(kTestString);
  EXPECT_NE(string.Get(), nullptr);
  VerifyHSTRINGEquals(string.Get(), kTestString);

  // Empty strings come back as null HSTRINGs, a valid HSTRING.
  const HStringReference empty_string(kEmptyString);
  EXPECT_EQ(empty_string.Get(), nullptr);
  VerifyHSTRINGEquals(empty_string.Get(), kEmptyString);

  // Passing a zero length and null string should also return a null HSTRING.
  const HStringReference null_string(nullptr, 0);
  EXPECT_EQ(null_string.Get(), nullptr);
  VerifyHSTRINGEquals(null_string.Get(), kEmptyString);
}

}  // namespace base::win
