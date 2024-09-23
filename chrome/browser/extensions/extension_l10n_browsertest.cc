// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using ExtensionL10nBrowserTest = ExtensionApiTest;

// Tests that extension CSS files are localized.
// See also ContentScriptApiTest.ContentScriptCSSLocalization, which
// tests the localization of content script CSS. We need both of these,
// because the localization happens at different times (content scripts
// are localized as they are loaded into shared memory).
IN_PROC_BROWSER_TEST_F(ExtensionL10nBrowserTest, CSSFilesAreLocalized) {
  constexpr char kManifest[] =
      R"({
           "name": "CSS Localization Test",
           "version": "1",
           "manifest_version": 3,
           "default_locale": "en"
         })";
  constexpr char kStyleCss[] =
      R"(p {
           /* We have two entries here so that, if the localized one is invalid,
              we fall back to the literal. This identifies whether the failure
              is in the localization or the CSS file failing to be applied. */
           color: "purple";
           color: __MSG_text_color__;
         })";
  constexpr char kPageHtml[] =
      R"(<!doctype html>
         <html>
           <head>
             <link href="style.css" rel="stylesheet" type="text/css">
           </head>
           <body>
             <p id="paragraph">Hello world!</p>
           </body>
           <script src="test.js"></script>
         </html>)";
  constexpr char kTestJs[] =
      R"(chrome.test.runTests([
           function checkColor() {
             const p = document.getElementById('paragraph');
             chrome.test.assertTrue(!!p);
             const color = getComputedStyle(p).color;
             const expectedColor = 'rgb(0, 128, 0)';  // "green"
             chrome.test.assertEq(expectedColor, color);
             chrome.test.succeed();
           }
         ]);)";
  constexpr char kMessages[] =
      R"({
           "text_color": { "message": "green" }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("style.css"), kStyleCss);
  test_dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJs);
  {
    // TODO(crbug.com/40151844): It's a bit clunky to write to nested
    // files in a TestExtensionDir.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath locales = test_dir.UnpackedPath().AppendASCII("_locales");
    base::FilePath locales_en = locales.AppendASCII("en");
    base::FilePath messages_path = locales_en.AppendASCII("messages.json");
    ASSERT_TRUE(base::CreateDirectory(locales));
    ASSERT_TRUE(base::CreateDirectory(locales_en));
    ASSERT_TRUE(base::WriteFile(messages_path, kMessages));
  }

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(),
                               {.extension_url = "page.html"}, {}))
      << message_;
}

}  // namespace extensions
