// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
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

// Tests that revoking file access for an extension closes the open DevTools
// window and prevents the extension from accessing local file resources when
// DevTools is reopened.
// Regression test for https://crbug.com/483435192.
// TODO(https://crbug.com/546216109): Enable on desktop android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CantGetFileResourceWhenFileAccessRevoked \
  DISABLED_CantGetFileResourceWhenFileAccessRevoked
#else
#define MAYBE_CantGetFileResourceWhenFileAccessRevoked \
  CantGetFileResourceWhenFileAccessRevoked
#endif
IN_PROC_BROWSER_TEST_F(DevtoolsApiTest,
                       MAYBE_CantGetFileResourceWhenFileAccessRevoked) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir devtools_extension_dir;
  devtools_extension_dir.WriteManifest(R"({
    "name": "Devtools Extension",
    "version": "1.0",
    "manifest_version": 3,
    "devtools_page": "devtools.html"
  })");
  devtools_extension_dir.WriteFile(
      FILE_PATH_LITERAL("devtools.html"),
      "<html><head><script src='devtools.js'></script></head></html>");
  devtools_extension_dir.WriteFile(FILE_PATH_LITERAL("devtools.js"), R"(
      function onResourceAdded(resource) {
        if (resource.url.includes('sentinel.js')) {
          chrome.devtools.inspectedWindow.getResources(resources => {
            const hasFile = resources.some(r => r.url.startsWith('file:'));
            chrome.test.sendMessage(
                hasFile ? 'has_file_access' : 'no_file_access');
          });
        }
      }

      chrome.devtools.inspectedWindow.onResourceAdded.addListener(
          onResourceAdded);

      chrome.test.sendMessage('ready');
  )");

  // Load the extension with file access enabled.
  const Extension* devtools_extension = LoadExtension(
      devtools_extension_dir.UnpackedPath(), {.allow_file_access = true});
  ASSERT_TRUE(devtools_extension);
  const ExtensionId extension_id = devtools_extension->id();

  // Helper script that adds a source map referencing a local file: URL followed
  // by a non-file sentinel resource.
  base::FilePath test_file =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .AppendASCII("content/test/data/devtools/navigation.html");
  GURL file_url = net::FilePathToFileURL(test_file);

  std::string inject_script = base::StringPrintf(
      R"(
        const script = document.createElement('script');
        script.textContent = 'console.log("loaded");' +
            '\n//# sourceMappingURL=data:application/json,{"version":3,"sources":["%s","%s"]}';
        document.body.appendChild(script);
      )",
      file_url.spec().c_str(),
      embedded_test_server()->GetURL("/sentinel.js").spec().c_str());

  // Step 1: Open an initial page and open DevTools.
  GURL initial_url = embedded_test_server()->GetURL("/simple.html");
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));

  ExtensionTestMessageListener ready_listener_1("ready");
  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(web_contents,
                                                    /*is_docked=*/false);
  ASSERT_TRUE(devtools_window);
  ASSERT_TRUE(ready_listener_1.WaitUntilSatisfied());

  // Inject the script and verify the extension has file access on the first
  // run.
  {
    ExtensionTestMessageListener file_listener;
    ASSERT_TRUE(content::ExecJs(web_contents, inject_script));
    EXPECT_TRUE(file_listener.WaitUntilSatisfied());
    EXPECT_EQ("has_file_access", file_listener.message());
  }

  // Step 2: Revoke file access for the extension while DevTools is open.
  // This triggers an extension reload (unload + load). Verify that the DevTools
  // window automatically closes upon unload.
  base::RunLoop close_run_loop;
  DevToolsWindowTesting::Get(devtools_window)
      ->SetCloseCallback(close_run_loop.QuitClosure());

  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         extension_id);
  util::SetAllowFileAccess(extension_id, profile(), false);
  observer.WaitForExtensionLoaded();
  close_run_loop.Run();

  // Step 3: Reopen DevTools on a fresh page and verify that the extension now
  // reports no file access.
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));

  ExtensionTestMessageListener ready_listener_2("ready");
  devtools_window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/false);
  ASSERT_TRUE(devtools_window);
  ASSERT_TRUE(ready_listener_2.WaitUntilSatisfied());

  {
    ExtensionTestMessageListener file_listener;
    ASSERT_TRUE(content::ExecJs(web_contents, inject_script));
    EXPECT_TRUE(file_listener.WaitUntilSatisfied());
    EXPECT_EQ("no_file_access", file_listener.message());
  }

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
}

}  // namespace extensions
