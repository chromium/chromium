// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/parallel_download_configs.h"
#include "components/download/public/common/simple_download_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "url/gurl.h"

namespace {

// Bytes the test SW serves from event.respondWith() for any request the SW
// intercepts.
constexpr std::string_view kSwResponseBody = "from-sw";
// Bytes the embedded test server serves directly when no SW intercepts.
constexpr std::string_view kNetworkResponseBody = "from-network";

constexpr char kPagePath[] = "/sw_download_page.html";
constexpr char kWorkerPath[] = "/sw_download_worker.js";
constexpr char kSwTargetPath[] = "/sw-target.txt";
constexpr char kSwImagePath[] = "/sw-image.png";
// SW observes the fetch event but does NOT call respondWith, exercising the
// "result == nullopt" fallback branch.
constexpr char kSwFallbackPath[] = "/sw-target-fallback.txt";
// SW returns a 404 response with a non-empty body. The user explicitly asked
// to save, so the body should still land on disk.
constexpr char kSwErrorPath[] = "/sw-target-error.txt";
// SW returns a ReadableStream that sends one chunk and then hangs, exposing
// signal.aborted on cancellation. Used by the cancel-mid-download test.
constexpr char kSwSlowPath[] = "/sw-target-slow.txt";
// SW returns a response with ETag and Accept-Ranges set so it would otherwise
// satisfy the parallel-download predicate. Used by the parallel-disabled test.
constexpr char kSwParallelEligiblePath[] = "/sw-target-parallel-eligible.txt";
// SW errors mid-stream on the first request and serves a complete body on
// subsequent requests. Used by the resume-restart test to drive the
// INTERRUPTED → restart-on-resume path against a SW-fetched download.
constexpr char kSwResumePath[] = "/sw-target-resume.txt";

constexpr char kPageHtml[] = R"HTML(
<!doctype html>
<title>SW download test</title>
<a id="link" href="/sw-target.txt" download="saved.txt">link</a>
<img id="image" src="/sw-image.png">
<script>
  // The SW uses skipWaiting() + clients.claim(); once `ready` resolves the
  // controller is either set or about to be set on the next controllerchange.
  window.swReady = (async () => {
    await navigator.serviceWorker.register('/sw_download_worker.js',
                                            {scope: '/'});
    await navigator.serviceWorker.ready;
    if (!navigator.serviceWorker.controller) {
      await new Promise(r => navigator.serviceWorker.addEventListener(
          'controllerchange', r, {once: true}));
    }
    return true;
  })();
</script>
)HTML";

