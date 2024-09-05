// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include <optional>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/favicon_service.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/load_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/url_constants.h"

namespace {

using testing::ElementsAre;

std::unique_ptr<net::test_server::HttpResponse> NoContentResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (path != request.relative_url)
    return nullptr;

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_NO_CONTENT);
  return std::move(http_response);
}

// Tracks which URLs are loaded and whether the requests bypass the cache.
class TestURLLoaderInterceptor {
 public:
  TestURLLoaderInterceptor()
      : interceptor_(
            base::BindRepeating(&TestURLLoaderInterceptor::InterceptURLRequest,
                                base::Unretained(this))) {}

  TestURLLoaderInterceptor(const TestURLLoaderInterceptor&) = delete;
  TestURLLoaderInterceptor& operator=(const TestURLLoaderInterceptor&) = delete;

  bool was_loaded(const GURL& url) const {
    return intercepted_urls_.find(url) != intercepted_urls_.end();
  }

  bool did_bypass_cache(const GURL& url) const {
    return bypass_cache_urls_.find(url) != bypass_cache_urls_.end();
  }

  void Reset() {
    intercepted_urls_.clear();
    bypass_cache_urls_.clear();
  }

  network::mojom::RequestDestination destination(const GURL& url) {
    return destinations_[url];
  }

 private:
  bool InterceptURLRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    intercepted_urls_.insert(params->url_request.url);
    destinations_[params->url_request.url] = params->url_request.destination;
    if (params->url_request.load_flags & net::LOAD_BYPASS_CACHE)
      bypass_cache_urls_.insert(params->url_request.url);
    return false;
  }

  std::set<GURL> intercepted_urls_;
  std::set<GURL> bypass_cache_urls_;
  content::URLLoaderInterceptor interceptor_;
  std::map<GURL, network::mojom::RequestDestination> destinations_;
};

// Waits for the following the finish:
// - The pending navigation.
// - FaviconHandler's pending favicon database requests.
// - FaviconHandler's pending downloads.
// - Optionally, for a specific page URL (as a mechanism to wait of Javascript
//   completion).
class PendingTaskWaiter : public content::WebContentsObserver {
 public:
  explicit PendingTaskWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  PendingTaskWaiter(const PendingTaskWaiter&) = delete;
  PendingTaskWaiter& operator=(const PendingTaskWaiter&) = delete;

  ~PendingTaskWaiter() override {}

  void AlsoRequireUrl(const GURL& url) { required_url_ = url; }

  void AlsoRequireTitle(const std::u16string& title) {
    required_title_ = title;
  }

  void Wait() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // content::WebContentsObserver:
  void DidUpdateFaviconURL(
      content::RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    TestUrlAndTitle();
  }

  void TitleWasSet(content::NavigationEntry* entry) override {
    TestUrlAndTitle();
  }

  void TestUrlAndTitle() {
    if (!required_url_.is_empty() &&
        required_url_ != web_contents()->GetLastCommittedURL()) {
      return;
    }

    if (required_title_.has_value() &&
        *required_title_ != web_contents()->GetTitle()) {
      return;
    }

    // We need to poll periodically because Delegate::OnFaviconUpdated() is not
    // guaranteed to be called upon completion of the last database request /
    // download. In particular, OnFaviconUpdated() might not be called if a
    // database request confirms the data sent in the previous
    // OnFaviconUpdated() call.
    CheckStopWaitingPeriodically();
  }

  void CheckStopWaitingPeriodically() {
    EndLoopIfCanStopWaiting();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PendingTaskWaiter::CheckStopWaitingPeriodically,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(1));
  }

  void EndLoopIfCanStopWaiting() {
    if (!quit_closure_.is_null() &&
        !favicon::ContentFaviconDriver::FromWebContents(web_contents())
             ->HasPendingTasksForTest()) {
      quit_closure_.Run();
    }
  }

  base::RepeatingClosure quit_closure_;
  GURL required_url_;
  std::optional<std::u16string> required_title_;
  base::WeakPtrFactory<PendingTaskWaiter> weak_factory_{this};
};

class PageLoadStopper : public content::WebContentsObserver {
 public:
  explicit PageLoadStopper(content::WebContents* web_contents)
      : WebContentsObserver(web_contents), stop_on_finish_(false) {}

