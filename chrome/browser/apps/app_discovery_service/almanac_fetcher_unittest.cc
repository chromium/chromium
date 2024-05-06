// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"

#include "base/callback_list.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_proto_loader.h"
#include "base/time/time.h"
#include "chrome/browser/apps/almanac_api_client/mock_almanac_icon_cache.h"
#include "chrome/browser/apps/app_discovery_service/almanac_api/launcher_app.pb.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_endpoint.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace apps {
namespace {
using testing::_;
using ::testing::Invoke;

constexpr char kOneApp[] =
    R"pb(app_groups: {
           uuid: "cf2890ac-486f-11ee-be56-0242ac120002"
           name: "app_name"
           badge_text: "GeForce NOW"
           action_link: "https://game-deeplink.com/cf2be56486f11ee"
           icons: {
             url: "http://icon/"
             width_in_pixels: 20
             is_masking_allowed: true
           }
         })pb";
constexpr char kTwoValidApps[] =
    R"pb(app_groups: {
           uuid: "e42c6c70-7732-437f-b2e7-0d17036b8cc1"
           name: "app_name1"
           badge_text: "GeForce NOW"
           action_link: "https://game-deeplink.com/jrioj324j2095245234320o"
           icons: {
             url: "http://icon1/"
             width_in_pixels: 20
             is_masking_allowed: true
           }
         }
         app_groups: {
           uuid: "d8eb7470-9d43-472c-aa49-125f5c3111d4"
           name: "app_name2"
           badge_text: "Play"
           action_link: "https://game-deeplink.com/reijarowaiore131983u12jkljs893"
           icons: {
             url: "http://icon2/"
             width_in_pixels: 30
             is_masking_allowed: false
           }
         }
         # The invalid games below are filtered out.
         app_groups: {
           uuid: "invalid-game1-id"
           name: ""
           action_link: "https://invalid-game1-deeplink.com/"
           icons: {
             url: "http://icon3/"
             width_in_pixels: 30
             is_masking_allowed: false
           }
         }
         app_groups: {
           uuid: "invalid-game2-id"
           name: "invalid-game2"
           action_link: ""
           icons: {
             url: "http://icon3/"
             width_in_pixels: 30
             is_masking_allowed: false
           }
         }
         app_groups: {
           uuid: "invalid-game3-id"
           name: "invalid-game3"
           action_link: "https://invalid-game3-deeplink.com/"
         })pb";

// The path is equivalent to $root_gen_dir, where the protos are generated.
base::FilePath GetTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
}

void SetServerResponse(network::TestURLLoaderFactory& url_loader_factory,
                       base::TestProtoLoader* proto_loader,
                       const std::string& text_proto,
                       net::HttpStatusCode status = net::HTTP_OK) {
  std::string serialized_message;
  proto_loader->ParseFromText(text_proto, serialized_message);
  url_loader_factory.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(), serialized_message,
      status);
}

