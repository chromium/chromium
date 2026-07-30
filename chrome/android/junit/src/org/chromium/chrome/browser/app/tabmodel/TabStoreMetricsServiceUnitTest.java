// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
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
        authFrozen.add(new TabCreationData(1, "http://url1.com", 1000L, false, null));
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
        authFrozen.add(new TabCreationData(1, "http://url-auth.com", 2000L, false, null));
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

    @Test
    public void testHasCountPref() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        assertFalse(tracker.hasCountPref(TabStoreMetricsService.TAB_COUNT_KEY_SUFFIX));
        assertFalse(tracker.hasCountPref(TabStoreMetricsService.GROUP_COUNT_KEY_SUFFIX));
        assertFalse(tracker.hasCountPref(TabStoreMetricsService.PINNED_TAB_COUNT_KEY_SUFFIX));

        tracker.recordTabCount(5);
        tracker.recordGroupCount(2);
        tracker.recordPinnedTabCount(1);

        assertTrue(tracker.hasCountPref(TabStoreMetricsService.TAB_COUNT_KEY_SUFFIX));
        assertTrue(tracker.hasCountPref(TabStoreMetricsService.GROUP_COUNT_KEY_SUFFIX));
        assertTrue(tracker.hasCountPref(TabStoreMetricsService.PINNED_TAB_COUNT_KEY_SUFFIX));

        tracker.clearTabStoreCounts();

        assertFalse(tracker.hasCountPref(TabStoreMetricsService.TAB_COUNT_KEY_SUFFIX));
        assertFalse(tracker.hasCountPref(TabStoreMetricsService.GROUP_COUNT_KEY_SUFFIX));
        assertFalse(tracker.hasCountPref(TabStoreMetricsService.PINNED_TAB_COUNT_KEY_SUFFIX));
    }

    @Test
    public void testRecordDiffMetrics_GatedDirectCountRecording() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        Token groupId1 = new Token(10L, 20L);
        Token groupId2 = new Token(30L, 40L);

        List<TabCreationData> authFrozen = new ArrayList<>();
        authFrozen.add(
                new TabCreationData(1, "http://url1.com", 1000L, /* isPinned= */ true, groupId1));
        authFrozen.add(
                new TabCreationData(2, "http://url2.com", 2000L, /* isPinned= */ false, groupId1));

        List<TabCreationData> authNew = new ArrayList<>();
        authNew.add(new TabCreationData(3, "http://url3.com", 0L, /* isPinned= */ true, groupId2));

        List<CreateFrozenTabArguments> shadowFrozen = new ArrayList<>();
        List<CreateNewTabArguments> shadowNew = new ArrayList<>();

        // When shadowStoreCaughtUp is false, no counts should be recorded.
        tracker.recordDiffMetrics(
                authFrozen, authNew, shadowFrozen, shadowNew, /* shadowStoreCaughtUp= */ false, 0);
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, tracker.getTabCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, tracker.getGroupCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, tracker.getPinnedTabCount());

        // Initial invocation with uninitialized preferences records counts without emitting delta
        // histograms.
        var initialWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tabs.TabStateStore.TabCountDelta.Positive")
                        .expectNoRecords("Tabs.TabStateStore.GroupCountDelta.Positive")
                        .expectNoRecords("Tabs.TabStateStore.PinnedTabCountDelta.Positive")
                        .build();
        tracker.recordDiffMetrics(
                authFrozen, authNew, shadowFrozen, shadowNew, /* shadowStoreCaughtUp= */ true, 0);
        assertEquals(3, tracker.getTabCount());
        assertEquals(2, tracker.getGroupCount());
        assertEquals(2, tracker.getPinnedTabCount());
        initialWatcher.assertExpected();

        // Subsequent invocation with count changes DOES emit expected delta histograms.
        Token groupId3 = new Token(50L, 60L);
        authNew.add(new TabCreationData(4, "http://url4.com", 0L, /* isPinned= */ true, groupId3));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Tabs.TabStateStore.TabCountDelta.Positive", 1)
                        .expectIntRecord("Tabs.TabStateStore.GroupCountDelta.Positive", 1)
                        .expectIntRecord("Tabs.TabStateStore.PinnedTabCountDelta.Positive", 1)
                        .build();
        tracker.recordDiffMetrics(
                authFrozen, authNew, shadowFrozen, shadowNew, /* shadowStoreCaughtUp= */ true, 0);
        assertEquals(4, tracker.getTabCount());
        assertEquals(3, tracker.getGroupCount());
        assertEquals(3, tracker.getPinnedTabCount());
        histogramWatcher.assertExpected();

        // Verify clearing tab store counts resets stored metrics.
        tracker.clearTabStoreCounts();
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, tracker.getTabCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, tracker.getGroupCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, tracker.getPinnedTabCount());
    }

    @Test
    public void testRecordDiffMetrics_NewlyMigratedUser_NoDeltaSpike() {
        WindowMetricsTracker tracker =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "WinTag", "Tag"));

        List<TabCreationData> authFrozen = new ArrayList<>();
        authFrozen.add(new TabCreationData(1, "http://url1.com", 1000L, false, null));
        authFrozen.add(new TabCreationData(2, "http://url2.com", 2000L, false, null));
        List<TabCreationData> authNew = new ArrayList<>();
        List<CreateFrozenTabArguments> shadowFrozen = new ArrayList<>();
        List<CreateNewTabArguments> shadowNew = new ArrayList<>();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tabs.TabStateStore.TabCountDelta.Positive")
                        .expectNoRecords("Tabs.TabStateStore.GroupCountDelta.Positive")
                        .expectNoRecords("Tabs.TabStateStore.PinnedTabCountDelta.Positive")
                        .expectNoRecords("Tabs.TabStateStore.TabCountDelta.Negative")
                        .expectNoRecords("Tabs.TabStateStore.GroupCountDelta.Negative")
                        .expectNoRecords("Tabs.TabStateStore.PinnedTabCountDelta.Negative")
                        .build();

        tracker.recordDiffMetrics(
                authFrozen, authNew, shadowFrozen, shadowNew, /* shadowStoreCaughtUp= */ true, 0);

        assertEquals(2, tracker.getTabCount());
        assertEquals(0, tracker.getGroupCount());
        assertEquals(0, tracker.getPinnedTabCount());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testClearTabStoreCounts() {
        when(mProfile1.isOffTheRecord()).thenReturn(false);
        when(mProfile2.isOffTheRecord()).thenReturn(true);

        WindowMetricsTracker trackerReg0 =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "0", "TabGroup"));
        WindowMetricsTracker trackerReg1 =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile1, "1", "TabGroup"));
        WindowMetricsTracker trackerInc0 =
                TabStoreMetricsService.getForBucket(new MetricsBucket(mProfile2, "0", "TabGroup"));

        trackerReg0.recordTabCount(10);
        trackerReg0.recordGroupCount(5);
        trackerReg0.recordPinnedTabCount(2);

        trackerReg1.recordTabCount(10);
        trackerReg1.recordGroupCount(5);
        trackerReg1.recordPinnedTabCount(2);

        trackerInc0.recordTabCount(10);
        trackerInc0.recordGroupCount(5);
        trackerInc0.recordPinnedTabCount(2);

        // 1. Clear window 0 for regular profile (mProfile1).
        TabStoreMetricsService.clearTabStoreCountsForProfileAndWindow(mProfile1, "0");
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg0.getTabCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg0.getGroupCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg0.getPinnedTabCount());

        // Ensure regular window 1 and incognito window 0 remain untouched.
        assertEquals(10, trackerReg1.getTabCount());
        assertEquals(5, trackerReg1.getGroupCount());
        assertEquals(2, trackerReg1.getPinnedTabCount());
        assertEquals(10, trackerInc0.getTabCount());
        assertEquals(5, trackerInc0.getGroupCount());
        assertEquals(2, trackerInc0.getPinnedTabCount());

        // 2. Clear via WindowMetricsTracker instance.
        trackerReg1.clearTabStoreCounts();
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg1.getTabCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg1.getGroupCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg1.getPinnedTabCount());

        // Ensure incognito window 0 remains untouched.
        assertEquals(10, trackerInc0.getTabCount());
        assertEquals(5, trackerInc0.getGroupCount());
        assertEquals(2, trackerInc0.getPinnedTabCount());

        // 3. Clear all tab store counts across all profiles and windows.
        trackerReg1.recordTabCount(10);
        TabStoreMetricsService.clearAllTabStoreCounts();
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerReg1.getTabCount());
        assertEquals(WindowMetricsTracker.NO_COUNT_PREF, trackerInc0.getTabCount());
    }
}
