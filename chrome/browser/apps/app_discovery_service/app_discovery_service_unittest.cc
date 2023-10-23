// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/play_extras.h"
#include "chrome/browser/apps/app_discovery_service/test_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestIconId[] = "fake_id";
const std::u16string kTestAppTitle = u"fake_title";

// Test PlayApp Constants
const char kTestPlayAppPackageName[] = "com.hbo.hbonow";
const char kTestPlayAppIconUrl[] = "https://play-lh.googleusercontent.com/fake";
const std::u16string kTestPlayAppCategory = u"Entertainment";
const std::u16string kTestPlayAppDescription =
    u"Stream all of HBO with new hit shows, classic favorites, and Max "
    u"Originals!";
const std::u16string kTestPlayAppContentRating = u"Teen";

}  // namespace

namespace apps {

class AppDiscoveryServiceTest : public testing::Test {
 public:
  AppDiscoveryServiceTest() {
    test_fetcher_ = std::make_unique<TestFetcher>();
    AppFetcherManager::SetOverrideFetcherForTesting(test_fetcher_.get());
  }

  void CheckResult(const Result& result,
                   AppSource source,
                   const std::string& icon_id,
                   const std::u16string& app_title) {
    EXPECT_EQ(result.GetAppSource(), source);
    EXPECT_EQ(result.GetIconId(), icon_id);
    EXPECT_EQ(result.GetAppTitle(), app_title);
  }

 protected:
  Profile* profile() { return &profile_; }
  TestFetcher* test_fetcher() { return test_fetcher_.get(); }
  base::CallbackListSubscription subscription_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<TestFetcher> test_fetcher_;
};

TEST_F(AppDiscoveryServiceTest, GetAppsFromFetcherNoExtras) {
  auto* app_discovery_service =
      AppDiscoveryServiceFactory::GetForProfile(profile());
  EXPECT_TRUE(app_discovery_service);

  std::vector<Result> fake_results;
  fake_results.emplace_back(AppSource::kTestSource, kTestIconId, kTestAppTitle,
                            nullptr);
  test_fetcher()->SetResults(std::move(fake_results));

  app_discovery_service->GetApps(
      ResultType::kTestType,
      base::BindLambdaForTesting([this](const std::vector<Result>& results,
                                        DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kSuccess);
        EXPECT_EQ(results.size(), 1u);
        CheckResult(results[0], AppSource::kTestSource, kTestIconId,
                    kTestAppTitle);
        EXPECT_FALSE(results[0].GetSourceExtras());
      }));
}

TEST_F(AppDiscoveryServiceTest, GetArcAppsFromFetcher) {
  auto* app_discovery_service =
      AppDiscoveryServiceFactory::GetForProfile(profile());
  EXPECT_TRUE(app_discovery_service);

  GURL kTestIconUrl(kTestPlayAppIconUrl);
  std::vector<Result> fake_results;
  auto play_extras = std::make_unique<PlayExtras>(
      kTestPlayAppPackageName, kTestIconUrl, kTestPlayAppCategory,
      kTestPlayAppDescription, kTestPlayAppContentRating, kTestIconUrl, true,
      false, false, false);
  fake_results.emplace_back(AppSource::kPlay, kTestIconId, kTestAppTitle,
                            std::move(play_extras));
  test_fetcher()->SetResults(std::move(fake_results));

  app_discovery_service->GetApps(
      ResultType::kTestType,
      base::BindLambdaForTesting([this](const std::vector<Result>& results,
                                        DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kSuccess);
        GURL kTestIconUrl(kTestPlayAppIconUrl);
        EXPECT_EQ(results.size(), 1u);
        CheckResult(results[0], AppSource::kPlay, kTestIconId, kTestAppTitle);
        EXPECT_TRUE(results[0].GetSourceExtras());
        auto* play_extras = results[0].GetSourceExtras()->AsPlayExtras();
        EXPECT_TRUE(play_extras);
        EXPECT_EQ(play_extras->GetPackageName(), kTestPlayAppPackageName);
        EXPECT_EQ(play_extras->GetIconUrl(), kTestIconUrl);
        EXPECT_EQ(play_extras->GetCategory(), kTestPlayAppCategory);
        EXPECT_EQ(play_extras->GetDescription(), kTestPlayAppDescription);
        EXPECT_EQ(play_extras->GetContentRating(), kTestPlayAppContentRating);
        EXPECT_EQ(play_extras->GetContentRatingIconUrl(), kTestIconUrl);
        EXPECT_EQ(play_extras->GetHasInAppPurchases(), true);
        EXPECT_EQ(play_extras->GetWasPreviouslyInstalled(), false);
        EXPECT_EQ(play_extras->GetContainsAds(), false);
        EXPECT_EQ(play_extras->GetOptimizedForChrome(), false);
      }));
}

TEST_F(AppDiscoveryServiceTest, RegisterForUpdates) {
  auto* app_discovery_service =
      AppDiscoveryServiceFactory::GetForProfile(profile());
  EXPECT_TRUE(app_discovery_service);

  std::vector<Result> fake_results;
  fake_results.emplace_back(AppSource::kTestSource, kTestIconId, kTestAppTitle,
                            nullptr);

  bool update_verified = false;
  subscription_ = app_discovery_service->RegisterForAppUpdates(
      ResultType::kTestType,
      base::BindLambdaForTesting(
          [this, &update_verified](const std::vector<Result>& results) {
            EXPECT_EQ(results.size(), 1u);
            CheckResult(results[0], AppSource::kTestSource, kTestIconId,
                        kTestAppTitle);
            EXPECT_FALSE(results[0].GetSourceExtras());
            update_verified = true;
          }));

  EXPECT_FALSE(update_verified);
  test_fetcher()->SetResults(std::move(fake_results));
  EXPECT_TRUE(update_verified);
}

}  // namespace apps
