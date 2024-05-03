// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/unpacked_installer.h"

#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/switches.h"

namespace extensions {

class UnpackedInstallerUnitTest : public ExtensionServiceTestWithInstall,
                                  public testing::WithParamInterface<bool> {
 public:
  UnpackedInstallerUnitTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
    } else {
      feature_list_.InitAndDisableFeature(
          extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
    }
  }
  ~UnpackedInstallerUnitTest() override = default;

  ManagementPolicy* GetManagementPolicy() {
    return ExtensionSystem::Get(browser_context())->management_policy();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests host permissions are withheld by default at installation when flag is
// enabled.
TEST_P(UnpackedInstallerUnitTest, WithheldHostPermissionsWithFlag) {
  InitializeEmptyExtensionService();

  // Load extension.
  TestExtensionRegistryObserver observer(registry());
  base::FilePath extension_path =
      data_dir().AppendASCII("api_test/simple_all_urls");
  extensions::UnpackedInstaller::Create(service())->Load(extension_path);
  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();

  // Verify extension was installed.
  EXPECT_EQ(loaded_extension->name(), "All Urls Extension");

  // Host permissions should be withheld at installation only when flag is
  // enabled.
  bool flag_enabled = GetParam();
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context());
  EXPECT_EQ(permissions_manager->HasWithheldHostPermissions(*loaded_extension),
            flag_enabled);
}

TEST_P(UnpackedInstallerUnitTest,
       RecordCommandLineDeveloperModeMetrics_EnabledDeveloperModeOff) {
  // Developer Mode is disabled by default.
  base::HistogramTester histograms;
  InitializeEmptyExtensionService();

  // Load an extension from command line.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->GenerateInstalledExtensionsSet().size());

  histograms.ExpectBucketCount(
      /*name=*/"Extensions.CommandLineWithDeveloperModeOff.Enabled",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_P(UnpackedInstallerUnitTest,
       RecordCommandLineDeveloperModeMetrics_EnabledDeveloperModeOn) {
  base::HistogramTester histograms;
  InitializeEmptyExtensionService();
  // Enable developer mode.
  util::SetDeveloperModeForProfile(profile(), true);

  // Load an extension from command line.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->GenerateInstalledExtensionsSet().size());

  histograms.ExpectBucketCount(
      /*name=*/"Extensions.CommandLineWithDeveloperModeOn.Enabled",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_P(UnpackedInstallerUnitTest,
       RecordCommandLineDeveloperModeMetrics_DisabledDeveloperModeOff) {
  // Developer Mode is disabled by default.
  base::HistogramTester histograms;
  InitializeEmptyExtensionService();

  // Register an ExtensionManagementPolicy that disables all extensions, with
  // a specified disable_reason::DisableReason.
  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_DISABLED);
  provider.SetDisableReason(disable_reason::DISABLE_NOT_VERIFIED);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Load an extension from command line, it should be installed but disabled.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(1u, registry()->GenerateInstalledExtensionsSet().size());

  histograms.ExpectBucketCount(
      /*name=*/"Extensions.CommandLineWithDeveloperModeOff.Disabled",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_P(UnpackedInstallerUnitTest,
       RecordCommandLineDeveloperModeMetrics_DisabledDeveloperModeOn) {
  base::HistogramTester histograms;
  InitializeEmptyExtensionService();
  // Enable developer mode.
  util::SetDeveloperModeForProfile(profile(), true);

  // Register an ExtensionManagementPolicy that disables all extensions, with
  // a specified disable_reason::DisableReason.
  GetManagementPolicy()->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_DISABLED);
  provider.SetDisableReason(disable_reason::DISABLE_NOT_VERIFIED);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Load an extension from command line, it should be installed but disabled.
  base::FilePath path =
      base::MakeAbsoluteFilePath(data_dir().AppendASCII("good_unpacked"));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kLoadExtension, path);
  service()->Init();
  task_environment()->RunUntilIdle();

  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  EXPECT_EQ(1u, registry()->disabled_extensions().size());
  EXPECT_EQ(1u, registry()->GenerateInstalledExtensionsSet().size());

  histograms.ExpectBucketCount(
      /*name=*/"Extensions.CommandLineWithDeveloperModeOn.Disabled",
      /*sample=*/1,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(All, UnpackedInstallerUnitTest, testing::Bool());

}  // namespace extensions
