// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/wallpaper_ash.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/crosapi/wallpaper_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "components/crash/core/common/crash_key.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace crosapi {

std::vector<uint8_t> CreateJpeg() {
  constexpr int kWidth = 100;
  constexpr int kHeight = 100;
  const SkColor kRed = SkColorSetRGB(255, 0, 0);
  constexpr int kQuality = 80;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kWidth, kHeight);
  bitmap.eraseColor(kRed);

  std::vector<uint8_t> jpg_data;

  bool encoded = gfx::JPEGCodec::Encode(bitmap, kQuality, &jpg_data);
  if (!encoded) {
    LOG(ERROR) << "Failed to encode a sample JPEG wallpaper image";
    jpg_data.clear();
  }

  return jpg_data;
}

class WallpaperAshTest : public testing::Test {
 public:
  WallpaperAshTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  WallpaperAshTest(const WallpaperAshTest&) = delete;
  ~WallpaperAshTest() override = default;

  void SetUp() override {
    // Log in user.
    ash::LoginState::Initialize();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ = testing_profile_manager_.CreateTestingProfile("profile");
    fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user_manager::StubAccountId());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        fake_user_manager_->GetPrimaryUser(), testing_profile_);

    // Create Wallpaper Controller Client.
    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    crash_reporter::InitializeCrashKeysForTesting();
  }

  void TearDown() override {
    crash_reporter::ResetCrashKeysForTesting();
    ash::LoginState::Shutdown();
    wallpaper_controller_client_.reset();
  }

 protected:
  // We need to satisfy DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment_;
  WallpaperAsh wallpaper_ash_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged | ExperimentalAsh> testing_profile_;
  TestWallpaperController test_wallpaper_controller_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
};

TEST_F(WallpaperAshTest, SetWallpaper) {
  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();
  settings->data = CreateJpeg();
  test_wallpaper_controller_.SetCurrentUser(user_manager::StubAccountId());

  base::RunLoop loop;
  wallpaper_ash_.SetWallpaper(
      std::move(settings), "extension_id", "extension_name",
      base::BindLambdaForTesting(
          [&loop](const crosapi::mojom::SetWallpaperResultPtr result) {
            ASSERT_TRUE(result->is_thumbnail_data());
            ASSERT_FALSE(result->get_thumbnail_data().empty());
            loop.Quit();
          }));
  loop.Run();

  ASSERT_EQ(1, test_wallpaper_controller_.get_third_party_wallpaper_count());
}

TEST_F(WallpaperAshTest, SetWallpaper_InvalidWallpaper) {
  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();
  test_wallpaper_controller_.SetCurrentUser(user_manager::StubAccountId());
  // Created invalid data by not adding a wallpaper image to the settings data.

  base::RunLoop loop;
  wallpaper_ash_.SetWallpaper(
      std::move(settings), "extension_id", "extension_name",
      base::BindLambdaForTesting(
          [&loop](const crosapi::mojom::SetWallpaperResultPtr result) {
            ASSERT_TRUE(result->is_error_message());
            ASSERT_EQ("Decoding wallpaper data failed.",
                      result->get_error_message());
            loop.Quit();
          }));
  loop.Run();

  ASSERT_EQ(0, test_wallpaper_controller_.get_third_party_wallpaper_count());
}

TEST_F(WallpaperAshTest, SetWallpaper_InvalidUser) {
  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();
  settings->data = CreateJpeg();
  // Setting the wallpaper fails because we haven't set the current user.

  base::RunLoop loop;
  wallpaper_ash_.SetWallpaper(
      std::move(settings), "extension_id", "extension_name",
      base::BindLambdaForTesting(
          [&loop](const crosapi::mojom::SetWallpaperResultPtr result) {
            ASSERT_TRUE(result->is_error_message());
            ASSERT_EQ("Setting the wallpaper failed due to user permissions.",
                      result->get_error_message());
            loop.Quit();
          }));
  loop.Run();

  ASSERT_EQ(0, test_wallpaper_controller_.get_third_party_wallpaper_count());
}

TEST_F(WallpaperAshTest, SetWallpaper_CrashKeys_OnSuccess) {
  test_wallpaper_controller_.SetCurrentUser(user_manager::StubAccountId());

  // Create valid settings.
  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();
  settings->data = CreateJpeg();

  // Invoke SetWallpaper(). It will respond with success.
  base::RunLoop loop;
  wallpaper_ash_.SetWallpaper(
      std::move(settings), "extension_id", "extension_name",
      base::BindLambdaForTesting(
          [&loop](const crosapi::mojom::SetWallpaperResultPtr result) {
            ASSERT_FALSE(result->is_error_message());
            loop.Quit();
          }));

  // Crash key is set when function starts running.
  using crash_reporter::GetCrashKeyValue;
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "extension_id");

  // Crash key is cleared after function completes.
  loop.Run();
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "");
}

TEST_F(WallpaperAshTest, SetWallpaper_CrashKeys_OnError) {
  test_wallpaper_controller_.SetCurrentUser(user_manager::StubAccountId());

  // Create invalid data by not adding a wallpaper image to the settings data.
  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();

  // Invoke SetWallpaper(). It will respond with an error.
  base::RunLoop loop;
  wallpaper_ash_.SetWallpaper(
      std::move(settings), "extension_id", "extension_name",
      base::BindLambdaForTesting(
          [&loop](const crosapi::mojom::SetWallpaperResultPtr result) {
            ASSERT_TRUE(result->is_error_message());
            loop.Quit();
          }));

  // Crash key is set when function starts running.
  using crash_reporter::GetCrashKeyValue;
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "extension_id");

  // Crash key is cleared after function completes.
  loop.Run();
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "");
}

}  // namespace crosapi
