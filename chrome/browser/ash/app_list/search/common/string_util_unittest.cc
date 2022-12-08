// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/string_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace app_list::test {
namespace {

TEST(AppListStringUtilTest, NormalizeId) {
  std::string id1 = NormalizeId("docs.google.com/spreadsheets/d");
  EXPECT_EQ(id1, "docs.google.com/spreadsheets/d");

  std::string id2 = NormalizeId("other.website.com/spreadsheets/d/");
  EXPECT_EQ(id2, "other.website.com/spreadsheets/d");

  std::string id3 = NormalizeId("https://other.website.com/spreadsheets/d");
  EXPECT_EQ(id3, "other.website.com/spreadsheets/d");

  std::string id4 = NormalizeId("https://docs.google.com/presentation/d/");
  EXPECT_EQ(id4, "docs.google.com/presentation/d");
}

TEST(AppListStringUtilTest, RemoveAppShortcutLabel) {
  std::string id1 = RemoveAppShortcutLabel("docs.google.com/spreadsheets/d");
  EXPECT_EQ(id1, "docs.google.com/spreadsheets");
}

TEST(AppListStringUtilTest, GetDriveId) {
  const auto id1 = GetDriveId(
      GURL("https://docs.google.com/presentation/d/"
           "1d0ccy4JvDOabMXpztaYfb-85OlUdIVnbYPbpKr1WSJA/edit#slide=id.p"));
  ASSERT_TRUE(id1);
  EXPECT_EQ(id1.value(), "1d0ccy4JvDOabMXpztaYfb-85OlUdIVnbYPbpKr1WSJA");

  const auto id2 = GetDriveId(
      GURL("https://docs.google.com/spreadsheets/d/"
           "11_Wh9tJrf5Jo1Kvr2A0RX7WBwtmIUBNt-vGpCXCTH9k?eops=0&usp=drive_fs"));
  ASSERT_TRUE(id2);
  EXPECT_EQ(id2.value(), "11_Wh9tJrf5Jo1Kvr2A0RX7WBwtmIUBNt-vGpCXCTH9k");

  EXPECT_FALSE(GetDriveId(GURL(
      "https://other.website.com/spreadsheets/d/"
      "11_Wh9tJrf5Jo1Kvr2A0RX7WBwtmIUBNt-vGpCXCTH9k?eops=0&usp=drive_fs")));

  EXPECT_FALSE(GetDriveId(GURL("https://docs.google.com/")));
}

}  // namespace
}  // namespace app_list::test
