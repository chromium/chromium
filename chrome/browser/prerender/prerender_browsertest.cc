// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

namespace {

// This is equal to content::PrerenderHost::FinalStatus::kActivated.
// TODO(crbug.com/1274021): Replace this with the FinalStatus enum value
// once it is exposed.
const int kFinalStatusActivated = 0;

}  // namespace

class PrerenderBrowserTest : public PlatformBrowserTest {
 public:
  PrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PrerenderBrowserTest::GetActiveWebContents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// An end-to-end test of prerendering and activating.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAndActivate) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);

  // Activate.
  content::TestNavigationManager navigation_manager(GetActiveWebContents(),
                                                    prerender_url);
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents()->GetMainFrame(),
                      content::JsReplace("location = $1", prerender_url)));
  navigation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(navigation_manager.was_prerendered_page_activation());
  EXPECT_TRUE(navigation_manager.was_successful());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      kFinalStatusActivated, 1);
}

// An end-to-end test of prerendering triggered by an embedder and activation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderTriggeredByEmbedderAndActivate) {
  base::HistogramTester histogram_tester;

  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");

  // Start embedder triggered prerendering.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      GetActiveWebContents()->StartPrerendering(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          "DirectURLInput");
  EXPECT_TRUE(prerender_handle);
  content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
      *GetActiveWebContents(), prerender_url);

  // Activate.
  content::TestNavigationManager navigation_manager(GetActiveWebContents(),
                                                    prerender_url);
  // Simulate a browser-initiated navigation.
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  navigation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(navigation_manager.was_prerendered_page_activation());
  EXPECT_TRUE(navigation_manager.was_successful());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      kFinalStatusActivated, 1);
}

// Tests that UseCounter for SpeculationRules-triggered prerender is recorded.
// This cannot be tested in content/ as SpeculationHostImpl records the usage
// with ContentBrowserClient::LogWebFeatureForCurrentPage() that is not
// implemented in content/.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, UseCounter) {
  base::HistogramTester histogram_tester;

  // Navigate to an initial page.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSpeculationRulesPrerender, 0);

  // Start a prerender. The API call should be recorded.
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  prerender_helper().AddPrerender(prerender_url);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSpeculationRulesPrerender, 1);

  // The API call should be recorded only one time per page.
  prerender_helper().AddPrerender(prerender_url);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSpeculationRulesPrerender, 1);
}

}  // namespace
