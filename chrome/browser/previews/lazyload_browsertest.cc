// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class LazyLoadBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kLazyImageLoading,
          {{"lazy_image_first_k_fully_load", "4G:0"}}}},
        {});
    InProcessBrowserTest::SetUp();
  }

  void EnableDataSaver(bool enabled) {
    Profile* profile = Profile::FromBrowserContext(browser()->profile());

    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile->GetPrefs(), enabled);
    base::RunLoop().RunUntilIdle();
  }

  void ScrollToAndWaitForScroll(unsigned int scroll_offset) {
    ASSERT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf("window.scrollTo(0, %d);", scroll_offset)));
    content::RenderFrameSubmissionObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    observer.WaitForScrollOffset(gfx::Vector2dF(0, scroll_offset));
  }

  // Sets up test pages with in-viewport and below-viewport cross-origin frames.
  void SetUpLazyLoadFrameTestPage() {
    cross_origin_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(cross_origin_server_.Start());

    embedded_test_server()->RegisterRequestHandler(base::Bind(
        [](uint16_t cross_origin_port,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          if (request.relative_url == "/mainpage.html") {
            response->set_content(base::StringPrintf(
                R"HTML(
          <body onload="on_load('document')">
            <script>
            function on_load(msg) {
              console.log("LAZY_LOAD CONSOLE", msg, "ON_LOAD FOR TEST");
            }
            </script>

            <iframe src="http://bar.com:%d/simple.html?auto"
              width="100" height="100"
              onload="on_load('in-viewport iframe')"></iframe>
            <iframe src="http://bar.com:%d/simple.html?lazy"
              width="100" height="100" loading="lazy"
              onload="on_load('in-viewport loading=lazy iframe')">
            </iframe>
            <iframe src="http://bar.com:%d/simple.html?eager"
              width="100" height="100" loading="eager"
              onload="on_load('in-viewport loading=eager iframe')">
            </iframe>

            <div style="height:11000px;"></div>
            Below the viewport cross-origin iframe <br>
            <iframe src="http://bar.com:%d/simple.html?auto&belowviewport"
              width="100" height="100"
              onload="on_load('below-viewport iframe')"></iframe>
            <iframe src="http://bar.com:%d/simple.html?lazy&belowviewport"
              width="100" height="100" loading="lazy"
              onload="on_load('below-viewport loading=lazy iframe')">
            </iframe>
            <iframe src="http://bar.com:%d/simple.html?eager&belowviewport"
              width="100" height="100" loading="eager"
              onload="on_load('below-viewport loading=eager iframe')">
            </iframe>
          </body>)HTML",
                cross_origin_port, cross_origin_port, cross_origin_port,
                cross_origin_port, cross_origin_port, cross_origin_port));
          }
          return response;
        },
        cross_origin_server_.port()));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpURLMonitor() {
    embedded_test_server()->RegisterRequestMonitor(base::Bind(
        [](std::vector<std::string>* request_paths,
           const net::test_server::HttpRequest& request) {
          request_paths->push_back(request.relative_url);
        },
        &request_paths_));
  }

  const std::vector<std::string>& request_paths() const {
    return request_paths_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer cross_origin_server_;
  std::vector<std::string> request_paths_;
};

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest, CSSBackgroundImageDeferred) {
  EnableDataSaver(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL("/lazyload/css-background-image.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::RunLoop().RunUntilIdle();
  // Navigate away to finish the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Verify that nothing is recorded for the image bucket.
  EXPECT_GE(0, histogram_tester.GetBucketCount(
                   "DataUse.ContentType.UserTrafficKB",
                   data_use_measurement::DataUseUserData::IMAGE));
}

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest, CSSPseudoBackgroundImageLoaded) {
  EnableDataSaver(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/lazyload/css-pseudo-background-image.html"));

  base::RunLoop().RunUntilIdle();
  // Navigate away to finish the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Verify that the image bucket has substantial kilobytes recorded.
  EXPECT_GE(30 /* KB */, histogram_tester.GetBucketCount(
                             "DataUse.ContentType.UserTrafficKB",
                             data_use_measurement::DataUseUserData::IMAGE));
}

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest,
                       LazyLoadImage_DeferredAndLoadedOnScroll) {
  EnableDataSaver(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/lazyload/img.html"));

  auto* contents = browser()->OpenURL(content::OpenURLParams(
      test_url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_TYPED, false));
  content::ConsoleObserverDelegate console_observer(
      contents, "LAZY_LOAD CONSOLE * ON_LOAD FOR TEST");
  contents->SetDelegate(&console_observer);

  // Wait for the four images (3 in-viewport, 1 eager below-viewport) and the
  // document to load.
  while (console_observer.messages().size() < 5) {
    base::RunLoop().RunUntilIdle();
  }
  console_observer.Wait();

  EXPECT_THAT(
      console_observer.messages(),
      testing::UnorderedElementsAre(
          "LAZY_LOAD CONSOLE document ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE in-viewport img ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE in-viewport loading=lazy img ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE in-viewport loading=eager img ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE below-viewport loading=eager img ON_LOAD FOR "
          "TEST"));

  // Scroll down and verify the below-viewport images load.
  ScrollToAndWaitForScroll(10000);
  while (console_observer.messages().size() < 7) {
    base::RunLoop().RunUntilIdle();
  }
  const auto messages = console_observer.messages();
  EXPECT_THAT(messages,
              testing::Contains(
                  "LAZY_LOAD CONSOLE below-viewport img ON_LOAD FOR TEST"));
  EXPECT_THAT(messages, testing::Contains("LAZY_LOAD CONSOLE below-viewport "
                                          "loading=lazy img ON_LOAD FOR TEST"));
}

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest,
                       LazyLoadFrame_DeferredAndLoadedOnScroll) {
  EnableDataSaver(true);
  SetUpLazyLoadFrameTestPage();
  GURL test_url(embedded_test_server()->GetURL("/mainpage.html"));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(
      contents, "LAZY_LOAD CONSOLE * ON_LOAD FOR TEST");
  contents->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Wait for the four iframes (3 in-viewport, 1 eager below-viewport) and the
  // document to load.
  while (console_observer.messages().size() < 5) {
    base::RunLoop().RunUntilIdle();
  }
  console_observer.Wait();

  EXPECT_THAT(
      console_observer.messages(),
      testing::UnorderedElementsAre(
          "LAZY_LOAD CONSOLE document ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE in-viewport iframe ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE in-viewport loading=lazy iframe ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE in-viewport loading=eager iframe ON_LOAD FOR TEST",
          "LAZY_LOAD CONSOLE below-viewport loading=eager iframe ON_LOAD FOR "
          "TEST"));

  // Scroll down and verify the below-viewport iframes load.
  ScrollToAndWaitForScroll(10000);
  while (console_observer.messages().size() < 7) {
    base::RunLoop().RunUntilIdle();
  }
  const auto messages = console_observer.messages();
  EXPECT_THAT(messages,
              testing::Contains(
                  "LAZY_LOAD CONSOLE below-viewport iframe ON_LOAD FOR TEST"));
  EXPECT_THAT(messages,
              testing::Contains("LAZY_LOAD CONSOLE below-viewport "
                                "loading=lazy iframe ON_LOAD FOR TEST"));
}

// Tests that need to verify lazyload should be disabled in certain cases.
class LazyLoadDisabledBrowserTest : public LazyLoadBrowserTest {
 public:
  enum class ExpectedLazyLoadAction {
    kOff,           // All iframes and images should load.
    kExplicitOnly,  // Only the above viewport elements and below viewport
                    // explicit lazyload elements will load.
  };

  content::WebContents* CreateBackgroundWebContents(Browser* browser,
                                                    const GURL& url) {
    return browser->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
  }

  void VerifyLazyLoadFrameBehavior(
      content::WebContents* web_contents,
      ExpectedLazyLoadAction expected_lazy_load_action) {
    content::ConsoleObserverDelegate console_observer(
        web_contents, "LAZY_LOAD CONSOLE * ON_LOAD FOR TEST");
    web_contents->SetDelegate(&console_observer);

    std::vector<std::string> expected_console_messages{
        "LAZY_LOAD CONSOLE document ON_LOAD FOR TEST",
        "LAZY_LOAD CONSOLE in-viewport iframe ON_LOAD FOR TEST",
        "LAZY_LOAD CONSOLE in-viewport loading=lazy iframe ON_LOAD FOR "
        "TEST",
        "LAZY_LOAD CONSOLE in-viewport loading=eager iframe ON_LOAD FOR "
        "TEST",
        "LAZY_LOAD CONSOLE below-viewport loading=eager iframe ON_LOAD FOR "
        "TEST"};
    switch (expected_lazy_load_action) {
      case ExpectedLazyLoadAction::kOff:
        expected_console_messages.push_back(
            "LAZY_LOAD CONSOLE below-viewport loading=lazy iframe ON_LOAD FOR "
            "TEST");
        ABSL_FALLTHROUGH_INTENDED;
      case ExpectedLazyLoadAction::kExplicitOnly:
        expected_console_messages.push_back(
            "LAZY_LOAD CONSOLE below-viewport iframe ON_LOAD FOR TEST");
        break;
    }

    // Wait for the expected elements and the document to load.
    while (console_observer.messages().size() <
           expected_console_messages.size()) {
      base::RunLoop().RunUntilIdle();
    }
    console_observer.Wait();

    EXPECT_THAT(console_observer.messages(),
                testing::UnorderedElementsAreArray(expected_console_messages));
  }

  void VerifyLazyLoadImageBehavior(
      content::WebContents* web_contents,
      ExpectedLazyLoadAction expected_lazy_load_action) {
    content::ConsoleObserverDelegate console_observer(
        web_contents, "LAZY_LOAD CONSOLE * ON_LOAD FOR TEST");
    web_contents->SetDelegate(&console_observer);

    std::vector<std::string> expected_console_messages{
        "LAZY_LOAD CONSOLE document ON_LOAD FOR TEST",
        "LAZY_LOAD CONSOLE in-viewport img ON_LOAD FOR TEST",
        "LAZY_LOAD CONSOLE in-viewport loading=lazy img ON_LOAD FOR TEST",
        "LAZY_LOAD CONSOLE in-viewport loading=eager img ON_LOAD FOR TEST",
        "LAZY_LOAD CONSOLE below-viewport loading=eager img ON_LOAD FOR "
        "TEST"};

    switch (expected_lazy_load_action) {
      case ExpectedLazyLoadAction::kOff:
        expected_console_messages.push_back(
            "LAZY_LOAD CONSOLE below-viewport loading=lazy img ON_LOAD FOR "
            "TEST");
        ABSL_FALLTHROUGH_INTENDED;
      case ExpectedLazyLoadAction::kExplicitOnly:
        expected_console_messages.push_back(
            "LAZY_LOAD CONSOLE below-viewport img ON_LOAD FOR TEST");
        break;
    }

    EXPECT_THAT(console_observer.messages(), testing::UnorderedElementsAre());

    // Wait for the expected elements and the document to load.
    while (console_observer.messages().size() <
           expected_console_messages.size()) {
      base::RunLoop().RunUntilIdle();
    }
    console_observer.Wait();
    EXPECT_THAT(console_observer.messages(),
                testing::UnorderedElementsAreArray(expected_console_messages));
  }
};

IN_PROC_BROWSER_TEST_F(LazyLoadDisabledBrowserTest,
                       LazyLoadImage_DisabledInBackgroundTab) {
  EnableDataSaver(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/lazyload/img.html"));
  auto* web_contents = CreateBackgroundWebContents(browser(), test_url);
  VerifyLazyLoadImageBehavior(web_contents, ExpectedLazyLoadAction::kOff);
}

IN_PROC_BROWSER_TEST_F(LazyLoadDisabledBrowserTest,
                       LazyLoadImage_DisabledInIncognito) {
  EnableDataSaver(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/lazyload/img.html"));
  auto* incognito_web_contents = browser()->OpenURL(content::OpenURLParams(
      test_url, content::Referrer(), WindowOpenDisposition::OFF_THE_RECORD,
      ui::PAGE_TRANSITION_TYPED, false));
  VerifyLazyLoadImageBehavior(incognito_web_contents,
                              ExpectedLazyLoadAction::kExplicitOnly);
}

IN_PROC_BROWSER_TEST_F(LazyLoadDisabledBrowserTest,
                       LazyLoadFrame_DisabledInBackgroundTab) {
  EnableDataSaver(true);
  SetUpLazyLoadFrameTestPage();
  GURL test_url(embedded_test_server()->GetURL("/mainpage.html"));
  auto* web_contents = CreateBackgroundWebContents(browser(), test_url);
  VerifyLazyLoadFrameBehavior(web_contents, ExpectedLazyLoadAction::kOff);
}

IN_PROC_BROWSER_TEST_F(LazyLoadDisabledBrowserTest,
                       LazyLoadFrame_DisabledInIncognito) {
  EnableDataSaver(true);
  SetUpLazyLoadFrameTestPage();
  GURL test_url(embedded_test_server()->GetURL("/mainpage.html"));
  auto* incognito_web_contents = browser()->OpenURL(content::OpenURLParams(
      test_url, content::Referrer(), WindowOpenDisposition::OFF_THE_RECORD,
      ui::PAGE_TRANSITION_TYPED, false));
  VerifyLazyLoadFrameBehavior(incognito_web_contents,
                              ExpectedLazyLoadAction::kExplicitOnly);
}

class LazyLoadPrerenderBrowserTest : public LazyLoadBrowserTest {
 public:
  void SetUpOnMainThread() override {
    LazyLoadBrowserTest::SetUpOnMainThread();

    prerender::PrerenderManager::SetMode(
        prerender::PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);
  }
};

IN_PROC_BROWSER_TEST_F(LazyLoadPrerenderBrowserTest, ImagesIgnored) {
  EnableDataSaver(true);
  SetUpURLMonitor();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/lazyload/img.html"));

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(prerender_manager);

  prerender::test_utils::TestPrerenderContentsFactory*
      prerender_contents_factory =
          new prerender::test_utils::TestPrerenderContentsFactory();
  prerender_manager->SetPrerenderContentsFactoryForTest(
      prerender_contents_factory);

  content::SessionStorageNamespace* storage_namespace =
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetController()
          .GetDefaultSessionStorageNamespace();
  ASSERT_TRUE(storage_namespace);

  std::unique_ptr<prerender::test_utils::TestPrerender> test_prerender =
      prerender_contents_factory->ExpectPrerenderContents(
          prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  std::unique_ptr<prerender::PrerenderHandle> prerender_handle =
      prerender_manager->AddPrerenderFromOmnibox(test_url, storage_namespace,
                                                 gfx::Size(640, 480));

  ASSERT_EQ(prerender_handle->contents(), test_prerender->contents());

  test_prerender->WaitForStop();
  EXPECT_THAT(request_paths(),
              testing::UnorderedElementsAre(
                  "/lazyload/img.html", "/lazyload/images/fruit1.jpg?auto",
                  "/lazyload/images/fruit1.jpg?lazy",
                  "/lazyload/images/fruit1.jpg?eager",
                  "/lazyload/images/fruit2.jpg?auto",
                  "/lazyload/images/fruit2.jpg?lazy",
                  "/lazyload/images/fruit2.jpg?eager"));
}