// The SW uses skipWaiting() + clients.claim() so the page that registered it
// becomes controlled without needing a reload.
constexpr char kWorkerJs[] = R"JS(
self.addEventListener('install', (e) => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));
self.addEventListener('fetch', (e) => {
  const url = new URL(e.request.url);
  if (url.pathname === '/sw-target.txt') {
    e.respondWith(new Response('from-sw',
        { headers: {'content-type': 'text/plain'} }));
    return;
  }
  if (url.pathname === '/sw-image.png') {
    e.respondWith(new Response('from-sw',
        { headers: {'content-type': 'image/png'} }));
    return;
  }
  if (url.pathname === '/sw-target-fallback.txt') {
    // Observe the event but decline to respond. The download path must
    // fall back to the network factory.
    return;
  }
  if (url.pathname === '/sw-target-error.txt') {
    e.respondWith(new Response('oops', {
      status: 404,
      headers: {'content-type': 'text/plain'},
    }));
    return;
  }
  if (url.pathname === '/sw-target-parallel-eligible.txt') {
    e.respondWith(new Response('from-sw', {
      headers: {
        'content-type': 'text/plain',
        'etag': '"sw-eligible"',
        'accept-ranges': 'bytes',
      },
    }));
    return;
  }
  if (url.pathname === '/sw-target-resume.txt') {
    // Module-scoped attempt counter; the SW worker stays alive across the
    // INTERRUPTED → resume sequence, so this advances on each fetch event.
    self.__resumeAttempt = (self.__resumeAttempt || 0) + 1;
    const ch = new BroadcastChannel('sw-resume-channel');
    ch.postMessage({type: 'fetch', attempt: self.__resumeAttempt});
    if (self.__resumeAttempt === 1) {
      // First fetch: enqueue some bytes then error the stream so the
      // download pipeline transitions to INTERRUPTED.
      e.respondWith((async () => {
        const stream = new ReadableStream({
          start(controller) {
            controller.enqueue(new Uint8Array([1, 2, 3]));
            controller.error(new Error('simulated mid-stream failure'));
          }
        });
        return new Response(stream, {
          headers: {'content-type': 'text/plain'},
        });
      })());
      return;
    }
    // Second+ fetch: serve the complete body. After our fix, restart-on-
    // resume of an SW-fetched download must invoke the SW again from
    // offset 0, and this body should land on disk in full (not appended
    // to the partial bytes from attempt #1).
    e.respondWith(new Response('from-sw-resume', {
      headers: {'content-type': 'text/plain'},
    }));
    return;
  }
  if (url.pathname === '/sw-target-slow.txt') {
    const ch = new BroadcastChannel('sw-cancel-channel');
    e.respondWith((async () => {
      const stream = new ReadableStream({
        start(controller) {
          // Notify when the consumer aborts.
          e.request.signal.addEventListener('abort', () => {
            ch.postMessage({type: 'aborted'});
            try { controller.close(); } catch (_) {}
          });
          controller.enqueue(new Uint8Array([1, 2, 3]));
          // Don't enqueue more; the stream remains open until aborted.
        }
      });
      return new Response(stream, {
        headers: {'content-type': 'application/octet-stream'},
      });
    })());
    return;
  }
});
)JS";

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto reply = [](std::string_view body, std::string_view content_type) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(std::string(body));
    response->set_content_type(std::string(content_type));
    return response;
  };
  if (request.relative_url == kPagePath) {
    return reply(kPageHtml, "text/html");
  }
  if (request.relative_url == kWorkerPath) {
    return reply(kWorkerJs, "application/javascript");
  }
  if (request.relative_url == kSwTargetPath) {
    return reply(kNetworkResponseBody, "text/plain");
  }
  if (request.relative_url == kSwImagePath) {
    return reply(kNetworkResponseBody, "image/png");
  }
  if (request.relative_url == kSwFallbackPath) {
    // Network response used by the SW-declines-to-respond fallback test.
    return reply(kNetworkResponseBody, "text/plain");
  }
  if (request.relative_url == kSwErrorPath) {
    // Network counterpart for kSwErrorPath; only reached if the SW path is
    // somehow missed (used for assertion clarity, never expected in tests).
    return reply("from-network-error", "text/plain");
  }
  if (request.relative_url == kSwParallelEligiblePath) {
    // Network counterpart; never expected to be reached by tests since the SW
    // always intercepts.
    return reply("from-network", "text/plain");
  }
  if (request.relative_url == kSwSlowPath) {
    // Same — never expected to be reached by tests; the SW always intercepts.
    return reply("from-network-slow", "application/octet-stream");
  }
  if (request.relative_url == kSwResumePath) {
    // Same — never expected to be reached; the SW always intercepts.
    return reply("from-network-resume", "text/plain");
  }
  return nullptr;
}

class ServiceWorkerDownloadBrowserTestBase : public DownloadTestBase {
 public:
  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();
    ASSERT_TRUE(InitialSetup());
    // Auto-handle the SaveAs file chooser (Save Link/Image As sets prompt=true
    // unconditionally), so tests don't block on a UI dialog.
    EnableFileChooser(true);
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  // Navigates to /sw_download_page.html and waits until the SW controls it.
  content::WebContents* NavigateAndWaitForSWControl() {
    GURL page_url = embedded_test_server()->GetURL(kPagePath);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(true, content::EvalJs(web_contents, "window.swReady"));
    return web_contents;
  }

  // Initiates a download via DownloadManager::DownloadUrl, exercising the
  // browser-initiated download path that hosts the SW interceptor.
  void StartDirectDownload(const GURL& url) {
    auto params = std::make_unique<download::DownloadUrlParameters>(
        url, TRAFFIC_ANNOTATION_FOR_TESTS);
    DownloadManagerForBrowser(browser())->DownloadUrl(std::move(params));
  }

