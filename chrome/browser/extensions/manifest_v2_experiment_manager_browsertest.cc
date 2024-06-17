// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/one_shot_event.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
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

}  // namespace

class ManifestV2ExperimentManagerBrowserTest : public ExtensionBrowserTest {
 public:
  ManifestV2ExperimentManagerBrowserTest() = default;
  ~ManifestV2ExperimentManagerBrowserTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Enable the "Disable with Re-Enable" phase only in the final test of the
    // sequence. This allows tests to initialize state (such as installing MV2
    // extensions) to simulate a user encountering the disabling experiment for
    // the first time. Otherwise, assume the user is in the "warning" phase for
    // the experiment.
    bool disable_mv2_extensions = GetTestPreCount() == 0;
    if (disable_mv2_extensions) {
      enabled_features.push_back(
          extensions_features::kExtensionManifestV2Disabled);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2DeprecationWarning);
    } else {
      enabled_features.push_back(
          extensions_features::kExtensionManifestV2DeprecationWarning);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Disabled);
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Step 1: Install an MV2 extension.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_PRE_ExtensionsAreDisabledOnStartup) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test MV2 Extension",
           "manifest_version": 2,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  const Extension* extension =
      InstallExtension(test_dir.UnpackedPath(), /*expected_change=*/1,
                       mojom::ManifestLocation::kInternal);
  ASSERT_TRUE(extension);
}

// Step 2: Verify the MV2 extension is still enabled after restarting the
// browser. Since this is still a PRE_ stage, the disabling experiment isn't
// active, and MV2 extensions should be unaffected.
IN_PROC_BROWSER_TEST_F(ManifestV2ExperimentManagerBrowserTest,
                       PRE_ExtensionsAreDisabledOnStartup) {
  WaitForExtensionSystemReady();

  const Extension* extension = GetExtensionByName(
      "Test MV2 Extension", extension_registry()->enabled_extensions());

  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();
  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_id));

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(0, extension_prefs->GetDisableReasons(extension_id));
}

// Step 3: Verify the extension is disabled. Now the disabling experiment is
// active, and any old MV2 extensions are disabled.
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

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(
      static_cast<int>(disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION),
      extension_prefs->GetDisableReasons(extension_id));
}

}  // namespace extensions
