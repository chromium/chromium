// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsTracker.PER_GROUP_HISTOGRAM_PREFIX;
import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsTracker.TOTAL_HISTOGRAM_PREFIX;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService.GroupCreationSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link SuggestionMetricsTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionMetricsTrackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTabWithNoGroup;

    @Captor private ArgumentCaptor<Callback<Tab>> mTabObserverCaptor;

    @Spy
    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();

    private SuggestionMetricsTracker mSuggestionMetricsTracker;

    private final Token mGtsGroupId = new Token(1, 1);
    private final Token mCpaGroupId = new Token(2, 2);
    private final Token mUnknownGroupId = new Token(3, 3);

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);

        mSuggestionMetricsTracker = new SuggestionMetricsTracker(mTabModelSelector);
        verify(mCurrentTabSupplier).addObserver(mTabObserverCaptor.capture());

        when(mTab1.getTabGroupId()).thenReturn(mGtsGroupId);
        when(mTab2.getTabGroupId()).thenReturn(mCpaGroupId);
        when(mTab3.getTabGroupId()).thenReturn(mUnknownGroupId);
        when(mTabWithNoGroup.getTabGroupId()).thenReturn(null);
    }

    @Test
    public void testOnSuggestionAccepted_GTS() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "GTS", 2)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "GTS", 2)
                        .build();
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);
        selectTab(mTab1);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testOnSuggestionAccepted_CPA() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "CPA", 3)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "CPA", 3)
                        .build();
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.CPA_SUGGESTION, mCpaGroupId);
        selectTab(mTab2);
        selectTab(mTab2);
        selectTab(mTab2);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testUnknownSource() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "Unknown", 2)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "Unknown", 2)
                        .build();
        selectTab(mTab3);
        selectTab(mTab3);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testMultipleSources() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "GTS", 2)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "GTS", 2)
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "CPA", 2)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "CPA", 2)
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "Unknown", 2)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "Unknown", 2)
                        .build();

        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.CPA_SUGGESTION, mCpaGroupId);

        selectTab(mTab1); // GTS
        selectTab(mTab2); // CPA
        selectTab(mTab1); // GTS
        selectTab(mTab3); // Unknown
        selectTab(mTab2); // CPA
        selectTab(mTab3); // Unknown

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testNoGroupId() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(TOTAL_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(TOTAL_HISTOGRAM_PREFIX + "CPA")
                        .expectNoRecords(TOTAL_HISTOGRAM_PREFIX + "Unknown")
                        .expectNoRecords(PER_GROUP_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(PER_GROUP_HISTOGRAM_PREFIX + "CPA")
                        .expectNoRecords(PER_GROUP_HISTOGRAM_PREFIX + "Unknown")
                        .build();
        selectTab(mTabWithNoGroup);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testRecordMetrics() {
        HistogramWatcher watcher1 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_HISTOGRAM_PREFIX + "GTS", 1)
                        .expectIntRecord(PER_GROUP_HISTOGRAM_PREFIX + "GTS", 1)
                        .build();
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);

        mSuggestionMetricsTracker.recordMetrics();
        watcher1.assertExpected();

        HistogramWatcher watcher2 =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(TOTAL_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(PER_GROUP_HISTOGRAM_PREFIX + "GTS")
                        .build();
        mSuggestionMetricsTracker.recordMetrics();
        watcher2.assertExpected();
    }

    @Test
    public void testReset() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(TOTAL_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(PER_GROUP_HISTOGRAM_PREFIX + "GTS")
                        .build();
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);

        mSuggestionMetricsTracker.reset();

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testDestroy() {
        mSuggestionMetricsTracker.destroy();
        verify(mCurrentTabSupplier).removeObserver(mTabObserverCaptor.getValue());
    }

    private void selectTab(Tab tab) {
        mTabObserverCaptor.getValue().onResult(tab);
    }
}
