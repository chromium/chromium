// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using ComponentExtensionBrowserTest = ExtensionBrowserTest;

#if BUILDFLAG(IS_CHROMEOS)
// Tests that MojoJS is enabled for component extensions that need it.
// Note the test currently only runs for ChromeOS because the test extension
// uses `mojoPrivate` to test and `mojoPrivate` is ChromeOS only.
IN_PROC_BROWSER_TEST_F(ComponentExtensionBrowserTest, MojoJS) {
  ResultCatcher result_catcher;

  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("service_worker/mojo"),
                    {.load_as_component = true});
  ASSERT_TRUE(extension);

  ASSERT_TRUE(result_catcher.GetNextResult());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

const ExtensionId kExtensionId = "iegclhlplifhodhkoafiokenjoapiobj";
constexpr char kExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8P"
    "Ffv8iucWvzO+3xpF/Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEf"
    "qxwovJwN+v1/SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv"
    "7TCwoVPKBfVshpFjdDOTeBg4iLctO3S/06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9i"
    "L8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6pBP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+Ydz"
    "afIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";

// Tests updating a Service Worker-based component extension across a restart.
// This simulates a browser update where a component extension might change.
class ComponentExtensionServiceWorkerUpdateBrowserTest
    : public ComponentExtensionBrowserTest {
 public:
  void WriteExtension(TestExtensionDir* dir, int version) {
    constexpr char kManifestTemplate[] =
        R"({
         "name": "Component SW Update Test",
         "manifest_version": 3,
         "version": "%d",
         "background": {"service_worker": "sw.js"},
         "key": "%s"
       })";
    dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, version, kExtensionKey));

    constexpr char kBackgroundScriptTemplate[] =
        R"(self.version = %d;
       chrome.test.sendMessage(`v${self.version} ready`);)";
    dir->WriteFile(FILE_PATH_LITERAL("sw.js"),
                   base::StringPrintf(kBackgroundScriptTemplate, version));
  }

  int GetWorkerVersion(const ExtensionId& id) {
    constexpr char kGetVersionScript[] =
        "chrome.test.sendScriptResult(self.version);";
    base::Value version = ExecuteScriptInBackgroundPage(id, kGetVersionScript);
    if (!version.is_int()) {
      ADD_FAILURE() << "Script did not return an integer. Value: " << version;
      return -1;
    }
    return version.GetInt();
  }

  TestExtensionDir test_dir_v1_;
  TestExtensionDir test_dir_v2_;
};

// PRE_ test: Installs V1 of the component extension. Verifies it runs.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerUpdateBrowserTest,
                       PRE_Update) {
  WriteExtension(&test_dir_v1_, 1);

  // Load V1 of the component extension.
  ExtensionTestMessageListener v1_ready("v1 ready");
  const Extension* extension_v1 =
      LoadExtension(test_dir_v1_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension_v1);

  // Ensure it has the correct ID and wait for it to start.
  const ExtensionId id = extension_v1->id();
  ASSERT_EQ(kExtensionId, id);
  ASSERT_TRUE(v1_ready.WaitUntilSatisfied());

  // Check service worker version.
  EXPECT_EQ("1", extension_v1->version().GetString());
  EXPECT_EQ(1, GetWorkerVersion(id));
}

// Main test: Installs V2 of the component extension. Verifies V2 runs.
IN_PROC_BROWSER_TEST_F(ComponentExtensionServiceWorkerUpdateBrowserTest,
                       Update) {
  ASSERT_FALSE(
      extension_registry()->enabled_extensions().GetByID(kExtensionId));
  WriteExtension(&test_dir_v2_, 2);

  // Load V2 of the component extension.
  ExtensionTestMessageListener v2_ready("v2 ready");
  const Extension* extension_v2 =
      LoadExtension(test_dir_v2_.UnpackedPath(), {.load_as_component = true});
  ASSERT_TRUE(extension_v2);

  // Ensure it has the correct ID (same as V1) and wait for it to start.
  const ExtensionId id = extension_v2->id();
  EXPECT_EQ(kExtensionId, id);
  ASSERT_TRUE(v2_ready.WaitUntilSatisfied());

  // Check service worker version.
  EXPECT_EQ("2", extension_v2->version().GetString());
  EXPECT_EQ(2, GetWorkerVersion(id));
}

}  // namespace extensions
