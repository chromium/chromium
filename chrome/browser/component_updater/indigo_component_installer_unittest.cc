// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/indigo_component_installer.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class IndigoComponentInstallerPolicyTest : public ::testing::Test {
 public:
  IndigoComponentInstallerPolicyTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { ResetIndigoInstallDirForTesting(); }

  base::ScopedTempDir component_install_dir_;
  content::BrowserTaskEnvironment env_;
};

TEST_F(IndigoComponentInstallerPolicyTest, VerifyInstallation_ValidDir) {
  IndigoComponentInstallerPolicy policy;

  base::FilePath install_dir = component_install_dir_.GetPath();

  EXPECT_FALSE(policy.VerifyInstallation(base::DictValue(), install_dir));

  ASSERT_TRUE(base::WriteFile(
      install_dir.Append(FILE_PATH_LITERAL("content_script.js")), ""));
  EXPECT_TRUE(policy.VerifyInstallation(base::DictValue(), install_dir));
}

TEST_F(IndigoComponentInstallerPolicyTest, ComponentNotReady_ReturnsNullopt) {
  EXPECT_EQ(GetIndigoComponentInstallDir(), std::nullopt);
  EXPECT_EQ(GetIndigoContentScriptPath(), std::nullopt);
}

TEST_F(IndigoComponentInstallerPolicyTest, ComponentReady_UpdatesPaths) {
  IndigoComponentInstallerPolicy policy;
  base::FilePath install_dir = component_install_dir_.GetPath();

  policy.ComponentReady(base::Version("1.2.3.4"), install_dir,
                        base::DictValue());

  EXPECT_EQ(GetIndigoComponentInstallDir(),
            std::optional<base::FilePath>(install_dir));
  EXPECT_EQ(GetIndigoContentScriptPath(),
            std::optional<base::FilePath>(
                install_dir.Append(FILE_PATH_LITERAL("content_script.js"))));
}

TEST_F(IndigoComponentInstallerPolicyTest, ComponentRegistered) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  ON_CALL(*service, GetSequencedTaskRunner())
      .WillByDefault(testing::Return(env_.GetMainThreadTaskRunner()));

  base::RunLoop run_loop;
  EXPECT_CALL(*service, RegisterComponent(testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }),
          testing::Return(true)));
  RegisterIndigoComponent(service.get());
  run_loop.Run();
}

}  // namespace component_updater
