// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/webapk_features.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapk {
namespace {

// Hosts covered by CERT_TEST_NAMES. `a.test` plays the role of the installed
// web app, `b.test` the role of the unrelated site being attacked.
constexpr char kAppHost[] = "a.test";
constexpr char kCrossSiteHost[] = "b.test";
constexpr char kTargetPath[] = "/echo";

constexpr char kFormEncodedHeader[] =
    "Content-Type: application/x-www-form-urlencoded\r\n";

scoped_refptr<network::ResourceRequestBody> MakeShareBody() {
  return network::ResourceRequestBody::CreateFromCopyOfBytes(
      base::as_byte_span(std::string_view("title=shared")));
}

}  // namespace

// Covers the SameSite treatment of Web Share Target POST navigations. A share
// target action is required to be within the app's scope, but the app may
// redirect it anywhere. Such a navigation must not be treated as if the user
// had typed the destination URL, otherwise the app can force the browser to
// send SameSite=Strict/Lax cookies on a cross-site POST of its choosing. See
// crbug.com/40061291.
class WebApkPostShareTargetNavigatorBrowserTest : public AndroidBrowserTest {
 public:
  WebApkPostShareTargetNavigatorBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&https_server_);
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &WebApkPostShareTargetNavigatorBrowserTest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Sets one cookie of each SameSite flavour on `url`.
  void SetSameSiteCookies(const GURL& url) {
    ASSERT_TRUE(content::SetCookie(chrome_test_utils::GetProfile(this), url,
                                   "strict=1; SameSite=Strict; Secure"));
    ASSERT_TRUE(content::SetCookie(chrome_test_utils::GetProfile(this), url,
                                   "lax=1; SameSite=Lax; Secure"));
    ASSERT_TRUE(content::SetCookie(chrome_test_utils::GetProfile(this), url,
                                   "none=1; SameSite=None; Secure"));
  }

  // The Cookie header the target endpoint received, or the empty string if it
  // received none.
  std::string target_cookie_header() {
    base::AutoLock lock(lock_);
    return target_cookie_header_;
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  // Runs on the embedded test server's thread.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, kTargetPath)) {
      return;
    }
    base::AutoLock lock(lock_);
    auto it = request.headers.find("Cookie");
    target_cookie_header_ =
        it == request.headers.end() ? std::string() : it->second;
  }

  base::Lock lock_;
  std::string target_cookie_header_ GUARDED_BY(lock_);
};

// The regression test for crbug.com/40061291: an in-scope action that
// 307-redirects to a cross-site URL must not leak SameSite=Strict/Lax cookies
// belonging to that site.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetNavigatorBrowserTest,
                       CrossSiteRedirectDropsSameSiteCookies) {
  const GURL target = https_server_.GetURL(kCrossSiteHost, kTargetPath);
  ASSERT_NO_FATAL_FAILURE(SetSameSiteCookies(target));

  const GURL action = https_server_.GetURL(
      kAppHost, base::StrCat({"/server-redirect-307?", target.spec()}));

  content::TestNavigationObserver observer(web_contents());
  NavigateShareTargetPost(MakeShareBody(), kFormEncodedHeader, action,
                          web_contents());
  observer.Wait();

  ASSERT_EQ(target, web_contents()->GetLastCommittedURL());

  const std::string cookies = target_cookie_header();
  EXPECT_THAT(cookies, testing::HasSubstr("none=1"));
  EXPECT_THAT(cookies, testing::Not(testing::HasSubstr("strict=1")));
  EXPECT_THAT(cookies, testing::Not(testing::HasSubstr("lax=1")));
}

