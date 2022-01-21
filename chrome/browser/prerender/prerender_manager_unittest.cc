// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prerender/prerender_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
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
      : prerender_helper_(
            base::BindRepeating(&PrerenderManagerTest::GetActiveWebContents,
                                base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kOmniboxTriggerForPrerender2,
        {{"SupportSearchSuggestion", "true"}});
  }

  void SetUp() override {
    prerender_helper_.SetUp(&test_server_);
    ASSERT_TRUE(test_server_.Start());
    ChromeRenderViewHostTestHarness::SetUp();

    PrerenderManager::CreateForWebContents(GetActiveWebContents());
    prerender_manager_ = PrerenderManager::FromWebContents(web_contents());
    ASSERT_TRUE(prerender_manager_);
  }

  content::WebContents* GetActiveWebContents() { return web_contents(); }

  GURL GetUrl(const std::string& path) { return test_server_.GetURL(path); }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

 protected:
  PrerenderManager* prerender_manager() { return prerender_manager_; }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer test_server_;
  raw_ptr<PrerenderManager> prerender_manager_;
};

TEST_F(PrerenderManagerTest, StartCleanPrerender) {
  GURL prerendering_url = GetUrl("/title1.html");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  prerender_manager()->Start(
      prerendering_url, PrerenderManager::TriggerReason::kSearchSuggestion);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
}

// Tests that the old prerender will be destroyed when starting prerendering a
// different url.
TEST_F(PrerenderManagerTest, StartNewPrerender) {
  GURL prerendering_url = GetUrl("/title1.html");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  prerender_manager()->Start(
      prerendering_url, PrerenderManager::TriggerReason::kSearchSuggestion);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  ASSERT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  content::test::PrerenderHostObserver host_observer(*GetActiveWebContents(),
                                                     prerender_host_id);
  GURL prerendering_url2 = GetUrl("/title2.html");
  prerender_manager()->Start(
      prerendering_url2, PrerenderManager::TriggerReason::kSearchSuggestion);
  host_observer.WaitForDestroyed();
  registry_observer.WaitForTrigger(prerendering_url2);
  EXPECT_TRUE(prerender_manager()->prerender_handle_for_testing());
  EXPECT_EQ(prerendering_url2, prerender_manager()
                                   ->prerender_handle_for_testing()
                                   ->GetInitialPrerenderingUrl());
}

// Tests that the old prerender is not destroyed when starting prerendering the
// same url.
TEST_F(PrerenderManagerTest, StartSamePrerender) {
  GURL prerendering_url = GetUrl("/title1.html");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  prerender_manager()->Start(
      prerendering_url, PrerenderManager::TriggerReason::kSearchSuggestion);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  prerender_manager()->Start(
      prerendering_url, PrerenderManager::TriggerReason::kSearchSuggestion);
  EXPECT_TRUE(prerender_manager()->prerender_handle_for_testing());

  // The created prerender for `prerendering_url` still exists, so the
  // prerender_host_id should be the same.
  int prerender_host_id2 = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_EQ(prerender_host_id2, prerender_host_id);
}

// Tests that the PrerenderHandle is destroyed when the primary page changed.
TEST_F(PrerenderManagerTest, DestroyedOnNavigateAway) {
  web_contents_tester()->NavigateAndCommit(GetUrl("/empty.html"));
  GURL prerendering_url = GetUrl("/title1.html");
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  prerender_manager()->Start(
      prerendering_url, PrerenderManager::TriggerReason::kSearchSuggestion);
  registry_observer.WaitForTrigger(prerendering_url);
  int prerender_host_id = prerender_helper().GetHostForUrl(prerendering_url);
  EXPECT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  content::test::PrerenderHostObserver host_observer(*GetActiveWebContents(),
                                                     prerender_host_id);
  web_contents_tester()->NavigateAndCommit(GetUrl("/empty.html"));
  host_observer.WaitForDestroyed();
  EXPECT_FALSE(prerender_manager()->prerender_handle_for_testing());
}

}  // namespace
