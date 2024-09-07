// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

// Attempts to load (parse) an extension from the given `file_path`, returning
// it on success. On failure, adds a test failure.
scoped_refptr<const Extension> LoadExtensionFromDirectory(
    const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string load_error;
  scoped_refptr<const Extension> extension = file_util::LoadExtension(
      file_path, mojom::ManifestLocation::kUnpacked, 0, &load_error);
  if (!extension) {
    ADD_FAILURE() << "Failed to create extension: " << load_error;
    return nullptr;
  }

  return extension;
}

}  // namespace

class DesktopAndroidExtensionsBrowserTest : public AndroidBrowserTest {
 public:
  DesktopAndroidExtensionsBrowserTest() = default;
  ~DesktopAndroidExtensionsBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// The following is a simple test exercising a basic navigation and script
// injection. This doesn't exercise any extensions logic, but ensures Chrome
// successfully starts and can navigate the web.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest, SanityCheck) {
  ASSERT_EQ(TabModelList::models().size(), 1u);

  EXPECT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("example.com", "/title1.html")));

  EXPECT_EQ("This page has no title.",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
}

// Tests the ability to parse and validate a simple extension.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       ParseAndValidateASimpleExtension) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  scoped_refptr<const Extension> extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Validate the fields in the extension.
  EXPECT_EQ("Test Extension", extension->name());
  EXPECT_EQ("0.1", extension->version().GetString());
  EXPECT_EQ(3, extension->manifest_version());
}

// Tests the adding an extension to the registry and navigating to a
// corresponding page in the extension, verifying the expected content, and
// leveraging the chrome.test API to pass a result. The latter verifies the
// core extension bindings system and API handling works, including
// exercising custom bindings.
IN_PROC_BROWSER_TEST_F(DesktopAndroidExtensionsBrowserTest,
                       NavigateToExtensionPage) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3
         })";
  static constexpr char kPageHtml[] =
      R"(<html>
           Hello, world
           <script src="page.js"></script>
         </html>)";
  static constexpr char kPageJs[] =
      R"(chrome.test.runTests([
           function sanityCheck() {
             chrome.test.assertEq(2, 1 + 1);
             chrome.test.succeed();
           }
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);

  scoped_refptr<const Extension> extension =
      LoadExtensionFromDirectory(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::BrowserContext* browser_context =
      GetActiveWebContents()->GetBrowserContext();

  auto* android_system = static_cast<DesktopAndroidExtensionSystem*>(
      ExtensionSystem::Get(browser_context));
  ASSERT_TRUE(android_system);
  android_system->AddExtension(extension);

  GURL extension_page = extension->GetResourceURL("page.html");

  ResultCatcher result_catcher;
  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), extension_page));
  EXPECT_EQ(extension_page, GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ("Hello, world",
            content::EvalJs(GetActiveWebContents(), "document.body.innerText"));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
