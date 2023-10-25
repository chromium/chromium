// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PrerenderManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PrerenderManagerTest(const PrerenderManagerTest&) = delete;
  PrerenderManagerTest& operator=(const PrerenderManagerTest&) = delete;
  PrerenderManagerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD),
        prerender_helper_(
            base::BindRepeating(&PrerenderManagerTest::GetActiveWebContents,
                                base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOmniboxTriggerForPrerender2,
        {{"SupportSearchSuggestion", "true"}});
  }

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(&test_server_);
    ASSERT_TRUE(test_server_.Start());
    ChromeRenderViewHostTestHarness::SetUp();

    // Set up TemplateURLService for search prerendering.
    TemplateURLServiceFactoryTestUtil factory_util(profile());
    factory_util.VerifyLoad();

    TemplateURLData template_url_data;
    template_url_data.SetURL(GetUrl(search_site() + "?q={searchTerms}").spec());
    factory_util.model()->SetUserSelectedDefaultSearchProvider(
        factory_util.model()->Add(
            std::make_unique<TemplateURL>(template_url_data)));

    PrerenderManager::CreateForWebContents(GetActiveWebContents());
    prerender_manager_ = PrerenderManager::FromWebContents(web_contents());
    ASSERT_TRUE(prerender_manager_);
    web_contents_delegate_ =
        std::make_unique<content::test::ScopedPrerenderWebContentsDelegate>(
            *web_contents());
  }

  content::WebContents* GetActiveWebContents() { return web_contents(); }

  GURL GetSearchSuggestionUrl(const std::string& original_query,
                              const std::string& search_terms) {
    return GetUrl(search_site() + "?q=" + search_terms +
                  "&oq=" + original_query);
  }

  GURL GetCanonicalSearchUrl(const GURL& search_suggestion_url) {
    GURL canonical_search_url;
    EXPECT_TRUE(HasCanoncialPreloadingOmniboxSearchURL(
        search_suggestion_url, profile(), &canonical_search_url));
    return canonical_search_url;
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

 protected:
  GURL GetUrl(const std::string& path) { return test_server_.GetURL(path); }

  PrerenderManager* prerender_manager() { return prerender_manager_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::PreloadingFailureReason ToPreloadingFailureReason(
      PrerenderPredictionStatus status) {
    return static_cast<content::PreloadingFailureReason>(
        static_cast<int>(status) +
        static_cast<int>(content::PreloadingFailureReason::
                             kPreloadingFailureReasonContentEnd));
  }

 private:
  static std::string search_site() { return "/title1.html"; }

  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;

  net::EmbeddedTestServer test_server_;
  raw_ptr<PrerenderManager, DanglingUntriaged> prerender_manager_;
};

TEST_F(PrerenderManagerTest, StartCleanSearchSuggestionPrerender) {
  GURL prerendering_url = GetSearchSuggestionUrl("pre", "prerender");
  GURL canonical_url = GetCanonicalSearchUrl(prerendering_url);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  prerender_manager()->StartPrerenderSearchResult(
      canonical_url, prerendering_url, /*attempt=*/nullptr);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// Tests that the old prerender will be destroyed when starting prerendering a
// different search result.
TEST_F(PrerenderManagerTest, StartNewSuggestionPrerender) {
  GURL prerendering_url1 = GetSearchSuggestionUrl("pre", "prefetch");
  GURL canonical_url1 = GetCanonicalSearchUrl(prerendering_url1);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  prerender_manager()->StartPrerenderSearchResult(
      canonical_url1, prerendering_url1, /*attempt=*/nullptr);

  registry_observer.WaitForTrigger(prerendering_url1);
  int prerender_host_id1 = prerender_helper().GetHostForUrl(prerendering_url1);
  ASSERT_NE(prerender_host_id1, content::RenderFrameHost::kNoFrameTreeNodeId);
  content::test::PrerenderHostObserver host_observer(*GetActiveWebContents(),
                                                     prerender_host_id1);
  GURL prerendering_url2 = GetSearchSuggestionUrl("prer", "prerender");
  GURL canonical_url2 = GetCanonicalSearchUrl(prerendering_url2);
  prerender_manager()->StartPrerenderSearchResult(
      canonical_url2, prerendering_url2, /*attempt=*/nullptr);
  host_observer.WaitForDestroyed();
  registry_observer.WaitForTrigger(prerendering_url2);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  EXPECT_EQ(canonical_url2,
            prerender_manager()->GetPrerenderCanonicalSearchURLForTesting());
}

// Tests that the old prerender is not destroyed when starting prerendering the
// same search suggestion.
TEST_F(PrerenderManagerTest, StartSameSuggestionPrerender) {
  GURL prerendering_url1 = GetSearchSuggestionUrl("pre", "prerender");
  GURL canonical_url = GetCanonicalSearchUrl(prerendering_url1);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  prerender_manager()->StartPrerenderSearchResult(
      canonical_url, prerendering_url1, /*attempt=*/nullptr);
  registry_observer.WaitForTrigger(prerendering_url1);
  int prerender_host_id1 = prerender_helper().GetHostForUrl(prerendering_url1);
  EXPECT_NE(prerender_host_id1, content::RenderFrameHost::kNoFrameTreeNodeId);
  GURL prerendering_url2 = GetSearchSuggestionUrl("prer", "prerender");
  prerender_manager()->StartPrerenderSearchResult(
      canonical_url, prerendering_url2, /*attempt=*/nullptr);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());

  // The created prerender for `prerendering_url1` still exists, so the
  // prerender_host_id should be the same.
  int prerender_host_id2 = prerender_helper().GetHostForUrl(prerendering_url2);
  EXPECT_EQ(prerender_host_id2, prerender_host_id1);
}

// Tests that the PrerenderHandle is destroyed when the primary page changed.
TEST_F(PrerenderManagerTest, DestroyedOnNavigateAway) {
  GURL prerendering_url = GetSearchSuggestionUrl("pre", "prerende");
  GURL canonical_url = GetCanonicalSearchUrl(prerendering_url);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  prerender_manager()->StartPrerenderSearchResult(
      canonical_url, prerendering_url, /*attempt=*/nullptr);

  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  content::test::PrerenderHostObserver host_observer(*GetActiveWebContents(),
                                                     prerender_host_id);
  web_contents_tester()->NavigateAndCommit(GetUrl("/empty.html"));
  host_observer.WaitForDestroyed();
  EXPECT_FALSE(prerender_manager()->HasSearchResultPagePrerendered());
}

TEST_F(PrerenderManagerTest, StartCleanPrerenderDirectUrlInput) {
  GURL prerendering_url = GetUrl("/foo");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  auto* preloading_data = content::PreloadingData::GetOrCreateForWebContents(
      GetActiveWebContents());
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(prerendering_url);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kOmniboxDirectURLInput,
          content::PreloadingType::kPrerender, same_url_matcher,
          GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  prerender_manager()->StartPrerenderDirectUrlInput(prerendering_url,
                                                    *preloading_attempt);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// Test that the PreloadingTriggeringOutcome is set to kFailure when the DUI
// predictor suggests a different URL.
TEST_F(PrerenderManagerTest, StartNewPrerenderDirectUrlInput) {
  GURL prerendering_url = GetUrl("/foo");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  auto* preloading_data = content::PreloadingData::GetOrCreateForWebContents(
      GetActiveWebContents());
  content::PreloadingURLMatchCallback same_url_matcher =
      content::PreloadingData::GetSameURLMatcher(prerendering_url);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kOmniboxDirectURLInput,
          content::PreloadingType::kPrerender, same_url_matcher,
          GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  prerender_manager()->StartPrerenderDirectUrlInput(prerendering_url,
                                                    *preloading_attempt);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  content::test::PrerenderHostObserver host_observer(*GetActiveWebContents(),
                                                     prerender_host_id);
  GURL prerendering_url2 = GetUrl("/bar");
  content::PreloadingURLMatchCallback same_url_matcher2 =
      content::PreloadingData::GetSameURLMatcher(prerendering_url);
  content::PreloadingAttempt* preloading_attempt2 =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kOmniboxDirectURLInput,
          content::PreloadingType::kPrerender, same_url_matcher,
          GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  prerender_manager()->StartPrerenderDirectUrlInput(prerendering_url2,
                                                    *preloading_attempt2);
  host_observer.WaitForDestroyed();
  registry_observer.WaitForTrigger(prerendering_url2);
  EXPECT_EQ(ToPreloadingFailureReason(PrerenderPredictionStatus::kCancelled),
            content::test::PreloadingAttemptAccessor(preloading_attempt)
                .GetFailureReason());
}

}  // namespace
