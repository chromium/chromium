// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"

#include <memory>
#include <string_view>

#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/component_updater/component_updater_paths.h"
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
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    component_manager_->set_supported_components({kLacrosComponentName});
    component_manager_->ResetComponentState(
        kLacrosComponentName,
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path"),
            version));
    browser_part_ = std::make_unique<BrowserProcessPlatformPartTestApi>(
        g_browser_process->platform_part());
    browser_part_->InitializeComponentManager(component_manager_);

    // Set up component path.
    base::FilePath root_dir;
    CHECK(base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                                 &root_dir));
    base::FilePath component_root =
        root_dir.Append(component_updater::kComponentsRootPath)
            .Append(kLacrosComponentName);
    base::CreateDirectory(component_root.Append("sub-dir"));

    stateful_lacros_loader_ = std::make_unique<StatefulLacrosLoader>(
        component_manager_, &mock_component_update_service_,
        kLacrosComponentName);
    EXPECT_TRUE(BrowserLoader::WillLoadStatefulComponentBuilds());
  }

  ~StatefulLacrosLoaderTest() override {
    stateful_lacros_loader_.reset();

    browser_part_->ShutdownComponentManager();
  }

  const base::Version version = base::Version("1.0.0");

 protected:
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedPathOverride scoped_component_user_dir_{
      component_updater::DIR_COMPONENT_USER};
  component_updater::MockComponentUpdateService mock_component_update_service_;
  scoped_refptr<component_updater::FakeComponentManagerAsh> component_manager_;
  std::unique_ptr<BrowserProcessPlatformPartTestApi> browser_part_;
  std::unique_ptr<StatefulLacrosLoader> stateful_lacros_loader_;
};

TEST_F(StatefulLacrosLoaderTest,
       LoadStatefulLacrosSelectedByCompatibilityCheck) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(std::string_view(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name, version, ""}}));

  EXPECT_EQ(StatefulLacrosLoader::State::kNotLoaded,
            stateful_lacros_loader_->GetState());

  // If stateful is selected by compatibility check, it first calls GetVersion
  // and complete loading, and then Load is called.
  base::test::TestFuture<const base::Version&> future;
  stateful_lacros_loader_->GetVersion(
      future.GetCallback<const base::Version&>());
  EXPECT_EQ(version, future.Get<0>());
  EXPECT_EQ(StatefulLacrosLoader::State::kLoaded,
            stateful_lacros_loader_->GetState());

  // Load is already completed in GetVersion, so this call is synchronous.
  base::Version result_version;
  stateful_lacros_loader_->Load(
      base::BindOnce([](base::Version* result_version, base::Version version,
                        const base::FilePath&) { *result_version = version; },
                     &result_version),
      /*forced=*/false);
  EXPECT_EQ(version, result_version);
}

TEST_F(StatefulLacrosLoaderTest, LoadStatefulLacrosSelectedByPolicy) {
  std::u16string lacros_component_name =
      base::UTF8ToUTF16(std::string_view(kLacrosComponentName));
  EXPECT_CALL(mock_component_update_service_, GetComponents())
      .WillOnce(Return(std::vector<component_updater::ComponentInfo>{
          {kLacrosComponentId, "", lacros_component_name, version, ""}}));

  EXPECT_EQ(StatefulLacrosLoader::State::kNotLoaded,
            stateful_lacros_loader_->GetState());

  // If stateful is forced by policy, it does not call GetVersion. Instead, it
  // calls Load directly and compute the version inside Load together.
  base::test::TestFuture<base::Version, const base::FilePath&> future;
  stateful_lacros_loader_->Load(
      future.GetCallback<base::Version, const base::FilePath&>(),
      /*forced=*/true);
  EXPECT_EQ(version, future.Get<0>());
}

}  // namespace
}  // namespace crosapi
