// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
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

class NewTabPagePreloadPipelineManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  NewTabPagePreloadPipelineManagerTest(
      const NewTabPagePreloadPipelineManagerTest&) = delete;
  NewTabPagePreloadPipelineManagerTest& operator=(
      const NewTabPagePreloadPipelineManagerTest&) = delete;
  NewTabPagePreloadPipelineManagerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD),
        prerender_helper_(base::BindRepeating(
            &NewTabPagePreloadPipelineManagerTest::GetActiveWebContents,
            base::Unretained(this))) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(test_server_.Start());

    // Set up TemplateURLService for search prerendering.
    TemplateURLServiceFactoryTestUtil factory_util(profile());
    // content::BrowserTaskEnvironment::RunUntilIdle is preferred over
    // content::RunAllTasksUntilIdle in unit tests, but the latter is provided
    // in `VerifyLoad`.
    factory_util.model()->Load();
    task_environment()->RunUntilIdle();

    TemplateURLData template_url_data;
    template_url_data.SetURL(
        GetUrl(search_page_path() + "?q={searchTerms}").spec());
    factory_util.model()->SetUserSelectedDefaultSearchProvider(
        factory_util.model()->Add(
            std::make_unique<TemplateURL>(template_url_data)));

    NewTabPagePreloadPipelineManager::CreateForWebContents(
        GetActiveWebContents());
  }

  content::WebContents* GetActiveWebContents() { return web_contents(); }

  GURL GetSearchSuggestionUrl(const std::string& original_query,
                              const std::string& search_terms) {
    return GetUrl(search_page_path() + "?q=" + search_terms +
                  "&oq=" + original_query);
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  GURL GetUrl(const std::string& path) { return test_server_.GetURL(path); }

  NewTabPagePreloadPipelineManager* new_tab_page_preload_manager() {
    return NewTabPagePreloadPipelineManager::FromWebContents(web_contents());
  }

 private:
  static std::string search_page_path() { return "/title1.html"; }

  content::test::PrerenderTestHelper prerender_helper_;

  net::EmbeddedTestServer test_server_;
};

// Test that a search related url is ignored by the prerender NewTabPage
// trigger.
TEST_F(NewTabPagePreloadPipelineManagerTest, DisallowSearchUrl) {
  base::HistogramTester histogram_tester;
  GURL prerendering_url = GetSearchSuggestionUrl("prer", "prerender");
  new_tab_page_preload_manager()->StartPrerender(
      prerendering_url,
      chrome_preloading_predictor::kMouseHoverOrMouseDownOnNewTabPage);

  histogram_tester.ExpectUniqueSample(
      "Prerender.IsPrerenderingSRPUrl.Embedder_NewTabPage", true, 1);
}

}  // namespace