  // Waits for one download to terminate and returns its target file path.
  base::FilePath WaitForOneCompletedDownload() {
    std::unique_ptr<content::DownloadTestObserver> observer(
        CreateWaiter(browser(), /*num_downloads=*/1));
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(
                      download::DownloadItem::COMPLETE));

    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
    GetDownloads(browser(), &items);
    EXPECT_EQ(1u, items.size());
    return items.empty() ? base::FilePath() : items[0]->GetTargetFilePath();
  }

  std::string ReadFile(const base::FilePath& path) const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(path, &contents));
    return contents;
  }
};

// Feature ON.
class ServiceWorkerDownloadBrowserTest
    : public ServiceWorkerDownloadBrowserTestBase {
 public:
  ServiceWorkerDownloadBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kServiceWorkerInterceptDownloads);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Direct DownloadManager::DownloadUrl on a SW-controlled URL must produce
// the SW's response bytes on disk.
IN_PROC_BROWSER_TEST_F(ServiceWorkerDownloadBrowserTest,
                       DirectDownloadServedByServiceWorker) {
  NavigateAndWaitForSWControl();
  StartDirectDownload(embedded_test_server()->GetURL(kSwTargetPath));

  base::FilePath saved = WaitForOneCompletedDownload();
  ASSERT_FALSE(saved.empty());
  EXPECT_EQ(std::string(kSwResponseBody), ReadFile(saved));
}

// Context-menu Save Link As / Save Image As -> SW intercepts -> SW's
// bytes on disk. The two entry points share the browser-initiated
// download path in RenderViewContextMenu; parameterize them so both
// variants run against the same assertions.
struct ContextMenuSaveAsCase {
  const char* test_name;
  const char* url_path;
  blink::mojom::ContextMenuDataMediaType media_type;
  int command_id;
};

class ServiceWorkerDownloadContextMenuTest
    : public ServiceWorkerDownloadBrowserTest,
      public ::testing::WithParamInterface<ContextMenuSaveAsCase> {};

IN_PROC_BROWSER_TEST_P(ServiceWorkerDownloadContextMenuTest,
                       ServedByServiceWorker) {
  const ContextMenuSaveAsCase& cs = GetParam();
  content::WebContents* web_contents = NavigateAndWaitForSWControl();

  GURL url = embedded_test_server()->GetURL(cs.url_path);
  content::ContextMenuParams params;
  params.media_type = cs.media_type;
  // Both paths consult unfiltered_link_url; the command-specific URL field
  // (link_url for SaveLinkAs, src_url for SaveImageAs) drives the actual
  // download target.
  params.unfiltered_link_url = url;
  if (cs.media_type == blink::mojom::ContextMenuDataMediaType::kImage) {
    params.src_url = url;
  } else {
    params.link_url = url;
  }
  params.page_url = web_contents->GetLastCommittedURL();
  params.frame_url = web_contents->GetLastCommittedURL();
  params.source_type = ui::mojom::MenuSourceType::kNone;

  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  menu.Init();
  ASSERT_TRUE(menu.IsItemPresent(cs.command_id));
  menu.ExecuteCommand(cs.command_id, /*event_flags=*/0);

  base::FilePath saved = WaitForOneCompletedDownload();
  ASSERT_FALSE(saved.empty());
  EXPECT_EQ(std::string(kSwResponseBody), ReadFile(saved));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerDownloadContextMenuTest,
    ::testing::Values(
        ContextMenuSaveAsCase{
            "SaveLinkAs",
            kSwTargetPath,
            blink::mojom::ContextMenuDataMediaType::kNone,
            IDC_CONTENT_CONTEXT_SAVELINKAS,
        },
        ContextMenuSaveAsCase{
            "SaveImageAs",
            kSwImagePath,
            blink::mojom::ContextMenuDataMediaType::kImage,
            IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
        }),
    [](const ::testing::TestParamInfo<ContextMenuSaveAsCase>& info) {
      return info.param.test_name;
    });

// SW observes the FetchEvent but does NOT call respondWith; the
// download must fall back to the network factory and the file on disk
// must contain the network's bytes.
//
// TODO(crbug.com/40410035): The current implementation passes a no-op
// FallbackCallback to ServiceWorkerMainResourceLoaderInterceptor::
// MaybeCreateLoader (returning nullptr), so when the SW declines to respond
// the download hangs instead of proceeding to the network. Re-enable this
// test once the FallbackCallback returns a valid network URLLoaderFactory.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerDownloadBrowserTest,
    DISABLED_DirectDownloadFallsBackToNetworkWhenSWDeclines) {
  NavigateAndWaitForSWControl();
  StartDirectDownload(embedded_test_server()->GetURL(kSwFallbackPath));

  base::FilePath saved = WaitForOneCompletedDownload();
  ASSERT_FALSE(saved.empty());
  EXPECT_EQ(std::string(kNetworkResponseBody), ReadFile(saved));
}

