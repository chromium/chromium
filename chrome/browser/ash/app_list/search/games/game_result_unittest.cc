// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/games/game_result.h"

#include <optional>

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

namespace app_list::test {
namespace {

using ::testing::_;
using ::testing::Return;

class MockAppListControllerDelegate
    : public ::test::TestAppListControllerDelegate {
 public:
  MOCK_METHOD((std::vector<std::string>),
              GetAppIdsForUrl,
              (Profile * profile,
               const GURL& url,
               bool exclude_browsers,
               bool exclude_browser_tab_apps),
              (override));
  MOCK_METHOD(void,
              LaunchAppWithUrl,
              (Profile * profile,
               const std::string& app_id,
               int32_t event_flags,
               const GURL& url,
               apps::LaunchSource launch_source),
              (override));
};

// Creates a 50x50 yellow test icon.
gfx::ImageSkia GetTestIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(50, 50);
  bitmap.eraseColor(SK_ColorYELLOW);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Creates the resized non-maskable variant of the test icon, which is a 22x22
// yellow square inside a 32x32 circle.
gfx::ImageSkia GetExpectedNonMaskableIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(22, 22);
  bitmap.eraseColor(SK_ColorYELLOW);
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      /*radius=*/16, SK_ColorWHITE, gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

// A mock app discovery service that can produce fake icons.
class TestAppDiscoveryService : public apps::AppDiscoveryService {
 public:
  explicit TestAppDiscoveryService(Profile* profile)
      : apps::AppDiscoveryService(profile) {}
  ~TestAppDiscoveryService() override = default;
  TestAppDiscoveryService(const TestAppDiscoveryService&) = delete;
  TestAppDiscoveryService& operator=(const TestAppDiscoveryService&) = delete;

  void GetIcon(const std::string& app_id,
               int32_t size_hint_in_dip,
               apps::ResultType result_type,
               apps::GetIconCallback callback) override {
    if (icons_available_) {
      std::move(callback).Run(GetTestIcon(), apps::DiscoveryError::kSuccess);
    } else {
      std::move(callback).Run(gfx::ImageSkia(),
                              apps::DiscoveryError::kErrorRequestFailed);
    }
  }

  void set_icons_available(bool icons_available) {
    icons_available_ = icons_available;
  }

 private:
  bool icons_available_ = true;
};

apps::Result MakeAppsResult(bool masking_allowed) {
  return apps::Result(apps::AppSource::kGames, "12345", u"Title",
                      std::make_unique<apps::GameExtras>(
                          u"SourceName", base::FilePath("/icons/test.png"),
                          masking_allowed, GURL("https://game.com/game")));
}

}  // namespace

class GameResultTest : public testing::Test {
 public:
  GameResultTest() {
    profile_ = std::make_unique<TestingProfile>();
    app_discovery_service_ =
        std::make_unique<TestAppDiscoveryService>(profile_.get());
  }

