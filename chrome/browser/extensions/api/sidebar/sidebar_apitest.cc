// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/install_warning.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

// Runs each test with `extensions_features::kApiSidebar` enabled and disabled,
// on the trunk channel that the `sidebar` features require.
class SidebarAvailabilityApiTest : public base::test::WithFeatureOverride,
                                   public ExtensionApiTest {
 public:
  SidebarAvailabilityApiTest()
      : base::test::WithFeatureOverride(extensions_features::kApiSidebar) {}

 private:
  ScopedCurrentChannel current_channel_{version_info::Channel::UNKNOWN};
};

// Tests that an extension can use the `sidebar` manifest key and access
// `chrome.sidebar` only when the feature is enabled.
IN_PROC_BROWSER_TEST_P(SidebarAvailabilityApiTest, NamespaceAvailability) {
  // Load an MV3 extension that declares the `sidebar` key.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Sidebar test",
    "manifest_version": 3,
    "version": "0.1",
    "background": {"service_worker": "background.js"},
    "sidebar": {}
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);

  // With the feature off, the `sidebar` key is unavailable, so loading adds a
  // warning for it.
  if (IsParamFeatureEnabled()) {
    EXPECT_THAT(extension->install_warnings(), testing::IsEmpty());
  } else {
    EXPECT_THAT(extension->install_warnings(),
                testing::ElementsAre(testing::Field(
                    &InstallWarning::message,
                    "'sidebar' requires the 'ApiSidebar' feature flag to be "
                    "enabled.")));
  }

  // Check whether the service worker can see the `chrome.sidebar` namespace.
  base::Value result = ExecuteScriptInBackgroundPage(
      extension->id(), "chrome.test.sendScriptResult(typeof chrome.sidebar);");
  EXPECT_EQ(IsParamFeatureEnabled() ? "object" : "undefined", result);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SidebarAvailabilityApiTest);

}  // namespace extensions
