// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_moniker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace fusebox {

TEST(MonikerMapTest, ExtractTokenResult) {
  using ResultType = MonikerMap::ExtractTokenResult::ResultType;

  {
    auto result = MonikerMap::ExtractToken("foo/bar");
    EXPECT_EQ(result.result_type, ResultType::NOT_A_MONIKER_FS_URL);
  }

  {
    auto result = MonikerMap::ExtractToken(
        "something_else/0123456789ABCDEF0000111122223333");
    EXPECT_EQ(result.result_type, ResultType::NOT_A_MONIKER_FS_URL);
  }

  {
    auto result = MonikerMap::ExtractToken("monikerz");
    EXPECT_EQ(result.result_type, ResultType::NOT_A_MONIKER_FS_URL);
  }

  {
    auto result = MonikerMap::ExtractToken("moniker");
    EXPECT_EQ(result.result_type, ResultType::MONIKER_FS_URL_BUT_ONLY_ROOT);
  }

  {
    auto result = MonikerMap::ExtractToken("moniker/");
    EXPECT_EQ(result.result_type,
              ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED);
  }

  {
    auto result = MonikerMap::ExtractToken("moniker/0123");
    EXPECT_EQ(result.result_type,
              ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED);
  }

  {
    auto result =
        MonikerMap::ExtractToken("moniker/0123456789ABCDEF0000111122223333");
    EXPECT_EQ(result.result_type, ResultType::OK);
    EXPECT_EQ(result.token,
              base::Token(0x0123456789ABCDEFull, 0x0000111122223333ull));
  }

  {
    auto result = MonikerMap::ExtractToken(
        "moniker/0123456789ABCDEF0000111122223333.html");
    EXPECT_EQ(result.result_type, ResultType::OK);
    EXPECT_EQ(result.token,
              base::Token(0x0123456789ABCDEFull, 0x0000111122223333ull));
  }

  {
    auto result = MonikerMap::ExtractToken(
        "moniker/0123456789ABCDEF0000111122223333_no_dot");
    EXPECT_EQ(result.result_type,
              ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED);
  }
}

}  // namespace fusebox
