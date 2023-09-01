// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"

#include "base/callback_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_proto_loader.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {
namespace {

constexpr char kOneApp[] =
    R"pb(app_groups: {
           uuid: "cf2890ac-486f-11ee-be56-0242ac120002"
           name: "group_name"
           app_instances: {
             name: "app_name"
             package_id: "gfn:cf2be56486f11ee"
             app_id_for_platform: "cf2be56486f11ee"
             deeplink: "https://game-deeplink.com/cf2be56486f11ee"
             icons: {
               url: "http://icon/"
               width_in_pixels: 20
               mime_type: "image/png"
               is_masking_allowed: true
             }
           }
         })pb";
constexpr char kTwoApps[] =
    R"pb(app_groups: {
           uuid: "e42c6c70-7732-437f-b2e7-0d17036b8cc1"
           name: "group_name1"
           app_instances: {
             name: "app_name1"
             package_id: "gfn:jrioj324j2095245234320o"
             app_id_for_platform: "jrioj324j2095245234320o"
             deeplink: "https://game-deeplink.com/jrioj324j2095245234320o"
             icons: {
               url: "http://icon/"
               width_in_pixels: 20
               mime_type: "image/png"
               is_masking_allowed: true
             }
           }
         }
         app_groups: {
           uuid: "d8eb7470-9d43-472c-aa49-125f5c3111d4"
           name: "group_name2"
           app_instances: {
             name: "app_name2"
             package_id: "gfn:reijarowaiore131983u12jkljs893"
             app_id_for_platform: "reijarowaiore131983u12jkljs893"
             deeplink: "https://game-deeplink.com/reijarowaiore131983u12jkljs893"
             icons: {
               url: "http://icon2/"
               width_in_pixels: 30
               mime_type: "image/png"
               is_masking_allowed: false
             }
           }
         })pb";

// The path is equivalent to $root_gen_dir, where the protos are generated.
base::FilePath GetTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
}

class AlmanacFetcherTest : public testing::Test {
 public:
  AlmanacFetcherTest() {
    almanac_fetcher_ = std::make_unique<AlmanacFetcher>(&profile_);
    launcher_app_descriptor_ = GetTestDataRoot().Append(FILE_PATH_LITERAL(
        "chrome/browser/apps/app_discovery_service/almanac_api/"
        "launcher_app.descriptor"));
  }

  AlmanacFetcher* almanac_fetcher() { return almanac_fetcher_.get(); }

  // The path of the descriptor file for the launcher app proto.
  base::FilePath launcher_app_descriptor_;

 private:
  std::unique_ptr<AlmanacFetcher> almanac_fetcher_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(AlmanacFetcherTest, RegisterForUpdates) {
  bool update_verified = false;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(base::BindLambdaForTesting(
          [&update_verified](const std::vector<Result>& results) {
            EXPECT_EQ(results.size(), 2u);
            EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
            EXPECT_EQ(results[0].GetAppId(), "jrioj324j2095245234320o");
            EXPECT_EQ(results[0].GetAppTitle(), u"group_name1");
            EXPECT_TRUE(results[0].GetSourceExtras());
            auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
            EXPECT_TRUE(game_extras);
            EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
            EXPECT_EQ(
                game_extras->GetDeeplinkUrl(),
                GURL("https://game-deeplink.com/jrioj324j2095245234320o"));

            EXPECT_EQ(results[1].GetAppSource(), AppSource::kGames);
            EXPECT_EQ(results[1].GetAppId(), "reijarowaiore131983u12jkljs893");
            EXPECT_EQ(results[1].GetAppTitle(), u"group_name2");
            EXPECT_TRUE(results[1].GetSourceExtras());
            game_extras = results[1].GetSourceExtras()->AsGameExtras();
            EXPECT_TRUE(game_extras);
            EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
            EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                      GURL("https://game-deeplink.com/"
                           "reijarowaiore131983u12jkljs893"));
            update_verified = true;
          }));

  base::TestProtoLoader loader(launcher_app_descriptor_,
                               "apps.proto.LauncherAppResponse");
  std::string serialized_message;
  loader.ParseFromText(kTwoApps, serialized_message);
  proto::LauncherAppResponse proto;
  ASSERT_TRUE(proto.ParseFromString(serialized_message));

  almanac_fetcher()->OnAppsUpdate(proto);
  EXPECT_TRUE(update_verified);
}

TEST_F(AlmanacFetcherTest, RegisterForUpdatesNoApps) {
  bool update_verified = false;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(base::BindLambdaForTesting(
          [&update_verified](const std::vector<Result>& results) {
            EXPECT_EQ(results.size(), 0u);
            update_verified = true;
          }));

  proto::LauncherAppResponse proto;
  almanac_fetcher()->OnAppsUpdate(proto);
  EXPECT_TRUE(update_verified);
}

TEST_F(AlmanacFetcherTest, GetApps) {
  base::TestProtoLoader loader(launcher_app_descriptor_,
                               "apps.proto.LauncherAppResponse");
  std::string serialized_message;
  loader.ParseFromText(kTwoApps, serialized_message);
  proto::LauncherAppResponse proto;
  ASSERT_TRUE(proto.ParseFromString(serialized_message));

  // Check there are no apps before the update.
  almanac_fetcher()->GetApps(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kErrorRequestFailed);
        EXPECT_EQ(results.size(), 0u);
      }));

  almanac_fetcher()->OnAppsUpdate(proto);
  almanac_fetcher()->GetApps(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kSuccess);
        EXPECT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
        EXPECT_EQ(results[0].GetAppId(), "jrioj324j2095245234320o");
        EXPECT_EQ(results[0].GetAppTitle(), u"group_name1");
        EXPECT_TRUE(results[0].GetSourceExtras());
        auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
        EXPECT_TRUE(game_extras);
        EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
        EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                  GURL("https://game-deeplink.com/jrioj324j2095245234320o"));

        EXPECT_EQ(results[1].GetAppSource(), AppSource::kGames);
        EXPECT_EQ(results[1].GetAppId(), "reijarowaiore131983u12jkljs893");
        EXPECT_EQ(results[1].GetAppTitle(), u"group_name2");
        EXPECT_TRUE(results[1].GetSourceExtras());
        game_extras = results[1].GetSourceExtras()->AsGameExtras();
        EXPECT_TRUE(game_extras);
        EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
        EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                  GURL("https://game-deeplink.com/"
                       "reijarowaiore131983u12jkljs893"));
      }));

  // Check the apps are overwritten on the second update.
  loader.ParseFromText(kOneApp, serialized_message);
  ASSERT_TRUE(proto.ParseFromString(serialized_message));
  almanac_fetcher()->OnAppsUpdate(proto);
  almanac_fetcher()->GetApps(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kSuccess);
        EXPECT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
        EXPECT_EQ(results[0].GetAppId(), "cf2be56486f11ee");
        EXPECT_EQ(results[0].GetAppTitle(), u"group_name");
        EXPECT_TRUE(results[0].GetSourceExtras());
        auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
        EXPECT_TRUE(game_extras);
        EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
        EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                  GURL("https://game-deeplink.com/cf2be56486f11ee"));
      }));
}
}  // namespace
}  // namespace apps
