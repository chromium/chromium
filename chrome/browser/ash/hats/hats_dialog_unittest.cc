// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "hats_dialog.h"
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

TEST(HatsDialogTest, HandleClientTriggeredAction) {
  // Client asks to close the window
  EXPECT_TRUE(HatsDialog::HandleClientTriggeredAction("close", "hist-name"));
  // There was an unhandled error, close the window
  EXPECT_TRUE(HatsDialog::HandleClientTriggeredAction(
      "survey-loading-error-12345", "a-suffix"));
  // Client sent an invalid action, ignore it
  EXPECT_FALSE(HatsDialog::HandleClientTriggeredAction("Invalid", "hist-name"));

  // Set up the histogram tester
  base::HistogramTester histogram_tester;
  std::string histogram("Browser.ChromeOS.HatsSatisfaction.General");
  histogram_tester.ExpectTotalCount(histogram, 0);

  EXPECT_FALSE(HatsDialog::HandleClientTriggeredAction("smiley-selected-4",
                                                       "full-histogram-name"));

  // Ensure we logged the right metric
  // For the example above, it means adding 1 entry in the bucket for score=4
  std::vector<base::Bucket> expected_buckets{{4, 1}};
  EXPECT_EQ(histogram_tester.GetAllSamples("full-histogram-name"),
            expected_buckets);
}
}  // namespace ash