  PageLoadStopper(const PageLoadStopper&) = delete;
  PageLoadStopper& operator=(const PageLoadStopper&) = delete;

  ~PageLoadStopper() override {}

  void StopOnDidFinishNavigation() { stop_on_finish_ = true; }

  const std::vector<GURL>& last_favicon_candidates() const {
    return last_favicon_candidates_;
  }

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInMainFrame() && stop_on_finish_) {
      web_contents()->Stop();
      stop_on_finish_ = false;
    }
  }

  void DidUpdateFaviconURL(
      content::RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    last_favicon_candidates_.clear();
    for (const auto& candidate : candidates)
      last_favicon_candidates_.push_back(candidate->icon_url);
  }

  bool stop_on_finish_;
  std::vector<GURL> last_favicon_candidates_;
};

}  // namespace

class ContentFaviconDriverTest : public InProcessBrowserTest {
 public:
  ContentFaviconDriverTest()
      : prerender_helper_(
            base::BindRepeating(&ContentFaviconDriverTest::web_contents,
                                base::Unretained(this))) {}

  ContentFaviconDriverTest(const ContentFaviconDriverTest&) = delete;
  ContentFaviconDriverTest& operator=(const ContentFaviconDriverTest&) = delete;

  ~ContentFaviconDriverTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  favicon::FaviconService* favicon_service() {
    return FaviconServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  favicon_base::FaviconRawBitmapResult GetFaviconForPageURL(
      const GURL& url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip) {
    base::CancelableTaskTracker tracker;
    base::test::TestFuture<
        const std::vector<favicon_base::FaviconRawBitmapResult>&>
        future;
    favicon_service()->GetFaviconForPageURL(
        url, {icon_type}, desired_size_in_dip, future.GetCallback(), &tracker);
    std::vector<favicon_base::FaviconRawBitmapResult> results = future.Take();
    for (const favicon_base::FaviconRawBitmapResult& result : results) {
      if (result.is_valid())
        return result;
    }
    return favicon_base::FaviconRawBitmapResult();
  }

  favicon_base::FaviconRawBitmapResult GetFaviconForPageURL(
      const GURL& url,
      favicon_base::IconType icon_type) {
    return GetFaviconForPageURL(url, icon_type, /*desired_size_in_dip=*/0);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       DoNotLoadFaviconsWhilePrerendering) {
  ASSERT_TRUE(embedded_test_server()->Start());
  testing::NiceMock<content::MockWebContentsObserver> observer(web_contents());

  GURL prerender_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_CALL(observer, DidUpdateFaviconURL(testing::_, testing::_));
  prerender_helper().NavigatePrimaryPage(initial_url);

  {
    PendingTaskWaiter waiter(web_contents());
    prerender_helper().AddPrerender(prerender_url);
    waiter.Wait();
  }

  // We should not fetch the URL while prerendering.
  prerender_helper().WaitForRequest(prerender_url, 1);
  EXPECT_EQ(prerender_helper().GetRequestCount(icon_url), 0);

  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(prerender_url);
  auto* prerendered = prerender_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_CALL(observer, DidUpdateFaviconURL(prerendered, testing::_));
  prerender_helper().NavigatePrimaryPage(prerender_url);

  // Check that we've fetched the URL upon activation. Should not hang.
  EXPECT_EQ(prerender_helper().GetRequestCount(prerender_url), 1);
  prerender_helper().WaitForRequest(icon_url, 1);
}

class NoCommittedNavigationWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit NoCommittedNavigationWebContentsObserver(
      content::WebContents* web_contents) {
    Observe(web_contents);
  }

  ~NoCommittedNavigationWebContentsObserver() override = default;

  bool DidUpdateFaviconURLWithNoCommittedNavigation() const {
    return did_update_favicon_url_with_no_committed_navigation_;
  }

 protected:
  // WebContentsObserver:
  void DidUpdateFaviconURL(
      content::RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
    content::NavigationEntry* current_entry =
        web_contents->GetController().GetLastCommittedEntry();
    if (current_entry->IsInitialEntry()) {
      did_update_favicon_url_with_no_committed_navigation_ = true;
    }
  }

 private:
  bool did_update_favicon_url_with_no_committed_navigation_ = false;
};

// Observes the creation of new tabs and, upon creation, sets up both a pending
// task waiter (to ensure that ContentFaviconDriver tasks complete) and a
// NoCommittedNavigationWebContentsObserver (to ensure that we observe the
// expected function calls).
class FaviconUpdateOnlyInitialEntryTabStripObserver
    : public TabStripModelObserver {
 public:
  explicit FaviconUpdateOnlyInitialEntryTabStripObserver(TabStripModel* model)
      : model_(model) {
    model_->AddObserver(this);
  }
  ~FaviconUpdateOnlyInitialEntryTabStripObserver() override {
    model_->RemoveObserver(this);
  }

  void WaitForNewTab() {
    if (!pending_task_waiter_)
      run_loop_.Run();
  }

  bool DidUpdateFaviconURLWithNoCommittedNavigation() const {
    return observer_->DidUpdateFaviconURLWithNoCommittedNavigation();
  }

  PendingTaskWaiter* pending_task_waiter() {
    return pending_task_waiter_.get();
  }

 protected:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted)
      return;
    auto* web_contents = model_->GetActiveWebContents();
    pending_task_waiter_ = std::make_unique<PendingTaskWaiter>(web_contents);
    observer_ = std::make_unique<NoCommittedNavigationWebContentsObserver>(
        web_contents);
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<TabStripModel> model_ = nullptr;
  std::unique_ptr<PendingTaskWaiter> pending_task_waiter_;
  std::unique_ptr<NoCommittedNavigationWebContentsObserver> observer_;
};

