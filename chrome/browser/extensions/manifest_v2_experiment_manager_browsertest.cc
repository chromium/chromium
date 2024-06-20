// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
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

// Each test may have a different desired stage. Store them here so the test
// harness properly instantiates them.
MV2ExperimentStage GetExperimentStageForTest(std::string_view test_name) {
  struct {
    const char* test_name;
    MV2ExperimentStage stage;
  } test_stages[] = {
      {"PRE_PRE_ExtensionsAreDisabledOnStartup", MV2ExperimentStage::kWarning},
      {"PRE_ExtensionsAreDisabledOnStartup", MV2ExperimentStage::kWarning},
      {"ExtensionsAreDisabledOnStartup",
       MV2ExperimentStage::kDisableWithReEnable},
      {"PRE_PRE_ExtensionsCanBeReEnabledByUsers", MV2ExperimentStage::kWarning},
      {"PRE_ExtensionsCanBeReEnabledByUsers",
       MV2ExperimentStage::kDisableWithReEnable},
      {"ExtensionsCanBeReEnabledByUsers",
       MV2ExperimentStage::kDisableWithReEnable},
  };

  for (const auto& test_stage : test_stages) {
    if (test_stage.test_name == test_name) {
      return test_stage.stage;
    }
  }

  NOTREACHED_NORETURN()
      << "Unknown test name '" << test_name << "'. "
      << "You need to add a new test stage entry into this collection.";
}

}  // namespace

class ManifestV2ExperimentManagerBrowserTest : public ExtensionBrowserTest {
 public:
  ManifestV2ExperimentManagerBrowserTest() = default;
  ~ManifestV2ExperimentManagerBrowserTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Each test may need a different value for the experiment stages, since
    // many need some kind of pre-experiment set up, then test the behavior on
    // subsequent startups. Initialize each test according to its preferred
    // stage.
    MV2ExperimentStage experiment_stage = GetExperimentStageForTest(
        testing::UnitTest::GetInstance()->current_test_info()->name());

    switch (experiment_stage) {
      case MV2ExperimentStage::kWarning:
        enabled_features.push_back(
            extensions_features::kExtensionManifestV2DeprecationWarning);
        disabled_features.push_back(
            extensions_features::kExtensionManifestV2Disabled);
        break;
      case MV2ExperimentStage::kDisableWithReEnable:
        enabled_features.push_back(
            extensions_features::kExtensionManifestV2Disabled);
        disabled_features.push_back(
            extensions_features::kExtensionManifestV2DeprecationWarning);
        break;
      case MV2ExperimentStage::kNone:
        NOTREACHED_NORETURN() << "Unhandled stage.";
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ExtensionBrowserTest::SetUp();
  }

  void WaitForExtensionSystemReady() {
    base::RunLoop run_loop;
    ExtensionSystem::Get(profile())->ready().Post(
        FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Adds a new MV2 extension with the given `name` to the profile, returning
  // it afterwards.
  const Extension* AddMV2Extension(std::string_view name) {
    static constexpr char kManifest[] =
        R"({
             "name": "%s",
             "manifest_version": 2,
             "version": "0.1"
           })";
    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(kManifest, name.data()));
    return InstallExtension(test_dir.UnpackedPath(), /*expected_change=*/1,
                            mojom::ManifestLocation::kInternal);
  }

  // Returns true if the extension was explicitly re-enabled by the user after
  // being disabled by the MV2 experiment.
  bool WasExtensionReEnabledByUser(const ExtensionId& extension_id) {
    return experiment_manager()->DidUserReEnableExtensionForTesting(
        extension_id);
  }

  MV2ExperimentStage GetActiveExperimentStage() {
    return experiment_manager()->GetCurrentExperimentStage();
  }

  ExtensionPrefs* extension_prefs() { return ExtensionPrefs::Get(profile()); }

  ManifestV2ExperimentManager* experiment_manager() {
    return ManifestV2ExperimentManager::Get(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test series to verify MV2 extensions are disabled on startup.
// Step 1 (Warning Only Stage): Install an MV2 extension.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_PRE_ExtensionsAreDisabledOnStartup) {
  EXPECT_EQ(MV2ExperimentStage::kWarning, GetActiveExperimentStage());

  const Extension* extension = AddMV2Extension("Test MV2 Extension");
  ASSERT_TRUE(extension);
}
// Step 2 (Warning Only Stage): Verify the MV2 extension is still enabled after
// restarting the browser. Since this is still a PRE_ stage, the disabling
// experiment isn't active, and MV2 extensions should be unaffected.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_ExtensionsAreDisabledOnStartup) {
  EXPECT_EQ(MV2ExperimentStage::kWarning, GetActiveExperimentStage());

  WaitForExtensionSystemReady();

  const Extension* extension = GetExtensionByName(
      "Test MV2 Extension", extension_registry()->enabled_extensions());

  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_id));

  EXPECT_EQ(0, extension_prefs()->GetDisableReasons(extension_id));
}
// Step 3 (Disable Stage): Verify the extension is disabled. Now the disabling
// experiment is active, and any old MV2 extensions are disabled.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       ExtensionsAreDisabledOnStartup) {
  EXPECT_EQ(MV2ExperimentStage::kDisableWithReEnable,
            GetActiveExperimentStage());

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

  EXPECT_EQ(
      static_cast<int>(disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION),
      extension_prefs()->GetDisableReasons(extension_id));
}

// A test series to verify extensions that are re-enabled by the user do not
// get re-disabled on subsequent starts.
// Step 1 (Warning Only Stage): Install an MV2 extension.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_PRE_ExtensionsCanBeReEnabledByUsers) {
  EXPECT_EQ(MV2ExperimentStage::kWarning, GetActiveExperimentStage());

  const Extension* extension = AddMV2Extension("Test MV2 Extension");
  ASSERT_TRUE(extension);
}
// Step 2 (Disable Stage): The extension will be disabled by the experiment.
// Re-enable the extension.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_ExtensionsCanBeReEnabledByUsers) {
  EXPECT_EQ(MV2ExperimentStage::kDisableWithReEnable,
            GetActiveExperimentStage());

  WaitForExtensionSystemReady();

  const Extension* extension = GetExtensionByName(
      "Test MV2 Extension", extension_registry()->disabled_extensions());
  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();

  // Re-enable the disabled extension.
  extension_service()->EnableExtension(extension_id);

  // The extension should be properly re-enabled, the disable reasons cleared,
  // and the extension should be marked as explicitly re-enabled.
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_id));
  EXPECT_EQ(0, extension_prefs()->GetDisableReasons(extension_id));
  EXPECT_TRUE(WasExtensionReEnabledByUser(extension_id));
}
// Step 3 (Disable Stage): The extension should still be enabled on a subsequent
// start since the user explicitly chose to re-enable it.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       ExtensionsCanBeReEnabledByUsers) {
  EXPECT_EQ(MV2ExperimentStage::kDisableWithReEnable,
            GetActiveExperimentStage());

  WaitForExtensionSystemReady();

  const Extension* extension = GetExtensionByName(
      "Test MV2 Extension", extension_registry()->enabled_extensions());
  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();

  EXPECT_EQ(0, extension_prefs()->GetDisableReasons(extension_id));
  EXPECT_TRUE(WasExtensionReEnabledByUser(extension_id));
}

}  // namespace extensions
