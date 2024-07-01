// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"

#include "base/one_shot_event.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class ScopedTestMV2EnablerBrowserTest : public ExtensionBrowserTest {
 public:
  ScopedTestMV2EnablerBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2Disabled);
  }
  ~ScopedTestMV2EnablerBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedTestMV2Enabler mv2_enabler_;
};

// Tests that, with the ScopedTestMV2Enabler, MV2 extensions can still be loaded
// and won't be disabled on startup.
IN_PROC_BROWSER_TEST_F(ScopedTestMV2EnablerBrowserTest,
                       MV2ExtensionsAreAllowedAndNotDisabled) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "manifest_version": 2,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  const Extension* extension =
      InstallExtension(test_dir.UnpackedPath(), /*expected_change=*/1,
                       mojom::ManifestLocation::kInternal);
  ASSERT_TRUE(extension);

  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(profile());

  // The experiment manager should not indicate the extension should be
  // blocked from being installed.
  EXPECT_FALSE(experiment_manager->ShouldBlockExtensionInstallation(
      extension->id(), extension->manifest_version(), extension->GetType(),
      extension->location(), extension->hashed_id()));

  // Even after disabling affected extensions, the extension should remain
  // enabled, since MV2 extensions are allowed for testing.
  experiment_manager->DisableAffectedExtensionsForTesting();

  EXPECT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension->id()));
  EXPECT_EQ(0,
            ExtensionPrefs::Get(profile())->GetDisableReasons(extension->id()));
}

}  // namespace extensions
