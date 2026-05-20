// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
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
}
