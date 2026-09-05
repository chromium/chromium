// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/aim_eligibility_component_installer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class AimEligibilityComponentInstallerPolicyTest : public testing::Test {
 public:
  AimEligibilityComponentInstallerPolicyTest() {
    EXPECT_TRUE(user_data_dir_.CreateUniqueTempDir());
    scoped_path_override_ = std::make_unique<base::ScopedPathOverride>(
        component_updater::DIR_COMPONENT_USER, user_data_dir_.GetPath());
  }

  void TearDown() override { scoped_path_override_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir user_data_dir_;
  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
};

TEST_F(AimEligibilityComponentInstallerPolicyTest, VerifyInstallation) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  // Empty directory is invalid.
  EXPECT_FALSE(
      policy.VerifyInstallation(base::DictValue(), install_dir.GetPath()));

  // Directory with manifest.json is valid.
  base::FilePath manifest_path =
      install_dir.GetPath().Append(FILE_PATH_LITERAL("manifest.json"));
  ASSERT_TRUE(base::WriteFile(manifest_path, ""));
  EXPECT_TRUE(
      policy.VerifyInstallation(base::DictValue(), install_dir.GetPath()));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest, ComponentReadyStaged) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  base::DictValue manifest;
  manifest.Set("key", "test");

  // ComponentReady with version 2.0 (> bundled 1.0) stages in Prefs.
  policy.ComponentReady(base::Version("2.0"), install_dir.GetPath(),
                        std::move(manifest));

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  EXPECT_EQ(local_state->GetString(
                extension_misc::kAimEligibilityExtensionStagedVersionPref),
            "2.0");
  EXPECT_FALSE(
      local_state
          ->GetString(
              extension_misc::kAimEligibilityExtensionStagedManifestPref)
          .empty());
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       ComponentReadyIgnoredIfOlderOrEqual) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedVersionPref, "2.0");
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedManifestPref, "{}");

  base::DictValue manifest;
  // ComponentReady with version 1.0 (<= bundled 1.0) clears staged prefs.
  policy.ComponentReady(base::Version("1.0"), install_dir.GetPath(),
                        std::move(manifest));

  EXPECT_EQ(local_state->GetString(
                extension_misc::kAimEligibilityExtensionStagedVersionPref),
            "");
  EXPECT_EQ(local_state->GetString(
                extension_misc::kAimEligibilityExtensionStagedManifestPref),
            "");
}

TEST_F(AimEligibilityComponentInstallerPolicyTest, RegisterComponentEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{omnibox::kAimEligibilityComponentExtension,
        {{"use_component_updater", "true"}}}},
      {});

  auto cus = std::make_unique<MockComponentUpdateService>();
  // ComponentInstaller::Register performs registration asynchronously on a
  // background task runner and replies on the current sequence. Use a
  // TestFuture to wait until RegisterComponent is invoked before verifying
  // expectations.
  base::test::TestFuture<void> registered;
  EXPECT_CALL(*cus, RegisterComponent(testing::_))
      .WillOnce([&registered](const auto&) {
        registered.SetValue();
        return true;
      });

  ManageAimEligibilityComponentRegistration(cus.get());
  EXPECT_TRUE(registered.Wait());
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       RegisterComponentDisabledByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      omnibox::kAimEligibilityComponentExtension);

  // Pre-populate staged prefs to verify that when the feature is disabled,
  // Uninstall() is triggered and it cleans up staged prefs.
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedVersionPref, "2.0");
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedManifestPref, "{}");

  auto cus = std::make_unique<MockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);

  ManageAimEligibilityComponentRegistration(cus.get());

  EXPECT_TRUE(base::test::RunUntil([&] {
    return local_state
               ->GetString(
                   extension_misc::kAimEligibilityExtensionStagedVersionPref)
               .empty() &&
           local_state
               ->GetString(
                   extension_misc::kAimEligibilityExtensionStagedManifestPref)
               .empty();
  }));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       RegisterComponentDisabledByFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{omnibox::kAimEligibilityComponentExtension,
        {{"use_component_updater", "false"}}}},
      {});

  // Pre-populate staged prefs to verify that when the feature param is
  // disabled, Uninstall() is triggered and it cleans up staged prefs.
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedVersionPref, "2.0");
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedManifestPref, "{}");

  auto cus = std::make_unique<MockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);

  ManageAimEligibilityComponentRegistration(cus.get());

  EXPECT_TRUE(base::test::RunUntil([&] {
    return local_state
               ->GetString(
                   extension_misc::kAimEligibilityExtensionStagedVersionPref)
               .empty() &&
           local_state
               ->GetString(
                   extension_misc::kAimEligibilityExtensionStagedManifestPref)
               .empty();
  }));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest, UninstallComponent) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedVersionPref, "2.0");
  local_state->SetString(
      extension_misc::kAimEligibilityExtensionStagedManifestPref, "{}");

  base::FilePath component_dir = user_data_dir_.GetPath().Append(
      extension_misc::kAimEligibilityExtensionDirName);
  base::FilePath version_dir = component_dir.Append(FILE_PATH_LITERAL("2.0"));
  ASSERT_TRUE(base::CreateDirectory(version_dir));
  ASSERT_TRUE(base::WriteFile(
      version_dir.Append(FILE_PATH_LITERAL("manifest.json")), ""));
  ASSERT_TRUE(base::PathExists(version_dir));

  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AimEligibilityComponentInstallerPolicy>())
      ->Uninstall();

  // Uninstall deletes the component directory from disk and calls
  // OnCustomUninstall() to clear the staged prefs.
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !base::PathExists(component_dir) &&
           local_state
               ->GetString(
                   extension_misc::kAimEligibilityExtensionStagedVersionPref)
               .empty() &&
           local_state
               ->GetString(
                   extension_misc::kAimEligibilityExtensionStagedManifestPref)
               .empty();
  }));
}

}  // namespace component_updater
