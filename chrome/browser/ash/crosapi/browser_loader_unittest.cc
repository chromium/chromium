// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

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
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using update_client::UpdateClient;

namespace crosapi {
namespace {

// Copied from browser_loader.cc
constexpr char kLacrosComponentName[] = "lacros-dogfood-dev";
constexpr char kLacrosComponentId[] = "ldobopbhiamakmncndpkeelenhdmgfhk";
constexpr char kLacrosMounterUpstartJob[] = "lacros_2dmounter";

}  // namespace

class BrowserLoaderTest : public testing::Test {
 public:
  BrowserLoaderTest() {
    browser_util::SetLacrosEnabledForTest(true);

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
    browser_util::SetLacrosEnabledForTest(false);
  }

  // Public because this is test code.
  content::BrowserTaskEnvironment task_environment_;

 protected:
  component_updater::MockComponentUpdateService mock_component_update_service_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  chromeos::FakeUpstartClient fake_upstart_client_;
  std::unique_ptr<BrowserProcessPlatformPartTestApi> browser_part_;
  std::unique_ptr<BrowserLoader> browser_loader_;
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
      base::BindOnce([](const base::FilePath&, LacrosSelection selection) {
        EXPECT_EQ(LacrosSelection::kRootfs, selection);
      }),
      false);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionStateful) {
  // Use stateful when a rootfs lacros-chrome version is invalid.
  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in an invalid `base::Version`.
  browser_loader_->OnLoadVersionSelection({}, base::Version());
  EXPECT_FALSE(callback_called);
}

TEST_F(BrowserLoaderTest, OnLoadVersionSelectionRootfs) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(base::StringPiece(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name,
           base::Version("1.0.0")}}));

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
      base::BindOnce([](const base::FilePath&, LacrosSelection selection) {
        EXPECT_EQ(LacrosSelection::kRootfs, selection);
      }),
      base::Version("2.0.0"));
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
           base::Version("3.0.0")}}));

  bool callback_called = false;
  fake_upstart_client_.set_start_job_cb(base::BindRepeating(
      [](bool* b, const std::string& job,
         const std::vector<std::string>& upstart_env) {
        *b = true;
        return true;
      },
      &callback_called));
  // Pass in a rootfs lacros-chrome version that is older.
  browser_loader_->OnLoadVersionSelection({}, base::Version("2.0.0"));
  EXPECT_FALSE(callback_called);
}

}  // namespace crosapi