// The sharing flow itself must keep working: a share target that does not
// redirect is same-site with its own origin and still receives its Strict
// cookies, exactly as if the app had submitted the form itself.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetNavigatorBrowserTest,
                       SameSiteShareTargetKeepsStrictCookies) {
  const GURL target = https_server_.GetURL(kAppHost, kTargetPath);
  ASSERT_NO_FATAL_FAILURE(SetSameSiteCookies(target));

  content::TestNavigationObserver observer(web_contents());
  NavigateShareTargetPost(MakeShareBody(), kFormEncodedHeader, target,
                          web_contents());
  observer.Wait();

  ASSERT_EQ(target, web_contents()->GetLastCommittedURL());

  const std::string cookies = target_cookie_header();
  EXPECT_THAT(cookies, testing::HasSubstr("strict=1"));
  EXPECT_THAT(cookies, testing::HasSubstr("lax=1"));
  EXPECT_THAT(cookies, testing::HasSubstr("none=1"));
}

namespace {

// Paths served by WebApkPostShareTargetWaitForServiceWorkerBrowserTest.
// `kStartPath` registers `kServiceWorkerPath`, whose default scope (`/app/`)
// covers `kShareTargetPath`. `kNoServiceWorkerStartPath` registers nothing.
constexpr char kStartPath[] = "/app/start.html";
constexpr char kNoServiceWorkerStartPath[] = "/app/no_sw.html";
constexpr char kServiceWorkerPath[] = "/app/sw.js";
constexpr char kShareTargetPath[] = "/app/share-target";
// Fails with an empty response, so that loading it commits an error page.
constexpr char kFailingStartPath[] = "/app/fail";
// Never responds, so that loading it stays in flight.
constexpr char kHungStartPath[] = "/app/hung";
// Registers a service worker whose scope (`/other/`) does not include
// `kShareTargetPath`.
constexpr char kOtherScopeStartPath[] = "/other/start.html";
constexpr char kOtherScopeServiceWorkerPath[] = "/other/sw.js";
// Redirects to `kNoServiceWorkerStartPath` on `kCrossSiteHost`, like a start
// URL that redirects to a sign-in page.
constexpr char kRedirectToCrossSitePath[] = "/app/redirect";

// Page titles that tell whether the share target POST was handled by the
// service worker or reached the network.
constexpr char16_t kInterceptedTitle[] = u"intercepted";
constexpr char16_t kNetworkTitle[] = u"network";

constexpr char kStartHtml[] =
    "<!doctype html><title>start</title>"
    "<script>navigator.serviceWorker.register('sw.js');</script>";
constexpr char kNoServiceWorkerStartHtml[] =
    "<!doctype html><title>start</title>";
constexpr char kServiceWorkerJs[] = R"(
self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  if (event.request.method === 'POST' && url.pathname === '/app/share-target') {
    event.respondWith(new Response('<!doctype html><title>intercepted</title>',
                                   {headers: {'Content-Type': 'text/html'}}));
  }
});
)";

}  // namespace

