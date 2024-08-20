// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/wallpaper/arc_wallpaper_service.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_wallpaper_instance.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
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
        fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}

  ArcWallpaperServiceTest(const ArcWallpaperServiceTest&) = delete;
  ArcWallpaperServiceTest& operator=(const ArcWallpaperServiceTest&) = delete;

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
    fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user_manager::StubAccountId());
    ASSERT_TRUE(fake_user_manager_->GetPrimaryUser());

    // Wallpaper
    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
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
    ash::SystemSaltGetter::Initialize();
    ash::SystemSaltGetter::Get()->SetRawSaltForTesting({0x01, 0x02, 0x03});
  }

  void TearDown() override {
    arc_service_manager_.arc_bridge_service()->wallpaper()->CloseInstance(
        wallpaper_instance_.get());
    arc_service_manager_.set_browser_context(nullptr);
    wallpaper_instance_.reset();

    wallpaper_controller_client_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    ash::SystemSaltGetter::Shutdown();
  }

 protected:
  raw_ptr<arc::ArcWallpaperService, DanglingUntriaged> service_ = nullptr;
  std::unique_ptr<arc::FakeWallpaperInstance> wallpaper_instance_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  TestWallpaperController test_wallpaper_controller_;

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  arc::ArcServiceManager arc_service_manager_;
  TestingPrefServiceSimple pref_service_;
  // testing_profile_ needs to be deleted before arc_service_manager_ and
  // pref_service_.
  TestingProfile testing_profile_;
};

}  // namespace

TEST_F(ArcWallpaperServiceTest, SetDefaultWallpaper) {
  test_wallpaper_controller_.ClearCounts();
  base::HistogramTester histogram_tester;

  service_->SetDefaultWallpaper();

  EXPECT_EQ(1, test_wallpaper_controller_.set_default_wallpaper_count());
  histogram_tester.ExpectUniqueSample("Arc.WallpaperApiUsage", 1,
                                      /*expected_bucket_count=*/1);
}

TEST_F(ArcWallpaperServiceTest, SetAndGetWallpaper) {
  service_->SetDecodeRequestSenderForTesting(
      std::make_unique<SuccessDecodeRequestSender>());
  std::vector<uint8_t> bytes;
  test_wallpaper_controller_.SetCurrentUser(user_manager::StubAccountId());
  base::HistogramTester histogram_tester;

  service_->SetWallpaper(bytes, 10 /*wallpaper_id=*/);

  ASSERT_EQ(1u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(10, wallpaper_instance_->changed_ids()[0]);
  ASSERT_EQ(1, test_wallpaper_controller_.get_third_party_wallpaper_count());
  histogram_tester.ExpectUniqueSample("Arc.WallpaperApiUsage", 0,
                                      /*expected_bucket_count=*/1);

  service_->GetWallpaper(
      base::BindOnce([](std::vector<uint8_t>* out,
                        const std::vector<uint8_t>& bytes) { *out = bytes; },
                     &bytes));
  content::RunAllTasksUntilIdle();

  ASSERT_NE(0u, bytes.size());
  histogram_tester.ExpectBucketCount("Arc.WallpaperApiUsage", 2,
                                     /*expected_count=*/1);
}

TEST_F(ArcWallpaperServiceTest, SetWallpaperFailure) {
  service_->SetDecodeRequestSenderForTesting(
      std::make_unique<FailureDecodeRequestSender>());
  test_wallpaper_controller_.SetCurrentUser(user_manager::StubAccountId());
  std::vector<uint8_t> bytes;
  service_->SetWallpaper(bytes, 10 /*wallpaper_id=*/);

  // For failure case, ArcWallpaperService reports that wallpaper is changed to
  // requested wallpaper (ID=10), then reports that the wallpaper is changed
  // back to the previous wallpaper immediately.
  ASSERT_EQ(2u, wallpaper_instance_->changed_ids().size());
  EXPECT_EQ(10, wallpaper_instance_->changed_ids()[0]);
  EXPECT_EQ(-1, wallpaper_instance_->changed_ids()[1]);
  ASSERT_EQ(0, test_wallpaper_controller_.get_third_party_wallpaper_count());
}

// For crbug.com/1325863
TEST_F(ArcWallpaperServiceTest, GetEmptyWallpaper) {
  test_wallpaper_controller_.ShowWallpaperImage(gfx::ImageSkia{});

  service_->GetWallpaper(base::DoNothing());
  content::RunAllTasksUntilIdle();
}
