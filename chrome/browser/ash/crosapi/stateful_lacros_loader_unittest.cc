// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/update_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

using testing::Return;

constexpr char kLacrosComponentName[] = "lacros-dogfood-dev";
constexpr char kLacrosComponentId[] = "ldobopbhiamakmncndpkeelenhdmgfhk";

class StatefulLacrosLoaderTest : public testing::Test {
 public:
  StatefulLacrosLoaderTest() {
    // Create dependencies for object under test.
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    component_manager_->set_supported_components({kLacrosComponentName});
    component_manager_->ResetComponentState(
        kLacrosComponentName,
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path"),
            version));
    browser_part_ = std::make_unique<BrowserProcessPlatformPartTestApi>(
        g_browser_process->platform_part());
    browser_part_->InitializeCrosComponentManager(component_manager_);

    stateful_lacros_loader_ = std::make_unique<StatefulLacrosLoader>(
        component_manager_, &mock_component_update_service_,
        kLacrosComponentName);
    EXPECT_TRUE(BrowserLoader::WillLoadStatefulComponentBuilds());
  }

  ~StatefulLacrosLoaderTest() override {
    browser_part_->ShutdownCrosComponentManager();
  }

  const base::Version version = base::Version("1.0.0");

 protected:
  content::BrowserTaskEnvironment task_environment_;

  component_updater::MockComponentUpdateService mock_component_update_service_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  std::unique_ptr<BrowserProcessPlatformPartTestApi> browser_part_;
  std::unique_ptr<StatefulLacrosLoader> stateful_lacros_loader_;
};

TEST_F(StatefulLacrosLoaderTest, LoadStatefulLacros) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(base::StringPiece(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name, version, ""}}));

  // Set stateful lacros-chrome version. Wait until the version calculation is
  // completed before verifying the version.
  base::test::TestFuture<base::Version, const base::FilePath&> future;
  stateful_lacros_loader_->Load(
      future.GetCallback<base::Version, const base::FilePath&>(),
      /*forced=*/false);
  EXPECT_EQ(version, future.Get<0>());
}

TEST_F(StatefulLacrosLoaderTest, LoadStatefulLacrosSelectedByPolicy) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(base::StringPiece(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name, version, ""}}));

  // Set stateful lacros-chrome version. Wait until the version calculation is
  // completed before verifying the version.
  base::test::TestFuture<base::Version, const base::FilePath&> future;
  // Load stateful lacros-chrome as enforced by a policy.
  stateful_lacros_loader_->Load(
      future.GetCallback<base::Version, const base::FilePath&>(),
      /*forced=*/true);
  EXPECT_EQ(version, future.Get<0>());
}

}  // namespace
}  // namespace crosapi