// Covers delivering a POST share target to a web app whose service worker has
// never been installed (e.g. a preinstalled TWA that has never been launched):
// the app's start URL must be loaded first so that the service worker handles
// the POST, instead of the POST going to the network and the share being lost.
class WebApkPostShareTargetWaitForServiceWorkerBrowserTest
    : public AndroidBrowserTest {
 public:
  // Uses a timeout long enough that tests only pass if the POST is sent, or
  // dropped, for some reason other than the timeout. Subclasses that test the
  // timeout, or the feature being disabled, override this.
  WebApkPostShareTargetWaitForServiceWorkerBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeatureWithParameters(
        kShareTargetWaitForServiceWorker, {{"timeout", "1h"}});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &WebApkPostShareTargetWaitForServiceWorkerBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  GURL AppUrl(std::string_view path) {
    return https_server_.GetURL(kAppHost, path);
  }

  GURL CrossSiteUrl(std::string_view path) {
    return https_server_.GetURL(kCrossSiteHost, path);
  }

  // Sends a share to `kShareTargetPath` and returns the title of the page the
  // share ended up on: `kInterceptedTitle` if the service worker handled it,
  // `kNetworkTitle` if it reached the network.
  std::u16string Share(const GURL& bootstrap_url) {
    content::TitleWatcher title_watcher(web_contents(), kInterceptedTitle);
    title_watcher.AlsoWaitForTitle(kNetworkTitle);
    StartShare(bootstrap_url);
    return title_watcher.WaitAndGetTitle();
  }

  // Sends a share to `kShareTargetPath` without waiting for it to be
  // delivered.
  void StartShare(const GURL& bootstrap_url) {
    NavigateShareTargetPostWhenServiceWorkerReady(
        MakeShareBody(), kFormEncodedHeader, AppUrl(kShareTargetPath),
        bootstrap_url, web_contents());
  }

  // Sends a share to `kShareTargetPath` and waits for the bootstrap load of
  // `bootstrap_url` to finish.
  void StartShareAndWaitForBootstrap(const GURL& bootstrap_url) {
    content::TestNavigationObserver observer(web_contents());
    StartShare(bootstrap_url);
    observer.Wait();
    ASSERT_EQ(bootstrap_url, web_contents()->GetLastCommittedURL());
  }

  // Registers the service worker by (browser-initiated) navigating to
  // `kStartPath`, and waits for it to be ready.
  void RegisterServiceWorker() {
    ASSERT_TRUE(content::NavigateToURL(web_contents(), AppUrl(kStartPath)));
    ASSERT_EQ(
        "ready",
        content::EvalJs(web_contents(),
                        "navigator.serviceWorker.ready.then(() => 'ready')"));
  }

  // Number of requests the server received for `path`.
  int RequestCount(std::string_view path) {
    base::AutoLock lock(lock_);
    auto it = request_counts_.find(std::string(path));
    return it == request_counts_.end() ? 0 : it->second;
  }

  // Sends a share whose bootstrap URL redirects to another origin, and waits
  // for the bootstrap load to finish there.
  void StartShareAndWaitForRedirectToCrossSite() {
    content::TestNavigationObserver observer(web_contents());
    StartShare(AppUrl(kRedirectToCrossSitePath));
    observer.Wait();
    ASSERT_EQ(CrossSiteUrl(kNoServiceWorkerStartPath),
              web_contents()->GetLastCommittedURL());
  }

  bool HistoryContains(const GURL& url) {
    content::NavigationController& controller = web_contents()->GetController();
    for (int i = 0; i < controller.GetEntryCount(); ++i) {
      if (controller.GetEntryAtIndex(i)->GetURL() == url) {
        return true;
      }
    }
    return false;
  }

 private:
  // Runs on the embedded test server's thread.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const std::string path(request.GetURL().path());
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (path == kStartPath || path == kOtherScopeStartPath) {
      response->set_content_type("text/html");
      response->set_content(kStartHtml);
    } else if (path == kNoServiceWorkerStartPath) {
      response->set_content_type("text/html");
      response->set_content(kNoServiceWorkerStartHtml);
    } else if (path == kServiceWorkerPath ||
               path == kOtherScopeServiceWorkerPath) {
      response->set_content_type("text/javascript");
      response->set_content(kServiceWorkerJs);
    } else if (path == kShareTargetPath) {
      // Like a static host without a server-side share handler.
      response->set_code(net::HTTP_METHOD_NOT_ALLOWED);
      response->set_content_type("text/html");
      response->set_content("<!doctype html><title>network</title>");
    } else if (path == kRedirectToCrossSitePath) {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location",
                                CrossSiteUrl(kNoServiceWorkerStartPath).spec());
    } else if (path == kHungStartPath) {
      return std::make_unique<net::test_server::HungResponse>();
    } else if (path == kFailingStartPath) {
      base::AutoLock lock(lock_);
      ++request_counts_[path];
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
    } else {
      return nullptr;
    }
    base::AutoLock lock(lock_);
    ++request_counts_[path];
    return response;
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  base::Lock lock_;
  std::map<std::string, int> request_counts_ GUARDED_BY(lock_);
};

