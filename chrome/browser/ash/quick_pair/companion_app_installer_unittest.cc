// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/quick_pair/companion_app_installer.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_app_host.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "content/public/test/browser_task_environment.h"

namespace {
constexpr char kTestPackageName[] = "com.bose.monet";
}  // namespace

namespace ash {
namespace quick_pair {

class CompanionAppInstallerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    app_host_ = std::make_unique<arc::FakeAppHost>(
        arc_service_manager_->arc_bridge_service()->app());
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host_.get());
    installer_ = std::make_unique<CompanionAppInstaller>();
    arc_service_manager_->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());
  }

  void TearDown() override {
    app_instance_.reset();
    app_host_.reset();
    arc_service_manager_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::FakeAppHost> app_host_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<CompanionAppInstaller> installer_;
};

TEST_F(CompanionAppInstallerUnitTest, CorrectAppStateWhenAppIsInstallable) {
  base::MockCallback<
      base::OnceCallback<void(CompanionAppInstaller::CompanionAppState)>>
      on_companion_app_installed;
  EXPECT_CALL(
      on_companion_app_installed,
      Run(testing::Eq(
          CompanionAppInstaller::CompanionAppState::kAvailableToDownload)));

  app_instance_->set_is_installable(true);
  installer_->CheckAppState(kTestPackageName, on_companion_app_installed.Get());
}

TEST_F(CompanionAppInstallerUnitTest, CorrectAppStateWhenAppIsNotInstallable) {
  base::MockCallback<
      base::OnceCallback<void(CompanionAppInstaller::CompanionAppState)>>
      on_companion_app_installed;
  EXPECT_CALL(on_companion_app_installed,
              Run(testing::Eq(
                  CompanionAppInstaller::CompanionAppState::kNotAvailable)));

  app_instance_->set_is_installable(false);
  installer_->CheckAppState(kTestPackageName, on_companion_app_installed.Get());
}

TEST_F(CompanionAppInstallerUnitTest,
       CorrectAppStateWhenArcServiceManagerIsNull) {
  base::MockCallback<
      base::OnceCallback<void(CompanionAppInstaller::CompanionAppState)>>
      on_companion_app_installed;
  EXPECT_CALL(on_companion_app_installed,
              Run(testing::Eq(
                  CompanionAppInstaller::CompanionAppState::kNotAvailable)));

  arc_service_manager_ = nullptr;
  installer_->CheckAppState(kTestPackageName, on_companion_app_installed.Get());
  arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
}

}  // namespace quick_pair
}  // namespace ash