gfx::Image& GetTestImage(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

class AlmanacFetcherTest : public testing::Test {
 public:
  AlmanacFetcherTest() {
    feature_list_.InitWithFeatures({chromeos::features::kCloudGamingDevice},
                                   {});
    launcher_app_descriptor_ = GetTestDataRoot().Append(FILE_PATH_LITERAL(
        "chrome/browser/apps/app_discovery_service/almanac_api/"
        "launcher_app.descriptor"));
  }

  void SetUp() override {
    testing::Test::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    auto icon_cache = std::make_unique<MockAlmanacIconCache>();
    icon_cache_ = icon_cache.get();
    AlmanacFetcher::SetSkipApiKeyCheckForTesting(true);
    almanac_fetcher_ =
        std::make_unique<AlmanacFetcher>(profile(), std::move(icon_cache));
    proto_loader_ = std::make_unique<base::TestProtoLoader>(
        launcher_app_descriptor_, "apps.proto.LauncherAppResponse");
    SetServerResponse(url_loader_factory_, proto_loader(), kTwoValidApps);
  }

  void TearDown() override { icon_cache_ = nullptr; }

  TestingProfile* profile() { return profile_.get(); }
  AlmanacFetcher* almanac_fetcher() { return almanac_fetcher_.get(); }
  base::TestProtoLoader* proto_loader() { return proto_loader_.get(); }

  // The path of the descriptor file for the launcher app proto.
  base::FilePath launcher_app_descriptor_;

  network::TestURLLoaderFactory url_loader_factory_;

  raw_ptr<MockAlmanacIconCache> icon_cache_;

 private:
  // BrowserTaskEnvironment has to be the first member or test will break.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AlmanacFetcher> almanac_fetcher_;
  std::unique_ptr<base::TestProtoLoader> proto_loader_;
  base::test::ScopedFeatureList feature_list_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

TEST_F(AlmanacFetcherTest, RegisterForUpdatesTwoApps) {
  base::Time before_download = almanac_fetcher()->GetLastAppsUpdateTime();
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  std::vector<Result> results = waiter.Take();
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
  EXPECT_EQ(results[0].GetIconId(), "http://icon1/");
  EXPECT_EQ(results[0].GetAppTitle(), u"app_name1");
  ASSERT_TRUE(results[0].GetSourceExtras());
  auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
  ASSERT_TRUE(game_extras);
  EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
  EXPECT_EQ(game_extras->GetDeeplinkUrl(),
            GURL("https://game-deeplink.com/jrioj324j2095245234320o"));
  EXPECT_TRUE(game_extras->GetIsIconMaskingAllowed());

  EXPECT_EQ(results[1].GetAppSource(), AppSource::kGames);
  EXPECT_EQ(results[1].GetIconId(), "http://icon2/");
  EXPECT_EQ(results[1].GetAppTitle(), u"app_name2");
  EXPECT_TRUE(results[1].GetSourceExtras());
  game_extras = results[1].GetSourceExtras()->AsGameExtras();
  ASSERT_TRUE(game_extras);
  EXPECT_EQ(game_extras->GetSource(), u"Play");
  EXPECT_EQ(game_extras->GetDeeplinkUrl(),
            GURL("https://game-deeplink.com/"
                 "reijarowaiore131983u12jkljs893"));
  EXPECT_FALSE(game_extras->GetIsIconMaskingAllowed());
  EXPECT_GT(almanac_fetcher()->GetLastAppsUpdateTime(), before_download);
}

TEST_F(AlmanacFetcherTest, RegisterForUpdatesNoApps) {
  base::Time before_download = almanac_fetcher()->GetLastAppsUpdateTime();
  proto::LauncherAppResponse proto;
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(),
      proto.SerializeAsString());
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  EXPECT_EQ(waiter.Take().size(), 0u);
  EXPECT_GT(almanac_fetcher()->GetLastAppsUpdateTime(), before_download);
}

TEST_F(AlmanacFetcherTest, RegisterForUpdatesAfterUpdate) {
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  std::vector<Result> results = waiter.Take();
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].GetAppTitle(), u"app_name1");
  EXPECT_EQ(results[1].GetAppTitle(), u"app_name2");

  // Confirm a new subscriber also gets notified as the apps are available.
  base::test::TestFuture<const std::vector<Result>&> waiter2;
  subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter2.GetRepeatingCallback());
  EXPECT_EQ(waiter2.Take().size(), 2u);
}

TEST_F(AlmanacFetcherTest, RegisterForUpdatesReadFromDisk) {
  base::Time before_download = almanac_fetcher()->GetLastAppsUpdateTime();
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  std::vector<Result> results = waiter.Take();
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].GetAppTitle(), u"app_name1");
  EXPECT_EQ(results[1].GetAppTitle(), u"app_name2");

  base::Time after_download = almanac_fetcher()->GetLastAppsUpdateTime();
  EXPECT_GT(after_download, before_download);

  // Read from disk as we've just successfully finished a download.
  AlmanacFetcher almanac_fetcher2(profile(), nullptr);
  base::test::TestFuture<const std::vector<Result>&> waiter2;
  base::CallbackListSubscription subscription2 =
      almanac_fetcher2.RegisterForAppUpdates(waiter2.GetRepeatingCallback());
  EXPECT_EQ(waiter2.Take().size(), 2u);
  EXPECT_EQ(almanac_fetcher()->GetLastAppsUpdateTime(), after_download);
}

