// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

// The value set by the script
// in chrome/test/data/extensions/test_resources_test/script.js.
constexpr int kSentinelValue = 42;

// Returns the value of window.injectedSentinel from the active web contents of
// |browser|.
base::Optional<int> RetrieveSentinelValue(Browser* browser) {
  int result = 0;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!content::ExecuteScriptAndExtractInt(
          web_contents,
          "domAutomationController.send(window.injectedSentinel);", &result)) {
    ADD_FAILURE() << "Failed to execute script.";
    return base::nullopt;
  }

  return result;
}

class ExtensionBrowserTestWithCustomTestResourcesLocation
    : public ExtensionBrowserTest {
 public:
  ExtensionBrowserTestWithCustomTestResourcesLocation() = default;
  ~ExtensionBrowserTestWithCustomTestResourcesLocation() override = default;

 private:
  // Instead of serving _test_resources/ paths from chrome/test/data/extensions,
  // serve them from chrome/test/data/extensions/test_resources_test.
  base::FilePath GetTestResourcesParentDir() override {
    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    return test_root_path.AppendASCII("extensions/test_resources_test");
  }

  DISALLOW_COPY_AND_ASSIGN(ExtensionBrowserTestWithCustomTestResourcesLocation);
};

}  // namespace

// A simple test to ensure resources can be served from _test_resources/, and
// properly load.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, TestResourcesLoad) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2
         })");
  constexpr char kPageHtml[] =
      R"(<html>
           <script src="_test_resources/test_resources_test/test_script.js">
           </script>
         </html>)";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("page.html"));

  base::Optional<int> sentinel = RetrieveSentinelValue(browser());
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(kSentinelValue, *sentinel);
}

// Tests that resources from _test_resources work in component extensions
// (which have a slightly different load path).
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestResourcesLoadInComponentExtension) {
  TestExtensionDir test_dir;
  constexpr char kKey[] =
      "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC+uU63MD6T82Ldq5wjrDFn5mGmPnnnj"
      "WZBWxYXfpG4kVf0s+p24VkXwTXsxeI12bRm8/ft9sOq0XiLfgQEh5JrVUZqvFlaZYoS+g"
      "iZfUqzKFGMLa4uiSMDnvv+byxrqAepKz5G8XX/q5Wm5cvpdjwgiu9z9iM768xJy+Ca/G5"
      "qQwIDAQAB";
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2,
           "key": "%s"
         })";
  test_dir.WriteManifest(base::StringPrintf(kManifestTemplate, kKey));

  constexpr char kPageHtml[] =
      R"(<html>
           <script src="_test_resources/test_resources_test/test_script.js">
           </script>
         </html>)";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);

  const Extension* extension =
      LoadExtensionAsComponent(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_EQ(Manifest::COMPONENT, extension->location());

  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("page.html"));

  base::Optional<int> sentinel = RetrieveSentinelValue(browser());
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(kSentinelValue, *sentinel);
}

// Tests that resources from _test_resources can be loaded from different
// directories. Though the default is chrome/test/data/extensions, a test class
// can specify its own.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTestWithCustomTestResourcesLocation,
                       TestResourcesLoadFromCustomPath) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2
         })");
  // Note: Since this class serves _test_resources from
  // chrome/test/data/extensions/test_resources_test, the
  // path is just _test_resources/test_script.js.
  constexpr char kPageHtml[] =
      R"(<html>
           <script src="_test_resources/test_script.js"></script>
         </html>)";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("page.html"));

  base::Optional<int> sentinel = RetrieveSentinelValue(browser());
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(kSentinelValue, *sentinel);
}

}  // namespace extensions
