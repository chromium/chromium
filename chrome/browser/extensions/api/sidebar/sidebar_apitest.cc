// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/install_warning.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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

// Runs `chrome.sidebar` function tests with `extensions_features::kApiSidebar`
// enabled, on the trunk channel that the `sidebar` features require.
class SidebarApiTest : public ExtensionApiTest {
 public:
  SidebarApiTest() {
    feature_list_.InitAndEnableFeature(extensions_features::kApiSidebar);
  }

 protected:
  // Loads an MV3 extension with the `sidebar` key and a service worker.
  // Returns the loaded extension.
  const Extension* LoadSidebarExtension() {
    test_dir_.WriteManifest(R"({
      "name": "Sidebar test",
      "manifest_version": 3,
      "version": "0.1",
      "background": {"service_worker": "background.js"},
      "sidebar": {}
    })");
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "");
    return LoadExtension(test_dir_.UnpackedPath());
  }

  // Calls `chrome.sidebar.getLayout()` in the service worker of the extension
  // with `extension_id`. Returns the `side` of the resolved layout.
  base::Value GetLayoutSide(const ExtensionId& extension_id) {
    return ExecuteScriptInBackgroundPage(extension_id, R"(
      chrome.sidebar.getLayout().then(
          (layout) => chrome.test.sendScriptResult(layout.side));
    )");
  }

 private:
  ScopedCurrentChannel current_channel_{version_info::Channel::UNKNOWN};
  base::test::ScopedFeatureList feature_list_;
  TestExtensionDir test_dir_;
};

// Tests that `chrome.sidebar.getLayout()` resolves with the side panel's
// configured horizontal alignment.
IN_PROC_BROWSER_TEST_F(SidebarApiTest, GetLayout) {
  const Extension* extension = LoadSidebarExtension();
  ASSERT_TRUE(extension);

#if !BUILDFLAG(IS_ANDROID)
  // Align the side panel to the left and check the reported side, then switch
  // back to right alignment before checking it across all platforms.
  profile()->GetPrefs()->SetBoolean(prefs::kSidePanelHorizontalAlignment,
                                    false);
  EXPECT_EQ("left", GetLayoutSide(extension->id()));
  profile()->GetPrefs()->SetBoolean(prefs::kSidePanelHorizontalAlignment, true);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Desktop Android hardcodes right alignment in
  // `SidePanelService::GetSidePanelLayout()`.
  EXPECT_EQ("right", GetLayoutSide(extension->id()));
}

}  // namespace extensions
