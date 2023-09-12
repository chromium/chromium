// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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

    PrerenderManager::CreateForWebContents(GetActiveWebContents());
    prerender_manager_ = PrerenderManager::FromWebContents(web_contents());
    ASSERT_TRUE(prerender_manager_);
    prerender_manager_->set_skip_template_url_service_for_testing();
    web_contents_delegate_ =
        std::make_unique<content::test::ScopedPrerenderWebContentsDelegate>(
            *web_contents());
  }

  content::WebContents* GetActiveWebContents() { return web_contents(); }

  GURL GetSearchSuggestionUrl(const std::string& search_site,
                              const std::string& original_query,
                              const std::string& search_terms) {
    return test_server_.GetURL(search_site + "?q=" + search_terms +
                               "&oq=" + original_query);
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

 protected:
  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& search_site,
      const std::string& original_query,
      const std::string& search_terms) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(search_terms));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.destination_url =
        GetSearchSuggestionUrl(search_site, original_query, search_terms);
    match.keyword = base::UTF8ToUTF16(original_query);
    match.RecordAdditionalInfo("should_prerender", "true");
    return match;
  }

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
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;

  net::EmbeddedTestServer test_server_;
  raw_ptr<PrerenderManager, DanglingUntriaged> prerender_manager_;
};

TEST_F(PrerenderManagerTest, StartCleanSearchSuggestionPrerender) {
  GURL prerendering_url =
      GetSearchSuggestionUrl("/title1.html", "pre", "prerender");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  AutocompleteMatch match =
      CreateSearchSuggestionMatch("/title1.html", "pre", "prerender");
  prerender_manager()->StartPrerenderSearchSuggestion(match, prerendering_url);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// Tests that the old prerender will be destroyed when starting prerendering a
// different search result.
TEST_F(PrerenderManagerTest, StartNewSuggestionPrerender) {
  GURL prerendering_url =
      GetSearchSuggestionUrl("/title1.html", "pre", "prefetch");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  AutocompleteMatch match =
      CreateSearchSuggestionMatch("/title1.html", "pre", "prefetch");
  prerender_manager()->StartPrerenderSearchSuggestion(match, prerendering_url);

  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  ASSERT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  content::test::PrerenderHostObserver host_observer(*GetActiveWebContents(),
                                                     prerender_host_id);
  GURL prerendering_url2 =
      GetSearchSuggestionUrl("/title1.html", "prer", "prerender");
  match = CreateSearchSuggestionMatch("/title1.html", "prer", "prerender");
  prerender_manager()->StartPrerenderSearchSuggestion(match, prerendering_url2);
  host_observer.WaitForDestroyed();
  registry_observer.WaitForTrigger(prerendering_url2);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  EXPECT_EQ(prerendering_url2,
            prerender_manager()->GetPrerenderCanonicalSearchURLForTesting());
}

// Tests that the old prerender is not destroyed when starting prerendering the
// same search suggestion.
TEST_F(PrerenderManagerTest, StartSameSuggestionPrerender) {
  GURL prerendering_url =
      GetSearchSuggestionUrl("/title1.html", "pre", "prerender");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  AutocompleteMatch match =
      CreateSearchSuggestionMatch("/title1.html", "pre", "prerender");
  prerender_manager()->StartPrerenderSearchSuggestion(match, prerendering_url);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  match = CreateSearchSuggestionMatch("/title1.html", "prer", "prerender");
  prerender_manager()->StartPrerenderSearchSuggestion(match, prerendering_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());

  // The created prerender for `prerendering_url` still exists, so the
  // prerender_host_id should be the same.
  int prerender_host_id2 = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_EQ(prerender_host_id2, prerender_host_id);
}

// Tests that the PrerenderHandle is destroyed when the primary page changed.
TEST_F(PrerenderManagerTest, DestroyedOnNavigateAway) {
  GURL prerendering_url =
      GetSearchSuggestionUrl("/title1.html", "pre", "prerende");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  AutocompleteMatch match =
      CreateSearchSuggestionMatch("/title1.html", "pre", "prerende");
  prerender_manager()->StartPrerenderSearchSuggestion(match, prerendering_url);

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