// Tests that ContentFaviconDriver can handle being sent updated favicon URLs
// if there is no committed navigation, so it will use the initial
// NavigationEntry. This occurs when script is injected in the initial empty
// document of a newly created window. See crbug.com/520759 for more details.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       FaviconUpdateOnlyInitialEntry) {
  const char kNoContentPath[] = "/nocontent";
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&NoContentResponseHandler, kNoContentPath));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL empty_url = embedded_test_server()->GetURL("/empty.html");
  GURL no_content_url = embedded_test_server()->GetURL("/nocontent");

  FaviconUpdateOnlyInitialEntryTabStripObserver observer(
      browser()->tab_strip_model());

  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), empty_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(content::ExecJs(rfh, content::JsReplace(R"(
        let w = window.open('/page204.html');
        w.document.write('abc');
        w.document.close();
        w.location.href = $1;)",
                                                      no_content_url)));

  // Ensure that we have created our tab and set up the pending task waiter and
  // web contents observer.
  observer.WaitForNewTab();

  // Wait for WebContentsObsever::DidUpdateFaviconURL() call and for any
  // subsequent ContentFaviconDriver tasks to finish.
  observer.pending_task_waiter()->Wait();

  // We expect DidUpdateFaviconURL to be called and for no crash to ensue.
  EXPECT_TRUE(observer.DidUpdateFaviconURLWithNoCommittedNavigation());
}

// Test that when a user reloads a page ignoring the cache that the favicon is
// is redownloaded and (not returned from either the favicon cache or the HTTP
// cache).
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, ReloadBypassingCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  TestURLLoaderInterceptor url_loader_interceptor;
  // Initial visit in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  ASSERT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
  url_loader_interceptor.Reset();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // A normal visit should fetch the favicon from either the favicon database or
  // the HTTP cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
  ASSERT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  url_loader_interceptor.Reset();

  // A regular reload should not refetch the favicon from the website.
  {
    PendingTaskWaiter waiter(web_contents());
    chrome::ExecuteCommand(browser(), IDC_RELOAD);
    waiter.Wait();
  }
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
  ASSERT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  url_loader_interceptor.Reset();

  // A reload ignoring the cache should refetch the favicon from the website.
  {
    PendingTaskWaiter waiter(web_contents());
    chrome::ExecuteCommand(browser(), IDC_RELOAD_BYPASSING_CACHE);
    waiter.Wait();
  }
  ASSERT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  ASSERT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  EXPECT_TRUE(url_loader_interceptor.did_bypass_cache(icon_url));
}

// Test that favicon mappings are removed if the page initially lists a favicon
// and later uses Javascript to replace it with a touch icon.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       ReplaceFaviconWithTouchIconViaJavascript) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_change_favicon_type_to_touch_icon_via_js.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireTitle(u"OK");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_EQ(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
}