// With no service worker registered, the start URL is loaded so the app can
// register its service worker, and the POST is then handled by that service
// worker. The bootstrap page is replaced in history by the share target page.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       NoServiceWorkerBootstrapsStartUrl) {
  EXPECT_EQ(kInterceptedTitle, Share(AppUrl(kStartPath)));

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1, RequestCount(kStartPath));
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
  EXPECT_FALSE(HistoryContains(AppUrl(kStartPath)));
}

// With a service worker already registered, the POST is sent straight away,
// without loading the start URL again.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       ExistingServiceWorkerPostsImmediately) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), AppUrl(kStartPath)));
  ASSERT_EQ("ready", content::EvalJs(
                         web_contents(),
                         "navigator.serviceWorker.ready.then(() => 'ready')"));
  ASSERT_EQ(1, RequestCount(kStartPath));

  EXPECT_EQ(kInterceptedTitle, Share(AppUrl(kStartPath)));

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1, RequestCount(kStartPath));
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
}

// A bootstrap URL that is not same-origin with the share target is ignored and
// the POST is sent straight away, as before.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       CrossOriginBootstrapUrlIsIgnored) {
  EXPECT_EQ(kNetworkTitle, Share(CrossSiteUrl(kStartPath)));

  EXPECT_EQ(0, RequestCount(kStartPath));
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
}

class WebApkPostShareTargetWaitForServiceWorkerDisabledBrowserTest
    : public WebApkPostShareTargetWaitForServiceWorkerBrowserTest {
 public:
  WebApkPostShareTargetWaitForServiceWorkerDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kShareTargetWaitForServiceWorker);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// With the feature disabled, the POST is sent straight away, as before.
IN_PROC_BROWSER_TEST_F(
    WebApkPostShareTargetWaitForServiceWorkerDisabledBrowserTest,
    PostsImmediately) {
  EXPECT_EQ(kNetworkTitle, Share(AppUrl(kStartPath)));

  EXPECT_EQ(0, RequestCount(kStartPath));
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
}

class WebApkPostShareTargetWaitForServiceWorkerTimeoutBrowserTest
    : public WebApkPostShareTargetWaitForServiceWorkerBrowserTest {
 public:
  WebApkPostShareTargetWaitForServiceWorkerTimeoutBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kShareTargetWaitForServiceWorker, {{"timeout", "500ms"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If the start URL never registers a service worker, the POST is still sent
// once the timeout expires, as before, replacing the bootstrap entry.
IN_PROC_BROWSER_TEST_F(
    WebApkPostShareTargetWaitForServiceWorkerTimeoutBrowserTest,
    PostsAfterTimeout) {
  EXPECT_EQ(kNetworkTitle, Share(AppUrl(kNoServiceWorkerStartPath)));

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
  EXPECT_FALSE(HistoryContains(AppUrl(kNoServiceWorkerStartPath)));
}

// If the tab already shows the start URL's document (e.g. a warm start) and the
// start URL has a fragment, the bootstrap navigation is same-document. The
// timeout must still apply, and the POST replaces the bootstrap entry.
IN_PROC_BROWSER_TEST_F(
    WebApkPostShareTargetWaitForServiceWorkerTimeoutBrowserTest,
    SameDocumentBootstrapPostsAfterTimeout) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     AppUrl(kNoServiceWorkerStartPath)));
  const GURL bootstrap_url =
      AppUrl(std::string(kNoServiceWorkerStartPath) + "#share");

  EXPECT_EQ(kNetworkTitle, Share(bootstrap_url));

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1, RequestCount(kNoServiceWorkerStartPath));
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
  EXPECT_TRUE(HistoryContains(AppUrl(kNoServiceWorkerStartPath)));
  EXPECT_FALSE(HistoryContains(bootstrap_url));
}

