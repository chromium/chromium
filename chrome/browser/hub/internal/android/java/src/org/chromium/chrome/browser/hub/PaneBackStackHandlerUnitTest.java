// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/** Tests for {@link PaneBackStackHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaneBackStackHandlerUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private Pane mBookmarksPane;
    @Mock private DisplayButtonData mReferenceButtonData;

    private ObservableSupplierImpl<DisplayButtonData> mEmptyReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mHubVisibilitySupplier = new ObservableSupplierImpl<>();

    private PaneManager mPaneManager;
    private PaneBackStackHandler mBackStackHandler;

    @Before
    public void setUp() {
        mReferenceButtonDataSupplier.set(mReferenceButtonData);
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        when(mBookmarksPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);
        when(mBookmarksPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);

        PaneListBuilder builder =
                new PaneListBuilder(new DefaultPaneOrderController())
                        .registerPane(
                                PaneId.TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mTabSwitcherPane))
                        .registerPane(
                                PaneId.INCOGNITO_TAB_SWITCHER,
                                LazyOneshotSupplier.fromValue(mIncognitoTabSwitcherPane))
                        .registerPane(
                                PaneId.BOOKMARKS, LazyOneshotSupplier.fromValue(mBookmarksPane));

        mPaneManager = new PaneManagerImpl(builder, mHubVisibilitySupplier);
    }

    @After
    public void tearDown() {
        mBackStackHandler.destroy();
        assertFalse(hasObservers(mPaneManager.getFocusedPaneSupplier()));
    }

    @Test
    @SmallTest
    public void testReset() {
        mBackStackHandler = new PaneBackStackHandler(mPaneManager);
        assertTrue(hasObservers(mPaneManager.getFocusedPaneSupplier()));
        ShadowLooper.runUiThreadTasks();

        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Focus each of three panes.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mIncognitoTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        // Reset.
        mBackStackHandler.reset();
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Ensure the focus tracking and back state still work.
        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mIncognitoTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testBackStack() {
        mBackStackHandler = new PaneBackStackHandler(mPaneManager);
        assertTrue(hasObservers(mPaneManager.getFocusedPaneSupplier()));
        ShadowLooper.runUiThreadTasks();

        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Focus each of three panes.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mIncognitoTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        // Ensure back works for multiple steps.
        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testRepeatedlyFocusSamePane() {
        mBackStackHandler = new PaneBackStackHandler(mPaneManager);
        assertTrue(hasObservers(mPaneManager.getFocusedPaneSupplier()));
        ShadowLooper.runUiThreadTasks();

        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Focus the first pane twice.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        // Focus the second pane twice.
        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        // Ensure the back stack still works.
        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testDeduplicatedOldEntries() {
        mBackStackHandler = new PaneBackStackHandler(mPaneManager);
        assertTrue(hasObservers(mPaneManager.getFocusedPaneSupplier()));
        ShadowLooper.runUiThreadTasks();

        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Focus tab switcher into bookmarks.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());

        // Refocus tab switcher.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        // Ensure that going back works and there are no more entries.
        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mBookmarksPane, mPaneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testSkipOnFailToFocus() {
        mBackStackHandler = new PaneBackStackHandler(mPaneManager);
        assertTrue(hasObservers(mPaneManager.getFocusedPaneSupplier()));
        ShadowLooper.runUiThreadTasks();
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Mock focusing each of three panes.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Fail to focus on the incognito pane so we go directly back to the tab switcher pane.
        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mEmptyReferenceButtonDataSupplier);

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());

        // Ensure the back tracking still works.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);
        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        // Not focusable after leaving.
        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mEmptyReferenceButtonDataSupplier);

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Changing this now should have no effect.
        when(mIncognitoTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);

        // Skip incongito on the way back since it wasn't reachable when switched away.
        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(mTabSwitcherPane, mPaneManager.getFocusedPaneSupplier().get());
    }

    @Test
    @SmallTest
    public void testCompletelyFailToFocus() {
        mBackStackHandler = new PaneBackStackHandler(mPaneManager);
        assertTrue(hasObservers(mPaneManager.getFocusedPaneSupplier()));
        ShadowLooper.runUiThreadTasks();
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Mock focusing on tab switcher and bookmarks.
        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.BOOKMARKS));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Fail the transition back to tab switcher.
        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mEmptyReferenceButtonDataSupplier);

        assertEquals(BackPressResult.FAILURE, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Ensure focus tracking continues to work.
        when(mTabSwitcherPane.getReferenceButtonDataSupplier())
                .thenReturn(mReferenceButtonDataSupplier);

        assertTrue(mPaneManager.focusPane(PaneId.TAB_SWITCHER));
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertTrue(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER));
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
    }

    private boolean hasObservers(ObservableSupplier<Pane> paneSupplier) {
        return ((ObservableSupplierImpl<Pane>) paneSupplier).hasObservers();
    }
}