// Test that favicon changes via Javascript affecting a specific type (touch
// icons) do not influence the rest of the types (favicons).
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, ChangeTouchIconViaJavascript) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_change_touch_icon_via_js.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireTitle(u"OK");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
}

// Test that favicon mappings are removed if the page initially lists a touch
// icon and later uses Javascript to remove it.
#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, RemoveTouchIconViaJavascript) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_change_favicon_type_to_favicon_via_js.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireTitle(u"OK");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_EQ(nullptr,
            GetFaviconForPageURL(url, favicon_base::IconType::kTouchIcon)
                .bitmap_data);
}
#endif

// Test that favicon mappings are not removed if the page with favicons (cached
// in favicon database) is stopped while being loaded. More precisely, we test
// the stop event immediately following DidFinishNavigation(), before favicons
// have been processed in the HTML, because this is an example where Content
// reports an incomplete favicon list (or, like in this test, falls back to the
// default favicon.ico).
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, DoNotRemoveMappingIfStopped) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/slow_page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");
  GURL default_icon_url = embedded_test_server()->GetURL("/favicon.ico");

  // Prepopulate the favicon cache for the page (with synthetic content).
  favicon_service()->SetFavicons({url}, icon_url,
                                 favicon_base::IconType::kFavicon,
                                 gfx::test::CreateImage(32, 32));
  ASSERT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);

  // Stop the loading of the page as soon as DidFinishNaviation() is received,
  // which should be during the load of the slow-loading script and before
  // favicons are processed (verified in assertion below).
  PageLoadStopper stopper(web_contents());
  stopper.StopOnDidFinishNavigation();

  PendingTaskWaiter waiter(web_contents());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  // The whole purpose of this test is due to the fact that Content returns a
  // partial list in the scenario described above. If this behavior changes
  // (e.g. if Content sent no favicon candidates), we could likely simplify some
  // code (and remove this test).
  ASSERT_THAT(stopper.last_favicon_candidates(), ElementsAre(default_icon_url));

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
}

// Test that loading a page that contains icons only in the Web Manifest causes
// those icons to be used.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, LoadIconFromWebManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_manifest.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  TestURLLoaderInterceptor url_loader_interceptor;

  PendingTaskWaiter waiter(web_contents());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  ASSERT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::IconType::kWebManifestIcon)
                .bitmap_data);
#else
  EXPECT_FALSE(url_loader_interceptor.was_loaded(icon_url));
#endif
}

// Test that a page which uses a meta refresh tag to redirect gets associated
// to the favicons listed in the landing page.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageDespiteMetaRefreshTag) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag.html");
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
}

// Test that a page which uses a meta refresh tag to redirect gets associated
// to the favicons listed in the landing page, for the case that the landing
// page has been visited earlier (favicon cached). Similar behavior is expected
// for server-side redirects although not covered in this test.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespiteMetaRefreshTagAndLandingPageCached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_with_meta_refresh_tag = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag.html");
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  // Initial visit in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), landing_url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_with_meta_refresh_tag, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(nullptr, GetFaviconForPageURL(url_with_meta_refresh_tag,
                                          favicon_base::IconType::kFavicon)
                         .bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
}

// Test that a page gets a server-side redirect followed by a meta refresh tag
// gets associated to the favicons listed in the landing page.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespite300ResponseAndMetaRefreshTag) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_with_meta_refresh_tag = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag.html");
  GURL url = embedded_test_server()->GetURL("/server-redirect?" +
                                            url_with_meta_refresh_tag.spec());
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
}

// Test that a page gets a server-side redirect, followed by a meta refresh tag,
// followed by a server-side redirect gets associated to the favicons listed in
// the landing page.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespite300ResponseAndMetaRefreshTagTo300) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_with_meta_refresh_tag = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag_to_server_redirect.html");
  GURL url = embedded_test_server()->GetURL("/server-redirect?" +
                                            url_with_meta_refresh_tag.spec());
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
}

// Test that a page which uses JavaScript document.location.replace() to
// navigate within the page gets associated favicons.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespiteLocationOverrideWithinPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_override_within_page.html");
  GURL landing_url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_override_within_page.html#foo");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
}

