// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));
namespace extensions {

using ExtensionI18nTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ExtensionI18nTest, I18nBasic) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("i18n")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionI18nTest, I18NUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create an Extension whose messages.json file will be updated.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());
  base::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("manifest.json"),
      extension_dir.GetPath().AppendASCII("manifest.json"));
  base::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("contentscript.js"),
      extension_dir.GetPath().AppendASCII("contentscript.js"));
  base::CopyDirectory(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("_locales"),
      extension_dir.GetPath().AppendASCII("_locales"), true);

  const Extension* extension = LoadExtension(extension_dir.GetPath());

  ResultCatcher catcher;

  // Test that the messages.json file is loaded and the i18n message is loaded.
  ASSERT_TRUE(NavigateToURL(
      embedded_test_server()->GetURL("/extensions/test_file.html")));
  EXPECT_TRUE(catcher.GetNextResult());

  std::u16string title;
  GetCurrentTabTitle(&title);
  EXPECT_EQ(u"FIRSTMESSAGE", title);

  // Change messages.json file and reload extension.
  base::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("messages2.json"),
      extension_dir.GetPath().AppendASCII("_locales/en/messages.json"));
  ReloadExtension(extension->id());

  // Check that the i18n message is also changed.
  ASSERT_TRUE(NavigateToURL(
      embedded_test_server()->GetURL("/extensions/test_file.html")));
  EXPECT_TRUE(catcher.GetNextResult());

  GetCurrentTabTitle(&title);
  EXPECT_EQ(u"SECONDMESSAGE", title);
}

// detectLanguage has some custom hooks that handle the asynchronous response
// manually, so explicitly test that it stays working as expected with promises.
IN_PROC_BROWSER_TEST_F(ExtensionI18nTest, I18NDetectLanguage) {
  constexpr char kManifest[] = R"(
      {
        "name": "detect language",
        "version": "1.0",
        "background": {
          "service_worker": "worker.js"
        },
        "manifest_version": 3
      })";
  constexpr char kWorker[] = R"(
    const text = 'Αυτό το κείμενο είναι γραμμένο στα ελληνικά';
    const expected = [{ language: "el", percentage: 100}];

    chrome.test.runTests([
      function detectLanguage() {
        chrome.i18n.detectLanguage(text, (result) => {
          chrome.test.assertEq(expected, result.languages);
          chrome.test.succeed();
        });
      },

      async function detectLanguagePromise() {
        let result = await chrome.i18n.detectLanguage(text);
        chrome.test.assertEq(expected, result.languages);
        chrome.test.succeed();
      }
    ]);
  )";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ASSERT_TRUE(RunExtensionTest(dir.UnpackedPath(), {}, {}));
}

}  // namespace extensions
