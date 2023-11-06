// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/token.h"
#include "build/build_config.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

using testing::SaveArg;

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD1(SetBackgroundToLocalResourceWithId, void(const base::Token&));
  MOCK_METHOD1(UpdateCustomLocalBackgroundColorAsync, void(const gfx::Image&));
  MOCK_METHOD0(IsCustomBackgroundDisabledByPolicy, bool());
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      NtpCustomBackgroundServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockNtpCustomBackgroundService>>(
            Profile::FromBrowserContext(context));
      }));
  auto profile = profile_builder.Build();
  return profile;
}

}  // namespace

class WallpaperSearchBackgroundManagerTest : public testing::Test {
 public:
  WallpaperSearchBackgroundManagerTest()
      : profile_(MakeTestingProfile()),
        mock_ntp_custom_background_service_(
            static_cast<MockNtpCustomBackgroundService*>(
                NtpCustomBackgroundServiceFactory::GetForProfile(
                    profile_.get()))) {}

  void SetUp() override {
    wallpaper_search_background_manager_ =
        std::make_unique<WallpaperSearchBackgroundManager>(profile_.get());
  }

  TestingProfile& profile() { return *profile_; }
  MockNtpCustomBackgroundService& mock_ntp_custom_background_service() {
    return *mock_ntp_custom_background_service_;
  }
  WallpaperSearchBackgroundManager& wallpaper_search_background_manager() {
    return *wallpaper_search_background_manager_;
  }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockNtpCustomBackgroundService> mock_ntp_custom_background_service_;
  std::unique_ptr<WallpaperSearchBackgroundManager>
      wallpaper_search_background_manager_;
};

TEST_F(WallpaperSearchBackgroundManagerTest, SetLocalBackgroundImage) {
  gfx::Image image_arg;
  base::Token token_arg;
  ON_CALL(mock_ntp_custom_background_service(),
          IsCustomBackgroundDisabledByPolicy)
      .WillByDefault(testing::Return(false));
  EXPECT_CALL(mock_ntp_custom_background_service(),
              SetBackgroundToLocalResourceWithId)
      .WillOnce(SaveArg<0>(&token_arg));
  EXPECT_CALL(mock_ntp_custom_background_service(),
              UpdateCustomLocalBackgroundColorAsync)
      .WillOnce(SaveArg<0>(&image_arg));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  bitmap.eraseColor(SK_ColorRED);

  base::Token token = base::Token::CreateRandom();
  wallpaper_search_background_manager().SelectLocalBackgroundImage(token,
                                                                   bitmap);
  task_environment().RunUntilIdle();

  // Check that image file was created.
  bool file_exists = base::PathExists(profile().GetPath().AppendASCII(
      token.ToString() +
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
  EXPECT_TRUE(file_exists);

  // Check that the args were passed to |NtpCustomBackgroundService|.
  EXPECT_EQ(token_arg.high(), token.high());
  EXPECT_EQ(token_arg.low(), token.low());
  EXPECT_EQ(SK_ColorRED, image_arg.ToSkBitmap()->getColor(0, 0));
}
