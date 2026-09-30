// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/mime_handler/mime_handler_body_cache.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/http_request.h"
#include "pdf/pdf_features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace extensions {
namespace {

constexpr char kFallbackSubDir[] = "fallback";
// Served from `chrome/test/data/pdf/test.pdf` via the second source
// directory mounted in `SetUpOnMainThread()`.
constexpr char kFallbackPdfPath[] = "/test.pdf";
constexpr char kEmbedHostPath[] = "/embed_host.html";
constexpr char kIframeHostPath[] = "/iframe_host.html";
constexpr char kTwoIframesSameUrlPath[] = "/two_iframes_same_url.html";
constexpr char kServiceWorkerFetchHeader[] = "X-Fallback-Service-Worker";

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
    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&MimeHandlerFallbackBrowserTest::CountPdfRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Number of times the PDF document itself was fetched over the network.
  int pdf_request_count() {
    base::AutoLock lock(pdf_request_count_lock_);
    return pdf_request_count_;
  }

  std::vector<std::pair<std::string, std::string>> pdf_requests() {
    base::AutoLock lock(pdf_request_count_lock_);
    return pdf_requests_;
  }

  int pdf_request_count_with_worker_header() {
    base::AutoLock lock(pdf_request_count_lock_);
    return pdf_request_count_with_worker_header_;
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

  const std::string& handler_extension_id() const {
    return handler_extension_id_;
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
  // Runs on the test server's IO thread.
  void CountPdfRequest(const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() == kFallbackPdfPath) {
      base::AutoLock lock(pdf_request_count_lock_);
      ++pdf_request_count_;
      pdf_requests_.emplace_back(request.method_string, request.content);
      if (request.headers.contains(kServiceWorkerFetchHeader)) {
        ++pdf_request_count_with_worker_header_;
      }
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel channel_{version_info::Channel::UNKNOWN};
  std::string handler_extension_id_;
  base::Lock pdf_request_count_lock_;
  int pdf_request_count_ GUARDED_BY(pdf_request_count_lock_) = 0;
  std::vector<std::pair<std::string, std::string>> pdf_requests_
      GUARDED_BY(pdf_request_count_lock_);
  int pdf_request_count_with_worker_header_
      GUARDED_BY(pdf_request_count_lock_) = 0;
};

// Generic MIME handler for application/pdf auto-aborts. The frame-scoped
// re-navigation causes the built-in PDF viewer to take over the same URL,
// and the hand-off must not fetch the document again: one request for the
// initial load and none for the fallback. The legacy GuestView path keeps
// no body cache, so it still re-fetches.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackSwapsToPdfViewerForTopLevelEmbedder) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  const GURL pdf_url = embedded_test_server()->GetURL(kFallbackPdfPath);

  auto pdf_extension_observer = MakePdfExtensionObserver();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  pdf_extension_observer->WaitForNavigationFinished();

  EXPECT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents))
      << "Generic handler aborted but built-in PDF viewer never loaded.";

  constexpr int kExpectedServerRequestCountWithoutRefetch = 1;
  // If OOPIF PDF is disabled, network request is done twice - once when the
  // pdf file is loaded and second time when abort and fallback is triggered as
  // there is no fallback without network request cache path.
  EXPECT_EQ(IsParamFeatureEnabled()
                ? kExpectedServerRequestCountWithoutRefetch
                : kExpectedServerRequestCountWithoutRefetch + 1,
            pdf_request_count());
  EXPECT_EQ(pdf_url, web_contents->GetLastCommittedURL());
}

// `MimeHandlerStreamManager` calls `LoadURLWithParams()` with
// `BYPASSING_CACHE`, so the MIME handler URL interceptor serves the cached
// body and the fallback makes no network round trip.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackBypassesServiceWorker) {
  if (!IsParamFeatureEnabled()) {
    GTEST_SKIP() << "Cached-body fallback routing is OOPIF-only.";
  }
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  ASSERT_EQ("ready", content::EvalJs(web_contents, R"(
      navigator.serviceWorker.register('/fallback_sw.js')
          .then(() => navigator.serviceWorker.ready)
          .then(() => 'ready'))"));

  const GURL pdf_url = embedded_test_server()->GetURL(kFallbackPdfPath);
  auto pdf_extension_observer = MakePdfExtensionObserver();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  pdf_extension_observer->WaitForNavigationFinished();
  ASSERT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents));

  // The service worker should only contact the server one time when initial
  // load of the pdf resource was done. When abort and fallback is called,
  // it's not expected to have any server requests or service worker
  // interceptions.
  constexpr int kExpectedServerRequestCount = 1;
  EXPECT_EQ(kExpectedServerRequestCount, pdf_request_count());
  EXPECT_EQ(kExpectedServerRequestCount,
            pdf_request_count_with_worker_header());
}

