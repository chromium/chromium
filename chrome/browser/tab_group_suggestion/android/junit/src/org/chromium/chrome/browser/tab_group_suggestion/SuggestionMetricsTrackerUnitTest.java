// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsTracker.PER_GROUP_SWITCHES_HISTOGRAM_PREFIX;
import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsTracker.PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX;
import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsTracker.TOTAL_SWITCHES_HISTOGRAM_PREFIX;
import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsTracker.TOTAL_TIME_SPENT_HISTOGRAM_PREFIX;

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
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService.GroupCreationSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.concurrent.TimeUnit;

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

    private final Token mGtsGroupId = new Token(1, 1);
    private final Token mCpaGroupId = new Token(2, 2);
    private final Token mUnknownGroupId = new Token(3, 3);

    private SuggestionMetricsTracker mSuggestionMetricsTracker;

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
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS", 2)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "GTS", 2)
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
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "CPA", 3)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "CPA", 3)
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
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "Unknown", 2)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "Unknown", 2)
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
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS", 2)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "GTS", 2)
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "CPA", 2)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "CPA", 2)
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "Unknown", 2)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "Unknown", 2)
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
                        .expectNoRecords(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "CPA")
                        .expectNoRecords(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "Unknown")
                        .expectNoRecords(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "CPA")
                        .expectNoRecords(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "Unknown")
                        .build();
        selectTab(mTabWithNoGroup);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testRecordMetrics() {
        HistogramWatcher watcher1 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS", 1)
                        .expectIntRecord(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "GTS", 1)
                        .build();
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);

        mSuggestionMetricsTracker.recordMetrics();
        watcher1.assertExpected();

        HistogramWatcher watcher2 =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .build();
        mSuggestionMetricsTracker.recordMetrics();
        watcher2.assertExpected();
    }

    @Test
    public void testReset() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .expectNoRecords(PER_GROUP_SWITCHES_HISTOGRAM_PREFIX + "GTS")
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
        assertFalse(mCurrentTabSupplier.hasObservers());
    }

    @Test
    public void testRecordMetrics_TimeSpent() {
        int gtsTime1 = 100;
        int gtsTime2 = 200;
        int cpaTime1 = 300;
        int unknownTime1 = 400;
        int noGroupTime = 500;

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime1 + gtsTime2)
                        .expectIntRecord(
                                PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime1 + gtsTime2)
                        .expectIntRecord(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "CPA", cpaTime1)
                        .expectIntRecord(PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "CPA", cpaTime1)
                        .expectIntRecord(
                                TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "Unknown", unknownTime1)
                        .expectIntRecord(
                                PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "Unknown", unknownTime1)
                        .build();

        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.CPA_SUGGESTION, mCpaGroupId);

        selectTab(mTab1); // GTS
        ShadowSystemClock.advanceBy(gtsTime1, TimeUnit.MILLISECONDS);

        selectTab(mTab2); // CPA
        ShadowSystemClock.advanceBy(cpaTime1, TimeUnit.MILLISECONDS);

        selectTab(mTab1); // GTS
        ShadowSystemClock.advanceBy(gtsTime2, TimeUnit.MILLISECONDS);

        selectTab(mTab3); // Unknown
        ShadowSystemClock.advanceBy(unknownTime1, TimeUnit.MILLISECONDS);

        selectTab(mTabWithNoGroup); // No group
        ShadowSystemClock.advanceBy(noGroupTime, TimeUnit.MILLISECONDS);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testTimeSpent_AttributedToCorrectGroup() {
        int gtsTime = 100;
        int cpaTime = 200;

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime)
                        .expectIntRecord(PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime)
                        .expectIntRecord(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "CPA", cpaTime)
                        .expectIntRecord(PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "CPA", cpaTime)
                        .build();

        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.CPA_SUGGESTION, mCpaGroupId);

        selectTab(mTab1); // GTS
        ShadowSystemClock.advanceBy(gtsTime, TimeUnit.MILLISECONDS);

        selectTab(mTab2); // CPA
        ShadowSystemClock.advanceBy(cpaTime, TimeUnit.MILLISECONDS);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testTimeSpent_FromGroupToNoGroup() {
        int gtsTime = 100;
        int noGroupTime = 50;

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime)
                        .expectIntRecord(PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime)
                        .expectNoRecords(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "Unknown")
                        .build();

        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);

        selectTab(mTab1); // GTS
        ShadowSystemClock.advanceBy(gtsTime, TimeUnit.MILLISECONDS);

        selectTab(mTabWithNoGroup); // Unknown
        ShadowSystemClock.advanceBy(noGroupTime, TimeUnit.MILLISECONDS);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    @Test
    public void testTimeSpent_FromNoGroupToGroup() {
        int noGroupTime = 50;
        int gtsTime = 100;

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime)
                        .expectIntRecord(PER_GROUP_TIME_SPENT_HISTOGRAM_PREFIX + "GTS", gtsTime)
                        .expectNoRecords(TOTAL_TIME_SPENT_HISTOGRAM_PREFIX + "Unknown")
                        .build();

        mSuggestionMetricsTracker.onSuggestionAccepted(
                GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);

        selectTab(mTabWithNoGroup); // Unknown
        ShadowSystemClock.advanceBy(noGroupTime, TimeUnit.MILLISECONDS);

        selectTab(mTab1); // GTS
        ShadowSystemClock.advanceBy(gtsTime, TimeUnit.MILLISECONDS);

        mSuggestionMetricsTracker.recordMetrics();
        watcher.assertExpected();
    }

    private void selectTab(Tab tab) {
        when(mCurrentTabSupplier.get()).thenReturn(tab);
        mTabObserverCaptor.getValue().onResult(tab);
    }
}
