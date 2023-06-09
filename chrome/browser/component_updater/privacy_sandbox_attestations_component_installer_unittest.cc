// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

std::string ReadToString(base::File file) {
  std::string contents;
  base::ScopedFILE scoped_file(base::FileToFILE(std::move(file), "r"));
  return base::ReadStreamToString(scoped_file.get(), &contents) ? contents : "";
}

}  // namespace

class PrivacySandboxAttestationsInstallerTest : public testing::Test {
 public:
  PrivacySandboxAttestationsInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  base::test::TaskEnvironment env_;
  base::ScopedTempDir component_install_dir_;
};

class PrivacySandboxAttestationsInstallerFeatureDisabledTest
    : public PrivacySandboxAttestationsInstallerTest {
 public:
  PrivacySandboxAttestationsInstallerFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        privacy_sandbox::kEnforcePrivacySandboxAttestations);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacySandboxAttestationsInstallerFeatureDisabledTest,
       DoNotRegisterIfFeatureDisabled) {
  component_updater::MockComponentUpdateService mock_update_service;
  EXPECT_CALL(mock_update_service, RegisterComponent(testing::_)).Times(0);
  RegisterPrivacySandboxAttestationsComponent(&mock_update_service);

  env_.RunUntilIdle();
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureDisabledTest,
       DeleteExistingFilesIfFeatureDisabled) {
  component_updater::MockComponentUpdateService mock_update_service;
  EXPECT_CALL(mock_update_service, RegisterComponent(testing::_)).Times(0);

  base::ScopedPathOverride user_data_override(
      chrome::DIR_USER_DATA, component_install_dir_.GetPath(), true, true);
  base::FilePath user_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_dir));

  base::FilePath install_dir =
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledDirectory(
          user_dir);
  base::FilePath install_path =
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
          install_dir);

  ASSERT_TRUE(base::CreateDirectory(install_path.DirName()));
  ASSERT_TRUE(base::WriteFile(install_path, "Attestations list"));
  ASSERT_TRUE(base::PathExists(install_path));

  RegisterPrivacySandboxAttestationsComponent(&mock_update_service);
  env_.RunUntilIdle();

  // Existing Privacy Sandbox Enrollment attestations files should be removed
  // if the feature is disabled.
  EXPECT_FALSE(base::PathExists(install_path));
  EXPECT_FALSE(base::PathExists(install_dir));
}

class PrivacySandboxAttestationsInstallerFeatureEnabledTest
    : public PrivacySandboxAttestationsInstallerTest {
 public:
  PrivacySandboxAttestationsInstallerFeatureEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        privacy_sandbox::kEnforcePrivacySandboxAttestations);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       RegisterIfFeatureEnabled) {
  component_updater::MockComponentUpdateService mock_update_service;
  EXPECT_CALL(mock_update_service, RegisterComponent(testing::_)).Times(1);
  RegisterPrivacySandboxAttestationsComponent(&mock_update_service);

  env_.RunUntilIdle();
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       LoadAttestationsFileOnComponentReady) {
  base::test::RepeatingTestFuture<base::Version, base::File> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  const std::string content = "Attestations list";
  ASSERT_TRUE(base::WriteFile(
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath()),
      content));

  const base::Version version = base::Version("0.0.1");
  policy.ComponentReady(version, component_install_dir_.GetPath(),
                        base::Value::Dict());

  auto [loaded_version, loaded_file] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, version);
  EXPECT_TRUE(loaded_file.IsValid());
  EXPECT_EQ(ReadToString(std::move(loaded_file)), content);
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       LoadNewAttestationsFileWhenUpdated) {
  base::test::RepeatingTestFuture<base::Version, base::File> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  const std::string content_v1 = "Attestations list v1";
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
          dir_v1.GetPath()),
      content_v1));

  const base::Version version_1 = base::Version("0.0.1");
  policy.ComponentReady(version_1, dir_v1.GetPath(), base::Value::Dict());

  auto [loaded_version_1, loaded_file_v1] = future.Take();
  EXPECT_TRUE(loaded_version_1.IsValid());
  EXPECT_EQ(loaded_version_1, version_1);
  EXPECT_TRUE(loaded_file_v1.IsValid());
  EXPECT_EQ(ReadToString(std::move(loaded_file_v1)), content_v1);

  // Install a newer version of the component, which should be picked up during
  // the `ComponentReady()` call.
  const std::string content_v2 = "Attestations list v2";
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
          dir_v2.GetPath()),
      content_v2));

  const base::Version version_2 = base::Version("0.0.2");
  policy.ComponentReady(version_2, dir_v2.GetPath(), base::Value::Dict());

  auto [loaded_version_2, loaded_file_v2] = future.Take();
  EXPECT_TRUE(loaded_version_2.IsValid());
  EXPECT_EQ(loaded_version_2, version_2);
  EXPECT_TRUE(loaded_file_v2.IsValid());
  EXPECT_EQ(ReadToString(std::move(loaded_file_v2)), content_v2);
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       NonexistentFileOnComponentReady) {
  ASSERT_TRUE(base::DeleteFile(
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath())));

  base::test::RepeatingTestFuture<base::Version, base::File> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());
  policy.ComponentReady(base::Version(), component_install_dir_.GetPath(),
                        base::Value::Dict());

  auto [loaded_version, loaded_file] = future.Take();
  EXPECT_FALSE(loaded_version.IsValid());
  EXPECT_FALSE(loaded_file.IsValid());
}

}  // namespace component_updater
