// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/url_pattern.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

constexpr char kComponentExtensionKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC+uU63MD6T82Ldq5wjrDFn5mGmPnnnj"
    "WZBWxYXfpG4kVf0s+p24VkXwTXsxeI12bRm8/ft9sOq0XiLfgQEh5JrVUZqvFlaZYoS+g"
    "iZfUqzKFGMLa4uiSMDnvv+byxrqAepKz5G8XX/q5Wm5cvpdjwgiu9z9iM768xJy+Ca/G5"
    "qQwIDAQAB";

// The value set by the script
// in chrome/test/data/extensions/test_resources_test/script.js.
constexpr int kSentinelValue = 42;

// Returns the value of window.injectedSentinel from the active web contents of
// |browser|.
int RetrieveSentinelValue(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return content::EvalJs(web_contents, "window.injectedSentinel;").ExtractInt();
}

class ExtensionBrowserTestWithCustomTestResourcesLocation
    : public ExtensionBrowserTest {
 public:
  ExtensionBrowserTestWithCustomTestResourcesLocation() = default;

  ExtensionBrowserTestWithCustomTestResourcesLocation(
      const ExtensionBrowserTestWithCustomTestResourcesLocation&) = delete;
  ExtensionBrowserTestWithCustomTestResourcesLocation& operator=(
      const ExtensionBrowserTestWithCustomTestResourcesLocation&) = delete;

  ~ExtensionBrowserTestWithCustomTestResourcesLocation() override = default;

 private:
  // Instead of serving _test_resources/ paths from chrome/test/data/extensions,
  // serve them from chrome/test/data/extensions/test_resources_test.
  base::FilePath GetTestResourcesParentDir() override {
    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    return test_root_path.AppendASCII("extensions/test_resources_test");
  }
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));

  EXPECT_EQ(kSentinelValue, RetrieveSentinelValue(browser()));
}

// Tests that resources from _test_resources work in component extensions
// (which have a slightly different load path).
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestResourcesLoadInComponentExtension) {
  TestExtensionDir test_dir;
  constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 2,
           "key": "%s"
         })";
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, kComponentExtensionKey));

  constexpr char kPageHtml[] =
      R"(<html>
           <script src="_test_resources/test_resources_test/test_script.js">
           </script>
         </html>)";
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);

  const Extension* extension =
      LoadExtensionAsComponent(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_EQ(mojom::ManifestLocation::kComponent, extension->location());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));

  EXPECT_EQ(kSentinelValue, RetrieveSentinelValue(browser()));
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       LoadComponentExtensionUpdateWithManifestChanges) {
  TestExtensionDir test_dir;

  static constexpr char test_domain1[] = "http://*.domain1.com/*";
  static constexpr char test_domain2[] = "http://*.domain2.com/*";

  static constexpr char kManifestTemplate[] =
      R"({
           "name": "Test Component Extension",
           "version": "1",
           "description": "",
           "manifest_version": 3,
           "key": "%s",
           "externally_connectable": {
             "matches": [
                "%s"
             ]
           }
         })";

  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, kComponentExtensionKey, test_domain1));

  const Extension* extension1 =
      LoadExtensionAsComponent(test_dir.UnpackedPath());
  ASSERT_TRUE(extension1);
  EXPECT_EQ(mojom::ManifestLocation::kComponent, extension1->location());

  ExternallyConnectableInfo* info1 = static_cast<ExternallyConnectableInfo*>(
      extension1->GetManifestData(manifest_keys::kExternallyConnectable));
  ASSERT_TRUE(info1);
  EXPECT_EQ(1ul, info1->matches.size());

  EXPECT_EQ(URLPattern(URLPattern::SCHEME_ALL, test_domain1),
            *info1->matches.begin());

  // Update the manifest and load the extension again.
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, kComponentExtensionKey, test_domain2));
  const Extension* extension2 =
      LoadExtensionAsComponent(test_dir.UnpackedPath());
  ASSERT_TRUE(extension2);
  EXPECT_EQ(mojom::ManifestLocation::kComponent, extension2->location());

  ExternallyConnectableInfo* info2 = static_cast<ExternallyConnectableInfo*>(
      extension2->GetManifestData(manifest_keys::kExternallyConnectable));
  ASSERT_TRUE(info2);
  EXPECT_EQ(1ul, info2->matches.size());

  EXPECT_EQ(URLPattern(URLPattern::SCHEME_ALL, test_domain2),
            *info2->matches.begin());
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));

  EXPECT_EQ(kSentinelValue, RetrieveSentinelValue(browser()));
}

}  // namespace extensions
