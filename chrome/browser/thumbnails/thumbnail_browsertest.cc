// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

// These tests use --disable-gpu which isn't supported on ChromeOS.
#if !defined(OS_CHROMEOS)

namespace thumbnails {

namespace {

using testing::AtMost;
using testing::DoAll;
using testing::Field;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::_;

ACTION_P(RunClosure, closure) {
  closure.Run();
}

// |arg| is a gfx::Image, |expected_color| is a SkColor.
MATCHER_P(ImageColorIs, expected_color, "") {
  SkBitmap bitmap = arg.AsBitmap();
  if (bitmap.empty()) {
    *result_listener << "expected color but no bitmap data available";
    return false;
  }

  SkColor actual_color = bitmap.getColor(1, 1);
  if (actual_color != expected_color) {
    *result_listener << "expected color "
                     << base::StringPrintf("%08X", expected_color)
                     << " but actual color is "
                     << base::StringPrintf("%08X", actual_color);
    return false;
  }
  return true;
}

const char kHttpResponseHeader[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";

std::string MakeHtmlDocument(const std::string& background_color) {
  return base::StringPrintf(
      "<html>"
      "<head><style>body { background-color: %s }</style></head>"
      "<body></body>"
      "</html>",
      background_color.c_str());
}

class MockThumbnailService : public ThumbnailService {
 public:
  MOCK_METHOD2(SetPageThumbnail,
               bool(const ThumbnailingContext& context,
                    const gfx::Image& thumbnail));

  MOCK_METHOD3(GetPageThumbnail,
               bool(const GURL& url,
                    bool prefix_match,
                    scoped_refptr<base::RefCountedMemory>* bytes));

  MOCK_METHOD1(AddForcedURL, void(const GURL& url));

  MOCK_METHOD2(ShouldAcquirePageThumbnail,
               bool(const GURL& url, ui::PageTransition transition));

  // Implementation of RefcountedKeyedService.
  void ShutdownOnUIThread() override {}

 protected:
  ~MockThumbnailService() override = default;
};

// A helper class to wait until a navigation finishes and completes the first
// paint (as per WebContentsObserver::DidFirstVisuallyNonEmptyPaint). Similar
// to content::TestNavigationObserver, but waits for the paint in addition to
// the navigation (and is otherwise simplified).
// Note: This is a single-use class, supporting only a single navigation. Create
// a new instance for each navigation.
class NavigationAndFirstPaintWaiter : public content::WebContentsObserver {
 public:
  explicit NavigationAndFirstPaintWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~NavigationAndFirstPaintWaiter() override {}

  void Wait() { run_loop_.Run(); }

 private:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() ||
        !navigation_handle->IsInMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }
    CHECK(!did_finish_navigation_) << "Only a single navigation is supported";
    did_finish_navigation_ = true;
  }

  void DidFirstVisuallyNonEmptyPaint() override {
    if (did_finish_navigation_) {
      run_loop_.Quit();
    }
  }

  bool did_finish_navigation_ = false;
  base::RunLoop run_loop_;
};

class ThumbnailTest : public InProcessBrowserTest {
 public:
  ThumbnailTest() {}

  MockThumbnailService* thumbnail_service() {
    return static_cast<MockThumbnailService*>(
        ThumbnailServiceFactory::GetForProfile(browser()->profile()).get());
  }

