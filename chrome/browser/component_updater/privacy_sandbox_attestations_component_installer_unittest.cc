// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
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
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

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
       VerifyInstallation) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  ASSERT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));

  base::FilePath install_path =
      PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath());
  ASSERT_TRUE(base::WriteFile(install_path, "Attestations list"));

  EXPECT_TRUE(policy.VerifyInstallation(base::Value::Dict(),
                                        component_install_dir_.GetPath()));
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest, OnCustomInstall) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_EQ(policy.OnCustomInstall(base::Value::Dict(), base::FilePath()).error,
            0);
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       RegisterIfFeatureEnabled) {
  component_updater::MockComponentUpdateService mock_update_service;
  EXPECT_CALL(mock_update_service, RegisterComponent(testing::_)).Times(1);
  RegisterPrivacySandboxAttestationsComponent(&mock_update_service);

  env_.RunUntilIdle();
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       InvokeOnAttestationsReadyCallbackOnComponentReady) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  const base::Version version = base::Version("0.0.1");
  policy.ComponentReady(version, component_install_dir_.GetPath(),
                        base::Value::Dict());

  auto [loaded_version, loaded_path] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, version);
  EXPECT_EQ(loaded_path, component_install_dir_.GetPath());
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       DoNotInvokeOnAttestationsReadyCallbackIfInvalidVersion) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  // First call with an invalid version.
  policy.ComponentReady(base::Version(), component_install_dir_.GetPath(),
                        base::Value::Dict());

  // Second call with a valid version.
  policy.ComponentReady(base::Version("0.0.1"),
                        component_install_dir_.GetPath(), base::Value::Dict());

  // Only the second call succeeded.
  auto [loaded_version, loaded_path] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, base::Version("0.0.1"));
  EXPECT_EQ(loaded_path, component_install_dir_.GetPath());
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       DoNotInvokeOnAttestationsReadyCallbackIfEmptyPath) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  // First call with an empty path.
  policy.ComponentReady(base::Version("0.0.1"), base::FilePath(),
                        base::Value::Dict());

  // Second call with a valid path.
  policy.ComponentReady(base::Version("0.0.1"),
                        component_install_dir_.GetPath(), base::Value::Dict());

  // Only the second call succeeded.
  auto [loaded_version, loaded_path] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, base::Version("0.0.1"));
  EXPECT_EQ(loaded_path, component_install_dir_.GetPath());
}

// Whenever there is an attestations file ready, `ComponentReady()` should
// invoke the stored callback `on_attestations_ready_`, even if this version
// is older than the existing one. The comparison of the passed and existing
// version should be done inside the callback. See
// `PrivacySandboxAttestations::LoadAttestationsInternal()`.
TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       CallLoadNewAttestationsFile) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  // Load the initial version.
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  const base::Version version_1 = base::Version("0.0.1");
  policy.ComponentReady(version_1, dir_v1.GetPath(), base::Value::Dict());

  auto [loaded_version_1, loaded_path_v1] = future.Take();
  EXPECT_TRUE(loaded_version_1.IsValid());
  EXPECT_EQ(loaded_version_1, version_1);
  EXPECT_EQ(loaded_path_v1, dir_v1.GetPath());

  // Load the newer version.
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  const base::Version version_2 = base::Version("0.0.2");
  policy.ComponentReady(version_2, dir_v2.GetPath(), base::Value::Dict());

  auto [loaded_version_2, loaded_path_v2] = future.Take();
  EXPECT_TRUE(loaded_version_2.IsValid());
  EXPECT_EQ(loaded_version_2, version_2);
  EXPECT_EQ(loaded_path_v2, dir_v2.GetPath());

  // Load the initial version again, callback `on_attestations_ready_` should
  // still be invoked.
  policy.ComponentReady(version_1, dir_v1.GetPath(), base::Value::Dict());

  auto [loaded_version_3, loaded_path_v3] = future.Take();
  EXPECT_TRUE(loaded_version_3.IsValid());
  EXPECT_EQ(loaded_version_3, version_1);
  EXPECT_EQ(loaded_path_v3, dir_v1.GetPath());
}

}  // namespace component_updater