TEST_F(AlmanacFetcherTest, RegisterForUpdatesServerCallFails) {
  base::Time before_download = almanac_fetcher()->GetLastAppsUpdateTime();
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  std::vector<Result> results = waiter.Take();
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].GetAppTitle(), u"app_name1");
  EXPECT_EQ(results[1].GetAppTitle(), u"app_name2");
  EXPECT_GT(almanac_fetcher()->GetLastAppsUpdateTime(), before_download);

  // Re-set to initiate a new login.
  almanac_fetcher()->SetLastAppsUpdateTime(before_download);
  SetServerResponse(url_loader_factory_, proto_loader(), kOneApp,
                    net::HTTP_INTERNAL_SERVER_ERROR);
  AlmanacFetcher almanac_fetcher2(profile(), nullptr);
  base::test::TestFuture<const std::vector<Result>&> waiter2;
  base::CallbackListSubscription subscription2 =
      almanac_fetcher2.RegisterForAppUpdates(waiter2.GetRepeatingCallback());
  EXPECT_EQ(waiter2.Take().size(), 2u);
  EXPECT_EQ(almanac_fetcher()->GetLastAppsUpdateTime(), before_download);
}

TEST_F(AlmanacFetcherTest, GetAppsUpdateOnSecondLogin) {
  // Check there are no apps before the update.
  almanac_fetcher()->GetApps(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kErrorRequestFailed);
        EXPECT_EQ(results.size(), 0u);
      }));

  base::Time before_download = almanac_fetcher()->GetLastAppsUpdateTime();
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  std::vector<Result> results = waiter.Take();
  EXPECT_EQ(results.size(), 2u);
  EXPECT_GT(almanac_fetcher()->GetLastAppsUpdateTime(), before_download);

  almanac_fetcher()->GetApps(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kSuccess);
        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
        EXPECT_EQ(results[0].GetIconId(), "http://icon1/");
        EXPECT_EQ(results[0].GetAppTitle(), u"app_name1");
        ASSERT_TRUE(results[0].GetSourceExtras());
        auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
        ASSERT_TRUE(game_extras);
        EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
        EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                  GURL("https://game-deeplink.com/jrioj324j2095245234320o"));
        EXPECT_TRUE(game_extras->GetIsIconMaskingAllowed());

        EXPECT_EQ(results[1].GetAppSource(), AppSource::kGames);
        EXPECT_EQ(results[1].GetIconId(), "http://icon2/");
        EXPECT_EQ(results[1].GetAppTitle(), u"app_name2");
        EXPECT_TRUE(results[1].GetSourceExtras());
        game_extras = results[1].GetSourceExtras()->AsGameExtras();
        ASSERT_TRUE(game_extras);
        EXPECT_EQ(game_extras->GetSource(), u"Play");
        EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                  GURL("https://game-deeplink.com/"
                       "reijarowaiore131983u12jkljs893"));
        EXPECT_FALSE(game_extras->GetIsIconMaskingAllowed());
      }));

  // Re-set to initiate a new login.
  almanac_fetcher()->SetLastAppsUpdateTime(before_download);
  SetServerResponse(url_loader_factory_, proto_loader(), kOneApp);
  AlmanacFetcher almanac_fetcher2(profile(), nullptr);
  base::test::TestFuture<const std::vector<Result>&> waiter2;
  base::CallbackListSubscription subscription2 =
      almanac_fetcher2.RegisterForAppUpdates(waiter2.GetRepeatingCallback());
  // Check the apps are overwritten on the second login.
  EXPECT_EQ(waiter2.Take().size(), 1u);
  EXPECT_GT(almanac_fetcher()->GetLastAppsUpdateTime(), before_download);
  almanac_fetcher2.GetApps(base::BindLambdaForTesting(
      [](const std::vector<Result>& results, DiscoveryError error) {
        EXPECT_EQ(error, DiscoveryError::kSuccess);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].GetAppSource(), AppSource::kGames);
        EXPECT_EQ(results[0].GetIconId(), "http://icon/");
        EXPECT_EQ(results[0].GetAppTitle(), u"app_name");
        ASSERT_TRUE(results[0].GetSourceExtras());
        auto* game_extras = results[0].GetSourceExtras()->AsGameExtras();
        ASSERT_TRUE(game_extras);
        EXPECT_EQ(game_extras->GetSource(), u"GeForce NOW");
        EXPECT_EQ(game_extras->GetDeeplinkUrl(),
                  GURL("https://game-deeplink.com/cf2be56486f11ee"));
        EXPECT_TRUE(game_extras->GetIsIconMaskingAllowed());
      }));
}

