// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link PaneManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaneManagerImplUnitTest {
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testFocusChangesPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(PaneId.TAB_SWITCHER, () -> mTabSwitcherPane)
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER, () -> mIncognitoTabSwitcherPane);
        PaneManager paneManager = new PaneManagerImpl(builder);

        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertEquals(mIncognitoTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testFocusUnregisteredPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(PaneId.TAB_SWITCHER, () -> mTabSwitcherPane);
        PaneManager paneManager = new PaneManagerImpl(builder);

        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testFocusUnsuppliedPane() {
        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(PaneId.TAB_SWITCHER, () -> mTabSwitcherPane)
                        .registerPane(PaneId.BOOKMARKS, () -> null);
        PaneManager paneManager = new PaneManagerImpl(builder);

        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertNull(paneManager.getFocusedPaneSupplier().get());

        assertTrue(paneManager.focusPane(PaneId.TAB_SWITCHER));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());

        assertFalse(paneManager.focusPane(PaneId.BOOKMARKS));
        assertEquals(mTabSwitcherPane, paneManager.getFocusedPaneSupplier().get());
    }
}
