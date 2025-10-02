// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using DevtoolsApiTest = ExtensionApiTest;

// Tests that other extensions are not allowed to fetch resources of a devtools
// extension that does not specify any web-accessible resources.
// Regression test for https://crbug.com/428397712.
IN_PROC_BROWSER_TEST_F(DevtoolsApiTest,
                       FetchBlockedWithoutWebAccessibleResources) {
  // Load an extension that specifies a devtools page.
  TestExtensionDir devtools_extension_dir;
  devtools_extension_dir.WriteManifest(R"({
    "name": "Devtools Extension",
    "version": "1.0",
    "manifest_version": 3,
    "devtools_page": "devtools.html"
  })");
  devtools_extension_dir.WriteFile(FILE_PATH_LITERAL("devtools.html"), "");

  const Extension* devtools_extension =
      LoadExtension(devtools_extension_dir.UnpackedPath());
  ASSERT_TRUE(devtools_extension);

  // Load a second extension that will attempt to fetch content from the first.
  TestExtensionDir fetching_extension_dir;
  fetching_extension_dir.WriteManifest(R"({
    "name": "Background Extension",
    "version": "1.0",
    "manifest_version": 3,
    "background": {
      "service_worker": "background.js"
    }
  })");
  fetching_extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");

  const Extension* fetching_extension =
      LoadExtension(fetching_extension_dir.UnpackedPath());
  ASSERT_TRUE(fetching_extension);

  // A script that will attempt to fetch the content of the manifest from the
  // devtools extension.
  std::string script = base::StringPrintf(
      R"((async () => {
           const url = 'chrome-extension://%s/manifest.json';
           try {
             const response = await fetch(url);
             const manifestContent = await response.text();
             chrome.test.sendScriptResult(manifestContent);
           } catch (e) {
             chrome.test.sendScriptResult(e.message);
           }
         })())",
      devtools_extension->id().c_str());

  BackgroundScriptExecutor executor(profile());
  base::Value result = executor.ExecuteScript(
      fetching_extension->id(), script,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  // The fetch should have failed.
  ASSERT_TRUE(result.is_string());
  EXPECT_THAT(result.GetString(), testing::HasSubstr("Failed to fetch"));
}

}  // namespace extensions
