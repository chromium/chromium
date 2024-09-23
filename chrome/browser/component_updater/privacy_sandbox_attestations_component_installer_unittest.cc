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
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class PrivacySandboxAttestationsInstallerTest : public testing::Test {
 public:
  PrivacySandboxAttestationsInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  using Installer = PrivacySandboxAttestationsComponentInstallerPolicy;
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

  ~PrivacySandboxAttestationsInstallerFeatureDisabledTest() override = default;

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

  // Write an attestations list file to simulate that there is existing file
  // from previous runs.
  base::FilePath install_dir = Installer::GetInstalledDirectory(user_dir);
  ASSERT_TRUE(base::CreateDirectory(install_dir));

  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      install_dir, "Attestations list"));

  base::FilePath install_path = Installer::GetInstalledFilePath(install_dir);
  ASSERT_TRUE(base::PathExists(install_dir));
  ASSERT_TRUE(base::PathExists(install_path));

  RegisterPrivacySandboxAttestationsComponent(&mock_update_service);
  env_.RunUntilIdle();

  // Existing Privacy Sandbox Enrollment attestations files should be removed
  // if the feature is disabled.
  EXPECT_FALSE(base::PathExists(install_dir));
  EXPECT_FALSE(base::PathExists(install_path));
}

class PrivacySandboxAttestationsInstallerFeatureEnabledTest
    : public PrivacySandboxAttestationsInstallerTest {
 public:
  PrivacySandboxAttestationsInstallerFeatureEnabledTest() = default;

  ~PrivacySandboxAttestationsInstallerFeatureEnabledTest() override = default;
};

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       VerifyInstallation) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  ASSERT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));

  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      component_install_dir_.GetPath(), "Attestations list"));

  EXPECT_TRUE(policy.VerifyInstallation(base::Value::Dict(),
                                        component_install_dir_.GetPath()));
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest, OnCustomInstall) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_EQ(policy.OnCustomInstall(base::Value::Dict(), base::FilePath())
                .result.code_,
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
  base::test::RepeatingTestFuture<base::Version, base::FilePath, bool> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  const base::Version version = base::Version("0.0.1");
  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      component_install_dir_.GetPath(), "Attestations list"));
  policy.ComponentReadyForTesting(version, component_install_dir_.GetPath(),
                                  base::Value::Dict());

  auto [loaded_version, loaded_path, is_pre_installed] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, version);
  EXPECT_FALSE(is_pre_installed);
  EXPECT_EQ(loaded_path,
            Installer::GetInstalledFilePath(component_install_dir_.GetPath()));
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       DoNotInvokeOnAttestationsReadyCallbackIfInvalidVersion) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath, bool> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  // First call with an invalid version.
  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      component_install_dir_.GetPath(), "Attestations list"));
  policy.ComponentReadyForTesting(
      base::Version(), component_install_dir_.GetPath(), base::Value::Dict());

  // Second call with a valid version.
  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::Value::Dict());

  // Only the second call succeeded.
  auto [loaded_version, loaded_path, is_pre_installed] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, base::Version("0.0.1"));
  EXPECT_FALSE(is_pre_installed);
  EXPECT_EQ(loaded_path,
            Installer::GetInstalledFilePath(component_install_dir_.GetPath()));
}

TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       DoNotInvokeOnAttestationsReadyCallbackIfEmptyPath) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath, bool> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  // First call with an empty path.
  policy.ComponentReadyForTesting(base::Version("0.0.1"), base::FilePath(),
                                  base::Value::Dict());

  // Second call with a valid path.
  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      component_install_dir_.GetPath(), "Attestations list"));
  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::Value::Dict());

  // Only the second call succeeded.
  auto [loaded_version, loaded_path, is_pre_installed] = future.Take();
  EXPECT_TRUE(loaded_version.IsValid());
  EXPECT_EQ(loaded_version, base::Version("0.0.1"));
  EXPECT_FALSE(is_pre_installed);
  EXPECT_EQ(loaded_path,
            Installer::GetInstalledFilePath(component_install_dir_.GetPath()));
}

