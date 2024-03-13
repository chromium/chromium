// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

using ExtensionCspApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ExtensionCspApiTest, ContentSecurityPolicy) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("content_security_policy")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionCspApiTest, DefaultContentSecurityPolicy) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("default_content_security_policy")) <<
      message_;
}

// Tests that the Manifest V3 extension CSP allows localhost sources to be
// embedded in extension pages.
IN_PROC_BROWSER_TEST_F(ExtensionCspApiTest,
                       ManifestV3AllowsLocalhostInPagesForUnpackedExtensions) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "manifest v3 allows localhost and 127.0.0.1",
           "version": "0.1",
           "manifest_version": 3,
           "content_security_policy": {
             "extension_pages":
                 "script-src 'self' http://localhost:* http://127.0.0.1:*;"
           }
         })";
  static constexpr char kPageHtmlTemplate[] =
      R"(<html>
           <script src="http://localhost:%d/extensions/local_includes/pass1.js">
           </script>
           <script src="http://127.0.0.1:%d/extensions/local_includes/pass2.js">
           </script>
           <script src="page.js"></script>
         </html>)";
  // Note that `jsPass1()` and `jsPass2()` are defined in the pass1.js and
  // pass2.js resources that are included; they each call chrome.test.succeed().
  static constexpr char kPageJs[] =
      R"(chrome.test.runTests([
           function testLocalHostInclude() {
             window.jsPass1();
           },
           function testLocalHostIPInclude() {
             window.jsPass2();
           }]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("page.html"),
      base::StringPrintf(kPageHtmlTemplate, embedded_test_server()->port(),
                         embedded_test_server()->port()));

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(),
                               {.extension_url = "page.html"}, {}))
      << message_;
}

// Tests that the Manifest V3 extension CSP allows for localhost sources being
// imported from service workers.
IN_PROC_BROWSER_TEST_F(
    ExtensionCspApiTest,
    ManifestV3AllowsLocalhostInServiceWorkersForUnpackedExtensions) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "manifest v3 allows localhost and 127.0.0.1",
           "version": "0.1",
           "manifest_version": 3,
           "content_security_policy": {
             "extension_pages":
                 "script-src 'self' http://localhost:* http://127.0.0.1:*; object-src 'self'"
           },
           "background": {"service_worker": "background.js", "type": "module"}
         })";
  static constexpr char kBackgroundJs[] =
      R"(import {jsPass1} from
             'http://localhost:%d/extensions/local_includes/module_pass1.js';
         import {jsPass2} from
             'http://localhost:%d/extensions/local_includes/module_pass2.js';
         chrome.test.runTests([
             function testLocalHostInclude() {
               jsPass1();
             },
             function testLocalHostIPInclude() {
               jsPass2();
             }]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundJs, embedded_test_server()->port(),
                         embedded_test_server()->port()));

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

// Tests that MV3 disallows localhost in packed extensions.
IN_PROC_BROWSER_TEST_F(ExtensionCspApiTest,
                       ManifestV3DisallowsLocalhostForPackedExtensions) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "manifest v3 allows localhost and 127.0.0.1",
           "version": "0.1",
           "manifest_version": 3,
           "content_security_policy": {
             "extension_pages":
                 "script-src 'self' http://localhost:* http://127.0.0.1:*; object-src 'self'"
           }
         })";

  static constexpr char kPageHtmlTemplate[] =
      R"(<html>
           <script src="http://localhost:%d/extensions/local_includes/pass1.js">
           </script>
           <script src="http://127.0.0.1:%d/extensions/local_includes/pass2.js">
           </script>
           <script src="page.js"></script>
         </html>)";
  // Note that `jsPass1()` and `jsPass2()` are defined in the pass1.js and
  // pass2.js resources that are included. However, since the scripts should be
  // blocked by CSP, the variables should be undefined.
  static constexpr char kPageJs[] =
      R"(chrome.test.runTests([
           function testLocalHostInclude() {
             chrome.test.assertTrue(!window.jsPass1);
             chrome.test.succeed();
           },
           function testLocalHostIPInclude() {
             chrome.test.assertTrue(!window.jsPass2);
             chrome.test.succeed();
           }]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("page.html"),
      base::StringPrintf(kPageHtmlTemplate, embedded_test_server()->port(),
                         embedded_test_server()->port()));

  ResultCatcher result_catcher;
  ChromeTestExtensionLoader test_loader(profile());
  test_loader.set_pack_extension(true);
  scoped_refptr<const Extension> extension =
      test_loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_FALSE(Manifest::IsUnpackedLocation(extension->location()));

  // Blocking the script load should emit a log.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern("Refused to load the script '*");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  EXPECT_EQ(2u, console_observer.messages().size());
}

}  // namespace extensions
