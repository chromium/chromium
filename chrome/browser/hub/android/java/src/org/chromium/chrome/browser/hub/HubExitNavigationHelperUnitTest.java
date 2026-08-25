// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;

@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class HubExitNavigationHelperUnitTest {
    private static class TestRunnable implements Runnable {
        private int mCallCount;

        @Override
        public void run() {
            mCallCount++;
        }

        public int getCallCount() {
            return mCallCount;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private HubManager mHubManager;
    @Mock private Tab mTab;

    private final TestRunnable mAction = new TestRunnable();
    private HubExitNavigationHelper mHelper;

    @Before
    public void setUp() {
        lenient().when(mTab.getId()).thenReturn(1);
        mHelper = new HubExitNavigationHelper(mLayoutStateProvider, mHubManager);
    }

    @Test
    public void testRunOrDefer_hubNotVisible() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(false);
        mHelper.runOrDefer(mTab, mAction);
        assertEquals(1, mAction.getCallCount());
        verify(mHubManager, never()).selectTabAndHideHub(anyInt());
    }

    @Test
    public void testRunOrDefer_hubVisible() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        mHelper.runOrDefer(mTab, mAction);
        assertEquals(0, mAction.getCallCount());
        verify(mHubManager).selectTabAndHideHub(1);

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        // Assert deferred completion via onFinishedHiding
        captor.getValue().onFinishedHiding(LayoutType.HUB);
        assertEquals(1, mAction.getCallCount());
    }

    @Test
    public void testReentrancyProtection() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        mHelper.runOrDefer(mTab, mAction);
        // Second click
        TestRunnable secondAction = new TestRunnable();
        mHelper.runOrDefer(mTab, secondAction);

        // Since we explicitly added LIFO overwriting, the second action will be cached.
        verify(mLayoutStateProvider, times(1)).addObserver(any(HubExitNavigationHelper.class));
        verify(mHubManager, times(1)).selectTabAndHideHub(1);

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        captor.getValue().onFinishedHiding(LayoutType.HUB);
        assertEquals(0, mAction.getCallCount());
        assertEquals(1, secondAction.getCallCount());
    }

    @Test
    public void testOnFinishedHiding_differentLayout() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        mHelper.runOrDefer(mTab, mAction);

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        captor.getValue().onFinishedHiding(LayoutType.BROWSING);
        assertEquals(0, mAction.getCallCount());
    }

    @Test
    public void testDestroy_idempotent() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        mHelper.runOrDefer(mTab, mAction);

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        mHelper.destroy();
        mHelper.destroy();

        verify(mLayoutStateProvider, times(2)).removeObserver(mHelper);

        // If we fire onFinishedHiding now, action should not run because it was nulled out.
        captor.getValue().onFinishedHiding(LayoutType.HUB);
        assertEquals(0, mAction.getCallCount());
    }

    @Test
    public void testReentrantActionExecution() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        TestRunnable secondAction = new TestRunnable();
        Runnable reentrantAction =
                () -> {
                    when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(false);
                    mHelper.runOrDefer(mTab, secondAction);
                };

        mHelper.runOrDefer(mTab, reentrantAction);

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        captor.getValue().onFinishedHiding(LayoutType.HUB);
        assertEquals(1, secondAction.getCallCount());
    }

    @Test
    public void testRunOrDefer_hubAlreadyStartingToHide() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);
        when(mLayoutStateProvider.isLayoutStartingToHide(LayoutType.HUB)).thenReturn(true);

        mHelper.runOrDefer(mTab, mAction);

        // Observer added and action deferred, but hide is NOT re-triggered.
        assertEquals(0, mAction.getCallCount());
        verify(mHubManager, never()).selectTabAndHideHub(anyInt());

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        captor.getValue().onFinishedHiding(LayoutType.HUB);
        assertEquals(1, mAction.getCallCount());
    }

    @Test
    public void testRapidConsecutiveClicks_latestIntentCoalescing() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);
        when(mLayoutStateProvider.isLayoutStartingToHide(LayoutType.HUB)).thenReturn(false);

        TestRunnable firstAction = new TestRunnable();
        TestRunnable secondAction = new TestRunnable();
        TestRunnable thirdAction = new TestRunnable();

        mHelper.runOrDefer(mTab, firstAction);
        mHelper.runOrDefer(mTab, secondAction);
        mHelper.runOrDefer(mTab, thirdAction);

        verify(mHubManager, times(1)).selectTabAndHideHub(1);
        verify(mLayoutStateProvider, times(1)).addObserver(any(HubExitNavigationHelper.class));

        ArgumentCaptor<HubExitNavigationHelper> captor =
                ArgumentCaptor.forClass(HubExitNavigationHelper.class);
        verify(mLayoutStateProvider).addObserver(captor.capture());

        captor.getValue().onFinishedHiding(LayoutType.HUB);
        assertEquals(0, firstAction.getCallCount());
        assertEquals(0, secondAction.getCallCount());
        assertEquals(1, thirdAction.getCallCount());
    }

    @Test
    public void testZombieStateCleanup_hubDismissedWithoutOnFinishedHiding() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(true);

        mHelper.runOrDefer(mTab, mAction);
        verify(mLayoutStateProvider).addObserver(any(HubExitNavigationHelper.class));

        // Hub dismissed externally without firing onFinishedHiding(HUB)
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)).thenReturn(false);

        TestRunnable newAction = new TestRunnable();
        mHelper.runOrDefer(mTab, newAction);

        // Observer should be cleaned up and newAction run immediately
        verify(mLayoutStateProvider).removeObserver(mHelper);
        assertEquals(1, newAction.getCallCount());
    }

    @Test
    public void testPostDestroyInvocations() {
        mHelper.destroy();
        mHelper.runOrDefer(mTab, mAction);

        assertEquals(0, mAction.getCallCount());
        verify(mHubManager, never()).selectTabAndHideHub(anyInt());
        verify(mLayoutStateProvider, never()).addObserver(any());
    }
}
