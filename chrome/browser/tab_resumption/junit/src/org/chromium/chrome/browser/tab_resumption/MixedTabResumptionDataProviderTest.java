// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MixedTabResumptionDataProviderTest extends TestSupport {
    @Mock private LocalTabTabResumptionDataProvider mLocalTabProvider;
    @Mock private SyncDerivedTabResumptionDataProvider mSyncDerivedProvider;

    @Captor private ArgumentCaptor<Callback<SuggestionsResult>> mLocalTabCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<SuggestionsResult>> mSyncDerivedCallbackCaptor;
    Callback<SuggestionsResult> mLocalTabCallback;
    Callback<SuggestionsResult> mSyncDerivedCallback;

    private MixedTabResumptionDataProvider mMixedProvider;
    private SuggestionsResult mResults;
    private int mFetchCount;
    private int mFetchSuggestionsCallbackCounter;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testLocalTabOnlyEmpty() {
        mMixedProvider = new MixedTabResumptionDataProvider(mLocalTabProvider, null);

        // Empty Local Tab result, which is necessarily FORCED_NULL.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ false);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testLocalTabOnlySingle() {
        Tab tab = makeMockBrowserTab();
        mMixedProvider = new MixedTabResumptionDataProvider(mLocalTabProvider, null);

        // Non-empty Local Tab result, which is STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ false);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertOneSuggestionWithTitle("Blue 1");
        Assert.assertEquals(tab, ((LocalTabSuggestionEntry) mResults.suggestions.get(0)).tab);

        // Local Tab result becomes empty. This can happen if user closes the suggested tab.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ false);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testSyncDerivedOnlyEmpty() {
        mMixedProvider = new MixedTabResumptionDataProvider(null, mSyncDerivedProvider);

        // Empty Sync Derived result, initilaly TENTATIVE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ false, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertNoSuggestions();

        // Empty Sync Derived result becomes STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ false, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertNoSuggestions();

        // Sync Derived result becomes FORCED_NULL. This can happen if user disables sync.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ false, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testSyncDerivedOnlyVarious() {
        mMixedProvider = new MixedTabResumptionDataProvider(null, mSyncDerivedProvider);

        // Start from non-empty TENTATIVE Sync Derived result.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ false, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertOneSuggestionWithTitle("Google Dog");

        // Different non-empty Sync Derived result, now STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ false, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.STABLE, 1, 0));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertTwoSuggestionsWithTitles("Google Cat", "Google Dog");

        // Sync Derived result becomes FORCED_NULL, e.g., if user disables sync.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ false, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testMixedEmpty() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(mLocalTabProvider, mSyncDerivedProvider);

        // Empty results, with Sync Derived result TENTATIVE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        // Callback passed to MixedTabResumptionDataProvider.fetchSuggestions() only gets called
        // after the callback of both sub-providers' fetchSuggestions() are called.
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertNoSuggestions();

        // Empty Sync Derived result now STABLE, swap order for variety,
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertNoSuggestions();

        // Everything is empty and FORCED_NULL.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testMixedSingleSyncDerived() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(mLocalTabProvider, mSyncDerivedProvider);

        // Non-empty Sync Derived result starts out TENTATIVE, no Local Tab result.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertOneSuggestionWithTitle("Google Dog");

        // Sync Derived result changes and becomes STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.STABLE, 1));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertOneSuggestionWithTitle("Google Cat");
    }

    @Test
    @SmallTest
    public void testMixedDoubleSyncDerived() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(mLocalTabProvider, mSyncDerivedProvider);

        // Non-empty Sync Derived result starts out TENTATIVE, no Local Tab result.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0, 1));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertTwoSuggestionsWithTitles("Google Dog", "Google Cat");

        // Non-empty Sync Derived result becomes STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.STABLE, 1, 0));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertTwoSuggestionsWithTitles("Google Cat", "Google Dog");
    }

    @Test
    @SmallTest
    public void testMixedSingleLocalTabToSingleSyncDerived() {
        Tab tab = makeMockBrowserTab();
        mMixedProvider =
                new MixedTabResumptionDataProvider(mLocalTabProvider, mSyncDerivedProvider);

        // Non-empty Local Tab result starts out STABLE; empty Sync Derived result is TENTATIVE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        // Empty Sync Derived result starts out TENTATIVE (gets ignored).
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertOneSuggestionWithTitle("Blue 1");
        Assert.assertEquals(tab, ((LocalTabSuggestionEntry) mResults.suggestions.get(0)).tab);

        // Local Tab result becomes empty, no change in Sync Derived results.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertNoSuggestions();

        // Sync Derived result becomes non-empty and STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.STABLE, 0));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertOneSuggestionWithTitle("Google Dog");
    }

    @Test
    @SmallTest
    public void testMixedDoubleLocalTabAndSyncDerived() {
        Tab tab = makeMockBrowserTab();
        SuggestionsResult stableSyncDerivedResult =
                makeSuggestionResult(ResultStrength.STABLE, 1, 0);

        mMixedProvider =
                new MixedTabResumptionDataProvider(mLocalTabProvider, mSyncDerivedProvider);

        // Non-empty Sync Derived result starts out TENTATIVE, no Local Tab result.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0, 1));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        // Local Tab results always show up first.
        assertTwoSuggestionsWithTitles("Blue 1", "Google Dog");

        // Non-empty Sync Derived result change and becomes STABLE.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(stableSyncDerivedResult);
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        // Local Tab results always show up first.
        assertTwoSuggestionsWithTitles("Blue 1", "Google Cat");

        // Local Tab result becomes empty; no change to Sync Derived result.
        startFetchAndCaptureCallbacks(/* hasLocalTab= */ true, /* hasSyncDerived= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(stableSyncDerivedResult);
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        // Now Sync Derived results take up both tiles.
        assertTwoSuggestionsWithTitles("Google Cat", "Google Dog");
    }

    /**
     * @param hasLocalTab Whether to expect Local Tab Provider usage, and to capture callback as
     *     `mLocalTabCallback` to inject suggestions.
     * @param hasSyncDerived Whether to expect Sync Derived provider usage, and to capture callback
     *     as `mSyncDerivedCallback` to inject suggestions.
     */
    private void startFetchAndCaptureCallbacks(boolean hasLocalTab, boolean hasSyncDerived) {
        mMixedProvider.fetchSuggestions(
                (SuggestionsResult results) -> {
                    mResults = results;
                    ++mFetchSuggestionsCallbackCounter;
                });
        ++mFetchCount;
        if (hasLocalTab) {
            verify(mLocalTabProvider, times(mFetchCount))
                    .fetchSuggestions(mLocalTabCallbackCaptor.capture());
            mLocalTabCallback = mLocalTabCallbackCaptor.getValue();
            Assert.assertNotNull(mLocalTabCallback);
        }
        if (hasSyncDerived) {
            verify(mSyncDerivedProvider, times(mFetchCount))
                    .fetchSuggestions(mSyncDerivedCallbackCaptor.capture());
            mSyncDerivedCallback = mSyncDerivedCallbackCaptor.getValue();
            Assert.assertNotNull(mSyncDerivedCallback);
        }
    }

    /** Helper to make a Local Tab suggestion result. */
    private SuggestionsResult makeLocalTabSuggestionResult(@ResultStrength int strength, Tab tab) {
        return new SuggestionsResult(strength, Arrays.asList(new LocalTabSuggestionEntry(tab)));
    }

    /** Helper to make a Derived suggestion result with 1 suggestion. */
    private SuggestionsResult makeSuggestionResult(@ResultStrength int strength, int index1) {
        return new SuggestionsResult(strength, Arrays.asList(makeSyncDerivedSuggestion(index1)));
    }

    /** Helper to make a Derived suggestion result with 2 suggestions. */
    private SuggestionsResult makeSuggestionResult(
            @ResultStrength int strength, int index1, int index2) {
        return new SuggestionsResult(
                strength,
                Arrays.asList(
                        makeSyncDerivedSuggestion(index1), makeSyncDerivedSuggestion(index2)));
    }

    /** Helper to assert that suggestion results are empty. */
    private void assertNoSuggestions() {
        Assert.assertNull(mResults.suggestions);
    }

    /**
     * Helper to assert that there is at least one suggestion result. Using `title` as a quick way
     * to identify the suggestion. Not bothering to check other fields, e.g., `url`.
     */
    private void assertOneSuggestionWithTitle(String title1) {
        Assert.assertTrue(mResults.suggestions.size() >= 1);
        Assert.assertEquals(title1, mResults.suggestions.get(0).title);
    }

    /**
     * Helper to assert that there are at least two suggestion results. Using `title` as a quick way
     * to identify the suggestions. Not bothering to check other fields, e.g., `url`.
     */
    private void assertTwoSuggestionsWithTitles(String title1, String title2) {
        Assert.assertTrue(mResults.suggestions.size() >= 2);
        Assert.assertEquals(title1, mResults.suggestions.get(0).title);
        Assert.assertEquals(title2, mResults.suggestions.get(1).title);
    }
}