// SW returns a non-2xx response; the download must transition to the
// INTERRUPTED terminal state (matching Chromium's existing HTTP-error
// handling), not crash, and not silently disappear. This pins behavior
// that's easy to regress when the SW path is changed.
IN_PROC_BROWSER_TEST_F(ServiceWorkerDownloadBrowserTest,
                       Non2xxResponseFromSWInterrupts) {
  NavigateAndWaitForSWControl();

  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser(), /*num_downloads=*/1));
  StartDirectDownload(embedded_test_server()->GetURL(kSwErrorPath));
  observer->WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  GetDownloads(browser(), &items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(download::DownloadItem::INTERRUPTED, items[0]->GetState());
}

// Cancelling a download whose response is being streamed by the SW must
// transition the DownloadItem to CANCELLED without crashing. This pins
// lifetime correctness across the SW-served stream + downstream cancel.
//
// TODO(crbug.com/40410035): Once the SW loader propagates URLLoader
// cancellation upstream to the FetchEvent's request.signal, add an assertion
// here that the SW receives an 'abort' event (observable via BroadcastChannel).
// Today that plumbing is missing and such an assertion would hang.
IN_PROC_BROWSER_TEST_F(ServiceWorkerDownloadBrowserTest,
                       CancelDuringSWStreamSucceeds) {
  NavigateAndWaitForSWControl();

  // Wait for the download to enter IN_PROGRESS state before cancelling, so
  // the SW has actually started streaming.
  std::unique_ptr<content::DownloadTestObserver> in_progress(
      CreateInProgressWaiter(browser(), 1));
  StartDirectDownload(embedded_test_server()->GetURL(kSwSlowPath));
  in_progress->WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  GetDownloads(browser(), &items);
  ASSERT_EQ(1u, items.size());
  download::DownloadItem* item = items[0];
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, item->GetState());

  item->Cancel(/*user_cancel=*/true);
  EXPECT_EQ(download::DownloadItem::CANCELLED, item->GetState());
}

// SW interception works in an incognito (OTR) profile too, exercising the
// OTR storage partition's interceptor wiring.
IN_PROC_BROWSER_TEST_F(ServiceWorkerDownloadBrowserTest,
                       IncognitoDirectDownloadServedByServiceWorker) {
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito);

  // Mirror the per-browser download prefs setup the base fixture does for
  // the regular browser, so the incognito download lands in the temp dir.
  DownloadPrefs* incognito_prefs =
      DownloadPrefs::FromBrowserContext(incognito->profile());
  incognito_prefs->SetDownloadPath(GetDownloadDirectory(browser()));
  incognito_prefs->SetSaveFilePath(GetDownloadDirectory(browser()));
  incognito->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);

  GURL page_url = embedded_test_server()->GetURL(kPagePath);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, page_url));
  content::WebContents* web_contents =
      incognito->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(web_contents, "window.swReady"));

  auto params = std::make_unique<download::DownloadUrlParameters>(
      embedded_test_server()->GetURL(kSwTargetPath),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  DownloadManagerForBrowser(incognito)->DownloadUrl(std::move(params));

  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(incognito, /*num_downloads=*/1));
  observer->WaitForFinished();
  ASSERT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  GetDownloads(incognito, &items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(std::string(kSwResponseBody),
            ReadFile(items[0]->GetTargetFilePath()));
}

