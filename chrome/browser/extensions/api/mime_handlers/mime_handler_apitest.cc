// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class MimeHandlerApiTest : public ExtensionApiTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kApiMimeHandler};
};

IN_PROC_BROWSER_TEST_F(MimeHandlerApiTest,
                       SetMimeHandlerOptionsWritesOnlyPresentMembers) {
  ASSERT_TRUE(RunExtensionTest("mime_handler_options")) << message_;
}

// Tests that an extension that declares `mime_types_handler`, but whose ID is
// not in `kMIMETypeHandlersAllowlist`
// (extensions/common/manifest_handlers/mime_types_handler.cc), gets the public
// `chrome.mimeHandler` API but not the private API:
// - `chrome.mimeHandlerPrivate` is not defined in its extension pages.
// - `Mojo` is not defined in its extension pages, since only APIs like
//   `mimeHandlerPrivate` turn on MojoJS for a context.
// Regression test for crbug.com/576943246.
IN_PROC_BROWSER_TEST_F(MimeHandlerApiTest,
                       MimeHandlerPrivateUnavailableToNonAllowlistedExtension) {
  // Load an extension without a `key`, so its ID is not allowlisted.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Non-allowlisted MIME handler",
    "version": "1.0",
    "manifest_version": 3,
    "mime_types_handler": {
      "application/pdf": {"handler_url": "handler.html"}
    }
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("handler.html"), "<html></html>");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Open the handler page in a tab, which is a privileged extension context.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            extension->ResolveExtensionURL("handler.html")));

  // The public API is available, so the manifest key was accepted.
  ASSERT_EQ(true, content::EvalJs(web_contents, "!!chrome.mimeHandler"));

  // Neither the private API nor the MojoJS bindings are exposed.
  EXPECT_EQ(false,
            content::EvalJs(web_contents, "!!chrome.mimeHandlerPrivate"));
  EXPECT_EQ("undefined", content::EvalJs(web_contents, "typeof Mojo"));
}

}  // namespace extensions
