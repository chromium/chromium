// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.RelatedSearchesControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeResolveSearch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
// TODO(crbug.com/1338223):update the tests to be batched.
@DoNotBatch(reason = "Tests cannot runn batched due to RecordHistogram#forgetHistogram method.")
public class ContextualSearchUnbatchedTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    //============================================================================================
    // UMA assertions
    //============================================================================================

    /**
     * UMA assertions for a sequence of user actions that peek and expand the panel with
     * Related Searches showing and then close the panel without selecting any suggestion.
     */
    private void assertUmaForPeekAndExpandWithRSearchesEnabled() throws Exception {
        assertUmaForPeekAndExpandWithRSearchesEnabled(-1);
    }

    /**
     * UMA assertions for a sequence of user actions that peek and expand the panel with
     * Related Searches showing and then close the panel.
     * @param whichSuggestion Which suggestion was selected. A value of -1 means none.
     */
    private void assertUmaForPeekAndExpandWithRSearchesEnabled(int whichSuggestion)
            throws Exception {
        final int relatedSearchesCount = whichSuggestion > -1 ? 1 : 0;
        Assert.assertEquals(
                "Some entry in the Search.ContextualSearch.All.Searches histogram was not logged "
                        + "as expected!",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.ContextualSearch.All.ResultsSeen"));
        Assert.assertEquals(
                "Failed to log a search seen in the Search.ContextualSearch.All.Searches "
                        + "histogram!",
                relatedSearchesCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.ContextualSearch.All.Searches", 1));
        Assert.assertEquals("Failed to log a search that was not seen in the "
                        + "Search.ContextualSearch.All.Searches histogram!",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.ContextualSearch.All.Searches", 0));
        Assert.assertEquals(
                "Failed to log the correct number of searches seen from Related Searches and/or "
                        + "Contextual Search in the Search.ContextualSearch.All.Searches "
                        + "histogram",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.ContextualSearch.All.Searches", relatedSearchesCount));
        Assert.assertEquals(
                "Failed to log the correct count of Related Searches suggestions clicked in the "
                        + "Search.RelatedSearches.NumberOfSuggestionsClicked2 histogram!",
                relatedSearchesCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.RelatedSearches.NumberOfSuggestionsClicked2"));
        Assert.assertEquals("Failed to log all the right Related Searches chips as clicked in the "
                        + "Search.RelatedSearches.SelectedCarouselIndex histogram!",
                relatedSearchesCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.RelatedSearches.SelectedCarouselIndex"));
        if (relatedSearchesCount > 0) {
            Assert.assertEquals(
                    "Failed to find the expected Related Searches chip logged as clicked in the "
                            + "Search.RelatedSearches.SelectedCarouselIndex histogram that tracks "
                            + "which chip was clicked!",
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Search.RelatedSearches.SelectedCarouselIndex", whichSuggestion));
        }
        Assert.assertEquals(
                "Failed to log all the right Related Searches suggestions as selected in the "
                        + "Search.RelatedSearches.SelectedSuggestionIndex histogram!",
                relatedSearchesCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.RelatedSearches.SelectedSuggestionIndex"));
        if (relatedSearchesCount > 0) {
            Assert.assertEquals(
                    "Failed to find the expected Related Searches suggestion logged as selected "
                            + "in the Search.RelatedSearches.SelectedSuggestionIndex histogram "
                            + "that tracks which suggestion was selected!",
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Search.RelatedSearches.SelectedSuggestionIndex", whichSuggestion));
        }
        Assert.assertEquals(
                "Failed to log that Related Searches were shown but none selected in the "
                        + "Search.RelatedSearches.CTR histogram!",
                1 - relatedSearchesCount,
                RecordHistogram.getHistogramValueCountForTesting("Search.RelatedSearches.CTR", 0));
        Assert.assertEquals(
                "Failed to log that Related Searches were shown and at least one was selected "
                        + "in the Search.RelatedSearches.CTR histogram!",
                relatedSearchesCount,
                RecordHistogram.getHistogramValueCountForTesting("Search.RelatedSearches.CTR", 1));
        Assert.assertEquals("Failed to log that the carousel is shown and it was not scrolled "
                        + "in the Search.RelatedSearches.CarouselScrolled histogram!",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.RelatedSearches.CarouselScrolled", 0));
        Assert.assertEquals(
                "Failed to log that the carousel is shown and its scroll and click status "
                        + "in the Search.RelatedSearches.CarouselScrollAndClick histogram!",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.RelatedSearches.CarouselScrollAndClick"));
        if (relatedSearchesCount > 0) {
            Assert.assertEquals(
                    "Failed to log that the carousel is shown and it was not scrolled and clicked "
                            + "in the Search.RelatedSearches.CarouselScrollAndClick histogram!",
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Search.RelatedSearches.CarouselScrollAndClick",
                            1 /* int NO_SCROLL_CLICKED = 1 */));
        } else {
            Assert.assertEquals(
                    "Failed to log that the carousel is shown and it was not scrolled and not "
                            + "clicked in the "
                            + "Search.RelatedSearches.CarouselScrollAndClick histogram!",
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Search.RelatedSearches.CarouselScrollAndClick",
                            0 /* int NO_SCROLL_NO_CLICK = 0 */));
        }
        Assert.assertTrue(
                "Failed to log the last visible position index for a chip in the carousel "
                        + "in the Search.RelatedSearches.CarouselLastVisibleItemPosition "
                        + "histogram!",
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.RelatedSearches.CarouselLastVisibleItemPosition")
                        >= 0);
    }

    /** Forgets all the histograms that we care about. */
    private void forgetHistograms() {
        // This is a placeholder since RecordHistogram.forgetHistogramForTesting causes flakes in
        // batched tests and this is an unbatched test suite -- which automatically forgets
        // histograms. See https://crbug.com/1270962.
    }

    //============================================================================================
    // Test Cases. Many of these tests check histograms, which have issues running batched.
    //============================================================================================

    /**
     * Tests that a simple Tap with language determination triggers translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapWithLanguage(@EnabledFeature int enabledFeature) throws Exception {
        // Resolving a German word should trigger translation.
        mFakeServer.setExpectations("german",
                new ResolvedSearchTerm.Builder(false, 200, "Deutsche", "Deutsche")
                        .setContextLanguage("de")
                        .build());
        simulateResolveSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testRelatedSearchesItemNotSelected(@EnabledFeature int enabledFeature)
            throws Exception {
        FeatureList.setTestFeatures(ENABLE_RELATED_SEARCHES_IN_BAR);
        mPolicy.overrideAllowSendingPageUrlForTesting(true);
        FakeResolveSearch fakeSearch = simulateResolveSearch("intelligence");
        Assert.assertFalse("Related Searches should have been requested but were not!",
                mFakeServer.getSearchContext().getRelatedSearchesStamp().isEmpty());
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue("Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Expand the panel and assert that it ends up in the right place.
        expandPanelAndAssert();

        // Don't select any Related Searches suggestion, and close the panel
        closePanel();
        assertUmaForPeekAndExpandWithRSearchesEnabled();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesItemSelected() throws Exception {
        FeatureList.setTestFeatures(ENABLE_RELATED_SEARCHES_IN_BAR);
        mFakeServer.reset();
        FakeResolveSearch fakeSearch = simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue("Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Expand the panel and assert that it ends up in the right place.
        expandPanelAndAssert();

        // Select a Related Searches suggestion.
        RelatedSearchesControl relatedSearchesControl = mPanel.getRelatedSearchesInBarControl();
        final int chipToSelect = 3;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> relatedSearchesControl.selectChipForTest(chipToSelect));
        Assert.assertEquals("The Related Searches query was not shown in the Bar!",
                "Selection Related 3", mPanel.getSearchBarControl().getSearchTerm());

        // Collapse the panel back to the peeking state
        peekPanel();
        Assert.assertEquals(
                "The default query was not shown in the Bar after returning to peeking state!",
                "Intelligence", mPanel.getSearchBarControl().getSearchTerm());

        // Close the panel
        closePanel();
        assertUmaForPeekAndExpandWithRSearchesEnabled(chipToSelect);
    }
}
