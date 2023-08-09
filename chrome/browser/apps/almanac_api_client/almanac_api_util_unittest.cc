// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {
namespace {

using AlmanacApiUtilTest = testing::Test;

TEST_F(AlmanacApiUtilTest, GetEndpointUrl) {
  EXPECT_EQ(GetAlmanacEndpointUrl("").spec(),
            "https://chromeosalmanac-pa.googleapis.com/");
  EXPECT_EQ(GetAlmanacEndpointUrl("endpoint").spec(),
            "https://chromeosalmanac-pa.googleapis.com/endpoint");
  EXPECT_EQ(GetAlmanacEndpointUrl("v1/app-preload").spec(),
            "https://chromeosalmanac-pa.googleapis.com/v1/app-preload");
}

}  // namespace
}  // namespace apps
