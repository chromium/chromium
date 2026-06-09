// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/manifest_v2_experiment_manager.h"

#include <algorithm>

#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/unpacked_installer.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {
namespace {

const Extension* GetExtensionByName(std::string_view name,
                                    const ExtensionSet& extensions) {
  const Extension* extension = nullptr;
  for (const auto& e : extensions) {
    if (e->name() == name) {
      extension = e.get();
      break;
    }
  }

  return extension;
}

}  // namespace

class ManifestV2ExperimentManagerBrowserTest : public ExtensionBrowserTest {
 public:
  ManifestV2ExperimentManagerBrowserTest() = default;
  ~ManifestV2ExperimentManagerBrowserTest() override = default;

  // Since this is testing the MV2 deprecation experiments, we probably don't
  // want to bypass their disabling for testing. There are some exceptions for
  // pre-tests that explicitly need to do this as part of the setup.
  bool ShouldAllowMV2Extensions() override {
    std::vector<std::string> tests_allowing_mv2 = {
        "PRE_ExtensionsAreDisabledOnStartup",
        "PRE_PRE_ExtensionsCanBeReEnabledByUsers",
        "PRE_MV2ExtensionsAreNotDisabledIfLegacyExtensionSwitchIsApplied",
        "PRE_PRE_FlowFromWarningToUnsupported",
    };
    return std::ranges::contains(
        tests_allowing_mv2,
        std::string(
            testing::UnitTest::GetInstance()->current_test_info()->name()));
  }

  void WaitForExtensionSystemReady() {
    base::RunLoop run_loop;
    ExtensionSystem::Get(profile())->ready().Post(
        FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Uninstalls the extension with the given `extension_id` and for the given
  // `uninstall_reason`, waiting until uninstallation has finished.
  void UninstallExtension(const ExtensionId& extension_id,
                          UninstallReason uninstall_reason) {
    base::RunLoop run_loop;
    extension_registrar()->UninstallExtension(extension_id, uninstall_reason,
                                              /*error=*/nullptr,
                                              run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Adds a new MV2 extension with the given `name` to the profile, returning
  // it afterwards.
  const Extension* AddMV2Extension(std::string_view name) {
    return AddExtensionWithManifestVersion(name, 2);
  }

  const Extension* AddExtensionWithManifestVersion(std::string_view name,
                                                   int manifest_version) {
    static constexpr char kManifest[] =
        R"({
             "name": "%s",
             "manifest_version": %d,
             "version": "0.1"
           })";
    TestExtensionDir test_dir;
    test_dir.WriteManifest(
        base::StringPrintf(kManifest, name.data(), manifest_version));
    return InstallExtension(test_dir.UnpackedPath(), /*expected_change=*/1,
                            mojom::ManifestLocation::kInternal);
  }

  ExtensionPrefs* extension_prefs() { return ExtensionPrefs::Get(profile()); }

  ManifestV2ExperimentManager* experiment_manager() {
    return ManifestV2ExperimentManager::Get(profile());
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// A test series to verify MV2 extensions are disabled on startup.
// Step 1: Install an MV2 extension.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_ExtensionsAreDisabledOnStartup) {
  const Extension* extension = AddMV2Extension("Test MV2 Extension");
  ASSERT_TRUE(extension);
}
// Step 2: Verify the extension is disabled.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       ExtensionsAreDisabledOnStartup) {
  WaitForExtensionSystemReady();

  const Extension* extension = GetExtensionByName(
      "Test MV2 Extension",
      extension_registry()->GenerateInstalledExtensionsSet());

  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();

  EXPECT_FALSE(
      extension_registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(extension_id));

  EXPECT_THAT(extension_prefs()->GetDisableReasons(extension_id),
              testing::UnorderedElementsAre(
                  disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION));

  // The extension is recorded as "hard disabled".
  histogram_tester().ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal",
      ManifestV2ExperimentManager::MV2ExtensionState::kHardDisabled, 1);

  // The user should not be allowed to re-enable the extension.
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  {
    disable_reason::DisableReason disable_reason = disable_reason::DISABLE_NONE;
    EXPECT_TRUE(system->management_policy()->MustRemainDisabled(
        extension, &disable_reason));
    EXPECT_EQ(disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION,
              disable_reason);
  }
}

// Tests that extensions are re-enabled automatically if they update to MV3.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       ExtensionsAreReEnabledWhenUpdatedToMV3) {
  WaitForExtensionSystemReady();

