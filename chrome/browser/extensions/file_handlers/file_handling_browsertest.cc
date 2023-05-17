// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {
namespace {

class FileHandlingWithoutFeatureBrowserTest : public ExtensionBrowserTest {
 public:
  FileHandlingWithoutFeatureBrowserTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionWebFileHandlers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Web File Handlers are either parsed or emit a warning, depending on the
// current browser channel and feature enablement.
IN_PROC_BROWSER_TEST_F(FileHandlingWithoutFeatureBrowserTest, Warning) {
  struct {
    const char* name;
    const char* manifest;
  } test_cases[] = {
      {"Valid `file_handlers` key",
       R"({
        "name": "Test",
        "version": "0.0.1",
        "manifest_version": 3,
        "file_handlers": [
          {
            "name": "Comma separated values",
            "action": "/open-csv.html",
            "accept": {"text/csv": [".csv"]}
          }
        ]
      })"},
      {"Invalid `file_handlers` key",
       R"({
        "name": "Test",
        "version": "0.0.1",
        "manifest_version": 3,
        "file_handlers": [
          {
            "error": "Invalid"
          }
        ]
      })"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    // Create an extension.
    TestExtensionDir extension_dir;
    extension_dir.WriteFile(FILE_PATH_LITERAL("open-csv.html"), "");
    extension_dir.WriteManifest(test_case.manifest);

    // Load the extension.
    const Extension* extension = LoadExtension(
        extension_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
    ASSERT_TRUE(extension);

    // Verify that there are no file handlers and that a warning was observed.
    ASSERT_FALSE(WebFileHandlers::HasFileHandlers(*extension));
    ASSERT_EQ(1u, extension->install_warnings().size());
    EXPECT_EQ(std::string("Unrecognized manifest key 'file_handlers'."),
              extension->install_warnings().front().message);
  }
}

}  // namespace
}  // namespace extensions