// Whenever there is an attestations file ready, `ComponentReady()` should
// invoke the stored callback `on_attestations_ready_`, even if this version
// is older than the existing one. The comparison of the passed and existing
// version should be done inside the callback. See
// `PrivacySandboxAttestations::LoadAttestationsInternal()`.
TEST_F(PrivacySandboxAttestationsInstallerFeatureEnabledTest,
       CallLoadNewAttestationsFile) {
  base::test::RepeatingTestFuture<base::Version, base::FilePath, bool> future;
  PrivacySandboxAttestationsComponentInstallerPolicy policy(
      future.GetCallback());

  // Load the initial version.
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      dir_v1.GetPath(), "Attestations list 0.01"));

  const base::Version version_1 = base::Version("0.0.1");
  policy.ComponentReadyForTesting(version_1, dir_v1.GetPath(),
                                  base::Value::Dict());

  auto [loaded_version_1, loaded_path_v1, is_pre_installed_v1] = future.Take();
  EXPECT_TRUE(loaded_version_1.IsValid());
  EXPECT_EQ(loaded_version_1, version_1);
  EXPECT_EQ(loaded_path_v1, Installer::GetInstalledFilePath(dir_v1.GetPath()));

  // Load the newer version.
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(WritePrivacySandboxAttestationsFileForTesting(
      dir_v2.GetPath(), "Attestations list 0.02"));
  const base::Version version_2 = base::Version("0.0.2");
  policy.ComponentReadyForTesting(version_2, dir_v2.GetPath(),
                                  base::Value::Dict());

  auto [loaded_version_2, loaded_path_v2, is_pre_installed_v2] = future.Take();
  EXPECT_TRUE(loaded_version_2.IsValid());
  EXPECT_EQ(loaded_version_2, version_2);
  EXPECT_EQ(loaded_path_v2, Installer::GetInstalledFilePath(dir_v2.GetPath()));

  // Load the initial version again, callback `on_attestations_ready_` should
  // still be invoked.
  policy.ComponentReadyForTesting(version_1, dir_v1.GetPath(),
                                  base::Value::Dict());

  auto [loaded_version_3, loaded_path_v3, is_pre_installed_v3] = future.Take();
  EXPECT_TRUE(loaded_version_3.IsValid());
  EXPECT_EQ(loaded_version_3, version_1);
  EXPECT_EQ(loaded_path_v3, Installer::GetInstalledFilePath(dir_v1.GetPath()));
}

class PrivacySandboxAttestationsHistogramsTest
    : public PrivacySandboxAttestationsInstallerFeatureEnabledTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  std::string GetHistogram() const {
    if (BrowserWindowFirstPaintRecorded()) {
      return privacy_sandbox::kComponentReadyFromBrowserWindowFirstPaintUMA;
    }

    if (NonBrowserUIDisplayed()) {
      return privacy_sandbox::
          kComponentReadyFromApplicationStartWithInterruptionUMA;
    }

    return privacy_sandbox::kComponentReadyFromApplicationStartUMA;
  }

  bool NonBrowserUIDisplayed() const { return std::get<0>(GetParam()); }
  bool BrowserWindowFirstPaintRecorded() const {
    return std::get<1>(GetParam());
  }

 protected:
  void SetUp() override {
    // Reset the singleton recorder to avoid interference across test cases.
    startup_metric_utils::GetBrowser().ResetSessionForTesting();

    if (NonBrowserUIDisplayed()) {
      // Simulate a non browser UI, e.g, profile picker has been displayed.
      startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();
    }

    if (BrowserWindowFirstPaintRecorded()) {
      // Simulate that the browser window paint has shown and been recorded.
      startup_metric_utils::GetBrowser().RecordBrowserWindowFirstPaintTicks(
          base::TimeTicks::Now());
    }
  }

  base::HistogramTester histogram_tester_;
};

TEST_P(PrivacySandboxAttestationsHistogramsTest,
       RecordHistogramWhenComponentReady) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  const base::Version version_1 = base::Version("0.0.1");
  policy.ComponentReadyForTesting(version_1, dir_v1.GetPath(),
                                  base::Value::Dict());

  histogram_tester_.ExpectTotalCount(GetHistogram(), 1);
}

TEST_P(PrivacySandboxAttestationsHistogramsTest,
       HistogramShouldOnlyRecordedOnce) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  // Load the initial version.
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  const base::Version version_1 = base::Version("0.0.1");
  policy.ComponentReadyForTesting(version_1, dir_v1.GetPath(),
                                  base::Value::Dict());

  histogram_tester_.ExpectTotalCount(GetHistogram(), 1);

  // Load the newer version.
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  const base::Version version_2 = base::Version("0.0.2");
  policy.ComponentReadyForTesting(version_2, dir_v2.GetPath(),
                                  base::Value::Dict());

  histogram_tester_.ExpectTotalCount(GetHistogram(), 1);
}

TEST_P(PrivacySandboxAttestationsHistogramsTest,
       HistogramNotRecordedIfInvalidInput) {
  PrivacySandboxAttestationsComponentInstallerPolicy policy(base::DoNothing());

  // Try loading with an empty path.
  policy.ComponentReadyForTesting(base::Version("0.0.1"), base::FilePath(),
                                  base::Value::Dict());
  histogram_tester_.ExpectTotalCount(GetHistogram(), 0);

  // Try loading with an invalid version.
  policy.ComponentReadyForTesting(
      base::Version(), component_install_dir_.GetPath(), base::Value::Dict());
  histogram_tester_.ExpectTotalCount(GetHistogram(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxAttestationsHistograms,
    PrivacySandboxAttestationsHistogramsTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf(
          "%s_%s",
          std::get<0>(info.param) ? "NonBrowserUIDisplayed" : "NormalStart",
          std::get<1>(info.param) ? "BrowserWindowFirstPaintRecorded"
                                  : "NoBrowserWindowFirstPaint");
    });

}  // namespace component_updater
