// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <optional>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/extensions/keyed_services/browser_context_keyed_service_factories.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

namespace contextual_cueing {

class ContextualCueingServiceBrowserTest : public InProcessBrowserTest {
 public:
  ContextualCueingServiceBrowserTest() = default;

  void SetUp() override {
    ContextualCueingServiceFactory::GetInstance();
    InProcessBrowserTest::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(ENABLE_GLIC)
class ContextualCueingServiceBrowserTestZSSFlag
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestZSSFlag() {
    scoped_feature_list_.InitAndEnableFeature(kGlicZeroStateSuggestions);
  }

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        glic::prefs::kGlicTabContextEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       ServiceSpawnsWithZSSFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       PrepareFetchesPageContentButNoModelExecution) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->PrepareToFetchContextualGlicZeroStateSuggestions(web_contents);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      0);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       GetFetchesSuggestions) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->GetContextualGlicZeroStateSuggestions(web_contents, /*is_fre=*/true,
                                                 future.GetCallback());
  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      1);
}
#endif

class ContextualCueingServiceBrowserTestCCFlag
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestCCFlag() {
    scoped_feature_list_.InitAndEnableFeature(kContextualCueing);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestCCFlag,
                       ServiceSpawnsWithCCFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

class ContextualCueingServiceBrowserTestDisabledFeatures
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestDisabledFeatures() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        {kContextualCueing, kGlicZeroStateSuggestions});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestDisabledFeatures,
                       NullServiceWithDisabledFeatures) {
  EXPECT_EQ(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

}  // namespace contextual_cueing
