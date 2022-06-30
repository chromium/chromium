// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fusebox_moniker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {

TEST(FuseBoxMonikerMapTest, ExtractTokenResult) {
  using ResultType = FuseBoxMonikerMap::ExtractTokenResult::ResultType;

  {
    auto result = FuseBoxMonikerMap::ExtractToken("foo://bar");
    EXPECT_EQ(result.result_type, ResultType::NOT_A_MONIKER_FS_URL);
  }

  {
    auto result = FuseBoxMonikerMap::ExtractToken(
        "dummy://something_else/0123456789ABCDEF0000111122223333");
    EXPECT_EQ(result.result_type, ResultType::NOT_A_MONIKER_FS_URL);
  }

  {
    auto result = FuseBoxMonikerMap::ExtractToken("dummy://moniker");
    EXPECT_EQ(result.result_type, ResultType::MONIKER_FS_URL_BUT_ONLY_ROOT);
  }

  {
    auto result = FuseBoxMonikerMap::ExtractToken("dummy://moniker/");
    EXPECT_EQ(result.result_type,
              ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED);
  }

  {
    auto result = FuseBoxMonikerMap::ExtractToken("dummy://moniker/0123");
    EXPECT_EQ(result.result_type,
              ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED);
  }

  {
    auto result = FuseBoxMonikerMap::ExtractToken(
        "dummy://moniker/0123456789ABCDEF0000111122223333");
    EXPECT_EQ(result.result_type, ResultType::OK);
    EXPECT_EQ(result.token,
              base::Token(0x0123456789ABCDEFull, 0x0000111122223333ull));
  }
}

}  // namespace file_manager
