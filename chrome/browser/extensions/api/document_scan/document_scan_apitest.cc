// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"

using DocumentScanApiTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(DocumentScanApiTest, TestLoadPermissions) {
  // This test simply checks to see if we have the correct permissions to load
  // the extension.
  extensions::TestExtensionDir test_dir;
  constexpr char kManifest[] =
      R"({
         "name": "Document Scan API Test",
         "version": "0.1",
         "manifest_version": 3,
         "background": { "service_worker": "background.js"},
         "permissions": ["documentScan"]
       })";
  constexpr char kBackgroundJs[] = R"(
      chrome.test.runTests([
        function apiFunctionExists() {
          chrome.test.assertTrue(!!chrome.documentScan);
          chrome.test.assertTrue(!!chrome.documentScan.scan);
          chrome.test.succeed();
        },
      ]);
  )";
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}
