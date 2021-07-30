// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include <string>

#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kLocaleKey[] = "locale";
const char kBrowserKey[] = "browser";
const char kPlatformKey[] = "platform";
const char kFirmwareKey[] = "firmware";
const char kPsdKey1[] = "psd1";
const char kPsdKey2[] = "psd2";
const char kPsdValue1[] = "psdValue1";
const char kPsdValue2[] = "psdValue2 =%^*$#&";
const char kLocaleValue1[] = "locale1";
const char kBrowserValue1[] = "browser1";

bool GetQueryParameter(const std::string& query,
                       const std::string& key,
                       std::string* value) {
  // Name and scheme actually don't matter, but are required to get a valid URL
  // for parsing.
  GURL query_url("http://localhost?" + query);
  return net::GetValueForKeyInQuery(query_url, key, value);
}

}  // namespace

namespace ash {

TEST(HatsDialogTest, GetFormattedSiteContext) {
  base::flat_map<std::string, std::string> product_specific_data = {
      {kPsdKey1, kPsdValue1},
      {kPsdKey2, kPsdValue2},
      {kBrowserKey, kBrowserValue1}};

  std::string context =
      HatsDialog::GetFormattedSiteContext(kLocaleValue1, product_specific_data);

  std::string value;
  EXPECT_TRUE(GetQueryParameter(context, kLocaleKey, &value));
  EXPECT_EQ(kLocaleValue1, value);
  EXPECT_TRUE(GetQueryParameter(context, kBrowserKey, &value));
  EXPECT_NE(kBrowserValue1, value);
  EXPECT_TRUE(GetQueryParameter(context, kPlatformKey, &value));
  EXPECT_TRUE(GetQueryParameter(context, kFirmwareKey, &value));

  EXPECT_TRUE(GetQueryParameter(context, kPsdKey1, &value));
  EXPECT_EQ(kPsdValue1, value);
  EXPECT_TRUE(GetQueryParameter(context, kPsdKey2, &value));
  EXPECT_EQ(kPsdValue2, value);

  // Confirm that the values are properly url escaped.
  EXPECT_NE(std::string::npos, context.find("psdValue2%20%3D%25%5E*%24%23%26"));
}

}  // namespace ash
