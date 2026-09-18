// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService.LoadIfNeededCallback;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService.LoadResult;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabLoadingService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabLoadingServiceTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private LoadIfNeededCallback mCallback;
    @Mock private LoadIfNeededCallback mCallback3;
    @Mock private LoadIfNeededCallback mSecondCallback;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private TabLoadingService mService;
    private static final int TAB_ID = 123;
    private static final int TAB_ID_2 = 124;
    private static final int TAB_ID_3 = 125;

    // The optimization params all default to false, so each test opts into the one it exercises.
    private static final String OPTIMIZATION_LIMIT_LOADS =
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":limit_concurrent_load_if_needed/true";
    private static final String OPTIMIZATION_FIRST_PAINT =
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":enable_first_paint/true";
    private static final String OPTIMIZATION_FIRST_PAINT_DELAYED =
            OPTIMIZATION_FIRST_PAINT + "/first_paint_delay_ms/500";

    @Before
    public void setUp() {
        when(mTab.getId()).thenReturn(TAB_ID);
        mService = new TabLoadingService();
        TabLoadingService.setInstanceForTesting(mService);
    }

    @After
    public void tearDown() {
        TabLoadingService.setInstanceForTesting(null);
    }

    @Test
    public void testQueueLoadIfNeeded_AlreadyLoaded() {
        when(mTab.loadIfNeeded(true)).thenReturn(false);
        assertFalse(mService.queueLoadIfNeeded(mTab));
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));

        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(false);
        assertFalse(mService.queueLoadIfNeeded(mTab));
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    public void testQueueLoadIfNeeded_Success() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.isTabQueuedForLoad(TAB_ID));
        verify(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    public void testQueueLoadIfNeeded_AlreadyQueued() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.queueLoadIfNeeded(mTab));
        // Should only add observer once
        verify(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    public void testAddAndRemoveCallback() {
        // When not queued, adding/removing callback should return false
        assertFalse(mService.addLoadIfNeededCallback(mTab, mCallback));
        assertFalse(mService.removeLoadIfNeededCallback(mTab, mCallback));

        // Queue the tab
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);
        assertTrue(mService.queueLoadIfNeeded(mTab));

        // Now adding/removing callback should succeed
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));
        assertTrue(mService.removeLoadIfNeededCallback(mTab, mCallback));
    }

    @Test
    public void testOnPageLoadFinished_Success() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);

        verify(mCallback).onLoadFinished(mTab, LoadResult.SUCCESS);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    public void testOnPageLoadFailed() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onPageLoadFailed(mTab, 404);

        verify(mCallback).onLoadFinished(mTab, LoadResult.FAILURE);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    public void testOnCrash() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onCrash(mTab);

        verify(mCallback).onLoadFinished(mTab, LoadResult.CRASH);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    public void testOnDestroyed() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onDestroyed(mTab);

        verify(mCallback).onLoadFinished(mTab, LoadResult.DESTROYED);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_FIRST_PAINT)
    public void testDidFirstVisuallyNonEmptyPaint_FeatureEnabled() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(mTab);

        verify(mCallback).onLoadFinished(mTab, LoadResult.SUCCESS);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION)
    public void testDidFirstVisuallyNonEmptyPaint_FeatureDisabled() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(mTab);

        verify(mCallback, never()).onLoadFinished(mTab, LoadResult.SUCCESS);
        assertTrue(mService.isTabQueuedForLoad(TAB_ID));

        observer.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);

        verify(mCallback).onLoadFinished(mTab, LoadResult.SUCCESS);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":enable_first_paint/false")
    public void
            testDidFirstVisuallyNonEmptyPaint_EnableFirstPaintFalse_FallsBackToPageLoadFinished() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(mTab);

        verify(mCallback, never()).onLoadFinished(mTab, LoadResult.SUCCESS);
        assertTrue(mService.isTabQueuedForLoad(TAB_ID));

        observer.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);

        verify(mCallback).onLoadFinished(mTab, LoadResult.SUCCESS);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_FIRST_PAINT_DELAYED)
    public void testDidFirstVisuallyNonEmptyPaint_WithDelay() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(mTab);

        // Before advancing looper tasks, the callback should not yet have been invoked.
        verify(mCallback, never()).onLoadFinished(mTab, LoadResult.SUCCESS);
        assertTrue(mService.isTabQueuedForLoad(TAB_ID));

        // Advance looper to run delayed task.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mCallback).onLoadFinished(mTab, LoadResult.SUCCESS);
        verify(mTab).removeObserver(observer);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_FIRST_PAINT_DELAYED)
    public void testDidFirstVisuallyNonEmptyPaint_FailureDuringDelayDoesNotResolveSuccess() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(mTab);
        verify(mCallback, never()).onLoadFinished(any(), anyInt());

        observer.onPageLoadFailed(mTab, 500);
        verify(mCallback).onLoadFinished(mTab, LoadResult.FAILURE);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mCallback, times(1)).onLoadFinished(any(), anyInt());
    }

    @Test
    @EnableFeatures(OPTIMIZATION_FIRST_PAINT_DELAYED)
    public void testDidFirstVisuallyNonEmptyPaint_RequeuedDuringDelay_DispatchesOnlyNewLoad() {
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mCallback));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.didFirstVisuallyNonEmptyPaint(mTab);

        // Tab fails and completes first load.
        observer.onPageLoadFailed(mTab, 500);
        verify(mCallback).onLoadFinished(mTab, LoadResult.FAILURE);

        // Reset and re-queue tab for a second load attempt.
        clearInvocations(mTab);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, mSecondCallback));

        // When delayed task from first generation runs, it should NOT prematurely resolve second
        // load.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSecondCallback, never()).onLoadFinished(any(), anyInt());
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testConcurrentLoadLimit_QueuesExcessTabs() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        setupTabForLoad(mTab3, TAB_ID_3);

        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.queueLoadIfNeeded(mTab2));
        assertTrue(mService.queueLoadIfNeeded(mTab3));

        verify(mTab).loadIfNeeded(true);
        verify(mTab2).loadIfNeeded(true);
        verify(mTab3, never()).loadIfNeeded(true);
        assertTrue(mService.isTabQueuedForLoad(TAB_ID_3));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testConcurrentLoadLimit_DrainsQueueOnCompletion() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        setupTabForLoad(mTab3, TAB_ID_3);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();
        observer.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);

        verify(mTab3).loadIfNeeded(true);
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testConcurrentLoadLimit_PendingTabDestroyedBeforeLoad() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);
        when(mTab3.isFrozen()).thenReturn(true);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);
        mService.addLoadIfNeededCallback(mTab3, mCallback3);

        verify(mTab3).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onDestroyed(mTab3);

        verify(mCallback3).onLoadFinished(mTab3, LoadResult.DESTROYED);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID_3));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_ActivelyLoadingTab() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        setupTabForLoad(mTab3, TAB_ID_3);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);

        assertTrue(mService.cancelLoadIfNeeded(mTab));
        verify(mTab).stopLoading();
        verify(mNavigationController).setNeedsReload();
        verify(mTab3).loadIfNeeded(true);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_PendingTab() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);
        when(mTab3.isFrozen()).thenReturn(true);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);

        assertTrue(mService.cancelLoadIfNeeded(mTab3));
        verify(mTab3, never()).stopLoading();
        assertFalse(mService.isTabQueuedForLoad(TAB_ID_3));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verify(mTab3, never()).loadIfNeeded(true);
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_PendingTabRemovesObserver() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);
        when(mTab3.isFrozen()).thenReturn(true);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);
        verify(mTab3).addObserver(any(TabObserver.class));

        assertTrue(mService.cancelLoadIfNeeded(mTab3));
        verify(mTab3).removeObserver(any(TabObserver.class));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_NotifiesCallbacks() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        mService.queueLoadIfNeeded(mTab);
        mService.addLoadIfNeededCallback(mTab, mCallback);

        assertTrue(mService.cancelLoadIfNeeded(mTab));

        verify(mCallback).onLoadFinished(mTab, LoadResult.CANCELLED);
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_DestroyedTabSkipsStopLoading() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        mService.queueLoadIfNeeded(mTab);
        when(mTab.isDestroyed()).thenReturn(true);

        assertTrue(mService.cancelLoadIfNeeded(mTab));

        verify(mTab, never()).stopLoading();
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_DestroyedTabStillSchedulesPendingTab() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        setupTabForLoad(mTab3, TAB_ID_3);
        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);
        when(mTab.isDestroyed()).thenReturn(true);

        assertTrue(mService.cancelLoadIfNeeded(mTab));

        verify(mTab3).loadIfNeeded(true);
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_DestroyedWebContentsSkipsNeedsReload() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.isDestroyed()).thenReturn(true);
        mService.queueLoadIfNeeded(mTab);

        assertTrue(mService.cancelLoadIfNeeded(mTab));

        verify(mNavigationController, never()).setNeedsReload();
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_ActivatedTabSkipsStopLoading() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        when(mTab.isActivated()).thenReturn(true);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);

        assertTrue(mService.cancelLoadIfNeeded(mTab));

        verify(mTab, never()).stopLoading();
        verify(mNavigationController, never()).setNeedsReload();
        verify(mTab2).loadIfNeeded(true);
        assertFalse(mService.isTabQueuedForLoad(TAB_ID));
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testConcurrentLoadLimit_RecordsQueueDepth() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        setupTabForLoad(mTab3, TAB_ID_3);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.TabLoadingService.PendingQueueDepth", 0)
                        .expectIntRecord("Android.TabLoadingService.PendingQueueDepth", 0)
                        .expectIntRecord("Android.TabLoadingService.PendingQueueDepth", 1)
                        .build();

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testConcurrentLoadLimit_RecordsQueueWaitDurationOnDrain() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        setupTabForLoad(mTab3, TAB_ID_3);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.TabLoadingService.QueueWaitDuration")
                        .build();

        observer.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(OPTIMIZATION_LIMIT_LOADS)
    public void testCancelLoadIfNeeded_PendingTab_DoesNotRecordQueueWaitDuration() {
        configureConcurrentServiceWithMemoryGb(2);
        setupTabForLoad(mTab, TAB_ID);
        setupTabForLoad(mTab2, TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);
        when(mTab3.isFrozen()).thenReturn(true);

        mService.queueLoadIfNeeded(mTab);
        mService.queueLoadIfNeeded(mTab2);
        mService.queueLoadIfNeeded(mTab3);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.TabLoadingService.QueueWaitDuration")
                        .build();

        mService.cancelLoadIfNeeded(mTab3);

        watcher.assertExpected();
    }

    private void setupTabForLoad(Tab tab, int id) {
        when(tab.getId()).thenReturn(id);
        when(tab.loadIfNeeded(true)).thenReturn(true);
        when(tab.isLoading()).thenReturn(true);
    }

    private void configureConcurrentServiceWithMemoryGb(int gb) {
        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                gb * OnDemandBackgroundTabCaptureConfig.KILOBYTES_PER_GIGABYTE);
        TabLoadingService service = new TabLoadingService();
        TabLoadingService.setInstanceForTesting(service);
        mService = service;
    }
}
