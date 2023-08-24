// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr const char kUrl1[] =
    "http://www.example.com/path1/index.html?q=test&id=5";
constexpr const char kSanitizedUrl1[] = "http://www.example.com/path1/";
constexpr const char kUrl2[] =
    "http://www.example.com/path1/path2/index.html?q=test&id=5";
constexpr const char kSanitizedUrl2[] = "http://www.example.com/path1/path2/";

TEST(TabsApiSignalTest, SanitizesUrls) {
  TabsApiSignal signal = TabsApiSignal(
      /*extension_id=*/"ext-0",
      /*api_method=*/TabsApiInfo::UPDATE,
      /*current_url=*/kUrl1,
      /*new_url=*/kUrl2);
  EXPECT_EQ(signal.current_url(), kSanitizedUrl1);
  EXPECT_EQ(signal.new_url(), kSanitizedUrl2);
}

TEST(TabsApiSignalTest, GeneratesIdWithAllArgsPresent) {
  TabsApiSignal signal = TabsApiSignal(
      /*extension_id=*/"ext-0",
      /*api_method=*/TabsApiInfo::UPDATE,
      /*current_url=*/kUrl1,
      /*new_url=*/kUrl2);
  std::string expected_id =
      "2," + std::string(kSanitizedUrl1) + "," + std::string(kSanitizedUrl2);
  EXPECT_EQ(signal.GetUniqueCallDetailsId(), expected_id);
}

TEST(TabsApiSignalTest, GeneratesIdWithDefaultArgs) {
  TabsApiSignal signal = TabsApiSignal(
      /*extension_id=*/"ext-0",
      /*api_method=*/TabsApiInfo::CREATE,
      /*current_url=*/"",
      /*new_url=*/kUrl2);
  std::string expected_id = "1,," + std::string(kSanitizedUrl2);
  EXPECT_EQ(signal.GetUniqueCallDetailsId(), expected_id);
}

}  // namespace

}  // namespace safe_browsing