TEST_F(AlmanacFetcherTest, GetIconSuccess) {
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  ASSERT_EQ(waiter.Take().size(), 2u);

  base::test::TestFuture<gfx::ImageSkia, apps::DiscoveryError> result;
  std::string icon_url = "https://icon";
  gfx::Image expected_image = GetTestImage(IDR_DEFAULT_FAVICON);
  EXPECT_CALL(*icon_cache_, GetIcon(GURL(icon_url), _))
      .WillOnce(
          Invoke([&expected_image](
                     const GURL&,
                     base::OnceCallback<void(const gfx::Image&)> callback) {
            std::move(callback).Run(expected_image);
          }));
  almanac_fetcher()->GetIcon(
      icon_url, /*size_hint_in_dip=*/32,
      result.GetCallback<const gfx::ImageSkia&, apps::DiscoveryError>());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(gfx::Image(result.Get<0>()), expected_image));
  EXPECT_EQ(result.Get<1>(), DiscoveryError::kSuccess);
}

TEST_F(AlmanacFetcherTest, GetIconError) {
  base::test::TestFuture<const std::vector<Result>&> waiter;
  base::CallbackListSubscription subscription =
      almanac_fetcher()->RegisterForAppUpdates(waiter.GetRepeatingCallback());
  ASSERT_EQ(waiter.Take().size(), 2u);

  base::test::TestFuture<gfx::ImageSkia, apps::DiscoveryError> result;
  std::string icon_url = "https://icon";
  EXPECT_CALL(*icon_cache_, GetIcon(GURL(icon_url), _))
      .WillOnce(
          Invoke([&](const GURL&,
                     base::OnceCallback<void(const gfx::Image&)> callback) {
            std::move(callback).Run(gfx::Image());
          }));
  almanac_fetcher()->GetIcon(
      icon_url, /*size_hint_in_dip=*/32,
      result.GetCallback<const gfx::ImageSkia&, apps::DiscoveryError>());
  EXPECT_TRUE(result.Get<0>().isNull());
  EXPECT_EQ(result.Get<1>(), DiscoveryError::kErrorRequestFailed);
}

TEST_F(AlmanacFetcherTest, GetIconNoAppsError) {
  base::test::TestFuture<gfx::ImageSkia, apps::DiscoveryError> result;
  std::string icon_url = "https://icon";
  EXPECT_CALL(*icon_cache_, GetIcon(GURL(icon_url), _)).Times(0);
  almanac_fetcher()->GetIcon(
      icon_url, /*size_hint_in_dip=*/32,
      result.GetCallback<const gfx::ImageSkia&, apps::DiscoveryError>());
  EXPECT_TRUE(result.Get<0>().isNull());
  EXPECT_EQ(result.Get<1>(), DiscoveryError::kErrorRequestFailed);
}
}  // namespace
}  // namespace apps