// Same as the top-level embedder case, but the PDF URL carries a fragment.
// The fallback re-navigates the embedder to its own current URL, so without
// a reload classification it is misclassified as a same-document scroll: the
// response throttle never re-runs and the built-in PDF viewer never takes
// over. The user-visible URL, fragment included, must survive the fallback.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackSwapsToPdfViewerForFragmentedUrl) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  const GURL pdf_url = embedded_test_server()->GetURL(
      base::StrCat({kFallbackPdfPath, "#page=2"}));
  ASSERT_TRUE(pdf_url.has_ref());

  auto pdf_extension_observer = MakePdfExtensionObserver();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  pdf_extension_observer->WaitForNavigationFinished();

  EXPECT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents))
      << "Generic handler aborted on a fragmented URL but the built-in PDF "
         "viewer never loaded.";
  // The fragmented URL must survive the fallback unchanged.
  EXPECT_EQ(pdf_url, web_contents->GetLastCommittedURL());
}

// Abort from an iframe-hosted generic handler must swap only the iframe
// to the built-in PDF viewer; the outer main frame's document must not
// be re-navigated. The fallback re-navigation is scoped to the embedder
// iframe's `FrameTreeNodeId`, so siblings and ancestors stay put.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackLeavesMainFrameIntactForIframeEmbedder) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
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

// Abort from a generic handler hosted by an <embed> element must hand the
// cached response body to the built-in PDF viewer as a completed load, not just
// load the viewer shell. The cached-body routing exists only in the OOPIF PDF
// stream pipeline; the legacy MimeHandlerView GuestView path drops the cache
// and re-fetches, so there is nothing to verify there.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       AbortAndFallbackRendersPdfViewerForEmbedElement) {
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    GTEST_SKIP() << "Cached-body fallback routing is OOPIF-only.";
  }
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  const GURL host_url = embedded_test_server()->GetURL(kEmbedHostPath);

  auto pdf_extension_observer = MakePdfExtensionObserver();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), host_url));
  pdf_extension_observer->WaitForNavigationFinished();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  content::RenderFrameHost* embedder_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(embedder_frame);

  pdf_extension_test_util::EnsurePDFHasLoadedOptions options;
  options.wait_for_hit_test_data = false;
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoadedWithOptions(
      embedder_frame, options));
  EXPECT_EQ(host_url, web_contents->GetLastCommittedURL());
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
      browser()->GetTabStripModel()->GetActiveWebContents();
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
// TODO(crbug.com/551592354): Flaky on Linux MSAN.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_BuiltInExtensionRejectsAbortAndFallback \
  DISABLED_BuiltInExtensionRejectsAbortAndFallback
#else
#define MAYBE_BuiltInExtensionRejectsAbortAndFallback \
  BuiltInExtensionRejectsAbortAndFallback
#endif
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       MAYBE_BuiltInExtensionRejectsAbortAndFallback) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
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

class MimeHandlerFallbackRedirectBrowserTest
    : public MimeHandlerFallbackBrowserTest {
 public:
  MimeHandlerFallbackRedirectBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    const base::FilePath chrome_test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    const base::FilePath pdf_path =
        chrome_test_data_dir.AppendASCII("pdf").AppendASCII("test.pdf");
    ASSERT_TRUE(base::PathExists(pdf_path));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(pdf_path, &pdf_data_));
    }

    // Register controllable responses before starting the server. Expect two
    // requests to "/spoof.pdf".
    controllable_response1_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/spoof.pdf");
    controllable_response2_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/spoof.pdf");

    MimeHandlerFallbackBrowserTest::SetUpOnMainThread();
  }

  const std::string& pdf_data() const { return pdf_data_; }

  net::test_server::ControllableHttpResponse* controllable_response1() {
    return controllable_response1_.get();
  }

  net::test_server::ControllableHttpResponse* controllable_response2() {
    return controllable_response2_.get();
  }

 private:
  std::string pdf_data_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_response1_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_response2_;
};

// A fallback with a cached body never reaches the network, so a redirect can
// only occur once the cache gives up. Cap it so the refetch happens, and check
// the redirected document commits.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackRedirectBrowserTest,
                       DegradedFallbackFollowsCrossOriginRedirect) {
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    GTEST_SKIP() << "Cached-body fallback routing is OOPIF-only.";
  }

  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* const web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  base::AutoReset<size_t> cap =
      MimeHandlerBodyCache::SetMaxCacheBytesForTesting(1);

  const GURL start_url = embedded_test_server()->GetURL("a.com", "/spoof.pdf");

  const auto pdf_extension_observer = MakePdfExtensionObserver();

  // Start navigation (non-blocking).
  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(start_url));

  // 1. Handle first request (attacker PDF).
  net::test_server::ControllableHttpResponse* response1 =
      controllable_response1();
  response1->WaitForRequest();
  response1->Send("HTTP/1.1 200 OK\r\n");
  response1->Send("Content-Type: application/pdf\r\n");
  response1->Send("\r\n");
  response1->Send(pdf_data());
  response1->Done();

  // 2. Handle second request (fallback reload redirected to b.com).
  net::test_server::ControllableHttpResponse* response2 =
      controllable_response2();
  response2->WaitForRequest();
  const GURL target_url =
      embedded_test_server()->GetURL("b.com", "/accessibility/multi-page.pdf");
  response2->Send("HTTP/1.1 302 Found\r\n");
  response2->Send("Location: " + target_url.spec() + "\r\n");
  response2->Send("\r\n");
  response2->Done();

  // Now wait for the navigation to settle.
  pdf_extension_observer->WaitForNavigationFinished();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  content::RenderFrameHost* const extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);
  ASSERT_TRUE(extension_host);

  // Verify that the PDF has loaded.
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  // Get the page count.
  const int page_count =
      content::EvalJs(extension_host, "viewer.docLength_").ExtractInt();

  // The page count says which document the viewer got: 1 for the pre-redirect
  // test.pdf, 2 for the redirect target.
  EXPECT_EQ(2, page_count);

  // Also verify the committed URL is the redirected one.
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
}

