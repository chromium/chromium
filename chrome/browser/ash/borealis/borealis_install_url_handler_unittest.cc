// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_install_url_handler.h"

#include <memory>

#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"
#include "chrome/browser/platform_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace borealis {
namespace {

class BorealisAppLauncherMock : public BorealisAppLauncher {
 public:
  BorealisAppLauncherMock() = default;
  ~BorealisAppLauncherMock() override = default;

  MOCK_METHOD(void,
              Launch,
              (std::string app_id,
               BorealisLaunchSource source,
               OnLaunchedCallback callback),
              ());
  MOCK_METHOD(void,
              Launch,
              (std::string app_id,
               const std::vector<std::string>& args,
               BorealisLaunchSource source,
               OnLaunchedCallback callback),
              ());
};

class BorealisInstallUrlHandlerTest : public testing::Test {
 public:
  BorealisInstallUrlHandlerTest() = default;
  ~BorealisInstallUrlHandlerTest() override = default;

  // Disallow copy and assign.
  BorealisInstallUrlHandlerTest(const BorealisInstallUrlHandlerTest&) = delete;
  BorealisInstallUrlHandlerTest& operator=(
      const BorealisInstallUrlHandlerTest&) = delete;

 protected:
  const GURL kInstallUrl{"chromeos-steam://install"};

  void SetUp() override {
    test_features_ = std::make_unique<BorealisFeatures>(&profile_);
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(&profile_);
    install_url_handler_ =
        std::make_unique<BorealisInstallUrlHandler>(&profile_);
    fake_service_ = BorealisServiceFake::UseFakeForTesting(&profile_);
    fake_service_->SetFeaturesForTesting(test_features_.get());
    fake_service_->SetAppLauncherForTesting(&app_launcher_);
    fake_service_->SetWindowManagerForTesting(borealis_window_manager_.get());

    scoped_allowance_ =
        std::make_unique<ScopedAllowBorealis>(&profile_, /*also_enable=*/false);

    ASSERT_FALSE(BorealisServiceFactory::GetForProfile(&profile_)
                     ->Features()
                     .IsEnabled());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BorealisFeatures> test_features_;
  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  testing::NaggyMock<BorealisAppLauncherMock> app_launcher_;
  std::unique_ptr<BorealisInstallUrlHandler> install_url_handler_;
  raw_ptr<BorealisServiceFake> fake_service_;
  std::unique_ptr<ScopedAllowBorealis> scoped_allowance_;
};

TEST_F(BorealisInstallUrlHandlerTest, LaunchesInstaller) {
  // Assert
  EXPECT_CALL(
      app_launcher_,
      Launch(kClientAppId, BorealisLaunchSource::kInstallUrl, testing::_))
      .Times(1);

  // Act
  guest_os::GuestOsUrlHandler::GetForUrl(&profile_, kInstallUrl)
      ->Handle(&profile_, kInstallUrl);
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisInstallUrlHandlerTest, InvokedFromExternalHandler) {
  // Assert
  EXPECT_CALL(
      app_launcher_,
      Launch(kClientAppId, BorealisLaunchSource::kInstallUrl, testing::_))
      .Times(1);

  // Act
  platform_util::OpenExternal(&profile_, kInstallUrl);
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace borealis
