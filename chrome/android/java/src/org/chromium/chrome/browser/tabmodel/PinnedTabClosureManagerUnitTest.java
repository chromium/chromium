// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link PinnedTabClosureManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PinnedTabClosureManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mSelector;
    @Mock private Tab mTab;
    private Context mContext;
    @Spy private PinnedTabClosureManager mPinnedTabClosureManager;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        mPinnedTabClosureManager = spy(PinnedTabClosureManagerFactory.getInstance());
        when(mTab.getId()).thenReturn(1);
        when(mTab.getIsPinned()).thenReturn(true);
        when(mTab.getContext()).thenReturn(mContext);
    }

    @After
    public void tearDown() {
        mPinnedTabClosureManager.clearPendingState(mSelector);
    }

    @Test
    public void testCloseTabByKeyboardShortcut_tabShouldClose() {
        when(mTab.getIsPinned()).thenReturn(false);

        // Verify unpinned tab should close.
        assertTrue(
                mPinnedTabClosureManager.shouldCloseTab(mSelector, mTab, /* isBulkClose= */ false));

        // Verify no entry in the pending tabs map.
        HashMap<TabModelSelector, Integer> pendingPinnedTabs =
                mPinnedTabClosureManager.getPendingPinnedTabsForTesting();
        assertEquals(0, pendingPinnedTabs.size());

        // Verify pending state is cleared.
        verify(mPinnedTabClosureManager).clearPendingState(mSelector);
    }

    @Test
    public void testClosePinnedTabByKeyboardShortcut_firstAttempt_tabShouldNotClose() {
        // Verify first close attempt should not close the pinned tab.
        assertFalse(
                mPinnedTabClosureManager.shouldCloseTab(mSelector, mTab, /* isBulkClose= */ false));

        HashMap<TabModelSelector, Integer> pendingPinnedTabs =
                mPinnedTabClosureManager.getPendingPinnedTabsForTesting();

        // Verify one entry in the pending tabs map.
        assertEquals(1, pendingPinnedTabs.size());

        // Verify the tab ID in the map matches.
        assertEquals(1, (int) pendingPinnedTabs.getOrDefault(mSelector, -1));

        // Verify toast is showing.
        verify(mPinnedTabClosureManager).showToast(any());
    }

    @Test
    public void testClosePinnedTabByKeyboardShortcut_timeout_pendingStateCleared() {
        // Verify first close attempt should not close the pinned tab.
        assertFalse(
                mPinnedTabClosureManager.shouldCloseTab(mSelector, mTab, /* isBulkClose= */ false));

        // Verify one entry in the Pending tabs map.
        HashMap<TabModelSelector, Integer> pendingPinnedTabs =
                mPinnedTabClosureManager.getPendingPinnedTabsForTesting();
        assertEquals(1, pendingPinnedTabs.size());

        // Advance time by exactly 4 seconds.
        ShadowLooper.idleMainLooper(4000, TimeUnit.MILLISECONDS);

        // Verify no entry in the pending tabs map.
        pendingPinnedTabs = mPinnedTabClosureManager.getPendingPinnedTabsForTesting();
        assertEquals(0, pendingPinnedTabs.size());

        // Verify pending state is cleared.
        verify(mPinnedTabClosureManager).clearPendingState(mSelector);
    }

    @Test
    public void testClosePinnedTabByKeyboardShortcut_secondAttempt_tabShouldClose() {
        // Verify first close attempt should not close the pinned tab.
        assertFalse(
                mPinnedTabClosureManager.shouldCloseTab(mSelector, mTab, /* isBulkClose= */ false));

        // Verify second close attempt should close the pinned tab.
        assertTrue(
                mPinnedTabClosureManager.shouldCloseTab(mSelector, mTab, /* isBulkClose= */ false));

        // Verify no entry in the pending tabs map.
        HashMap<TabModelSelector, Integer> pendingPinnedTabs =
                mPinnedTabClosureManager.getPendingPinnedTabsForTesting();
        assertEquals(0, pendingPinnedTabs.size());

        // Verify pending state is cleared.
        verify(mPinnedTabClosureManager).clearPendingState(mSelector);
    }

    @Test
    public void testClosePinnedTabByKeyboardShortcut_multiselect_tabShouldClose() {
        // Verify first close attempt should close bulk tabs.
        assertTrue(
                mPinnedTabClosureManager.shouldCloseTab(mSelector, mTab, /* isBulkClose= */ true));

        // Verify no entry in the pending tabs map.
        HashMap<TabModelSelector, Integer> pendingPinnedTabs =
                mPinnedTabClosureManager.getPendingPinnedTabsForTesting();
        assertEquals(0, pendingPinnedTabs.size());

        // Verify pending state is cleared.
        verify(mPinnedTabClosureManager).clearPendingState(mSelector);
    }
}
