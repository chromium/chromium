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
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/component_loader_prefs.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

constexpr char kAimEligibilityExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApBqd0NT1c0F+CI4e"
    "NK09isJoysNY8QZhW7UrqO2XWwOCG5QH8TykzpAHNdM5vDJwSxDg1vO69dZKhjMdyg4e"
    "MaL4U3qoYAwqobmZilZ/ig/Bzi0XdGKY6rN5xDakWKdR9BkhJlE+xEVqKXd5NoV1gg69s"
    "4RNRq88GiT+r/GTMxg3lSrIa5u1ROesujmifZbgoyuuLiNE9hr3WVB1OzhWuFkm/mVzoo"
    "EcNhiqs8UcsAKWgJK65fHMlktmDzW6K+g0WVOHkkgtp7H9w6K5nz3UAM4XyTHSESIZuw9"
    "D07/BkKr2U+TSPxjSya/goCm7KMjQbuMbaqj5SYQoMFIgOvJr2QIDAQAB";

// Valid RSA public key, but belongs to a different extension ID.
constexpr char kDifferentExtensionKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8xv6iO+j4kzj1HiBL93+XVJH/"
    "CRyAQMUHS/Z0l8nCAzaAFkW/JsNwxJqQhrZspnxLqbQxNncXs6g6bsXAwKHiEs+"
    "lGh4Am6fgMKy0iQIDAQAB";

