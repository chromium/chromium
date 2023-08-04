// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tpcd_metadata_component_installer.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {
using ::testing::_;

const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("tpcd_metadata.pb");
}  // namespace

class TpcdMetadataComponentInstallerTest : public ::testing::Test {
 public:
  TpcdMetadataComponentInstallerTest() {
    CHECK(install_dir_.CreateUniqueTempDir());
    CHECK(install_dir_.IsValid());
  }

  ~TpcdMetadataComponentInstallerTest() override = default;

 protected:
  const base::FilePath& install_dir() { return install_dir_.GetPath(); }

  void SetComponentContents(base::StringPiece contents) {
    base::FilePath path = install_dir().Append(kComponentFileName);
    CHECK(base::WriteFile(path, contents));
    CHECK(base::PathExists(path));
  }

  content::BrowserTaskEnvironment& task_env() { return task_env_; }

  auto* policy() { return policy_.get(); }

 private:
  base::ScopedTempDir install_dir_;
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_ =
      std::make_unique<TpcdMetadataComponentInstaller>(base::DoNothing());
};

TEST_F(TpcdMetadataComponentInstallerTest, ComponentRegistered) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterTpcdMetadataComponent(service.get());

  task_env().RunUntilIdle();
}

TEST_F(TpcdMetadataComponentInstallerTest,
       VerifyInstallation_InvalidInstallDir) {
  EXPECT_FALSE(policy()->VerifyInstallation(
      base::Value::Dict(), install_dir().Append(FILE_PATH_LITERAL("x"))));
}

TEST_F(TpcdMetadataComponentInstallerTest,
       VerifyInstallation_RejectsMissingFile) {
  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
}
}  // namespace component_updater