  static constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Extension",
           "manifest_version": %d,
           "version": "%s"
         })";
  std::string mv2_manifest = base::StringPrintf(kManifestTemplate, 2, "1.0");
  std::string mv3_manifest = base::StringPrintf(kManifestTemplate, 3, "2.0");

  TestExtensionDir test_dir;
  test_dir.WriteManifest(mv2_manifest);
  base::FilePath mv2_crx = test_dir.Pack("mv2.crx");
  test_dir.WriteManifest(mv3_manifest);
  base::FilePath mv3_crx = test_dir.Pack("mv3.crx");

  const Extension* extension = nullptr;
  {
    ScopedTestMV2Enabler mv2_enabler;
    extension = InstallExtension(mv2_crx, /*expected_change=*/1,
                                 mojom::ManifestLocation::kInternal);
  }
  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();

  // Technically, this could be accomplished using a PRE_ test, similar to
  // other browser tests in this file. However, that makes it much more
  // difficult to update the extension to an MV3 version, since we couldn't
  // construct the extension dynamically.
  experiment_manager()->DisableAffectedExtensionsForTesting();

  // The MV2 extension is disabled.
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(extension_id));
  EXPECT_THAT(extension_prefs()->GetDisableReasons(extension_id),
              testing::UnorderedElementsAre(
                  disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION));

  // Update the extension to MV3. Note: Even though this doesn't result in a
  // _new_ extension, the `expected_change` is 1 here because this results in
  // the extension being added to the enabled set (so the enabled extension
  // count is 1 higher than it was before).
  const Extension* updated_extension = UpdateExtension(extension_id, mv3_crx,
                                                       /*expected_change=*/1);
  ASSERT_TRUE(updated_extension);
  EXPECT_EQ(updated_extension->id(), extension_id);

  // The new MV3 extension should be enabled.
  EXPECT_EQ(3, updated_extension->manifest_version());
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(extension_prefs()->GetDisableReasons(extension_id).empty());
}

// Tests that externally-installed extensions are allowed to be installed, but
// will still be disabled by the MV2 experiments.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       ExternalExtensionsCanBeInstalledButAreAlsoDisabled) {
  // External extensions are default-disabled on Windows and Mac. This won't
  // be affected by the MV2 deprecation, but for consistency of testing, we
  // disable this prompting in the test.
  FeatureSwitch::ScopedOverride feature_override(
      FeatureSwitch::prompt_for_external_extensions(), false);

  // TODO(devlin): Update this to a different extension so we use one dedicated
  // to this test ("good.crx" should likely be updated to MV3).
  static constexpr char kExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  base::FilePath crx_path = test_data_dir_.AppendASCII("good.crx");

  // Install a new external extension.
  ExternalProviderManager* external_provider_manager =
      ExternalProviderManager::Get(profile());
  TestExtensionRegistryObserver observer(extension_registry());
  auto provider = std::make_unique<MockExternalProvider>(
      external_provider_manager, mojom::ManifestLocation::kExternalPref);
  provider->UpdateOrAddExtension(kExtensionId, "1.0.0.0", crx_path);
  external_provider_manager->AddProviderForTesting(std::move(provider));
  scoped_refptr<const Extension> extension;
  {
    ScopedTestMV2Enabler mv2_enabler;
    external_provider_manager->CheckForExternalUpdates();
    extension = observer.WaitForExtensionInstalled();
  }
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(), kExtensionId);

  // The extension should install and be enabled. We allow installation of
  // external extensions (unlike webstore extensions) because we can't know if
  // the extension is MV2 or MV3 until we install it.
  // We could theoretically disable it immediately if it's MV2, but it'll get
  // disabled on the next run of Chrome.
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(kExtensionId));
  EXPECT_TRUE(extension_prefs()->GetDisableReasons(kExtensionId).empty());

  // The extension should still be counted as "affected" by the MV2 deprecation.
  EXPECT_TRUE(experiment_manager()->IsExtensionAffected(*extension));

  // And should also be disabled when we check again.
  experiment_manager()->DisableAffectedExtensionsForTesting();
  EXPECT_TRUE(
      extension_registry()->disabled_extensions().Contains(kExtensionId));
  EXPECT_THAT(extension_prefs()->GetDisableReasons(kExtensionId),
              testing::UnorderedElementsAre(
                  disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION));
}

// Tests that unpacked extensions cannot be installed in the unsupported
// experiment phase.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       UnpackedExtensionsCannotBeInstalledInUnsupportedPhase) {
  WaitForExtensionSystemReady();

  static constexpr char kMv2Manifest[] =
      R"({
           "name": "Simple MV2",
           "manifest_version": 2,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kMv2Manifest);

  base::RunLoop run_loop;
  std::u16string install_error;
  scoped_refptr<UnpackedInstaller> installer =
      UnpackedInstaller::Create(profile());
  auto on_complete = [&run_loop, &install_error](
                         const Extension* extension,
                         const base::FilePath& file_path,
                         const std::u16string& error) {
    install_error = error;
    run_loop.Quit();
  };
  installer->set_completion_callback(base::BindLambdaForTesting(on_complete));
  installer->set_be_noisy_on_failure(false);
  installer->Load(test_dir.UnpackedPath());
  run_loop.Run();

  EXPECT_EQ(
      u"Cannot install extension because it uses an unsupported "
      u"manifest version.",
      install_error);
}

}  // namespace extensions
