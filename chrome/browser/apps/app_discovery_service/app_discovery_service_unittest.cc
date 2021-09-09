// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/play_extras.h"
#include "chrome/browser/apps/app_discovery_service/test_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestAppId[] = "fake_id";
const std::u16string kTestAppTitle = u"fake_title";

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
                   const std::string& app_id,
                   const std::u16string& app_title) {
    EXPECT_EQ(result.GetAppSource(), source);
    EXPECT_EQ(result.GetAppId(), app_id);
    EXPECT_EQ(result.GetAppTitle(), app_title);
  }

 protected:
  Profile* profile() { return &profile_; }
  TestFetcher* test_fetcher() { return test_fetcher_.get(); }

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
  fake_results.emplace_back(
      Result(AppSource::kPlay, kTestAppId, kTestAppTitle, nullptr));
  test_fetcher()->SetResults(std::move(fake_results));

  app_discovery_service->GetApps(
      ResultType::kRecommendedArcApps,
      base::BindLambdaForTesting([this](std::vector<Result> results) {
        EXPECT_EQ(results.size(), 1u);
        CheckResult(results[0], AppSource::kPlay, kTestAppId, kTestAppTitle);
        EXPECT_FALSE(results[0].GetSourceExtras());
      }));
}

TEST_F(AppDiscoveryServiceTest, GetArcAppsFromFetcher) {
  auto* app_discovery_service =
      AppDiscoveryServiceFactory::GetForProfile(profile());
  EXPECT_TRUE(app_discovery_service);

  std::vector<Result> fake_results;
  auto play_extras = std::make_unique<PlayExtras>(false);
  fake_results.emplace_back(Result(AppSource::kPlay, kTestAppId, kTestAppTitle,
                                   std::move(play_extras)));
  test_fetcher()->SetResults(std::move(fake_results));

  app_discovery_service->GetApps(
      ResultType::kRecommendedArcApps,
      base::BindLambdaForTesting([this](std::vector<Result> results) {
        EXPECT_EQ(results.size(), 1u);
        CheckResult(results[0], AppSource::kPlay, kTestAppId, kTestAppTitle);
        EXPECT_TRUE(results[0].GetSourceExtras());
        auto* play_extras = results[0].GetSourceExtras()->AsPlayExtras();
        EXPECT_TRUE(play_extras);
        EXPECT_EQ(play_extras->GetPreviouslyInstalled(), false);
      }));
}

}  // namespace apps
