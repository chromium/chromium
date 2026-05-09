// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace extensions {
namespace {

constexpr char kFallbackSubDir[] = "fallback";
// Served from `chrome/test/data/pdf/test.pdf` via the second source
// directory mounted in `SetUpOnMainThread()`.
constexpr char kFallbackPdfPath[] = "/test.pdf";
constexpr char kIframeHostPath[] = "/iframe_host.html";
constexpr char kTwoIframesSameUrlPath[] = "/two_iframes_same_url.html";

// The built-in PDF extension's top-level document URL. Tests wait for a
// navigation to this URL to confirm that the built-in PDF viewer has
// taken over an embedder frame.
GURL PdfExtensionIndexUrl() {
  return Extension::GetResourceURL(
      Extension::GetBaseURLFromExtensionId(extension_misc::kPdfExtensionId),
      "index.html");
}

}  // namespace

class MimeHandlerFallbackBrowserTest : public base::test::WithFeatureOverride,
                                       public ExtensionApiTest {
 public:
  MimeHandlerFallbackBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiMimeHandler},
        /*disabled_features=*/{});
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("generic_mime_handler"));
    // Serve a real PDF body from the shared PDF test data so we don't
    // duplicate one under this directory.
    base::FilePath chrome_test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    embedded_test_server()->ServeFilesFromDirectory(
        chrome_test_data_dir.AppendASCII("pdf"));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Loads the test 3p extension. `handler.js` either auto-aborts (when
  // the embedder is the primary main frame) or asks the host page what
  // to do via postMessage (when embedded in an iframe). Each test's
  // host HTML scripts the desired action.
  void LoadThirdPartyHandler() {
    const Extension* ext =
        LoadExtension(test_data_dir_.AppendASCII("generic_mime_handler")
                          .AppendASCII(kFallbackSubDir));
    ASSERT_TRUE(ext);
    handler_extension_id_ = ext->id();
  }

  // URL prefix (chrome-extension://<id>/) for the loaded 3p handler.
  std::string handler_extension_url_prefix() {
    return Extension::GetBaseURLFromExtensionId(handler_extension_id_).spec();
  }

  // Builds a `TestNavigationObserver` waiting for the built-in PDF
  // extension's index document to commit. Watches both existing and
  // newly-created `WebContents`: under OOPIF the document commits in
  // the embedder's `WebContents`; under legacy MimeHandlerView it
  // commits in a guest `WebContents` created during the swap.
  std::unique_ptr<content::TestNavigationObserver> MakePdfExtensionObserver() {
    auto observer = std::make_unique<content::TestNavigationObserver>(
        PdfExtensionIndexUrl());
    observer->WatchExistingWebContents();
    observer->StartWatchingNewWebContents();
    return observer;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel channel_{version_info::Channel::UNKNOWN};
  std::string handler_extension_id_;
};

// Generic MIME handler for application/pdf auto-aborts. The frame-scoped
// re-navigation causes the built-in PDF viewer to take over the same URL.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackSwapsToPdfViewerForTopLevelEmbedder) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL pdf_url = embedded_test_server()->GetURL(kFallbackPdfPath);

  auto pdf_extension_observer = MakePdfExtensionObserver();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  pdf_extension_observer->WaitForNavigationFinished();

  EXPECT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents))
      << "Generic handler aborted but built-in PDF viewer never loaded.";
}

// Abort from an iframe-hosted generic handler must swap only the iframe
// to the built-in PDF viewer; the outer main frame's document must not
// be re-navigated. The fallback re-navigation is scoped to the embedder
// iframe's `FrameTreeNodeId`, so siblings and ancestors stay put.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackLeavesMainFrameIntactForIframeEmbedder) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL host_url = embedded_test_server()->GetURL(kIframeHostPath);

  auto pdf_extension_observer = MakePdfExtensionObserver();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), host_url));

  // Stamp a witness value into the main frame's JS world before the
  // iframe's abort+swap completes. If the main frame is ever re-navigated
  // by the abort path, the global is destroyed and the post-swap read
  // sees `undefined`.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(content::ExecJs(
      main_frame, "window.mainFrameSentinel = 'set_before_iframe_swap';"));

  pdf_extension_observer->WaitForNavigationFinished();

  EXPECT_EQ(host_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ("set_before_iframe_swap",
            content::EvalJs(web_contents->GetPrimaryMainFrame(),
                            "window.mainFrameSentinel")
                .ExtractString());
  EXPECT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents))
      << "Iframe-hosted abort did not swap into built-in PDF viewer.";
}

