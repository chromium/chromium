// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using ::component_updater::FakeCrOSComponentManager;

constexpr char kCampaignsComponent[] = "growth-campaigns";

constexpr char kTestCampaignsComponentMountedPath[] =
    "/run/imageloader/growth_campaigns";

}  // namespace

class CampaignsManagerClientTest : public testing::Test {
 public:
  CampaignsManagerClientTest()
      : browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {}

  CampaignsManagerClientTest(const CampaignsManagerClientTest&) = delete;
  CampaignsManagerClientTest& operator=(const CampaignsManagerClientTest&) =
      delete;

  ~CampaignsManagerClientTest() override = default;

  void SetUp() override {
    InitializeCrosComponentManager();

    campaigns_manager_client_ = std::make_unique<CampaignsManagerClientImpl>();
  }

  void TearDown() override {
    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
  }

 protected:
  void InitializeCrosComponentManager() {
    auto fake_cros_component_manager =
        base::MakeRefCounted<FakeCrOSComponentManager>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kCampaignsComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeCrosComponentManager(
        std::move(fake_cros_component_manager));
  }

  bool FinishComponentLoad(
      const base::FilePath& mount_path,
      component_updater::FakeCrOSComponentManager::Error error) {
    EXPECT_TRUE(
        cros_component_manager_->HasPendingInstall(kCampaignsComponent));
    EXPECT_TRUE(cros_component_manager_->UpdateRequested(kCampaignsComponent));

    auto install_path = base::FilePath();
    if (error == component_updater::FakeCrOSComponentManager::Error::NONE) {
      install_path = base::FilePath("/dev/null");
    }

    return cros_component_manager_->FinishLoadRequest(
        kCampaignsComponent,
        FakeCrOSComponentManager::ComponentInfo(
            /*load_response=*/error, install_path, mount_path));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CampaignsManagerClientImpl> campaigns_manager_client_;
  raw_ptr<FakeCrOSComponentManager, ExperimentalAsh> cros_component_manager_ =
      nullptr;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;
};

TEST_F(CampaignsManagerClientTest, LoadCampaignsComponent) {
  campaigns_manager_client_->LoadCampaignsComponent(base::BindLambdaForTesting(
      [](const absl::optional<const base::FilePath>& file_path) {
        // ASSERT_TRUE(file_path.has_value());
        ASSERT_TRUE(file_path.has_value());
        ASSERT_EQ(file_path.value().value(),
                  kTestCampaignsComponentMountedPath);
      }));

  ASSERT_TRUE(FinishComponentLoad(
      base::FilePath(kTestCampaignsComponentMountedPath),
      component_updater::CrOSComponentManager::Error::NONE));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}

TEST_F(CampaignsManagerClientTest, LoadCampaignsComponentFailed) {
  campaigns_manager_client_->LoadCampaignsComponent(base::BindLambdaForTesting(
      [](const absl::optional<const base::FilePath>& file_path) {
        ASSERT_FALSE(file_path.has_value());
      }));

  ASSERT_TRUE(FinishComponentLoad(
      base::FilePath(),
      component_updater::CrOSComponentManager::Error::NOT_FOUND));
  EXPECT_FALSE(cros_component_manager_->HasPendingInstall(kCampaignsComponent));
}
