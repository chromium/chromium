// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

TEST(CookiesGetAllSignalTest, StripUrlCorrectly) {
  CookiesGetAllSignal signal = CookiesGetAllSignal(
      /*extension_id=*/"ext-0",
      /*domain=*/"domain",
      /*name=*/"cookie-1",
      /*path=*/"/path1",
      /*secure=*/true,
      /*store_id=*/"store-1",
      /*url=*/"https://www.example.com/path1/path2/index.html?q=test&id=5",
      /*is_session=*/true);
  EXPECT_EQ(signal.url(), "https://www.example.com/path1/path2/");
}

TEST(CookiesGetAllSignalTest, ConcatFieldsWithAllArgs) {
  CookiesGetAllSignal signal = CookiesGetAllSignal(
      /*extension_id=*/"ext-0",
      /*domain=*/"domain",
      /*name=*/"cookie-1",
      /*path=*/"/path1",
      /*secure=*/true,
      /*store_id=*/"store-1",
      /*url=*/"http://www.example.com/",
      /*is_session=*/false);
  EXPECT_EQ(signal.getUniqueArgSetId(),
            "domain,cookie-1,/path1,1,store-1,http://www.example.com/,0");
}

TEST(CookiesGetAllSignalTest, ConcatFieldsWithDefaultArgs) {
  CookiesGetAllSignal signal = CookiesGetAllSignal(
      /*extension_id=*/"",
      /*domain=*/"",
      /*name=*/"",
      /*path=*/"",
      /*secure=*/std::nullopt,
      /*store_id=*/"",
      /*url=*/"",
      /*is_session=*/std::nullopt);
  EXPECT_EQ(signal.getUniqueArgSetId(), ",,,,,,");
}

}  // namespace

}  // namespace safe_browsing