constexpr base::FilePath::CharType kExtensionManifestFilename[] =
    FILE_PATH_LITERAL("extension_manifest.json");

}  // namespace

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

  // Directory with extension_manifest.json is valid.
  base::FilePath manifest_path =
      install_dir.GetPath().Append(kExtensionManifestFilename);
  ASSERT_TRUE(base::WriteFile(manifest_path, ""));
  EXPECT_TRUE(
      policy.VerifyInstallation(base::DictValue(), install_dir.GetPath()));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest, ComponentReadyStaged) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  base::FilePath manifest_path =
      install_dir.GetPath().Append(kExtensionManifestFilename);
  ASSERT_TRUE(base::WriteFile(
      manifest_path,
      base::StringPrintf(R"({"name": "AIM Staged", "version": "2.0", )"
                         R"("manifest_version": 3, "key": "%s"})",
                         kAimEligibilityExtensionKey)));

  // ComponentReady with extension version 2.0 (> bundled 1.0) stages in Prefs.
  policy.ComponentReady(base::Version("1.1"), install_dir.GetPath(),
                        base::DictValue());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return extensions::component_loader_prefs::GetExtension(
               *local_state, extension_misc::kAimEligibilityExtensionId)
        .has_value();
  }));

  std::optional<
      extensions::component_loader_prefs::StagedComponentExtensionInfo>
      staged = extensions::component_loader_prefs::GetExtension(
          *local_state, extension_misc::kAimEligibilityExtensionId);
  ASSERT_TRUE(staged.has_value());
  EXPECT_EQ(staged->relative_path,
            policy.GetRelativeInstallDir().AppendASCII("1.1"));
  const std::string* version =
      staged->manifest.FindString(extensions::manifest_keys::kVersion);
  ASSERT_TRUE(version);
  EXPECT_EQ(*version, "2.0");
  const std::string* name =
      staged->manifest.FindString(extensions::manifest_keys::kName);
  ASSERT_TRUE(name);
  EXPECT_EQ(*name, "AIM Staged");
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       ComponentReadyIgnoredIfOlderOrEqual) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  base::FilePath manifest_path =
      install_dir.GetPath().Append(kExtensionManifestFilename);
  ASSERT_TRUE(base::WriteFile(
      manifest_path,
      base::StringPrintf(R"({"name": "AIM Staged", "version": "1.0", )"
                         R"("manifest_version": 3, "key": "%s"})",
                         kAimEligibilityExtensionKey)));

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      policy.GetRelativeInstallDir().AppendASCII("2.0"),
      std::move(staged_manifest));

  // ComponentReady with extension version 1.0 (<= bundled 1.0) clears staged
  // prefs.
  policy.ComponentReady(base::Version("1.1"), install_dir.GetPath(),
                        base::DictValue());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
  }));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       ComponentReadyIgnoredIfKeyInvalid) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  base::FilePath manifest_path =
      install_dir.GetPath().Append(kExtensionManifestFilename);
  ASSERT_TRUE(base::WriteFile(
      manifest_path, R"({"name": "AIM Staged", "version": "2.0", )"
                     R"("manifest_version": 3, "key": "not-valid-pem"})"));

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      policy.GetRelativeInstallDir().AppendASCII("2.0"),
      std::move(staged_manifest));

  policy.ComponentReady(base::Version("1.1"), install_dir.GetPath(),
                        base::DictValue());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
  }));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       ComponentReadyIgnoredIfExtensionIdMismatch) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  base::FilePath manifest_path =
      install_dir.GetPath().Append(kExtensionManifestFilename);
  ASSERT_TRUE(base::WriteFile(
      manifest_path,
      base::StringPrintf(R"({"name": "AIM Staged", "version": "2.0", )"
                         R"("manifest_version": 3, "key": "%s"})",
                         kDifferentExtensionKey)));

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      policy.GetRelativeInstallDir().AppendASCII("2.0"),
      std::move(staged_manifest));

  policy.ComponentReady(base::Version("1.1"), install_dir.GetPath(),
                        base::DictValue());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
  }));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest,
       ComponentReadyIgnoredIfManifestMissing) {
  AimEligibilityComponentInstallerPolicy policy;
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());

  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      policy.GetRelativeInstallDir().AppendASCII("2.0"),
      std::move(staged_manifest));

  // Directory is empty (no extension_manifest.json). ComponentReady fails to
  // load the manifest and clears staged prefs.
  policy.ComponentReady(base::Version("2.0"), install_dir.GetPath(),
                        base::DictValue());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
  }));
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
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      base::FilePath(extension_misc::kAimEligibilityExtensionDirName)
          .AppendASCII("2.0"),
      std::move(staged_manifest));

  auto cus = std::make_unique<MockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);

  ManageAimEligibilityComponentRegistration(cus.get());

  EXPECT_TRUE(base::test::RunUntil([&] {
    return !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
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
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      base::FilePath(extension_misc::kAimEligibilityExtensionDirName)
          .AppendASCII("2.0"),
      std::move(staged_manifest));

  auto cus = std::make_unique<MockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);

  ManageAimEligibilityComponentRegistration(cus.get());

  EXPECT_TRUE(base::test::RunUntil([&] {
    return !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
  }));
}

TEST_F(AimEligibilityComponentInstallerPolicyTest, UninstallComponent) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  base::DictValue staged_manifest;
  staged_manifest.Set("version", "2.0");
  extensions::component_loader_prefs::StageExtension(
      *local_state, extension_misc::kAimEligibilityExtensionId,
      base::FilePath(extension_misc::kAimEligibilityExtensionDirName)
          .AppendASCII("2.0"),
      std::move(staged_manifest));

  base::FilePath component_dir = user_data_dir_.GetPath().Append(
      extension_misc::kAimEligibilityExtensionDirName);
  base::FilePath version_dir = component_dir.Append(FILE_PATH_LITERAL("2.0"));
  ASSERT_TRUE(base::CreateDirectory(version_dir));
  ASSERT_TRUE(
      base::WriteFile(version_dir.Append(kExtensionManifestFilename), ""));
  ASSERT_TRUE(base::PathExists(version_dir));

  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AimEligibilityComponentInstallerPolicy>())
      ->Uninstall();

  // Uninstall deletes the component directory from disk and calls
  // OnCustomUninstall() to clear the staged prefs.
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !base::PathExists(component_dir) &&
           !extensions::component_loader_prefs::GetExtension(
                *local_state, extension_misc::kAimEligibilityExtensionId)
                .has_value();
  }));
}

}  // namespace component_updater
