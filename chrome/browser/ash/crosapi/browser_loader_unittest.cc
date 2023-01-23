// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace crosapi {
namespace {

// Copied from browser_loader.cc
constexpr char kLacrosComponentName[] = "lacros-dogfood-dev";
constexpr char kLacrosComponentId[] = "ldobopbhiamakmncndpkeelenhdmgfhk";
constexpr char kLacrosMounterUpstartJob[] = "lacros_2dmounter";
constexpr char kLacrosUnmounterUpstartJob[] = "lacros_2dunmounter";

}  // namespace

class BrowserLoaderTest : public testing::Test {
 public:
  BrowserLoaderTest() {
    // Create dependencies for object under test.
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    component_manager_->set_supported_components({kLacrosComponentName});
    component_manager_->ResetComponentState(
        kLacrosComponentName,
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_ = std::make_unique<BrowserProcessPlatformPartTestApi>(
        g_browser_process->platform_part());
    browser_part_->InitializeCrosComponentManager(component_manager_);

    browser_loader_ = std::make_unique<BrowserLoader>(
        component_manager_, &mock_component_update_service_,
        &fake_upstart_client_);
  }

  ~BrowserLoaderTest() override {
    browser_part_->ShutdownCrosComponentManager();
  }

  // Public because this is test code.
  content::BrowserTaskEnvironment task_environment_;

 protected:
  component_updater::MockComponentUpdateService mock_component_update_service_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  ash::FakeUpstartClient fake_upstart_client_;
  std::unique_ptr<BrowserProcessPlatformPartTestApi> browser_part_;
  std::unique_ptr<BrowserLoader> browser_loader_;

 private:
  base::AutoReset<bool> set_lacros_enabled_ =
      browser_util::SetLacrosEnabledForTest(true);
};

TEST_F(BrowserLoaderTest, OnLoadSelectionQuicklyChooseRootfs) {
  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosMounterUpstartJob);
        *b = true;
        return true;
      },
      &callback_called));
  // Set `was_installed` to false, in order to quickly mount rootfs
  // lacros-chrome.
  browser_loader_->OnLoadSelection(
      base::BindOnce([](const base::FilePath&, LacrosSelection selection,
                        base::Version version) {
        EXPECT_EQ(LacrosSelection::kRootfs, selection);
      }),
      false);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionNeitherIsAvailable) {
  // Use stateful when a rootfs lacros-chrome version is invalid.
  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosUnmounterUpstartJob);
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in an invalid `base::Version`.
  browser_loader_->OnLoadVersionSelection(
      /*is_stateful_lacros_available=*/false,
      base::BindOnce([](const base::FilePath& path, LacrosSelection selection,
                        base::Version version) { EXPECT_TRUE(path.empty()); }),
      /*rootfs_lacros_version=*/base::Version());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionStatefulIsUnavailable) {
  // Use rootfs when a stateful lacros-chrome version is invalid.
  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosMounterUpstartJob);
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in an invalid `base::Version`.
  browser_loader_->OnLoadVersionSelection(
      /*is_stateful_lacros_available=*/false,
      base::BindOnce([](const base::FilePath& path, LacrosSelection selection,
                        base::Version version) {
        EXPECT_EQ(LacrosSelection::kRootfs, selection);
      }),
      /*rootfs_lacros_version=*/base::Version("2.0.0"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsUnavailable) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(base::StringPiece(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name,
           base::Version("1.0.0"), ""}}));

  // Use stateful when a rootfs lacros-chrome version is invalid.
  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosUnmounterUpstartJob);
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in an invalid `base::Version`.
  browser_loader_->OnLoadVersionSelection(
      /*is_stateful_lacros_available=*/true,
      base::BindOnce([](const base::FilePath& path, LacrosSelection selection,
                        base::Version version) {
        EXPECT_EQ(LacrosSelection::kStateful, selection);
      }),
      /*rootfs_lacros_version=*/base::Version());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsNewer) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(base::StringPiece(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name,
           base::Version("1.0.0"), ""}}));

  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosMounterUpstartJob);
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in a rootfs lacros-chrome version that is newer.
  browser_loader_->OnLoadVersionSelection(
      /*is_stateful_lacros_available=*/true,
      base::BindOnce([](const base::FilePath& path, LacrosSelection selection,
                        base::Version version) {
        EXPECT_EQ(LacrosSelection::kRootfs, selection);
      }),
      /*rootfs_lacros_version=*/base::Version("2.0.0"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfsIsOlder) {
  // Use stateful when a rootfs lacros-chrome version is older.
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(base::StringPiece(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name,
           base::Version("3.0.0"), ""}}));

  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        EXPECT_EQ(job, kLacrosUnmounterUpstartJob);
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in a rootfs lacros-chrome version that is older.
  browser_loader_->OnLoadVersionSelection(
      /*is_stateful_lacros_available=*/true,
      base::BindOnce([](const base::FilePath& path, LacrosSelection selection,
                        base::Version version) {
        EXPECT_EQ(LacrosSelection::kStateful, selection);
      }),
      /*rootfs_lacros_version=*/base::Version("2.0.0"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

}  // namespace crosapi
