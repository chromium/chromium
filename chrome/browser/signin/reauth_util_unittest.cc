// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/reauth_util.h"

#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class ReauthUtilURLTest : public ::testing::TestWithParam<int> {};

TEST_P(ReauthUtilURLTest, GetAndParseReauthConfirmationURL) {
  auto access_point =
      static_cast<signin_metrics::ReauthAccessPoint>(GetParam());
  GURL url = GetReauthConfirmationURL(access_point);
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISigninReauthHost);
  signin_metrics::ReauthAccessPoint get_access_point =
      GetReauthAccessPointForReauthConfirmationURL(url);
  EXPECT_EQ(get_access_point, access_point);
}

INSTANTIATE_TEST_CASE_P(
    AllAccessPoints,
    ReauthUtilURLTest,
    ::testing::Range(
        static_cast<int>(signin_metrics::ReauthAccessPoint::kUnknown),
        static_cast<int>(signin_metrics::ReauthAccessPoint::kMaxValue) + 1));

}  // namespace signin
