// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionSecureContextBrowserTest : public ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Tests that an extension subframe (iframe) embedded in an insecure HTTP
// top-level page is treated as a secure context, because extension schemes
// bypass Blink's ancestor-walk secure context check via
// secure_origin_allowlist::GetSchemesBypassingSecureContextCheck().
IN_PROC_BROWSER_TEST_F(ExtensionSecureContextBrowserTest,
                       IframeInInsecurePageIsSecureContext) {
  TestExtensionDir test_dir;
  static constexpr char kManifest[] =
      R"({
           "name": "Secure Context Test",
           "version": "0.1",
           "manifest_version": 3,
           "web_accessible_resources": [{
             "resources": ["page.html"],
             "matches": ["<all_urls>"]
           }]
         })";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
         <html>
         <head><title>Extension Page</title></head>
         <body>
           Extension Subframe
         </body>
         </html>)";

  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate the top-level window to an insecure (HTTP) origin.
  GURL insecure_page_url =
      embedded_test_server()->GetURL("example.com", "/empty.html");
  EXPECT_FALSE(network::IsUrlPotentiallyTrustworthy(insecure_page_url));

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, insecure_page_url));

  // The top-level HTTP page is NOT a secure context.
  // Note: We need EXPECT_EQ(false, ...) (instead of EXPECT_FALSE(...)) here
  // and below because it forces the conversion of the EvalJs() result.
  EXPECT_EQ(false, content::EvalJs(web_contents, "window.isSecureContext"));

  // Append an iframe pointing to the extension's web accessible resource.
  GURL extension_url = extension->GetResourceURL("page.html");
  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace(
          "const iframe = document.createElement('iframe'); iframe.src = $1; "
          "document.body.appendChild(iframe);",
          extension_url)));
  observer.Wait();

  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);
  EXPECT_EQ(extension_url, child_rfh->GetLastCommittedURL());

  // Because extension schemes bypass secure context checks via
  // secure_origin_allowlist, the extension subframe is treated as a secure
  // context despite having an insecure HTTP ancestor.
  EXPECT_EQ(true, content::EvalJs(child_rfh, "window.isSecureContext"));

  // Verify that a web API that is restricted to secure contexts (SubtleCrypto)
  // is available and functions within the extension iframe.
  constexpr char kTestSubtleCrypto[] =
      R"(
        (async () => {
          if (!window.crypto || !window.crypto.subtle) {
            return false;
          }
          const key = await window.crypto.subtle.generateKey(
              {name: 'AES-GCM', length: 256}, true, ['encrypt', 'decrypt']);
          return !!key;
        })()
      )";
  EXPECT_EQ(true, content::EvalJs(child_rfh, kTestSubtleCrypto));
}

}  // namespace extensions
