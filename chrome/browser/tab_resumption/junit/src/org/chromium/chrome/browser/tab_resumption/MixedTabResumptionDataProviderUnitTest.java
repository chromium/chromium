// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MixedTabResumptionDataProviderUnitTest extends TestSupport {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LocalTabTabResumptionDataProvider mLocalTabProvider;
    @Mock private SyncDerivedTabResumptionDataProvider mSyncDerivedProvider;

    @Captor private ArgumentCaptor<Callback<SuggestionsResult>> mLocalTabCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<SuggestionsResult>> mSyncDerivedCallbackCaptor;
    Callback<SuggestionsResult> mLocalTabCallback;
    Callback<SuggestionsResult> mSyncDerivedCallback;

    private MixedTabResumptionDataProvider mMixedProvider;
    private SuggestionsResult mResults;
    private int mLocalTabFetchCount;
    private int mSyncDerivedFetchCount;
    private int mFetchSuggestionsCallbackCounter;

    @Test
    @SmallTest
    public void testLocalTabOnlyEmpty() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, null, /* disableBlend= */ false);

        // Empty Local Tab result, which is necessarily FORCED_NULL.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ false);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testLocalTabOnlySingle() {
        Tab tab = makeMockBrowserTab();
        mMixedProvider =
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, null, /* disableBlend= */ false);

        // Non-empty Local Tab result, which is STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ false);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertOneSuggestionWithTitle("Blue 1");

        // Local Tab result becomes empty. This can happen if user closes the suggested tab.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ false);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testSyncDerivedOnlyEmpty() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(
                        null, mSyncDerivedProvider, /* disableBlend= */ false);

        // Empty Sync Derived result, initilaly TENTATIVE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ false, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertNoSuggestions();

        // Empty Sync Derived result becomes STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ false, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertNoSuggestions();

        // Sync Derived result becomes FORCED_NULL. This can happen if user disables sync.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ false, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testSyncDerivedOnlyVarious() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(
                        null, mSyncDerivedProvider, /* disableBlend= */ false);

        // Start from non-empty TENTATIVE Sync Derived result.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ false, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertOneSuggestionWithTitle("Google Dog");

        // Different non-empty Sync Derived result, now STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ false, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.STABLE, 1, 0));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertTwoSuggestionsWithTitles("Google Cat", "Google Dog");

        // Sync Derived result becomes FORCED_NULL, e.g., if user disables sync.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ false, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.FORCED_NULL, mResults.strength);
        assertNoSuggestions();
    }

    @Test
    @SmallTest
    public void testMixedEmpty() {
        mMixedProvider =
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, mSyncDerivedProvider, /* disableBlend= */ false);

        // Empty results, with Sync Derived result TENTATIVE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        // Callback passed to MixedTabResumptionDataProvider.fetchSuggestions() only gets called
        // after the callback of both sub-providers' fetchSuggestions() are called.
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertNoSuggestions();

        // Empty Sync Derived result now STABLE, swap order for variety,
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertNoSuggestions();

        // Everything is empty and FORCED_NULL.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
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
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, mSyncDerivedProvider, /* disableBlend= */ false);

        // Non-empty Sync Derived result starts out TENTATIVE, no Local Tab result.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertOneSuggestionWithTitle("Google Dog");

        // Sync Derived result changes and becomes STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
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
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, mSyncDerivedProvider, /* disableBlend= */ false);

        // Non-empty Sync Derived result starts out TENTATIVE, no Local Tab result.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0, 1));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertTwoSuggestionsWithTitles("Google Dog", "Google Cat");

        // Non-empty Sync Derived result becomes STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
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
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, mSyncDerivedProvider, /* disableBlend= */ false);

        // Non-empty Local Tab result starts out STABLE; empty Sync Derived result is TENTATIVE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        // Empty Sync Derived result starts out TENTATIVE (gets ignored).
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertOneSuggestionWithTitle("Blue 1");

        // Local Tab result becomes empty, no change in Sync Derived results.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(new SuggestionsResult(ResultStrength.TENTATIVE, null));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertNoSuggestions();

        // Sync Derived result becomes non-empty and STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
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
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, mSyncDerivedProvider, /* disableBlend= */ false);

        // Non-empty Sync Derived result starts out TENTATIVE, no Local Tab result.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0, 1));
        Assert.assertEquals(0, mFetchSuggestionsCallbackCounter);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        // Local Tab results always show up first.
        assertTwoSuggestionsWithTitles("Blue 1", "Google Dog");

        // Non-empty Sync Derived result change and becomes STABLE.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.STABLE, tab));
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(stableSyncDerivedResult);
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        // Local Tab results always show up first.
        assertTwoSuggestionsWithTitles("Blue 1", "Google Cat");

        // Local Tab result becomes empty; no change to Sync Derived result.
        startFetchAndCaptureCallbacks(
                /* expectLocalTabUsage= */ true, /* expectSyncDerivedUsage= */ true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        mSyncDerivedCallback.onResult(stableSyncDerivedResult);
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        // Now Sync Derived results take up both tiles.
        assertTwoSuggestionsWithTitles("Google Cat", "Google Dog");
    }

    @Test
    @SmallTest
    public void testDisableBlend() {
        Tab tab = makeMockBrowserTab();

        mMixedProvider =
                new MixedTabResumptionDataProvider(
                        mLocalTabProvider, mSyncDerivedProvider, /* disableBlend= */ true);

        // Not using startFetchAndCaptureCallbacks() since events are now more granular.

        // Non-empty Local Tab result. Sync Derived won't have fetch request.
        startFetch();
        maybeCaptureLocalTabCallback(true);
        mLocalTabCallback.onResult(makeLocalTabSuggestionResult(ResultStrength.TENTATIVE, tab));
        maybeCaptureSyncDerivedCallback(false); // Sync Derived Provider elided.
        Assert.assertNull(mSyncDerivedCallback);
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        // Only Local Tab results are shown, even though Sync Derived results are available.
        assertOneSuggestionWithTitle("Blue 1");

        // Local Tab result becomes empty. Now Sync Derived results will be requested and used.
        startFetch();
        maybeCaptureLocalTabCallback(true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        maybeCaptureSyncDerivedCallback(true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.TENTATIVE, 0, 1));
        Assert.assertEquals(2, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.TENTATIVE, mResults.strength);
        assertTwoSuggestionsWithTitles("Google Dog", "Google Cat");

        // Sync Derived results become STABLE.
        startFetch();
        maybeCaptureLocalTabCallback(true);
        mLocalTabCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, null));
        maybeCaptureSyncDerivedCallback(true);
        mSyncDerivedCallback.onResult(makeSuggestionResult(ResultStrength.STABLE, 1));
        Assert.assertEquals(3, mFetchSuggestionsCallbackCounter);
        Assert.assertEquals(ResultStrength.STABLE, mResults.strength);
        assertOneSuggestionWithTitle("Google Cat");
    }

    /** Initiate Mixed Provider fetch. Still need to capture and call callbacks to continue. */
    private void startFetch() {
        mMixedProvider.fetchSuggestions(
                (SuggestionsResult results) -> {
                    mResults = results;
                    ++mFetchSuggestionsCallbackCounter;
                });
    }

    /**
     * Checks Local Tab Provider usage. If expected, capture callback as `mLocalTabCallback` to
     * inject suggestions. Else nullifies the variable.
     *
     * @param expectUsage Whether to expect Local Tab Provider usage.
     */
    private void maybeCaptureLocalTabCallback(boolean expectUsage) {
        if (expectUsage) {
            ++mLocalTabFetchCount;
            verify(mLocalTabProvider, times(mLocalTabFetchCount))
                    .fetchSuggestions(mLocalTabCallbackCaptor.capture());
            mLocalTabCallback = mLocalTabCallbackCaptor.getValue();
            Assert.assertNotNull(mLocalTabCallback);
        } else {
            verify(mLocalTabProvider, never()).fetchSuggestions(mLocalTabCallbackCaptor.capture());
            mLocalTabCallback = null;
        }
    }

    /**
     * Checks Sync Derived Provider usage. If expected, capture callback as `mSyncDerivedCallback`
     * to inject suggestions. Else nullifies the variable.
     *
     * @param expectUsage Whether to expect Sync Derived Provider usage.
     */
    private void maybeCaptureSyncDerivedCallback(boolean expectUsage) {
        if (expectUsage) {
            ++mSyncDerivedFetchCount;
            verify(mSyncDerivedProvider, times(mSyncDerivedFetchCount))
                    .fetchSuggestions(mSyncDerivedCallbackCaptor.capture());
            mSyncDerivedCallback = mSyncDerivedCallbackCaptor.getValue();
            Assert.assertNotNull(mSyncDerivedCallback);
        } else {
            verify(mSyncDerivedProvider, never())
                    .fetchSuggestions(mSyncDerivedCallbackCaptor.capture());
            mSyncDerivedCallback = null;
        }
    }

    /**
     * @param expectLocalTabUsage Whether to expect Local Tab Provider usage.
     * @param expectSyncDerivedUsage Whether to expect Sync Derived Provider usage.
     */
    private void startFetchAndCaptureCallbacks(
            boolean expectLocalTabUsage, boolean expectSyncDerivedUsage) {
        startFetch();
        maybeCaptureLocalTabCallback(expectLocalTabUsage);
        maybeCaptureSyncDerivedCallback(expectSyncDerivedUsage);
    }

    /** Helper to make a Local Tab suggestion result. */
    private SuggestionsResult makeLocalTabSuggestionResult(@ResultStrength int strength, Tab tab) {
        return new SuggestionsResult(
                strength, Arrays.asList(SuggestionEntry.createFromLocalTab(tab)));
    }

    /** Helper to make a Sync Derived suggestion result with 1 suggestion. */
    private SuggestionsResult makeSuggestionResult(@ResultStrength int strength, int index1) {
        return new SuggestionsResult(strength, Arrays.asList(makeSyncDerivedSuggestion(index1)));
    }

    /** Helper to make a Sync Derived suggestion result with 2 suggestions. */
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
