// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_resumption.SyncDerivedSuggestionEntrySource.SourceDataChangedObserver;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for SyncDerivedTabResumptionDataProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SyncDerivedTabResumptionDataProviderTest extends TestSupport {
    static final SuggestionEntry ENTRY1 =
            new SuggestionEntry("My Tablet", TAB6.url, TAB6.title, TAB6.lastActiveTime, TAB6.id);
    static final SuggestionEntry ENTRY2 =
            new SuggestionEntry("My Tablet", TAB5.url, TAB5.title, TAB5.lastActiveTime, TAB5.id);
    static final SuggestionEntry ENTRY3 =
            new SuggestionEntry("My Desktop", TAB1.url, TAB1.title, TAB1.lastActiveTime, TAB1.id);

    @Mock private SyncDerivedSuggestionEntrySource mSource;

    @Captor private ArgumentCaptor<SourceDataChangedObserver> mSourceDataChangedObserverCaptor;

    private SyncDerivedTabResumptionDataProvider mDataProvider;
    private SourceDataChangedObserver mSourceDataChangedObserver;

    private int mStatusChangedCallbackCounter;
    private boolean mFetchSuggestionsAndCheckCallFlag;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mDataProvider = new SyncDerivedTabResumptionDataProvider(mSource, () -> {});
        mDataProvider.setStatusChangedCallback(
                () -> {
                    ++mStatusChangedCallbackCounter;
                });

        verify(mSource).addObserver(mSourceDataChangedObserverCaptor.capture());
        mSourceDataChangedObserver = mSourceDataChangedObserverCaptor.getValue();
        Assert.assertNotNull(mSourceDataChangedObserver);
    }

    @After
    public void tearDown() {
        if (mDataProvider != null) {
            mDataProvider.destroy();
            verify(mSource).removeObserver(mSourceDataChangedObserver);
        }
    }

    @Test
    @SmallTest
    public void testMainFlow() {
        when(mSource.canUseData()).thenReturn(true);
        when(mSource.getCurrentTimeMs()).thenReturn(CURRENT_TIME_MS);
        // Initial fetch yields TENTATIVE results.
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY1, ENTRY2, ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.TENTATIVE);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertEquals(3, suggestions.size());
                    Assert.assertEquals(ENTRY1, suggestions.get(0));
                    Assert.assertEquals(ENTRY2, suggestions.get(1));
                    Assert.assertEquals(ENTRY3, suggestions.get(2));
                });
        Assert.assertEquals(0, mStatusChangedCallbackCounter);

        // Non-permission data change event triggers dispatch (causing module refresh).
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ false);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        // Fetch now yields STABLE results.
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY2, ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.STABLE);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertEquals(2, suggestions.size());
                    Assert.assertEquals(ENTRY2, suggestions.get(0));
                    Assert.assertEquals(ENTRY3, suggestions.get(1));
                });

        // Results are stable: Subsequent non-permission events do not trigger dispatch.
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ false);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ false);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);

        // However, fetching would still yield updated data. This is useful for forced refresh.
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.STABLE);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertEquals(1, suggestions.size());
                    Assert.assertEquals(ENTRY3, suggestions.get(0));
                });

        // Permission events still triggered dispatches.
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ true);
        Assert.assertEquals(2, mStatusChangedCallbackCounter);
        // Fetch now yields FORCED_NULL results with null suggestions, not actual suggestions.
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY2, ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.FORCED_NULL);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertNull(suggestions);
                });

        mDataProvider.onDataChanged(/* isPermissionUpdate= */ true);
        Assert.assertEquals(3, mStatusChangedCallbackCounter);
    }

    @Test
    @SmallTest
    public void testStabilityFromTimeOut() {
        when(mSource.canUseData()).thenReturn(true);
        when(mSource.getCurrentTimeMs()).thenReturn(CURRENT_TIME_MS);
        // Initial fetch yields TENTATIVE results.
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY1, ENTRY2, ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.TENTATIVE);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertEquals(3, suggestions.size());
                    Assert.assertEquals(ENTRY1, suggestions.get(0));
                    Assert.assertEquals(ENTRY2, suggestions.get(1));
                    Assert.assertEquals(ENTRY3, suggestions.get(2));
                });
        Assert.assertEquals(0, mStatusChangedCallbackCounter);

        // 1 min elapses: Any new suggestions should be discounted.
        when(mSource.getCurrentTimeMs()).thenReturn(CURRENT_TIME_MS + TimeUnit.MINUTES.toMillis(1));

        // Non-permission data change event no longer triggers dispatch.
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ false);
        Assert.assertEquals(0, mStatusChangedCallbackCounter);

        // Results are stable: Subsequent non-permission events do not trigger dispatch.
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ false);
        Assert.assertEquals(0, mStatusChangedCallbackCounter);
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ false);
        Assert.assertEquals(0, mStatusChangedCallbackCounter);

        // Permission events still triggered dispatches.
        mDataProvider.onDataChanged(/* isPermissionUpdate= */ true);
        Assert.assertEquals(1, mStatusChangedCallbackCounter);
        // Fetch now yields FORCED_NULL results with null suggestions, ignoring existing data.
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY2, ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.FORCED_NULL);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertNull(suggestions);
                });

        mDataProvider.onDataChanged(/* isPermissionUpdate= */ true);
        Assert.assertEquals(2, mStatusChangedCallbackCounter);
    }

    @Test
    @SmallTest
    public void testCannotUseData() {
        when(mSource.canUseData()).thenReturn(false);
        when(mSource.getCurrentTimeMs()).thenReturn(CURRENT_TIME_MS);
        plantSourceGetSuggestionsResult(new ArrayList<>(Arrays.asList(ENTRY1, ENTRY2, ENTRY3)));
        fetchSuggestionsAndCheck(
                (SuggestionsResult result) -> {
                    Assert.assertEquals(result.strength, ResultStrength.FORCED_NULL);
                    List<SuggestionEntry> suggestions = result.suggestions;
                    Assert.assertNull(suggestions);
                });
    }

    /**
     * Plants callback-passed results for mSource.getSuggestions(), similar to
     * when(...).thenReturn(...) and less committal than using ArgumentCaptor.
     */
    private void plantSourceGetSuggestionsResult(List<SuggestionEntry> suggestions) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((Callback<List<SuggestionEntry>>) invocation.getArguments()[0])
                                    .onResult(suggestions);
                            return null;
                        })
                .when(mSource)
                .getSuggestions(any(Callback.class));
    }

    /** Calls `mDataProvider.fetchSuggestions` using passed `callback`, whose call is asserted. */
    private void fetchSuggestionsAndCheck(Callback<SuggestionsResult> callback) {
        mFetchSuggestionsAndCheckCallFlag = false;
        // The test setup ensures that the passed lambda is eagerly called.
        mDataProvider.fetchSuggestions(
                (SuggestionsResult result) -> {
                    callback.onResult(result);
                    mFetchSuggestionsAndCheckCallFlag = true;
                });
        assert mFetchSuggestionsAndCheckCallFlag;
    }
}