 private:
  void SetUp() override {
    // Enabling pixel output is required for readback to work.
    EnablePixelOutput();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableGpu);
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(&ThumbnailTest::OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  static scoped_refptr<RefcountedKeyedService> CreateThumbnailService(
      content::BrowserContext* context) {
    return scoped_refptr<RefcountedKeyedService>(
        new NiceMock<MockThumbnailService>());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ThumbnailServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&ThumbnailTest::CreateThumbnailService));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailTest,
                       ShouldCaptureOnNavigatingAwayExplicitWait) {
  net::test_server::ControllableHttpResponse response_red(
      embedded_test_server(), "/red.html");
  net::test_server::ControllableHttpResponse response_yellow(
      embedded_test_server(), "/yellow.html");
  net::test_server::ControllableHttpResponse response_green(
      embedded_test_server(), "/green.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL about_blank_url("about:blank");
  const GURL red_url = embedded_test_server()->GetURL("/red.html");
  const GURL yellow_url = embedded_test_server()->GetURL("/yellow.html");
  const GURL green_url = embedded_test_server()->GetURL("/green.html");

  // Normally, ShouldAcquirePageThumbnail depends on many things, e.g. whether
  // the given URL is in TopSites. For the purposes of this test, bypass all
  // that and always take thumbnails, except for about:blank.
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(about_blank_url, _))
      .WillByDefault(Return(false));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(red_url, _))
      .WillByDefault(Return(true));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(yellow_url, _))
      .WillByDefault(Return(true));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(green_url, _))
      .WillByDefault(Return(true));

  // The test framework opens an about:blank tab by default.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(about_blank_url, active_tab->GetLastCommittedURL());

  EXPECT_CALL(
      *thumbnail_service(),
      SetPageThumbnail(Field(&ThumbnailingContext::url, about_blank_url), _))
      .Times(0);

  {
    // Navigate to the red page.
    NavigationAndFirstPaintWaiter waiter(active_tab);
    browser()->OpenURL(content::OpenURLParams(
        red_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
    response_red.WaitForRequest();
    response_red.Send(kHttpResponseHeader);
    response_red.Send(MakeHtmlDocument("red"));
    response_red.Done();
    waiter.Wait();
  }

  {
    // Before navigating away from the red page, we should take a thumbnail.
    base::RunLoop run_loop;
    EXPECT_CALL(*thumbnail_service(),
                SetPageThumbnail(Field(&ThumbnailingContext::url, red_url),
                                 ImageColorIs(SK_ColorRED)))
        .WillOnce(DoAll(RunClosure(run_loop.QuitClosure()), Return(true)));

    NavigationAndFirstPaintWaiter waiter(active_tab);
    browser()->OpenURL(content::OpenURLParams(
        yellow_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));

    response_yellow.WaitForRequest();
    // Note: It's important that we wait for the thumbnailing to finish *before*
    // sending the response. Otherwise, the DocumentAvailableInMainFrame event
    // might fire first, and we'll discard the captured thumbnail.
    run_loop.Run();
    response_yellow.Send(kHttpResponseHeader);
    response_yellow.Send(MakeHtmlDocument("yellow"));
    response_yellow.Done();
    waiter.Wait();
  }

  {
    // Before navigating away from the yellow page, we should take a thumbnail.
    base::RunLoop run_loop;
    EXPECT_CALL(*thumbnail_service(),
                SetPageThumbnail(Field(&ThumbnailingContext::url, yellow_url),
                                 ImageColorIs(SK_ColorYELLOW)))
        .WillOnce(DoAll(RunClosure(run_loop.QuitClosure()), Return(true)));

    NavigationAndFirstPaintWaiter waiter(active_tab);
    browser()->OpenURL(content::OpenURLParams(
        green_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));

    response_green.WaitForRequest();
    // As above, but send the response header before waiting for the
    // thumbnailing to finish. This should still be safe.
    response_green.Send(kHttpResponseHeader);
    run_loop.Run();
    response_green.Send(MakeHtmlDocument("green"));
    response_green.Done();
    waiter.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(ThumbnailTest,
                       ShouldCaptureOnNavigatingAwaySlowPageLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL about_blank_url("about:blank");
  const GURL red_url =
      embedded_test_server()->GetURL("/thumbnail_capture/red.html");
  const GURL slow_url = embedded_test_server()->GetURL("/slow?1");

  // Normally, ShouldAcquirePageThumbnail depends on many things, e.g. whether
  // the given URL is in TopSites. For the purposes of this test, bypass all
  // that and just take thumbnails of the red page.
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(about_blank_url, _))
      .WillByDefault(Return(false));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(red_url, _))
      .WillByDefault(Return(true));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(slow_url, _))
      .WillByDefault(Return(false));

  // The test framework opens an about:blank tab by default.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(about_blank_url, active_tab->GetLastCommittedURL());

  EXPECT_CALL(
      *thumbnail_service(),
      SetPageThumbnail(Field(&ThumbnailingContext::url, about_blank_url), _))
      .Times(0);

  // Navigate to the red page.
  {
    NavigationAndFirstPaintWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    ui_test_utils::NavigateToURL(browser(), red_url);
    waiter.Wait();
  }

  // Before navigating away from the red page, we should take a thumbnail.
  // Note that the page load is deliberately slowed down, so that the
  // thumbnailing process has time to finish before the new page comes in.
  EXPECT_CALL(*thumbnail_service(),
              SetPageThumbnail(Field(&ThumbnailingContext::url, red_url),
                               ImageColorIs(SK_ColorRED)))
      .WillOnce(Return(true));
  ui_test_utils::NavigateToURL(browser(), slow_url);
}

