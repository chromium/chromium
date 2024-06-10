// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using SidePanelManifestBrowserTest = ExtensionBrowserTest;

// Succeed when a file exists, when there is a query, or a bookmark.
IN_PROC_BROWSER_TEST_F(SidePanelManifestBrowserTest, ValidateSuccess) {
  // Load the extenesion and a real file for validation.
  static constexpr char kManifest[] =
      R"({
        "name": "Test",
        "version": "1.0",
        "manifest_version": 3,
        "side_panel": {"default_path": "default_path.html?a=b#c"}
      })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("default_path.html"), "");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Verify that the extension loads without error.
  std::string error;
  std::vector<InstallWarning> warnings;
  base::ScopedAllowBlockingForTesting allow_blocking_for_validate_extension;
  ManifestHandler::ValidateExtension(extension, &error, &warnings);
  ASSERT_TRUE(error.empty());
}

}  // namespace extensions