// The timeout does not run while a page of another origin is committed, e.g. a
// sign-in page that the start URL redirected to, so the POST is not sent there.
IN_PROC_BROWSER_TEST_F(
    WebApkPostShareTargetWaitForServiceWorkerTimeoutBrowserTest,
    TimeoutPausedOnOtherOrigin) {
  ASSERT_NO_FATAL_FAILURE(StartShareAndWaitForRedirectToCrossSite());

  // Stay on the other origin for longer than the (500ms) timeout.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  run_loop.Run();

  EXPECT_EQ(CrossSiteUrl(kNoServiceWorkerStartPath),
            web_contents()->GetLastCommittedURL());
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
}

// If the origin already has a service worker registration, just not one whose
// scope includes the share target action, loading the start URL will not add
// one, so the POST is sent straight away rather than after the timeout.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       OriginWithOtherRegistrationPostsImmediately) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), AppUrl(kOtherScopeStartPath)));
  ASSERT_EQ("ready", content::EvalJs(
                         web_contents(),
                         "navigator.serviceWorker.ready.then(() => 'ready')"));

  EXPECT_EQ(kNetworkTitle, Share(AppUrl(kStartPath)));

  EXPECT_EQ(0, RequestCount(kStartPath));
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
}

// If the bootstrap load fails, the page cannot register a service worker, so
// the POST is sent straight away rather than after the timeout. It replaces
// the error page.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       FailedBootstrapPostsImmediately) {
  EXPECT_EQ(kNetworkTitle, Share(AppUrl(kFailingStartPath)));

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_LE(1, RequestCount(kFailingStartPath));
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
  EXPECT_FALSE(HistoryContains(AppUrl(kFailingStartPath)));
}

// Navigations started by the page while waiting (e.g. a sign-in redirect chain
// or following a link) are part of the app's flow: the POST is still sent once
// the service worker activates. It does not replace the page navigated to,
// since that is no longer the bootstrap entry.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       RendererInitiatedNavigationKeepsPendingPost) {
  ASSERT_NO_FATAL_FAILURE(
      StartShareAndWaitForBootstrap(AppUrl(kNoServiceWorkerStartPath)));

  content::TitleWatcher title_watcher(web_contents(), kInterceptedTitle);
  title_watcher.AlsoWaitForTitle(kNetworkTitle);
  // Navigate from the page to a page that registers the service worker.
  ASSERT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("location = $1", AppUrl(kStartPath))));
  EXPECT_EQ(kInterceptedTitle, title_watcher.WaitAndGetTitle());

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
  EXPECT_TRUE(HistoryContains(AppUrl(kNoServiceWorkerStartPath)));
  EXPECT_TRUE(HistoryContains(AppUrl(kStartPath)));
}

