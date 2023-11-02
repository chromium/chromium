// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_test_extension_loader.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class ChromeTestExtensionLoaderUnitTest : public ExtensionApiTest {
 public:
  ChromeTestExtensionLoaderUnitTest() {}
  ~ChromeTestExtensionLoaderUnitTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// Tests that when loading an extension, the test loading code waits for
// content scripts to be fully read and initialized before continuing.
// Regression test for https://crbug.com/898682.
IN_PROC_BROWSER_TEST_F(ChromeTestExtensionLoaderUnitTest,
                       ContentScriptsAreFullyLoaded) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Test Ext",
           "manifest_version": 2,
           "version": "1.0",
           "content_scripts": [{
             "matches": ["http://example.com:*/*"],
             "js": ["script1.js", "script2.js"],
             "run_at": "document_end"
           }]
         })");
  constexpr char kScriptTemplate[] =
      R"((function() {
           let span = document.createElement('span');
           span.id = '%s';
           document.body.appendChild(span);
         })();
         %s)";

  // Create an extension that has reasonably large content scripts. The size of
  // the file is important, since it needs to be enough that reading it isn't
  // instantaneous. In local testing, this size (roughly 1MB) was sufficient to
  // consistently cause failures when the loading code did not properly wait for
  // content scripts.
  std::string lots_of_content =
      base::StrCat({"// ", std::string(1024 * 1024, 'a')});
  test_dir.WriteFile(
      FILE_PATH_LITERAL("script1.js"),
      base::StringPrintf(kScriptTemplate, "script1", lots_of_content.c_str()));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("script2.js"),
      base::StringPrintf(kScriptTemplate, "script2", lots_of_content.c_str()));

  scoped_refptr<const Extension> extension =
      ChromeTestExtensionLoader(profile()).LoadExtension(
          test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionSystem* extension_system = ExtensionSystem::Get(profile());
  EXPECT_TRUE(extension_system->user_script_manager()
                  ->GetUserScriptLoaderForExtension(extension->id())
                  ->HasLoadedScripts());

  // Sanity check: Test that the scripts inject.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "!!document.getElementById('script1');"));
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "!!document.getElementById('script2');"));
}

}  // namespace extensions
