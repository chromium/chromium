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

/** Unit tests for {@link TabGroupSyncNavigationSuppression}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupSyncNavigationSuppressionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.isDestroyed()).thenReturn(false);
    }

    @Test
    public void testSuppressAndIsSuppressed() {
        assertFalse(TabGroupSyncNavigationSuppression.isSuppressed(mTab));
        assertNull(TabGroupSyncNavigationSuppression.from(mTab));

        TabGroupSyncNavigationSuppression suppression =
                TabGroupSyncNavigationSuppression.suppress(mTab);
        assertNotNull(suppression);
        assertTrue(TabGroupSyncNavigationSuppression.isSuppressed(mTab));
        assertEquals(suppression, TabGroupSyncNavigationSuppression.from(mTab));
    }

    @Test
    public void testSuppress_AlreadyCreated_ThrowsAssertionError() {
        TabGroupSyncNavigationSuppression.suppress(mTab);
        assertThrows(
                AssertionError.class, () -> TabGroupSyncNavigationSuppression.suppress(mTab));
    }

    @Test
    public void testSuppress_NullTab() {
        assertNull(TabGroupSyncNavigationSuppression.suppress(null));
    }

    @Test
    public void testSuppress_DestroyedTab() {
        when(mTab.isDestroyed()).thenReturn(true);
        assertNull(TabGroupSyncNavigationSuppression.suppress(mTab));
    }

    @Test
    public void testIsSuppressed_NullTab() {
        assertFalse(TabGroupSyncNavigationSuppression.isSuppressed(null));
    }

    @Test
    public void testClear() {
        TabGroupSyncNavigationSuppression.suppress(mTab);
        assertTrue(TabGroupSyncNavigationSuppression.isSuppressed(mTab));

        TabGroupSyncNavigationSuppression.clear(mTab);
        assertFalse(TabGroupSyncNavigationSuppression.isSuppressed(mTab));
        assertNull(TabGroupSyncNavigationSuppression.from(mTab));

        // Clearing an unsuppressed tab is a safe no-op.
        TabGroupSyncNavigationSuppression.clear(mTab);
        assertFalse(TabGroupSyncNavigationSuppression.isSuppressed(mTab));

        // Clearing null tab is a safe no-op.
        TabGroupSyncNavigationSuppression.clear(null);
    }

    @Test
    public void testLifecycleCleanup_TabDestroyed() {
        TabGroupSyncNavigationSuppression.suppress(mTab);
        assertTrue(TabGroupSyncNavigationSuppression.isSuppressed(mTab));

        when(mTab.isDestroyed()).thenReturn(true);

        assertFalse(TabGroupSyncNavigationSuppression.isSuppressed(mTab));
        assertNull(TabGroupSyncNavigationSuppression.from(mTab));
    }
}
