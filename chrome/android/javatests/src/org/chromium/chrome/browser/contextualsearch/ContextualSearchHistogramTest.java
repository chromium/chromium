// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.RelatedSearchesControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFakeServer.FakeResolveSearch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests the Contextual Search histograms. */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchHistogramTest extends ContextualSearchInstrumentationBase {
    private HistogramWatcher mResultsSeenHistogramWatcher;
    private HistogramWatcher mAllSearchesHistogramWatcher;
    private HistogramWatcher mTapResultsSeenHistogramWatcher;
    private HistogramWatcher mNumberOfSuggestionsClicked2HistogramWatcher;
    private HistogramWatcher mSelectedCarouselIndexHistogramWatcher;
    private HistogramWatcher mSelectedSuggestionIndexHistogramWatcher;
    private HistogramWatcher mCTRHistogramWatcher;
    private HistogramWatcher mCarouselScrolledHistogramWatcher;
    private HistogramWatcher mCarouselScrollAndClickHistogramWatcher;
    private HistogramWatcher mCarouselLastVisibleItemPositionHistogramWatcher;

    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    // ============================================================================================
    // UMA assertions
    // ============================================================================================

    /**
     * Create HistogramWatcher for a sequence of user actions that peek and expand the panel with
     * Related Searches showing and then close the panel without selecting any suggestion.
     *
     * @param isUKMEnabled Whether UKM is enabled and whether related histograms should be recorded.
     */
    private void createHistogramWatcherForPeekAndExpandForRSearches(boolean isUKMEnabled) {
        createHistogramWatcherForPeekAndExpandForRSearches(-1, isUKMEnabled);
    }

    /**
     * UMA HistogramWatcher for a sequence of user actions that peek and expand the panel with
     * Related Searches showing and then close the panel.
     *
     * @param whichSuggestion Which suggestion was selected. A value of -1 means none.
     * @param isUKMEnabled Whether UKM is enabled and whether related histograms should be recorded.
     */
    private void createHistogramWatcherForPeekAndExpandForRSearches(
            int whichSuggestion, boolean isUKMEnabled) {
        final int relatedSearchesCount = whichSuggestion > -1 ? 1 : 0;
        mResultsSeenHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Search.ContextualSearch.All.ResultsSeen", true, 1)
                        .build();
        HistogramWatcher.Builder histogramWatcherBuilder =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes("Search.ContextualSearch.All.Searches", false, 1);
        if (relatedSearchesCount > 0) {
            histogramWatcherBuilder.expectBooleanRecordTimes(
                    "Search.ContextualSearch.All.Searches", true, relatedSearchesCount);
        }
        mAllSearchesHistogramWatcher = histogramWatcherBuilder.build();

        histogramWatcherBuilder =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Search.ContextualSearch.Tap.ResultsSeen", true, 1);
        if (isUKMEnabled) {
            histogramWatcherBuilder.expectBooleanRecordTimes(
                    "Search.ContextualSearch.Tap.SyncEnabled.ResultsSeen", true, 1);
        } else {
            histogramWatcherBuilder.expectNoRecords(
                    "Search.ContextualSearch.Tap.SyncEnabled.ResultsSeen");
        }
        mTapResultsSeenHistogramWatcher = histogramWatcherBuilder.build();

        if (relatedSearchesCount > 0) {
            mNumberOfSuggestionsClicked2HistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectBooleanRecordTimes(
                                    "Search.RelatedSearches.NumberOfSuggestionsClicked2", true, 1)
                            .build();

            mSelectedCarouselIndexHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(
                                    "Search.RelatedSearches.SelectedCarouselIndex",
                                    whichSuggestion,
                                    1)
                            .build();

            mSelectedSuggestionIndexHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(
                                    "Search.RelatedSearches.SelectedSuggestionIndex",
                                    whichSuggestion,
                                    1)
                            .build();

            mCTRHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes("Search.RelatedSearches.CTR", 1, 1)
                            .build();
        } else {
            mNumberOfSuggestionsClicked2HistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectNoRecords("Search.RelatedSearches.NumberOfSuggestionsClicked2")
                            .build();

            mSelectedCarouselIndexHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectNoRecords("Search.RelatedSearches.SelectedCarouselIndex")
                            .build();

            mSelectedSuggestionIndexHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectNoRecords("Search.RelatedSearches.SelectedSuggestionIndex")
                            .build();

            mCTRHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes("Search.RelatedSearches.CTR", 0, 1)
                            .build();
        }

        mCarouselScrolledHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Search.RelatedSearches.CarouselScrolled", false, 1)
                        .build();

        mCarouselScrollAndClickHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Search.RelatedSearches.CarouselScrollAndClick",
                                relatedSearchesCount > 0 ? 1 : 0,
                                1)
                        .build();

        mCarouselLastVisibleItemPositionHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Search.RelatedSearches.CarouselLastVisibleItemPosition")
                        .allowExtraRecords("Search.RelatedSearches.CarouselLastVisibleItemPosition")
                        .build();
    }

    /** Assert HistogramWatcher to check the histograms are as expected. */
    private void assertHistogramWatcherForPeekAndExpandForRSearches() {
        mResultsSeenHistogramWatcher.assertExpected(
                "Some entry in the Search.ContextualSearch.All.Searches histogram was not logged as"
                        + " expected!");
        mAllSearchesHistogramWatcher.assertExpected(
                "Failed to log if a search was seen in the Search.ContextualSearch.All.Searches"
                        + " histogram!");
        mTapResultsSeenHistogramWatcher.assertExpected(
                "Some entry in the Search.ContextualSearch.Tap histograms was not logged as"
                        + " expected!");
        mNumberOfSuggestionsClicked2HistogramWatcher.assertExpected(
                "Failed to log the correct count of Related Searches suggestions clicked in the"
                        + " Search.RelatedSearches.NumberOfSuggestionsClicked2 histogram!");
        mSelectedCarouselIndexHistogramWatcher.assertExpected(
                "Failed to find the expected Related Searches chip logged as clicked in the"
                    + " Search.RelatedSearches.SelectedCarouselIndex histogram that tracks which"
                    + " chip was clicked!");
        mSelectedSuggestionIndexHistogramWatcher.assertExpected(
                "Failed to find the expected Related Searches suggestion logged as selected in the"
                    + " Search.RelatedSearches.SelectedSuggestionIndex histogram that tracks which"
                    + " suggestion was selected!");
        mCTRHistogramWatcher.assertExpected(
                "Failed to log that Related Searches were shown and if one was selected in the"
                        + " Search.RelatedSearches.CTR histogram!");
        mCarouselScrolledHistogramWatcher.assertExpected(
                "Failed to log that the carousel is shown and it was not scrolled in the"
                        + " Search.RelatedSearches.CarouselScrolled histogram!");
        mCarouselScrollAndClickHistogramWatcher.assertExpected(
                "Failed to log that the carousel is shown and it was not scrolled and clicked in"
                        + " the Search.RelatedSearches.CarouselScrollAndClick histogram!");
        mCarouselLastVisibleItemPositionHistogramWatcher.assertExpected(
                "Failed to log the last visible position index for a chip in the carousel in the"
                        + " Search.RelatedSearches.CarouselLastVisibleItemPosition histogram!");
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testRelatedSearchesItemNotSelected() throws Exception {
        mPolicy.overrideAllowSendingPageUrlForTesting(true);
        createHistogramWatcherForPeekAndExpandForRSearches(/* isUKMEnabled= */ false);
        FakeResolveSearch fakeSearch = simulateResolveSearch("intelligence");
        Assert.assertFalse(
                "Related Searches should have been requested but were not!",
                mFakeServer.getSearchContext().getRelatedSearchesStamp().isEmpty());
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue(
                "Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Expand the panel and assert that it ends up in the right place.
        expandPanelAndAssert();

        // Don't select any Related Searches suggestion, and close the panel
        closePanel();
        assertHistogramWatcherForPeekAndExpandForRSearches();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testRelatedSearchesItemNotSelectedUKMEnabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                ProfileManager.getLastUsedRegularProfile(), true));
        mPolicy.overrideAllowSendingPageUrlForTesting(true);
        createHistogramWatcherForPeekAndExpandForRSearches(/* isUKMEnabled= */ true);
        simulateResolveSearch("intelligence");
        // Expand the panel and assert that it ends up in the right place.
        expandPanelAndAssert();
        // Don't select any Related Searches suggestion, and close the panel
        closePanel();
        assertHistogramWatcherForPeekAndExpandForRSearches();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesItemSelected() throws Exception {
        FakeResolveSearch fakeSearch = simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue(
                "Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Expand the panel and assert that it ends up in the right place.
        expandPanelAndAssert();

        // Select a Related Searches suggestion.
        RelatedSearchesControl relatedSearchesControl = mPanel.getRelatedSearchesInBarControl();
        final int chipToSelect = 3;
        createHistogramWatcherForPeekAndExpandForRSearches(chipToSelect, /* isUKMEnabled= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> relatedSearchesControl.selectChipForTest(chipToSelect));
        Assert.assertEquals(
                "The Related Searches query was not shown in the Bar!",
                "Selection Related 3",
                mPanel.getSearchBarControl().getSearchTerm());

        // Collapse the panel back to the peeking state
        peekPanel();
        Assert.assertEquals(
                "The default query was not shown in the Bar after returning to peeking state!",
                "Intelligence",
                mPanel.getSearchBarControl().getSearchTerm());

        // Close the panel
        closePanel();
        assertHistogramWatcherForPeekAndExpandForRSearches();
    }
}