  ~GameResultTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;
  ::test::TestAppListControllerDelegate list_controller_;
  MockAppListControllerDelegate mock_list_controller_;
  std::unique_ptr<TestAppDiscoveryService> app_discovery_service_;
};

TEST_F(GameResultTest, Basic) {
  apps::Result apps_result = MakeAppsResult(/*masking_allowed=*/false);
  GameResult result(profile_.get(), &list_controller_,
                    app_discovery_service_.get(), apps_result, 0.6,
                    u"SomeGame");

  EXPECT_EQ(result.title(), u"Title");
  EXPECT_EQ(StringFromTextVector(result.details_text_vector()), u"SourceName");
  EXPECT_EQ(result.accessible_name(), u"Title, SourceName");
}

TEST_F(GameResultTest, Icons) {
  // The maskable icon should not be modified from its original form.
  apps::Result maskable_app = MakeAppsResult(/*masking_allowed=*/true);
  GameResult maskable_result(profile_.get(), &list_controller_,
                             app_discovery_service_.get(), maskable_app, 0.6,
                             u"SomeGame");

  EXPECT_EQ(maskable_result.icon().dimension, kAppIconDimension);
  EXPECT_EQ(maskable_result.icon().shape, ash::SearchResultIconShape::kCircle);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *maskable_result.icon().icon.Rasterize(nullptr).bitmap(),
      *GetTestIcon().bitmap()));

  // The non-maskable icon must be resized and placed on a white circle.
  apps::Result non_maskable_app = MakeAppsResult(/*masking_allowed=*/false);
  GameResult non_maskable_result(profile_.get(), &list_controller_,
                                 app_discovery_service_.get(), non_maskable_app,
                                 0.6, u"SomeGame");

  EXPECT_EQ(non_maskable_result.icon().dimension, kAppIconDimension);
  EXPECT_EQ(non_maskable_result.icon().shape,
            ash::SearchResultIconShape::kCircle);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *non_maskable_result.icon().icon.Rasterize(nullptr).bitmap(),
      *GetExpectedNonMaskableIcon().bitmap()));

  // If there is no icon, then the result should be filtered out.
  app_discovery_service_->set_icons_available(false);
  apps::Result no_icon_app = MakeAppsResult(/*masking_allowed=*/false);
  GameResult no_icon_result(profile_.get(), &list_controller_,
                            app_discovery_service_.get(), no_icon_app, 0.6,
                            u"SomeGame");

  EXPECT_TRUE(no_icon_result.scoring().filtered());
}

TEST_F(GameResultTest, OpensDeepLinkURLWhenAppNotFound) {
  apps::Result apps_result = MakeAppsResult(/*masking_allowed=*/false);
  GameResult result(profile_.get(), &mock_list_controller_,
                    app_discovery_service_.get(), apps_result, 0.6,
                    u"SomeGame");

  EXPECT_CALL(mock_list_controller_,
              GetAppIdsForUrl(_, GURL("https://game.com/game"), _, _))
      .WillOnce(Return(std::vector<std::string>{}));
  EXPECT_CALL(mock_list_controller_, LaunchAppWithUrl(_, _, _, _, _)).Times(0);

  result.Open(/*event_flags*/ 0);

  EXPECT_EQ(mock_list_controller_.last_opened_url(),
            GURL("https://game.com/game"));
}

TEST_F(GameResultTest, OpensDeepLinkURLWhenAppFoundButNotAllowed) {
  apps::Result apps_result = MakeAppsResult(/*masking_allowed=*/false);
  GameResult result(profile_.get(), &mock_list_controller_,
                    app_discovery_service_.get(), apps_result, 0.6,
                    u"SomeGame");

  std::string not_allowed_app_id{"not_allowed_app"};
  EXPECT_CALL(mock_list_controller_,
              GetAppIdsForUrl(_, GURL("https://game.com/game"), _, _))
      .WillOnce(Return(std::vector<std::string>{not_allowed_app_id}));
  EXPECT_CALL(mock_list_controller_, LaunchAppWithUrl(_, _, _, _, _)).Times(0);

  result.Open(/*event_flags*/ 0);

  EXPECT_EQ(mock_list_controller_.last_opened_url(),
            GURL("https://game.com/game"));
}

TEST_F(GameResultTest, LaunchesAppWhenAppFoundAndAllowed) {
  apps::Result apps_result = MakeAppsResult(/*masking_allowed=*/false);
  GameResult result(profile_.get(), &mock_list_controller_,
                    app_discovery_service_.get(), apps_result, 0.6,
                    u"SomeGame");

  std::string found_app_id{"egmafekfmcnknbdlbfbhafbllplmjlhn"};
  EXPECT_CALL(mock_list_controller_,
              GetAppIdsForUrl(_, GURL("https://game.com/game"), _, _))
      .WillOnce(Return(std::vector<std::string>{found_app_id}));
  EXPECT_CALL(
      mock_list_controller_,
      LaunchAppWithUrl(_, found_app_id, _, GURL("https://game.com/game"),
                       apps::LaunchSource::kFromAppListQuery))
      .Times(1);

  result.Open(/*event_flags*/ 0);

  EXPECT_EQ(mock_list_controller_.last_opened_url(), GURL(""));
}
}  // namespace app_list::test