// Test that a page which uses JavaScript document.location.replace() to
// navigate to a different landing page gets associated favicons listed in the
// landing page.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespiteLocationOverrideToOtherPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_override_to_other_page.html");
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
}

// Test that a page which uses JavaScript's history.replaceState() to update
// the URL in the omnibox (and history) gets associated favicons.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageIconDespiteReplaceState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/replacestate_with_favicon.html");
  GURL replacestate_url = embedded_test_server()->GetURL(
      "/favicon/replacestate_with_favicon_replaced.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(replacestate_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr, GetFaviconForPageURL(replacestate_url,
                                          favicon_base::IconType::kFavicon)
                         .bitmap_data);
}

// Test that a page which uses JavaScript's history.pushState() to update
// the URL in the omnibox (and history) gets associated favicons.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageIconDespitePushState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/pushstate_with_favicon.html");
  GURL pushstate_url = embedded_test_server()->GetURL(
      "/favicon/pushstate_with_favicon_pushed.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(pushstate_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
  EXPECT_NE(nullptr, GetFaviconForPageURL(pushstate_url,
                                          favicon_base::IconType::kFavicon)
                         .bitmap_data);
}

// Test that a page which uses JavaScript to navigate to a different page
// by overriding window.location.href (similar behavior as clicking on a link)
// does *not* get associated favicons from the landing page.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       DoNotAssociateIconWithInitialPageAfterHrefAssign) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_href_assign.html");
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  ASSERT_NE(nullptr,
            GetFaviconForPageURL(landing_url, favicon_base::IconType::kFavicon)
                .bitmap_data);
  EXPECT_EQ(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::IconType::kFavicon).bitmap_data);
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       LoadIconFromWebManifestDespitePushState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/pushstate_with_manifest.html");
  GURL pushstate_url = embedded_test_server()->GetURL(
      "/favicon/pushstate_with_manifest.html#pushState");

  PendingTaskWaiter waiter(web_contents());
  waiter.AlsoRequireUrl(pushstate_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(pushstate_url,
                                 {favicon_base::IconType::kWebManifestIcon})
                .bitmap_data);
}
#endif

class ContentFaviconDriverTestWithAutoupgradesDisabled
    : public ContentFaviconDriverTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentFaviconDriverTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Checks that a favicon loaded over HTTP is blocked on a secure page.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTestWithAutoupgradesDisabled,
                       MixedContentInsecureFaviconBlocked) {
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(ssl_server.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL favicon_url =
      embedded_test_server()->GetURL("example.test", "/favicon/icon.png");
  const GURL favicon_page = ssl_server.GetURL(
      "example.test",
      "/favicon/page_with_favicon_by_url.html?url=" + favicon_url.spec());

  // Observe the message for a blocked favicon.
  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("*icon.png*");

  // Observe if the favicon URL is requested.
  TestURLLoaderInterceptor url_interceptor;

  PendingTaskWaiter waiter(web_contents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), favicon_page));
  ASSERT_TRUE(console_observer.Wait());
  waiter.Wait();

  EXPECT_TRUE(base::MatchPattern(console_observer.GetMessageAt(0u),
                                 "*insecure favicon*"));
  EXPECT_TRUE(base::MatchPattern(console_observer.GetMessageAt(0u),
                                 "*request has been blocked*"));
  EXPECT_FALSE(url_interceptor.was_loaded(favicon_url));
}

// Checks that a favicon loaded over HTTPS is allowed on a secure page.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTestWithAutoupgradesDisabled,
                       MixedContentSecureFaviconAllowed) {
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(ssl_server.Start());

  const GURL favicon_url =
      ssl_server.GetURL("example.test", "/favicon/icon.png");
  const GURL favicon_page = ssl_server.GetURL(
      "example.test",
      "/favicon/page_with_favicon_by_url.html?url=" + favicon_url.spec());

  // Observe if the favicon URL is requested.
  TestURLLoaderInterceptor url_interceptor;

  PendingTaskWaiter waiter(web_contents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), favicon_page));
  waiter.Wait();

  EXPECT_TRUE(url_interceptor.was_loaded(favicon_url));
  ASSERT_EQ(network::mojom::RequestDestination::kImage,
            url_interceptor.destination(favicon_url));
}

IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, SVGFavicon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/page_with_svg_favicon.html");

  PendingTaskWaiter waiter(web_contents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter.Wait();

  auto result = GetFaviconForPageURL(url, favicon_base::IconType::kFavicon, 16);
  EXPECT_EQ(gfx::Size(16, 16), result.pixel_size);
  EXPECT_NE(nullptr, result.bitmap_data);
}

IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, SizesAny) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/page_with_sizes_any.html");
  GURL expected_icon_url = embedded_test_server()->GetURL("/favicon/icon.svg");

  PendingTaskWaiter waiter(web_contents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter.Wait();

  auto result = GetFaviconForPageURL(url, favicon_base::IconType::kFavicon, 16);
  EXPECT_EQ(expected_icon_url, result.icon_url);
  EXPECT_EQ(gfx::Size(16, 16), result.pixel_size);
  EXPECT_NE(nullptr, result.bitmap_data);
}

// Test that when a user visits a site after a cache deletion, the favicon is
// fetched again.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       FetchFaviconAfterCacheDeletion) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  TestURLLoaderInterceptor url_loader_interceptor;
  // Initial visit in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  url_loader_interceptor.Reset();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // A normal visit should fetch the favicon from the favicon database.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_FALSE(url_loader_interceptor.was_loaded(icon_url));
  url_loader_interceptor.Reset();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Clear cache.
  {
    content::BrowsingDataRemover* remover =
        browser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
    observer.BlockUntilCompletion();
  }

  // The favicon should be fetched again for navigations after cache deletion.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  url_loader_interceptor.Reset();
}

// Test that when a user visits a site in incognito, we download the favicon
// even if it was cached in regular mode.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       IncognitoDownloadsCachedFavicon) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  TestURLLoaderInterceptor url_loader_interceptor;
  // Initial visit in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  url_loader_interceptor.Reset();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Visiting the site in incognito mode should always load the favicon.
  Browser* incognito = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(incognito);
  {
    PendingTaskWaiter waiter(
        incognito->tab_strip_model()->GetActiveWebContents());
    ui_test_utils::NavigateToURLWithDisposition(
        incognito, url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
    waiter.Wait();
  }
  ASSERT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  url_loader_interceptor.Reset();
}

// Test that different origins share the underlying favicon cache over http.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, CrossOriginCacheHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TestURLLoaderInterceptor url_loader_interceptor;
  GURL icon_url = embedded_test_server()->GetURL("a.com", "/favicon/icon.png");
  GURL url_a = embedded_test_server()->GetURL(
      "a.com", "/favicon/page_with_favicon_by_url.html?url=" + icon_url.spec());
  GURL url_b = embedded_test_server()->GetURL(
      "b.com", "/favicon/page_with_favicon_by_url.html?url=" + icon_url.spec());

  // Initial visit to a.com in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url_a, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    waiter.Wait();
  }
  EXPECT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
  EXPECT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  url_loader_interceptor.Reset();

  // Initial visit to b.com shouldn't reuse the existing cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url_b, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    waiter.Wait();
  }
  EXPECT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
}

// Test that different origins share the underlying favicon cache over https.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, CrossOriginCacheHTTPS) {
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(ssl_server.Start());
  TestURLLoaderInterceptor url_loader_interceptor;
  GURL icon_url = ssl_server.GetURL("a.com", "/favicon/icon.png");
  GURL url_a = ssl_server.GetURL(
      "a.com", "/favicon/page_with_favicon_by_url.html?url=" + icon_url.spec());
  GURL url_b = ssl_server.GetURL(
      "b.com", "/favicon/page_with_favicon_by_url.html?url=" + icon_url.spec());

  // Initial visit to a.com in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url_a, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    waiter.Wait();
  }
  EXPECT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
  EXPECT_EQ(network::mojom::RequestDestination::kImage,
            url_loader_interceptor.destination(icon_url));
  url_loader_interceptor.Reset();

  // Initial visit to b.com shouldn't reuse the existing cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url_b, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    waiter.Wait();
  }
  EXPECT_TRUE(url_loader_interceptor.was_loaded(icon_url));
  EXPECT_FALSE(url_loader_interceptor.did_bypass_cache(icon_url));
}
