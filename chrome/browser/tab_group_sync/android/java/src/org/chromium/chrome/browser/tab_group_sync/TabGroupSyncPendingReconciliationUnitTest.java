// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;

/** Unit tests for {@link TabGroupSyncPendingReconciliation}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupSyncPendingReconciliationUnitTest {
    private static final @TabId int REPLACED_TAB_ID = 42;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final UserDataHost mUserDataHost = new UserDataHost();

    @Mock private Tab mTab;

    @Before
    public void setUp() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.isDestroyed()).thenReturn(false);
    }

    @Test
    public void testSuppressAndIsSuppressed() {
        assertFalse(TabGroupSyncPendingReconciliation.isSuppressed(mTab));
        assertNull(TabGroupSyncPendingReconciliation.from(mTab));

        TabGroupSyncPendingReconciliation suppression =
                TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID);
        assertNotNull(suppression);
        assertTrue(TabGroupSyncPendingReconciliation.isSuppressed(mTab));
        assertEquals(suppression, TabGroupSyncPendingReconciliation.from(mTab));
        assertEquals(
                Tab.INVALID_TAB_ID, TabGroupSyncPendingReconciliation.getReplacedLocalTabId(mTab));
    }

    @Test
    public void testGetReplacedLocalTabId() {
        assertEquals(
                Tab.INVALID_TAB_ID, TabGroupSyncPendingReconciliation.getReplacedLocalTabId(null));
        assertEquals(
                Tab.INVALID_TAB_ID, TabGroupSyncPendingReconciliation.getReplacedLocalTabId(mTab));

        @TabId int replacedTabId = REPLACED_TAB_ID;
        TabGroupSyncPendingReconciliation suppression =
                TabGroupSyncPendingReconciliation.suppress(mTab, replacedTabId);
        assertNotNull(suppression);
        assertEquals(replacedTabId, TabGroupSyncPendingReconciliation.getReplacedLocalTabId(mTab));
    }

    @Test
    public void testResolveEffectiveReplacedTabId() {
        assertEquals(
                Tab.INVALID_TAB_ID,
                TabGroupSyncPendingReconciliation.resolveEffectiveReplacedTabId(null));

        when(mTab.getId()).thenReturn(100);
        assertEquals(100, TabGroupSyncPendingReconciliation.resolveEffectiveReplacedTabId(mTab));

        TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID);
        assertEquals(100, TabGroupSyncPendingReconciliation.resolveEffectiveReplacedTabId(mTab));

        TabGroupSyncPendingReconciliation.clear(mTab);
        TabGroupSyncPendingReconciliation.suppress(mTab, REPLACED_TAB_ID);
        assertEquals(
                REPLACED_TAB_ID,
                TabGroupSyncPendingReconciliation.resolveEffectiveReplacedTabId(mTab));

        when(mTab.isDestroyed()).thenReturn(true);
        assertEquals(
                Tab.INVALID_TAB_ID,
                TabGroupSyncPendingReconciliation.resolveEffectiveReplacedTabId(mTab));
    }

    @Test
    public void testSuppress_AlreadyCreated_ThrowsAssertionError() {
        TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID);
        assertThrows(
                AssertionError.class,
                () -> TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID));
    }

    @Test
    public void testSuppress_NullTab() {
        assertNull(TabGroupSyncPendingReconciliation.suppress(null, Tab.INVALID_TAB_ID));
    }

    @Test
    public void testSuppress_DestroyedTab() {
        when(mTab.isDestroyed()).thenReturn(true);
        assertNull(TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID));
    }

    @Test
    public void testIsSuppressed_NullTab() {
        assertFalse(TabGroupSyncPendingReconciliation.isSuppressed(null));
    }

    @Test
    public void testClear() {
        TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID);
        assertTrue(TabGroupSyncPendingReconciliation.isSuppressed(mTab));

        TabGroupSyncPendingReconciliation.clear(mTab);
        assertFalse(TabGroupSyncPendingReconciliation.isSuppressed(mTab));
        assertNull(TabGroupSyncPendingReconciliation.from(mTab));

        // Clearing an unsuppressed tab is a safe no-op.
        TabGroupSyncPendingReconciliation.clear(mTab);
        assertFalse(TabGroupSyncPendingReconciliation.isSuppressed(mTab));

        // Clearing null tab is a safe no-op.
        TabGroupSyncPendingReconciliation.clear(null);
    }

    @Test
    public void testLifecycleCleanup_TabDestroyed() {
        TabGroupSyncPendingReconciliation.suppress(mTab, Tab.INVALID_TAB_ID);
        assertTrue(TabGroupSyncPendingReconciliation.isSuppressed(mTab));

        when(mTab.isDestroyed()).thenReturn(true);

        assertFalse(TabGroupSyncPendingReconciliation.isSuppressed(mTab));
        assertNull(TabGroupSyncPendingReconciliation.from(mTab));
    }
}
