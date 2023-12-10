// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_hstring.h"

#include <winstring.h>

#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kTestString1[] = L"123";
constexpr wchar_t kTestString2[] = L"456789";

}  // namespace

TEST(ScopedHStringTest, Init) {
  ScopedHString hstring = ScopedHString::Create(kTestString1);
  std::string buffer = hstring.GetAsUTF8();
  EXPECT_EQ(kTestString1, UTF8ToWide(buffer));
  std::wstring_view contents = hstring.Get();
  EXPECT_EQ(kTestString1, contents);

  hstring.reset();
  EXPECT_TRUE(hstring == nullptr);
  EXPECT_EQ(nullptr, hstring.get());

  hstring = ScopedHString::Create(kTestString2);

  buffer = hstring.GetAsUTF8();
  EXPECT_EQ(kTestString2, UTF8ToWide(buffer));
  contents = hstring.Get();
  EXPECT_EQ(kTestString2, contents);
}

}  // namespace base::win
