// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class SidePanelApiTest : public ExtensionApiTest {
 private:
  ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
};

// Verify normal chrome.sidePanel functionality.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("side_panel/extension")) << message_;
}

// Verify chrome.sidePanel behavior without permissions.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, PermissionMissing) {
  ASSERT_TRUE(RunExtensionTest("side_panel/permission_missing")) << message_;
}

// Verify chrome.sidePanel.getOptions behavior without side_panel manifest key.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, MissingManifestKey) {
  ASSERT_TRUE(RunExtensionTest("side_panel/missing_manifest_key")) << message_;
}

// Verify chrome.sidePanel.get/setPanelBehavior behavior.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, PanelBehavior) {
  ASSERT_TRUE(RunExtensionTest("side_panel/panel_behavior")) << message_;
}

// Verify normal chrome.sidePanel functionality.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, ApiOnly) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("side_panel/api_only")) << message_;
}

class SidePanelApiWithExtensionTest : public SidePanelApiTest {
 public:
  // Load and get extension.
  const Extension* GetExtension() {
    static constexpr char kManifest[] =
        R"({
            "name": "Test",
            "manifest_version": 3,
            "version": "0.1",
            "side_panel": {"default_path": "default_path.html"}
          })";
    static constexpr char kDefaultPathHtml[] = "<html><body>Test</body></html>";
    static constexpr char kCustomPathHtml[] =
        "<html><body>Custom</body></html>";
    TestExtensionDir test_dir;
    test_dir.WriteManifest(kManifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("default_path.html"),
                       kDefaultPathHtml);
    test_dir.WriteFile(FILE_PATH_LITERAL("custom_path.html"), kCustomPathHtml);
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    return extension;
  }
};

// Test the behavior of SetOptions for a tab and fallbacks on disable/uninstall.
IN_PROC_BROWSER_TEST_F(SidePanelApiWithExtensionTest, ExtensionRegistry) {
  static constexpr int tab_id = 0;

  // Test cases.
  void (*test_cases[])(const ExtensionId& id,
                       extensions::TestExtensionRegistryObserver* observer,
                       ExtensionService* extension_service) = {
      // "Unload extension"
      [](const ExtensionId& id,
         extensions::TestExtensionRegistryObserver* observer,
         ExtensionService* extension_service) {
        extension_service->DisableExtension(
            id, disable_reason::DISABLE_USER_ACTION);
        observer->WaitForExtensionUnloaded();
      },
      // "Uninstall extension",
      // The uninstall case should technically not finish as default_path.html.
      // However, the good news is that it's cleared from `panels_`, as
      // desired. A real extension would not be able to GetOptions() after it
      // has been uninstalled. Therefore this test vacuously succeeds.
      // Confirmation is obtained via `HasExtensionPanelOptions()`.
      [](const ExtensionId& id,
         extensions::TestExtensionRegistryObserver* observer,
         ExtensionService* extension_service) {
        extension_service->UninstallExtension(
            id, UninstallReason::UNINSTALL_REASON_FOR_TESTING, nullptr);
        observer->WaitForExtensionUninstalled();
      }};

  // Run test cases.
  for (const auto& test_case : test_cases) {
    auto* extension = GetExtension();
    ASSERT_TRUE(extension);

    // Set panel options for extension and verify they are stored as expected.
    SidePanelService* service = SidePanelService::Get(profile());
    auto options = service->GetOptions(*extension, tab_id);
    EXPECT_EQ("default_path.html", options.path.value());
    options.path = "custom_path.html";
    options.tab_id = tab_id;
    service->SetOptions(*extension, std::move(options));
    options = service->GetOptions(*extension, tab_id);
    EXPECT_EQ("custom_path.html", options.path.value());

    // The options for a different tab should still be default.
    options = service->GetOptions(*extension, tab_id + 1);
    EXPECT_EQ("default_path.html", options.path.value());

    // Test case to verify that stored options are cleared on un-load/install.
    EXPECT_TRUE(service->HasExtensionPanelOptionsForTest(extension->id()));
    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(profile()), extension->id());
    test_case(extension->id(), &observer, extension_service());
    options = service->GetOptions(*extension, tab_id);
    EXPECT_EQ("default_path.html", options.path.value());
    EXPECT_FALSE(service->HasExtensionPanelOptionsForTest(extension->id()));
  }
}

IN_PROC_BROWSER_TEST_F(SidePanelApiTest, OpenPanelErrors) {
  ASSERT_TRUE(RunExtensionTest("side_panel/open_panel_errors"));
}

}  // namespace extensions
