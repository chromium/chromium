// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include "base/command_line.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_hints {

class PerformanceHintsObserverBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
    // Add switch to avoid racing navigations in the test.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableFetchingHintsAtNavigationStartForTesting);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void AddPerformanceHint(const GURL& url,
                          const char* pattern,
                          optimization_guide::proto::PerformanceClass value) {
    optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
    auto* hint = hints_metadata.add_performance_hints();
    hint->set_wildcard_pattern(pattern);
    hint->set_performance_class(value);
    optimization_guide::OptimizationMetadata metadata;
    metadata.set_performance_hints_metadata(hints_metadata);

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(url, optimization_guide::proto::PERFORMANCE_HINTS,
                            metadata);
  }
};

IN_PROC_BROWSER_TEST_F(PerformanceHintsObserverBrowserTest,
                       PerformanceHintsFound) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());

  const GURL url = embedded_test_server()->GetURL("test.com", "/title1.html");

  // Adds a performance hint for Google URL as kFast for the main frame URL.
  AddPerformanceHint(url, "www.google.com",
                     optimization_guide::proto::PERFORMANCE_FAST);

  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  base::HistogramTester histogram_tester;

  // PERFORMANCE_FAST should be fetched for the Google URL.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.google.com"),
                  /*record_metrics=*/true),
              testing::Eq(optimization_guide::proto::PERFORMANCE_FAST));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kFast*/ 2, 1);
}

class PerformanceHintsObserverFencedFrameTest
    : public PerformanceHintsObserverBrowserTest {
 public:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PerformanceHintsObserverFencedFrameTest,
                       FencedFrameDoesNotAffectPerformanceHints) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());

  const GURL main_frame_url =
      embedded_test_server()->GetURL("test.com", "/title1.html");
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("test.com", "/fenced_frames/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), main_frame_url));

  base::HistogramTester histogram_tester;

  // PERFORMANCE_UNKNOWN should be fetched since any hints haven't been added.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.google.com"),
                  /*record_metrics=*/true),
              testing::Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 1);

  // Adds a performance hint for Google URL as kFast for the fenced frame URL.
  AddPerformanceHint(fenced_frame_url, "www.google.com",
                     optimization_guide::proto::PERFORMANCE_FAST);

  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetMainFrame(), fenced_frame_url));

  // The fenced frame URL has a hint for Google, but is not fetched.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.google.com"),
                  /*record_metrics=*/true),
              testing::Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 2);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 2);
}

class PerformanceHintsObserverPrerenderTest
    : public PerformanceHintsObserverBrowserTest {
 public:
  PerformanceHintsObserverPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &PerformanceHintsObserverBrowserTest::web_contents,
            base::Unretained(this))) {}

  void SetUpOnMainThread() override {
    prerender_helper_.SetUp(embedded_test_server());
    PerformanceHintsObserverBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PerformanceHintsObserverPrerenderTest,
                       PrerenderDoesNotAffectPerformanceHints) {
  PerformanceHintsObserver::CreateForWebContents(web_contents());

  const GURL main_frame_url =
      embedded_test_server()->GetURL("test.com", "/title1.html");
  const GURL prerender_url =
      embedded_test_server()->GetURL("test.com", "/title2.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), main_frame_url));

  base::HistogramTester histogram_tester;

  // PERFORMANCE_UNKNOWN should be fetched since any hints haven't been added.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.google.com"),
                  /*record_metrics=*/true),
              testing::Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 1);

  // Adds a performance hint for Google URL as kFast for the prerender URL.
  AddPerformanceHint(prerender_url, "www.google.com",
                     optimization_guide::proto::PERFORMANCE_FAST);

  // Add a prerender.
  int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());

  // The prerender URL has a hint for Google, but is not fetched.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.google.com"),
                  /*record_metrics=*/true),
              testing::Eq(optimization_guide::proto::PERFORMANCE_UNKNOWN));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 2);
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kUnknown*/ 0, 2);

  // Activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());

  // PERFORMANCE_FAST should be fetched for the Google URL.
  EXPECT_THAT(PerformanceHintsObserver::PerformanceClassForURL(
                  web_contents(), GURL("http://www.google.com"),
                  /*record_metrics=*/true),
              testing::Eq(optimization_guide::proto::PERFORMANCE_FAST));

  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.PerformanceClassForURL", /*kFast*/ 2, 1);
}

}  // namespace performance_hints