// Both SW interception and parallel downloading enabled, with the parallel
// min-slice size lowered so even small responses qualify on size.
class ServiceWorkerDownloadParallelDisabledBrowserTest
    : public ServiceWorkerDownloadBrowserTestBase {
 public:
  ServiceWorkerDownloadParallelDisabledBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kServiceWorkerInterceptDownloads, {}},
         {download::features::kParallelDownloading,
          {{download::kMinSliceSizeFinchKey, "1"},
           {download::kParallelRequestCountFinchKey, "3"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A Service Worker fetch handler runs once and its single-use loader cannot
// be re-driven for parallel range slices. Even when the SW response carries
// ETag + Accept-Ranges and the parallel-download feature is enabled, the
// resulting download must NOT be parallelized.
IN_PROC_BROWSER_TEST_F(ServiceWorkerDownloadParallelDisabledBrowserTest,
                       SwInterceptedDownloadIsNotParallelized) {
  NavigateAndWaitForSWControl();
  StartDirectDownload(embedded_test_server()->GetURL(kSwParallelEligiblePath));

  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser(), /*num_downloads=*/1));
  observer->WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  GetDownloads(browser(), &items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(download::DownloadItem::COMPLETE, items[0]->GetState());
  EXPECT_FALSE(items[0]->IsParallelDownload());
  EXPECT_EQ(std::string(kSwResponseBody),
            ReadFile(items[0]->GetTargetFilePath()));
}

// Feature OFF.
class ServiceWorkerDownloadFeatureFlagOffBrowserTest
    : public ServiceWorkerDownloadBrowserTestBase {
 public:
  ServiceWorkerDownloadFeatureFlagOffBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kServiceWorkerInterceptDownloads);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// With the feature flag off, the SW must NOT intercept the download —
// the file on disk must contain the network's response bytes.
IN_PROC_BROWSER_TEST_F(ServiceWorkerDownloadFeatureFlagOffBrowserTest,
                       DirectDownloadGoesToNetworkWhenFlagOff) {
  NavigateAndWaitForSWControl();
  StartDirectDownload(embedded_test_server()->GetURL(kSwTargetPath));

  base::FilePath saved = WaitForOneCompletedDownload();
  ASSERT_FALSE(saved.empty());
  EXPECT_EQ(std::string(kNetworkResponseBody), ReadFile(saved));
}

// A SW-fetched download that errors mid-stream must restart on resume
// from offset 0 and re-dispatch the SW fetch event (because SW responses
// can't be range-served). End-to-end the worker errors attempt #1 after
// 3 bytes, the download interrupts, resumption restarts and re-runs the
// SW handler which on attempt #2 returns the full body. Final on-disk
// bytes must equal the second-attempt body; never the first attempt's
// partial bytes prepended to the second response.
//
// TODO(crbug.com/40410035): An SW-served download whose body stream errors
// terminates as CANCELLED (terminal, non-resumable) instead of INTERRUPTED.
// The premise is wrong for SW-served downloads: the abort is a transient
// stream failure, not a user action. One option would be to make the
// HandleRequestCompletionStatus mapping to distinguish SW-originated
// aborts and sending ERR_FAILED instead of ERR_ABORT, so that
// StreamWaiter::OnAborted advances the download to INTERRUPTED with
// reason NETWORK_FAILED; the user-resume call below then drives the
// restart-on-resume path under test here. The restart-on-resume
// invariant itself is covered by the
// DownloadItemTest.ResumeOfSWFetchedDownloadRestartsAndKeepsSW unit test.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerDownloadBrowserTest,
    DISABLED_ResumeOfSWFetchedDownloadRestartsViaServiceWorker) {
  NavigateAndWaitForSWControl();

  std::unique_ptr<content::DownloadTestObserver> interrupt_observer(
      new content::DownloadTestObserverInterrupted(
          DownloadManagerForBrowser(browser()), /*wait_count=*/1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  StartDirectDownload(embedded_test_server()->GetURL(kSwResumePath));
  interrupt_observer->WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  GetDownloads(browser(), &items);
  ASSERT_EQ(1u, items.size());
  download::DownloadItem* item = items[0];
  ASSERT_EQ(download::DownloadItem::INTERRUPTED, item->GetState());

  // NETWORK_FAILED at offset 0 without strong validators resolves to
  // ResumeMode::USER_RESTART, so auto-resume bails. Drive the resume.
  std::unique_ptr<content::DownloadTestObserver> complete_observer(
      CreateWaiter(browser(), /*num_downloads=*/1));
  item->Resume(/*user_resume=*/true);
  complete_observer->WaitForFinished();

  EXPECT_EQ(download::DownloadItem::COMPLETE, item->GetState());
  EXPECT_EQ("from-sw-resume", ReadFile(item->GetTargetFilePath()));
}

}  // namespace