// A navigation that replaces the fallback while the fallback still waits for
// the body should load from the network. The fallback should not be pending
// once the replacement finishes.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackRedirectBrowserTest,
                       AbortAndFallbackReplacedBeforeStartKeepsNextLoad) {
  if (!IsParamFeatureEnabled()) {
    GTEST_SKIP() << "Cached-body fallback routing is OOPIF-only.";
  }
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  const GURL pdf_url = embedded_test_server()->GetURL("/spoof.pdf");
  const content::FrameTreeNodeId embedder_ftn =
      web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId();

  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(pdf_url));
  net::test_server::ControllableHttpResponse* initial_response =
      controllable_response1();
  initial_response->WaitForRequest();
  // The body is left unfinished, so the fallback cannot start yet.
  initial_response->Send("HTTP/1.1 200 OK\r\n");
  initial_response->Send("Content-Type: application/pdf\r\n");
  initial_response->Send("\r\n");
  initial_response->Send(pdf_data().substr(0, pdf_data().size() / 2));

  ASSERT_TRUE(base::test::RunUntil([&] {
    auto* manager =
        mime_handler::MimeHandlerStreamManager::FromWebContents(web_contents);
    return manager && manager->IsPendingNativeFallback(embedder_ftn, pdf_url);
  }));
  ASSERT_FALSE(web_contents->GetController().GetPendingEntry());

  content::TestNavigationObserver replacement_observer(pdf_url);
  replacement_observer.WatchExistingWebContents();
  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(pdf_url));
  net::test_server::ControllableHttpResponse* replacement_response =
      controllable_response2();
  replacement_response->WaitForRequest();
  replacement_response->Send("HTTP/1.1 200 OK\r\n");
  replacement_response->Send("Content-Type: application/pdf\r\n");
  replacement_response->Send("\r\n");
  replacement_response->Send(pdf_data());
  replacement_response->Done();
  replacement_observer.WaitForNavigationFinished();
  EXPECT_TRUE(replacement_observer.last_navigation_succeeded());

  auto* manager =
      mime_handler::MimeHandlerStreamManager::FromWebContents(web_contents);
  EXPECT_FALSE(manager &&
               manager->IsPendingNativeFallback(embedder_ftn, pdf_url));
}

// Without a replayable body cache the request really is sent again, and this
// path never reaches the form-resubmission prompt. So the degraded fallback
// must not repeat the POST.
IN_PROC_BROWSER_TEST_P(MimeHandlerFallbackBrowserTest,
                       DegradedFallbackDoesNotResubmitPost) {
  ASSERT_NO_FATAL_FAILURE(LoadThirdPartyHandler());
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  // A one-byte cap makes the cache give up, so no body is kept.
  base::AutoReset<size_t> cap =
      MimeHandlerBodyCache::SetMaxCacheBytesForTesting(1);
  const GURL pdf_url = embedded_test_server()->GetURL(kFallbackPdfPath);

  content::NavigationController::LoadURLParams params(pdf_url);
  params.load_type = content::NavigationController::LOAD_TYPE_HTTP_POST;
  params.post_data = network::ResourceRequestBody::CreateFromCopyOfBytes(
      base::as_byte_span(std::string_view("probe=1")));
  auto pdf_extension_observer = MakePdfExtensionObserver();
  web_contents->GetController().LoadURLWithParams(params);
  pdf_extension_observer->WaitForNavigationFinished();
  ASSERT_TRUE(pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents));

  EXPECT_THAT(pdf_requests(),
              testing::ElementsAre(testing::Pair("POST", "probe=1"),
                                   testing::Pair("GET", "")));
  EXPECT_FALSE(
      web_contents->GetController().GetLastCommittedEntry()->GetHasPostData());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(MimeHandlerFallbackBrowserTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(MimeHandlerFallbackRedirectBrowserTest);

}  // namespace extensions
