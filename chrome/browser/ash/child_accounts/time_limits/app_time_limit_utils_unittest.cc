// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"

#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace app_time {

TEST(AppTimeLimitUtils, IsValidExtensionUrl) {
  EXPECT_FALSE(IsValidExtensionUrl(GURL("https://chromium.org")));
  EXPECT_FALSE(IsValidExtensionUrl(GURL("http://example.org")));
  EXPECT_TRUE(IsValidExtensionUrl(
      GURL("chrome-extension://oafaagfgbdpldilgjjfjocjglfbolmac")));
  EXPECT_FALSE(IsValidExtensionUrl(GURL("ftp://www.chromium.org")));
  EXPECT_FALSE(IsValidExtensionUrl(GURL("chrome://flags")));
  EXPECT_FALSE(IsValidExtensionUrl(GURL("about:blank")));
  EXPECT_FALSE(
      IsValidExtensionUrl(GURL("file://mhjfbmdgcfjbbpaeojofohoefgiehjai")));
  EXPECT_FALSE(IsValidExtensionUrl(GURL("chrome://extensions")));
  EXPECT_FALSE(IsValidExtensionUrl(
      GURL("filesystem:http://example.com/path/file.html")));
}

}  // namespace app_time
}  // namespace ash
