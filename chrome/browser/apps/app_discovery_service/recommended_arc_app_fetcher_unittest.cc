// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_discovery_service/play_extras.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class RecommendedArcAppFetcherTest : public testing::Test {
 public:
  RecommendedArcAppFetcherTest() = default;

  void SetUp() override {
    arc_app_fetcher_ = std::make_unique<RecommendedArcAppFetcher>(&profile_);
  }

  RecommendedArcAppFetcher* arc_app_fetcher() { return arc_app_fetcher_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<RecommendedArcAppFetcher> arc_app_fetcher_;
};

TEST_F(RecommendedArcAppFetcherTest, OnLoadSuccess) {
  const std::string response =
      R"json({"recommendedApp": [{
    "androidApp": {
      "packageName": "com.game.name",
      "title": "NameOfFunGame",
      "icon": {
        "imageUri": "https://play-lh.googleusercontent.com/1234IDECLAREATHUMBWAR",
        "dimensions": {
          "width": 512,
          "height": 512
        }
      },
      "starRating": {
        "averageRating": 4.3319736
      },
      "category": "Casual",
      "appDescription": {
        "shortDescription": "Wow this game is so fun!"
      },
      "contentRating": {
        "name": "General",
        "image": {
          "imageUri": "https://play-lh.googleusercontent.com/5678WHODOWEAPPRECIATE",
          "dimensions": {
            "width": 272,
            "height": 272
          }
        },
        "imageCanBeDisplayedWithoutName": false
      },
      "downloadsInformation": {
        "numDownloadsRounded": "100000000"
      },
      "adsInformation": {
        "disclaimerText": "Contains ads"
      },
      "inAppPurchaseInformation": {
        "disclaimerText": "In-app purchases"
      }
    },
    "merchCurated": {
    }
  }]})json";
  arc_app_fetcher()->SetCallbackForTesting(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        ASSERT_EQ(error, DiscoveryError::kSuccess);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].GetAppSource(), AppSource::kPlay);
        EXPECT_EQ(results[0].GetIconId(), "com.game.name");
        EXPECT_EQ(results[0].GetAppTitle(), u"NameOfFunGame");
        EXPECT_TRUE(results[0].GetSourceExtras());
        auto* play_extras = results[0].GetSourceExtras()->AsPlayExtras();
        EXPECT_TRUE(play_extras);
        EXPECT_EQ(play_extras->GetPackageName(), "com.game.name");
        EXPECT_EQ(
            play_extras->GetIconUrl(),
            GURL(
                "https://play-lh.googleusercontent.com/1234IDECLAREATHUMBWAR"));
        EXPECT_EQ(play_extras->GetCategory(), u"Casual");
        EXPECT_EQ(play_extras->GetDescription(), u"Wow this game is so fun!");
        EXPECT_EQ(play_extras->GetContentRating(), u"General");
        EXPECT_EQ(
            play_extras->GetContentRatingIconUrl(),
            GURL(
                "https://play-lh.googleusercontent.com/5678WHODOWEAPPRECIATE"));
        EXPECT_EQ(play_extras->GetHasInAppPurchases(), true);
        EXPECT_EQ(play_extras->GetWasPreviouslyInstalled(), false);
        EXPECT_EQ(play_extras->GetContainsAds(), true);
        EXPECT_EQ(play_extras->GetOptimizedForChrome(), true);
      }));
  auto output = base::JSONReader::ReadAndReturnValueWithError(response);
  ASSERT_TRUE(output.has_value());
  arc_app_fetcher()->OnLoadSuccess(std::move(output.value()));
}

TEST_F(RecommendedArcAppFetcherTest, OnLoadError) {
  arc_app_fetcher()->SetCallbackForTesting(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        ASSERT_EQ(results.size(), 0u);
        ASSERT_EQ(error, DiscoveryError::kErrorRequestFailed);
      }));
  arc_app_fetcher()->OnLoadError();
}

TEST_F(RecommendedArcAppFetcherTest, OnParseResponseError) {
  arc_app_fetcher()->SetCallbackForTesting(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        ASSERT_EQ(results.size(), 0u);
        ASSERT_EQ(error, DiscoveryError::kErrorMalformedData);
      }));
  arc_app_fetcher()->OnParseResponseError();
}

}  // namespace apps
