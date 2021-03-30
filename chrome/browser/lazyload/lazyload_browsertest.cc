// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/prefetch/no_state_prefetch/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/prerender_final_status.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/nqe/effective_connection_type.h"
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
          {{"lazy_image_first_k_fully_load",
            base::StringPrintf("%s:0,%s:0,%s:0,%s:0,%s:0,%s:0",
                               net::kEffectiveConnectionTypeUnknown,
                               net::kEffectiveConnectionTypeOffline,
                               net::kEffectiveConnectionTypeSlow2G,
                               net::kEffectiveConnectionType2G,
                               net::kEffectiveConnectionType3G,
                               net::kEffectiveConnectionType4G)}}}},
        {});
    InProcessBrowserTest::SetUp();
  }

  void EnableDataSaver(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
    base::RunLoop().RunUntilIdle();
  }

  content::EvalJsResult WaitForElementLoad(content::WebContents* contents,
                                           const char* element_id) {
    return content::EvalJs(contents, base::StringPrintf(R"JS(
              new Promise((resolve, reject) => {
                let e = document.getElementById('%s');
                if (loaded_ids.includes(e.id)) {
                  resolve(true);
                } else {
                  e.addEventListener('load', function() {
                    resolve(true);
                  });
                }
              });)JS",
                                                        element_id));
  }

  content::EvalJsResult ScrollToAndWaitForElementLoad(
      content::WebContents* contents,
      const char* element_id) {
    return content::EvalJs(contents, base::StringPrintf(R"JS(
              new Promise((resolve, reject) => {
                let e = document.getElementById('%s');
                e.scrollIntoView();
                if (loaded_ids.includes(e.id)) {
                  resolve(true);
                } else {
                  e.addEventListener('load', function() {
                    resolve(true);
                  });
                }
              });)JS",
                                                        element_id));
  }

  content::EvalJsResult IsElementLoaded(content::WebContents* contents,
                                        const char* element_id) {
    return content::EvalJs(
        contents, base::StringPrintf("loaded_ids.includes('%s');", element_id));
  }

  // Sets up test pages with in-viewport and below-viewport cross-origin frames.
  void SetUpLazyLoadFrameTestPage() {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    cross_origin_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(cross_origin_server_.Start());

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](uint16_t cross_origin_port,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          if (request.relative_url == "/mainpage.html") {
            response->set_content(base::StringPrintf(
                R"HTML(
          <body>
            <script>
            let loaded_ids = new Array();
            </script>

            <iframe id="atf_auto" src="http://bar.com:%d/simple.html?auto"
              width="100" height="100"
              onload="loaded_ids.push(this.id); console.log(this.id);">
            </iframe>
            <iframe id="atf_lazy" src="http://bar.com:%d/simple.html?lazy"
              width="100" height="100" loading="lazy"
              onload="loaded_ids.push(this.id); console.log(this.id);">
            </iframe>
            <iframe id="atf_eager" src="http://bar.com:%d/simple.html?eager"
              width="100" height="100" loading="eager"
              onload="loaded_ids.push(this.id); console.log(this.id);">
            </iframe>

            <div style="height:11000px;"></div>
            Below the viewport cross-origin iframes <br>
            <iframe id="btf_auto" src="http://bar.com:%d/simple.html?auto&belowviewport"
              width="100" height="100"
              onload="loaded_ids.push(this.id); console.log(this.id);">
            </iframe>
            <iframe id="btf_lazy" src="http://bar.com:%d/simple.html?lazy&belowviewport"
              width="100" height="100" loading="lazy"
              onload="loaded_ids.push(this.id); console.log(this.id);">
            </iframe>
            <iframe id="btf_eager" src="http://bar.com:%d/simple.html?eager&belowviewport"
              width="100" height="100" loading="eager"
              onload="loaded_ids.push(this.id); console.log(this.id);">
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer cross_origin_server_;
};

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest, CSSBackgroundImageDeferred) {
  EnableDataSaver(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL("/lazyload/css-background-image.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

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
  ASSERT_TRUE(content::WaitForLoadStop(contents));

  EXPECT_EQ(true, WaitForElementLoad(contents, "atf_auto"));
  EXPECT_EQ(true, WaitForElementLoad(contents, "atf_lazy"));
  EXPECT_EQ(true, WaitForElementLoad(contents, "atf_eager"));
  EXPECT_EQ(true, WaitForElementLoad(contents, "btf_eager"));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(false, IsElementLoaded(contents, "btf_auto"));
  EXPECT_EQ(false, IsElementLoaded(contents, "btf_lazy"));

  EXPECT_EQ(true, ScrollToAndWaitForElementLoad(contents, "btf_auto"));
  EXPECT_EQ(true, ScrollToAndWaitForElementLoad(contents, "btf_lazy"));
}

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest,
                       LazyLoadFrame_DeferredAndLoadedOnScroll) {
  EnableDataSaver(true);
  SetUpLazyLoadFrameTestPage();
  GURL test_url(embedded_test_server()->GetURL("/mainpage.html"));

  ui_test_utils::NavigateToURL(browser(), test_url);
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, WaitForElementLoad(contents, "atf_auto"));
  EXPECT_EQ(true, WaitForElementLoad(contents, "atf_lazy"));
  EXPECT_EQ(true, WaitForElementLoad(contents, "atf_eager"));
  EXPECT_EQ(true, WaitForElementLoad(contents, "btf_eager"));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(false, IsElementLoaded(contents, "btf_auto"));
  EXPECT_EQ(false, IsElementLoaded(contents, "btf_lazy"));

  EXPECT_EQ(true, ScrollToAndWaitForElementLoad(contents, "btf_auto"));
  EXPECT_EQ(true, ScrollToAndWaitForElementLoad(contents, "btf_lazy"));
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
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));

    EXPECT_EQ(true, WaitForElementLoad(web_contents, "atf_auto"));
    EXPECT_EQ(true, WaitForElementLoad(web_contents, "atf_lazy"));
    EXPECT_EQ(true, WaitForElementLoad(web_contents, "atf_eager"));
    EXPECT_EQ(true, WaitForElementLoad(web_contents, "btf_eager"));

    base::RunLoop().RunUntilIdle();

    switch (expected_lazy_load_action) {
      case ExpectedLazyLoadAction::kOff:
        EXPECT_EQ(true, IsElementLoaded(web_contents, "btf_auto"));
        EXPECT_EQ(true, IsElementLoaded(web_contents, "btf_lazy"));
        break;

      case ExpectedLazyLoadAction::kExplicitOnly:
        EXPECT_EQ(true, IsElementLoaded(web_contents, "btf_auto"));
        EXPECT_EQ(false, IsElementLoaded(web_contents, "btf_lazy"));
        break;
    }
  }

  void VerifyLazyLoadImageBehavior(
      content::WebContents* web_contents,
      ExpectedLazyLoadAction expected_lazy_load_action) {
    ASSERT_TRUE(content::WaitForLoadStop(web_contents));

    EXPECT_EQ(true, WaitForElementLoad(web_contents, "atf_auto"));
    EXPECT_EQ(true, WaitForElementLoad(web_contents, "atf_lazy"));
    EXPECT_EQ(true, WaitForElementLoad(web_contents, "atf_eager"));
    EXPECT_EQ(true, WaitForElementLoad(web_contents, "btf_eager"));

    base::RunLoop().RunUntilIdle();

    switch (expected_lazy_load_action) {
      case ExpectedLazyLoadAction::kOff:
        EXPECT_EQ(true, IsElementLoaded(web_contents, "btf_auto"));
        EXPECT_EQ(true, IsElementLoaded(web_contents, "btf_lazy"));
        break;

      case ExpectedLazyLoadAction::kExplicitOnly:
        EXPECT_EQ(true, IsElementLoaded(web_contents, "btf_auto"));
        EXPECT_EQ(false, IsElementLoaded(web_contents, "btf_lazy"));
        break;
    }
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

class LazyLoadPrerenderBrowserTest
    : public prerender::test_utils::PrerenderInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    prerender::test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
  }
  void EnableDataSaver(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(browser()->profile()->GetPrefs(),
                                      enabled);
    base::RunLoop().RunUntilIdle();
  }
};

IN_PROC_BROWSER_TEST_F(LazyLoadPrerenderBrowserTest, ImagesIgnored) {
  EnableDataSaver(true);
  UseHttpsSrcServer();

  std::unique_ptr<prerender::test_utils::TestPrerender> test_prerender =
      no_state_prefetch_contents_factory()->ExpectNoStatePrefetchContents(
          prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle =
      GetNoStatePrefetchManager()->AddPrerenderFromOmnibox(
          src_server()->GetURL("/lazyload/img.html"),
          GetSessionStorageNamespace(), gfx::Size(640, 480));

  ASSERT_EQ(no_state_prefetch_handle->contents(), test_prerender->contents());

  test_prerender->WaitForStop();
  for (const auto* url :
       {"/lazyload/img.html", "/lazyload/images/fruit1.jpg?auto",
        "/lazyload/images/fruit1.jpg?lazy", "/lazyload/images/fruit1.jpg?eager",
        "/lazyload/images/fruit2.jpg?auto", "/lazyload/images/fruit2.jpg?lazy",
        "/lazyload/images/fruit2.jpg?eager"}) {
    WaitForRequestCount(src_server()->GetURL(url), 1);
  }
}