// A browser-initiated navigation while waiting (e.g. a new intent, or Back)
// means the user has moved on, so the pending POST is dropped: it is not sent
// when a service worker later activates.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       BrowserInitiatedNavigationDropsPendingPost) {
  ASSERT_NO_FATAL_FAILURE(
      StartShareAndWaitForBootstrap(AppUrl(kNoServiceWorkerStartPath)));

  ASSERT_NO_FATAL_FAILURE(RegisterServiceWorker());
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_EQ(AppUrl(kStartPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
  EXPECT_FALSE(HistoryContains(AppUrl(kShareTargetPath)));
}

// A direct NavigateShareTargetPost() supersedes a POST that is still waiting,
// which is then not sent again when a service worker later activates.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       NavigateShareTargetPostDropsPendingPost) {
  ASSERT_NO_FATAL_FAILURE(
      StartShareAndWaitForBootstrap(AppUrl(kNoServiceWorkerStartPath)));

  content::TitleWatcher title_watcher(web_contents(), kNetworkTitle);
  NavigateShareTargetPost(MakeShareBody(), kFormEncodedHeader,
                          AppUrl(kShareTargetPath), web_contents());
  EXPECT_EQ(kNetworkTitle, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(1, RequestCount(kShareTargetPath));

  ASSERT_NO_FATAL_FAILURE(RegisterServiceWorker());
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_EQ(AppUrl(kStartPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1, RequestCount(kShareTargetPath));
}

// If the service worker activates (e.g. in another tab) while a page of
// another origin is committed (e.g. a sign-in page that the start URL
// redirected to), the POST is not sent there, but once a page of the app's
// origin commits.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       ActivationOnOtherOriginWaitsForAppOrigin) {
  ASSERT_NO_FATAL_FAILURE(StartShareAndWaitForRedirectToCrossSite());

  std::unique_ptr<content::WebContents> other_tab =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext()));
  ASSERT_TRUE(content::NavigateToURL(other_tab.get(), AppUrl(kStartPath)));
  ASSERT_EQ("ready", content::EvalJs(
                         other_tab.get(),
                         "navigator.serviceWorker.ready.then(() => 'ready')"));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_EQ(CrossSiteUrl(kNoServiceWorkerStartPath),
            web_contents()->GetLastCommittedURL());

  content::TitleWatcher title_watcher(web_contents(), kInterceptedTitle);
  title_watcher.AlsoWaitForTitle(kNetworkTitle);
  // Return to the app, as after signing in.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("location = $1", AppUrl(kNoServiceWorkerStartPath))));
  EXPECT_EQ(kInterceptedTitle, title_watcher.WaitAndGetTitle());

  EXPECT_EQ(AppUrl(kShareTargetPath), web_contents()->GetLastCommittedURL());
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
}

class WebApkPostShareTargetWaitForServiceWorkerMaxWaitBrowserTest
    : public WebApkPostShareTargetWaitForServiceWorkerBrowserTest {
 public:
  WebApkPostShareTargetWaitForServiceWorkerMaxWaitBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kShareTargetWaitForServiceWorker,
        {{"timeout", "1h"}, {"max_wait", "500ms"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// If the user stays on a page of another origin (e.g. abandons a sign-in) for
// longer than the maximum wait, the POST is dropped: it is not sent when the
// user later returns to the app and its service worker activates.
IN_PROC_BROWSER_TEST_F(
    WebApkPostShareTargetWaitForServiceWorkerMaxWaitBrowserTest,
    MaxWaitOnOtherOriginDropsPendingPost) {
  ASSERT_NO_FATAL_FAILURE(StartShareAndWaitForRedirectToCrossSite());

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
  run_loop.Run();

  ASSERT_TRUE(
      content::NavigateToURLFromRenderer(web_contents(), AppUrl(kStartPath)));
  ASSERT_EQ("ready", content::EvalJs(
                         web_contents(),
                         "navigator.serviceWorker.ready.then(() => 'ready')"));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_EQ(AppUrl(kStartPath), web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(HistoryContains(AppUrl(kShareTargetPath)));
}

// If the WebContents is destroyed while the bootstrap navigation is in flight,
// that navigation finishes without committing. This must not be treated as a
// failed bootstrap: the POST is not sent.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetWaitForServiceWorkerBrowserTest,
                       WebContentsDestroyedDuringBootstrapDropsPendingPost) {
  std::unique_ptr<content::WebContents> other_tab =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext()));
  content::TestNavigationManager bootstrap(other_tab.get(),
                                           AppUrl(kHungStartPath));
  NavigateShareTargetPostWhenServiceWorkerReady(
      MakeShareBody(), kFormEncodedHeader, AppUrl(kShareTargetPath),
      AppUrl(kHungStartPath), other_tab.get());
  ASSERT_TRUE(bootstrap.WaitForRequestStart());
  bootstrap.ResumeNavigation();

  other_tab.reset();

  // Give a POST, had one been sent, time to reach the server.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), AppUrl(kStartPath)));
  EXPECT_EQ(0, RequestCount(kShareTargetPath));
}

}  // namespace webapk
