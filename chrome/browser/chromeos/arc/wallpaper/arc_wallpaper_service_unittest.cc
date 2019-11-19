// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/wallpaper/arc_wallpaper_service.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_wallpaper_instance.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SuccessDecodeRequestSender
    : public arc::ArcWallpaperService::DecodeRequestSender {
 public:
  ~SuccessDecodeRequestSender() override = default;
  void SendDecodeRequest(ImageDecoder::ImageRequest* request,
                         const std::vector<uint8_t>& data) override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(256 /* width */, 256 /* height */);
    bitmap.eraseColor(SK_ColorRED);
    request->OnImageDecoded(bitmap);
  }
};

class FailureDecodeRequestSender
    : public arc::ArcWallpaperService::DecodeRequestSender {
 public:
  ~FailureDecodeRequestSender() override = default;
  void SendDecodeRequest(ImageDecoder::ImageRequest* request,
                         const std::vector<uint8_t>& data) override {
    request->OnDecodeImageFailed();
  }
};

class ArcWallpaperServiceTest : public testing::Test {
 public:
  ArcWallpaperServiceTest()
      : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()),
        user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(user_manager_)) {}
  ~ArcWallpaperServiceTest() override = default;

  void SetUp() override {
    // Prefs
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    pref_service_.registry()->RegisterDictionaryPref(
        ash::prefs::kUserWallpaperInfo);
    pref_service_.registry()->RegisterDictionaryPref(
        ash::prefs::kWallpaperColors);
    pref_service_.registry()->RegisterStringPref(
        prefs::kDeviceWallpaperImageFilePath, std::string());

    // User
    user_manager_->AddUser(user_manager::StubAccountId());
    user_manager_->LoginUser(user_manager::StubAccountId());
    ASSERT_TRUE(user_manager_->GetPrimaryUser());

    // Wallpaper
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClient>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    // Arc services
    arc_service_manager_.set_browser_context(&testing_profile_);
    service_ =
        arc::ArcWallpaperService::GetForBrowserContext(&testing_profile_);
    ASSERT_TRUE(service_);
    wallpaper_instance_ = std::make_unique<arc::FakeWallpaperInstance>();
    arc_service_manager_.arc_bridge_service()->wallpaper()->SetInstance(
        wallpaper_instance_.get());
    WaitForInstanceReady(
        arc_service_manager_.arc_bridge_service()->wallpaper());

    // Salt
    chromeos::SystemSaltGetter::Initialize();
    chromeos::SystemSaltGetter::Get()->SetRawSaltForTesting({0x01, 0x02, 0x03});
  }

  void TearDown() override {
    arc_service_manager_.arc_bridge_service()->wallpaper()->CloseInstance(
        wallpaper_instance_.get());
    wallpaper_instance_.reset();

    wallpaper_controller_client_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    chromeos::SystemSaltGetter::Shutdown();
  }

 protected:
  arc::ArcWallpaperService* service_ = nullptr;
  std::unique_ptr<arc::FakeWallpaperInstance> wallpaper_instance_ = nullptr;
  std::unique_ptr<WallpaperControllerClient> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  chromeos::FakeChromeUserManager* const user_manager_ = nullptr;
  user_manager::ScopedUserManager user_manager_enabler_;
  arc::ArcServiceManager arc_service_manager_;
  // testing_profile_ needs to be deleted before arc_service_manager_.
  TestingProfile testing_profile_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperServiceTest);
};

}  // namespace

TEST_F(ArcWallpaperServiceTest, SetDefaultWallpaper) {
  test_wallpaper_controller_.ClearCounts();
  service_->SetDefaultWallpaper();
  EXPECT_EQ(1, test_wallpaper_controller_.set_default_wallpaper_count());
}

TEST_F(ArcWallpaperServiceTest, SetAndGetWallpaper) {
  service_->SetDecodeRequestSenderForTesting(
      std::make_unique<SuccessDecodeRequestSender>());
  std::vector<uint8_t> bytes;
  service_->SetWallpaper(bytes, 10 /*wallpaper_id=*/);
  ASSERT_EQ(1u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(10, wallpaper_instance_->changed_ids()[0]);

  service_->GetWallpaper(
      base::BindOnce([](std::vector<uint8_t>* out,
                        const std::vector<uint8_t>& bytes) { *out = bytes; },
                     &bytes));
  content::RunAllTasksUntilIdle();
  ASSERT_NE(0u, bytes.size());
}

TEST_F(ArcWallpaperServiceTest, SetWallpaperFailure) {
  service_->SetDecodeRequestSenderForTesting(
      std::make_unique<FailureDecodeRequestSender>());
  std::vector<uint8_t> bytes;
  service_->SetWallpaper(bytes, 10 /*wallpaper_id=*/);

  // For failure case, ArcWallpaperService reports that wallpaper is changed to
  // requested wallpaper (ID=10), then reports that the wallpaper is changed
  // back to the previous wallpaper immediately.
  ASSERT_EQ(2u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(10, wallpaper_instance_->changed_ids()[0]);
  EXPECT_EQ(-1, wallpaper_instance_->changed_ids()[1]);
}
