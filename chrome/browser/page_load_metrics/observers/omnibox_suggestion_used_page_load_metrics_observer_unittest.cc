// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

class OmniboxSuggestionUsedMetricsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  OmniboxSuggestionUsedMetricsObserverTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tester_ =
        std::make_unique<page_load_metrics::PageLoadMetricsObserverTester>(
            web_contents(), this,
            base::BindRepeating(
                &OmniboxSuggestionUsedMetricsObserverTest::RegisterObservers,
                base::Unretained(this)));
    web_contents()->WasShown();
  }

  page_load_metrics::PageLoadMetricsObserverTester* tester() {
    return tester_.get();
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) {
    tracker->AddObserver(
        std::make_unique<OmniboxSuggestionUsedMetricsObserver>());
  }

  void SimulateOmniboxNavigationAndCommit(const GURL& url,
                                         ui::PageTransition core_transition) {
    auto sim = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    sim->SetTransition(ui::PageTransitionFromInt(
        core_transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
    sim->Commit();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<page_load_metrics::PageLoadMetricsObserverTester> tester_;
};

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_InitialEmptyDocument) {
  SimulateOmniboxNavigationAndCommit(
      GURL("https://www.google.com/search?q=test"),
      ui::PAGE_TRANSITION_GENERATED);

  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kPreviousDocumentWasInitialOrEmpty,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kPreviousDocumentWasInitialOrEmpty,
      1);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_AboutBlankDocument) {
  NavigateAndCommit(GURL("about:blank"));

  SimulateOmniboxNavigationAndCommit(
      GURL("https://www.google.com/search?q=test"),
      ui::PAGE_TRANSITION_GENERATED);

  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kPreviousDocumentWasInitialOrEmpty,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kPreviousDocumentWasInitialOrEmpty,
      1);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_StoredInBackForwardCache) {
  NavigateAndCommit(GURL("https://example.com"));

  SimulateOmniboxNavigationAndCommit(
      GURL("https://www.google.com/search?q=test"),
      ui::PAGE_TRANSITION_GENERATED);

  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kStoredInBackForwardCache,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kStoredInBackForwardCache,
      1);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_SearchFromExistingPage) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommit(GURL("https://example.com"));

  SimulateOmniboxNavigationAndCommit(
      GURL("https://www.google.com/search?q=test"),
      ui::PAGE_TRANSITION_GENERATED);

  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kUnloadedCrossProcess,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kUnloadedCrossProcess,
      1);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_NonGoogleSearch) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommit(GURL("https://example.com"));

  SimulateOmniboxNavigationAndCommit(
      GURL("https://example.org/search?q=test"),
      ui::PAGE_TRANSITION_GENERATED);

  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kUnloadedCrossProcess,
      1);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      0);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_URLFromExistingPage) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommit(GURL("https://example.com"));

  SimulateOmniboxNavigationAndCommit(GURL("https://other-domain.com"),
                                     ui::PAGE_TRANSITION_TYPED);

  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal, 0);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      0);
  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kUnloadedCrossProcess,
      1);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_SameProcessNav) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigateAndCommit(GURL("https://example.com/page1"));

  SimulateOmniboxNavigationAndCommit(GURL("https://example.com/page2"),
                                     ui::PAGE_TRANSITION_TYPED);

  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal, 0);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      0);
  tester()->histogram_tester().ExpectUniqueSample(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal,
      OmniboxSuggestionUsedMetricsObserver::OutgoingPageDisposal::
          kUnloadedSameProcess,
      1);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_IgnoredForLinkNavigations) {
  NavigateAndCommit(GURL("https://example.com"));

  // Normal link navigation without PAGE_TRANSITION_FROM_ADDRESS_BAR.
  NavigateAndCommit(GURL("https://example.com/page2"),
                    ui::PAGE_TRANSITION_LINK);

  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal, 0);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      0);
  tester()->histogram_tester().ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

TEST_F(OmniboxSuggestionUsedMetricsObserverTest,
       OutgoingPageDisposal_IgnoredForHistoryNavigation) {
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  SimulateOmniboxNavigationAndCommit(GURL("https://example.com/page1"),
                                     ui::PAGE_TRANSITION_TYPED);
  SimulateOmniboxNavigationAndCommit(GURL("https://example.com/page2"),
                                     ui::PAGE_TRANSITION_TYPED);

  base::HistogramTester history_tester;
  content::NavigationSimulator::GoBack(web_contents());

  history_tester.ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchOutgoingPageDisposal, 0);
  history_tester.ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kSearchGoogleOutgoingPageDisposal,
      0);
  history_tester.ExpectTotalCount(
      OmniboxSuggestionUsedMetricsObserver::kURLOutgoingPageDisposal, 0);
}

}  // namespace
