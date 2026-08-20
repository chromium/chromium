// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/dse_prewarm_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_service_factory.h"
#include "chrome/browser/preloading/prerender/search_prewarm_progress_test_utils.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class DSEPrewarmNavigationThrottleBrowserTest : public PlatformBrowserTest {
 public:
  DSEPrewarmNavigationThrottleBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &DSEPrewarmNavigationThrottleBrowserTest::GetWebContents,
            base::Unretained(this))),
        scoped_prewarm_feature_list_(test::ScopedPrewarmFeatureList::
                                         PrewarmState::kEnabledWithNoTrigger) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(GetProfile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(u"search.example.com");
    data.SetURL(
        embedded_test_server()
            ->GetURL("search.example.com", "/title1.html?q={searchTerms}")
            .spec());
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DSEPrewarmNavigationThrottleBrowserTest,
                       ThrottleSearchNavigationDuringPrewarm) {
  auto* profile = GetProfile();
  auto* service = SearchPrewarmProgressServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->HasOnGoingSearchPrewarm());

  GURL search_url = embedded_test_server()->GetURL("search.example.com",
                                                   "/title1.html?q=test");

  // When prewarm is not ongoing, navigation proceeds without being deferred.
  {
    content::TestNavigationObserver nav_observer(GetWebContents(), 1);
    ASSERT_TRUE(
        content::BeginNavigateToURLFromRenderer(GetWebContents(), search_url));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  // When prewarm is ongoing, navigation is deferred by
  // DSEPrewarmNavigationThrottle, and resumed once prewarm finishes.
  {
    content::PrerenderHostId dummy_host_id(1);
    service->OnSearchPrewarmStarted(dummy_host_id);
    EXPECT_TRUE(service->HasOnGoingSearchPrewarm());

    content::TestNavigationObserver nav_observer(GetWebContents(), 1);
    content::DidStartNavigationObserver start_observer(GetWebContents());

    ASSERT_TRUE(
        content::BeginNavigateToURLFromRenderer(GetWebContents(), search_url));
    start_observer.Wait();
    ASSERT_TRUE(start_observer.navigation_handle());
    EXPECT_TRUE(start_observer.navigation_handle()->IsDeferredForTesting());

    // Finish prewarm. This should invoke DSEPrewarmNavigationThrottle::Resume()
    // and resume the deferred navigation.
    service->OnSearchPrewarmFinished(
        dummy_host_id, content::PrerenderLifecycleStatus::kHTTPSuccessResponse);
    EXPECT_FALSE(service->HasOnGoingSearchPrewarm());

    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }
}

}  // namespace
