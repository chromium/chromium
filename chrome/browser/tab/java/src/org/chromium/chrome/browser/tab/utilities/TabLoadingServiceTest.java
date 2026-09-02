// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService.LoadIfNeededCallback;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService.LoadResult;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabLoadingService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabLoadingServiceTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private LoadIfNeededCallback mCallback;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private TabLoadingService mService;
    private static final int TAB_ID = 123;

    @Before
    public void setUp() {
        when(mTab.getId()).thenReturn(TAB_ID);
        mService = TabLoadingService.getInstance();
        mService.clearForTesting();
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
    @EnableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION)
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
    @EnableFeatures(
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":first_paint_delay_ms/500")
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
    @EnableFeatures(
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":first_paint_delay_ms/500")
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
    @EnableFeatures(
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":first_paint_delay_ms/500")
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
        reset(mTab);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.loadIfNeeded(true)).thenReturn(true);
        when(mTab.isLoading()).thenReturn(true);

        LoadIfNeededCallback secondCallback = Mockito.mock(LoadIfNeededCallback.class);
        assertTrue(mService.queueLoadIfNeeded(mTab));
        assertTrue(mService.addLoadIfNeededCallback(mTab, secondCallback));

        // When delayed task from first generation runs, it should NOT prematurely resolve second
        // load.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(secondCallback, never()).onLoadFinished(any(), anyInt());
    }
}