IN_PROC_BROWSER_TEST_F(ThumbnailTest,
                       ShouldContainProperContentIfCapturedOnNavigatingAway) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL about_blank_url("about:blank");
  const GURL red_url =
      embedded_test_server()->GetURL("/thumbnail_capture/red.html");
  const GURL yellow_url =
      embedded_test_server()->GetURL("/thumbnail_capture/yellow.html");

  // Normally, ShouldAcquirePageThumbnail depends on many things, e.g. whether
  // the given URL is in TopSites. For the purposes of this test, bypass all
  // that and just take thumbnails of the red page.
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(about_blank_url, _))
      .WillByDefault(Return(false));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(red_url, _))
      .WillByDefault(Return(true));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(yellow_url, _))
      .WillByDefault(Return(false));

  // The test framework opens an about:blank tab by default.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(about_blank_url, active_tab->GetLastCommittedURL());

  EXPECT_CALL(
      *thumbnail_service(),
      SetPageThumbnail(Field(&ThumbnailingContext::url, about_blank_url), _))
      .Times(0);

  // Navigate to the red page.
  ui_test_utils::NavigateToURL(browser(), red_url);

  // Before navigating away from the red page, we should attempt to take a
  // thumbnail, which might or might not succeed before we arrive at the new
  // page. In any case, we must not get an image with wrong (non-red) contents.
  EXPECT_CALL(*thumbnail_service(),
              SetPageThumbnail(Field(&ThumbnailingContext::url, red_url),
                               ImageColorIs(SK_ColorRED)))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  ui_test_utils::NavigateToURL(browser(), yellow_url);
}

IN_PROC_BROWSER_TEST_F(ThumbnailTest,
                       ShouldContainProperContentIfCapturedOnTabSwitch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL about_blank_url("about:blank");
  const GURL red_url =
      embedded_test_server()->GetURL("/thumbnail_capture/red.html");
  const GURL yellow_url =
      embedded_test_server()->GetURL("/thumbnail_capture/yellow.html");

  // Normally, ShouldAcquirePageThumbnail depends on many things, e.g. whether
  // the given URL is in TopSites. For the purposes of this test, bypass all
  // that and just take thumbnails of the red and the yellow page.
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(about_blank_url, _))
      .WillByDefault(Return(false));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(red_url, _))
      .WillByDefault(Return(true));
  ON_CALL(*thumbnail_service(), ShouldAcquirePageThumbnail(yellow_url, _))
      .WillByDefault(Return(true));

  // The test framework opens an about:blank tab by default.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(about_blank_url, active_tab->GetLastCommittedURL());

  // Open a new tab with the red page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), red_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Open another tab with the yellow page. Before the red tab gets hidden, we
  // should attempt to take a thumbnail of it, which might or might not succeed.
  // In any case, we must not get an image with wrong (non-red) contents.
  EXPECT_CALL(*thumbnail_service(),
              SetPageThumbnail(Field(&ThumbnailingContext::url, red_url),
                               ImageColorIs(SK_ColorRED)))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), yellow_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Switch back to the first tab. Again, we should attempt to take a thumbnail
  // (of the yellow page this time), which might or might not succeed, but if it
  // does must have the correct contents.
  EXPECT_CALL(*thumbnail_service(),
              SetPageThumbnail(Field(&ThumbnailingContext::url, yellow_url),
                               ImageColorIs(SK_ColorYELLOW)))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(yellow_url, active_tab->GetLastCommittedURL());
  browser()->tab_strip_model()->ActivateTabAt(0, /*user_gesture=*/true);
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(about_blank_url, active_tab->GetLastCommittedURL());

  // Open another about:blank tab, to give the thumbnailing process above some
  // time to finish (or fail). Without this, the test always finishes before
  // the thumbnailing process does, so we'd have no chance to check the image
  // contents.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), about_blank_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

}  // namespace

}  // namespace thumbnails

#endif  // !defined(OS_CHROMEOS)
