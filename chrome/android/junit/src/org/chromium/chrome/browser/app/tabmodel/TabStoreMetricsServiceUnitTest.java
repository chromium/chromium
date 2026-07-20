// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.app.tabmodel.TabStoreMetricsService.MetricsBucket;
import org.chromium.chrome.browser.app.tabmodel.TabStoreMetricsService.WindowMetricsTracker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateNewTabArguments;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator.TabCreationData;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabStoreMetricsService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStoreMetricsServiceUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile1;
    @Mock private Profile mProfile2;
    @Mock private Profile mOriginalProfile1;
    @Mock private Profile mOriginalProfile2;

    @Before
    public void setUp() {
        when(mProfile1.getOriginalProfile()).thenReturn(mOriginalProfile1);
        when(mProfile2.getOriginalProfile()).thenReturn(mOriginalProfile2);
    }

    @Test
    public void testGetForBucket() {
        MetricsBucket bucket1 = new MetricsBucket(mProfile1, "WinTag1", "OrchTag1");
        WindowMetricsTracker tracker1 = TabStoreMetricsService.getForBucket(bucket1);
        assertNotNull("Tracker should not be null", tracker1);

        MetricsBucket bucket1Copy = new MetricsBucket(mProfile1, "WinTag1", "OrchTag1");
        WindowMetricsTracker tracker1Copy = TabStoreMetricsService.getForBucket(bucket1Copy);
        assertSame("Should return same tracker for same bucket", tracker1, tracker1Copy);

        MetricsBucket bucket2 = new MetricsBucket(mProfile1, "WinTag2", "OrchTag1");
        WindowMetricsTracker tracker2 = TabStoreMetricsService.getForBucket(bucket2);
        assertNotNull("Tracker for different window tag should not be null", tracker2);
        assertNotSame(
                "Should return different tracker for different window tag", tracker1, tracker2);

        MetricsBucket bucket3 = new MetricsBucket(mProfile1, "WinTag1", "OrchTag2");
        WindowMetricsTracker tracker3 = TabStoreMetricsService.getForBucket(bucket3);
        assertNotNull("Tracker for different orchestrator tag should not be null", tracker3);
        assertNotSame(
                "Should return different tracker for different orchestrator tag",
                tracker1,
                tracker3);

        MetricsBucket bucket4 = new MetricsBucket(mProfile2, "WinTag1", "OrchTag1");
        WindowMetricsTracker tracker4 = TabStoreMetricsService.getForBucket(bucket4);
        assertNotNull("Tracker for different profile should not be null", tracker4);
        assertNotSame("Should return different tracker for different profile", tracker1, tracker4);
    }

    @Test
    public void testRecordDiffMetrics_Equal() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        List<TabCreationData> authFrozen = new ArrayList<>();
        List<TabCreationData> authNew = new ArrayList<>();
        List<CreateFrozenTabArguments> shadowFrozen = new ArrayList<>();
        List<CreateNewTabArguments> shadowNew = new ArrayList<>();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Tabs.TabStateStore.TabCountDelta.Equal.Tag", true)
                        .build();

        tracker.recordDiffMetrics(authFrozen, authNew, shadowFrozen, shadowNew, true, 0);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordDiffMetrics_AuthoritativeHigher() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        List<TabCreationData> authFrozen = new ArrayList<>();
        authFrozen.add(new TabCreationData(1, "http://url1.com", 1000));
        List<TabCreationData> authNew = new ArrayList<>();
        List<CreateFrozenTabArguments> shadowFrozen = new ArrayList<>();
        List<CreateNewTabArguments> shadowNew = new ArrayList<>();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Tabs.TabStateStore.TabCountDelta.AuthoritativeHigher.Tag", 1)
                        .expectIntRecord("Tabs.TabStateStore.RegularFallbackTabCount", 5)
                        .build();

        tracker.recordDiffMetrics(authFrozen, authNew, shadowFrozen, shadowNew, true, 5);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordDiffMetrics_ShadowHigher() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        List<TabCreationData> authFrozen = new ArrayList<>();
        List<TabCreationData> authNew = new ArrayList<>();
        List<CreateFrozenTabArguments> shadowFrozen = new ArrayList<>();
        List<CreateNewTabArguments> shadowNew = new ArrayList<>();

        CreateNewTabArguments mockArgs = mock(CreateNewTabArguments.class);
        shadowNew.add(mockArgs);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Tabs.TabStateStore.TabCountDelta.ShadowHigher.Tag", 1)
                        .build();

        tracker.recordDiffMetrics(authFrozen, authNew, shadowFrozen, shadowNew, true, 0);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordDiffMetrics_UrlMismatch_AuthoritativeNewer() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        List<TabCreationData> authFrozen = new ArrayList<>();
        authFrozen.add(new TabCreationData(1, "http://url-auth.com", 2000));
        List<TabCreationData> authNew = new ArrayList<>();
        List<CreateFrozenTabArguments> shadowFrozen = new ArrayList<>();
        List<CreateNewTabArguments> shadowNew = new ArrayList<>();

        TabState shadowState = new TabState();
        shadowState.url = new GURL("http://url-shadow.com");
        shadowState.timestampMillis = 1000;

        shadowFrozen.add(new CreateFrozenTabArguments(shadowState, 1, 0));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Tabs.TabStateStore.TabCountDelta.Equal.Tag", true)
                        .expectIntRecord(
                                "Tabs.TabStateStore.TimeDeltaOnMismatch.AuthoritativeNewer.Tag",
                                1000)
                        .build();

        tracker.recordDiffMetrics(authFrozen, authNew, shadowFrozen, shadowNew, true, 0);
        histogramWatcher.assertExpected();
    }
}