// Two sibling iframes loading the exact same URL: the host page tells
// only the first handler to abort, the second to stay on the third-party
// handler. With `FrameTreeNodeId`-keyed fallback state, only the aborting
// iframe swaps to the built-in PDF viewer; the other keeps the 3p
// handler. URL-keyed state would alias the two embedders and produce a
// different outcome.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackDistinguishesConcurrentIframesSameUrl) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL host_url = embedded_test_server()->GetURL(kTwoIframesSameUrlPath);

  auto pdf_extension_observer = MakePdfExtensionObserver();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), host_url));
  pdf_extension_observer->WaitForNavigationFinished();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // The host page deterministically tells iframe A's handler to abort
  // and iframe B's handler to stay. Verify each iframe's role directly.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* iframe_a = content::ChildFrameAt(main_frame, 0);
  content::RenderFrameHost* iframe_b = content::ChildFrameAt(main_frame, 1);
  ASSERT_TRUE(iframe_a);
  ASSERT_TRUE(iframe_b);
  ASSERT_FALSE(content::ChildFrameAt(main_frame, 2))
      << "Expected exactly two iframes in the host page.";

  auto subtree_contains_url_prefix = [](content::RenderFrameHost* root,
                                        std::string_view prefix) {
    bool found = false;
    root->ForEachRenderFrameHost(
        [&found, prefix](content::RenderFrameHost* rfh) {
          if (rfh->IsActive() &&
              rfh->GetLastCommittedURL().spec().starts_with(prefix)) {
            found = true;
          }
        });
    return found;
  };

  const std::string pdf_prefix =
      Extension::GetBaseURLFromExtensionId(extension_misc::kPdfExtensionId)
          .spec();
  const std::string handler_prefix = handler_extension_url_prefix();

  // Iframe A aborted: contains the built-in PDF extension.
  EXPECT_TRUE(subtree_contains_url_prefix(iframe_a, pdf_prefix));

  // Iframe B stayed: still hosts the 3p handler.
  EXPECT_TRUE(subtree_contains_url_prefix(iframe_b, handler_prefix));

  EXPECT_EQ(host_url, web_contents->GetLastCommittedURL());
}

// Calling abortAndFallbackToNativeHandler from the built-in PDF
// extension must reject with an error and not navigate.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       BuiltInExtensionRejectsAbortAndFallback) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL pdf_url = embedded_test_server()->GetURL(kFallbackPdfPath);

  // With no generic MIME handler loaded, the built-in PDF extension
  // handles the navigation. Wait for its OOPIF to commit before reaching
  // into it with EvalJs.
  auto pdf_extension_observer = MakePdfExtensionObserver();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  pdf_extension_observer->WaitForNavigationFinished();

  content::RenderFrameHost* pdf_ext_frame =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);
  ASSERT_TRUE(pdf_ext_frame);

  static constexpr char kScript[] = R"(
    new Promise((resolve) => {
      chrome.mimeHandler.abortAndFallbackToNativeHandler(() => {
        if (chrome.runtime.lastError) {
          resolve('err:' + chrome.runtime.lastError.message);
        } else {
          resolve('unexpected_success');
        }
      });
    });
  )";
  EXPECT_THAT(content::EvalJs(pdf_ext_frame, kScript).ExtractString(),
              ::testing::HasSubstr(
                  "not available for built-in MIME handler extensions"));

  // Rejection happens before the stream manager is touched, so the
  // primary main frame URL must not change.
  EXPECT_EQ(pdf_url, web_contents->GetLastCommittedURL());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(MimeHandlerFallbackBrowserTest);

}  // namespace extensions
