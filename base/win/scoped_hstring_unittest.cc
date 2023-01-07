// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_hstring.h"

#include <winstring.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

constexpr wchar_t kTestString1[] = L"123";
constexpr wchar_t kTestString2[] = L"456789";

}  // namespace

TEST(ScopedHStringTest, Init) {
  // ScopedHString requires WinRT core functions, which are not available in
  // older versions.
  if (GetVersion() < Version::WIN8) {
    EXPECT_FALSE(ScopedHString::ResolveCoreWinRTStringDelayload());
    return;
  }

  EXPECT_TRUE(ScopedHString::ResolveCoreWinRTStringDelayload());

  ScopedHString hstring = ScopedHString::Create(kTestString1);
  std::string buffer = hstring.GetAsUTF8();
  EXPECT_EQ(kTestString1, UTF8ToWide(buffer));
  WStringPiece contents = hstring.Get();
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

}  // namespace win
}  // namespace base
