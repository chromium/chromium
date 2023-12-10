// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
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
    @Mock private PaneManager mMockPaneManager;
    private ObservableSupplierImpl<Pane> mMockPaneManagerPaneSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mHubVisibilitySupplier = new ObservableSupplierImpl<>();

    private PaneManager mPaneManager;
    private PaneBackStackHandler mBackStackHandler;

    @Before
    public void setUp() {
        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mBookmarksPane.getPaneId()).thenReturn(PaneId.BOOKMARKS);

        when(mMockPaneManager.getFocusedPaneSupplier()).thenReturn(mMockPaneManagerPaneSupplier);

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
        assertFalse(hasObservers(mMockPaneManagerPaneSupplier));
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
        mBackStackHandler = new PaneBackStackHandler(mMockPaneManager);
        assertTrue(hasObservers(mMockPaneManagerPaneSupplier));
        ShadowLooper.runUiThreadTasks();
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Mock focusing each of three panes.
        mMockPaneManagerPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        mMockPaneManagerPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        mMockPaneManagerPaneSupplier.set(mBookmarksPane);
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Fail to focus on the incognito pane so we go directly back to the tab switcher pane.
        when(mMockPaneManager.focusPane(eq(PaneId.INCOGNITO_TAB_SWITCHER))).thenReturn(false);
        when(mMockPaneManager.focusPane(eq(PaneId.TAB_SWITCHER))).thenReturn(true);

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        verify(mMockPaneManager).focusPane(eq(PaneId.INCOGNITO_TAB_SWITCHER));
        verify(mMockPaneManager).focusPane(eq(PaneId.TAB_SWITCHER));

        // Ensure the back tracking still works.
        mMockPaneManagerPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        mMockPaneManagerPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
    }

    @Test
    @SmallTest
    public void testCompletelyFailToFocus() {
        mBackStackHandler = new PaneBackStackHandler(mMockPaneManager);
        assertTrue(hasObservers(mMockPaneManagerPaneSupplier));
        ShadowLooper.runUiThreadTasks();
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Mock focusing on tab switcher and bookmarks.
        mMockPaneManagerPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        mMockPaneManagerPaneSupplier.set(mBookmarksPane);
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        // Fail the transition back to tab switcher.
        when(mMockPaneManager.focusPane(eq(PaneId.TAB_SWITCHER))).thenReturn(false);

        assertEquals(BackPressResult.FAILURE, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
        verify(mMockPaneManager).focusPane(eq(PaneId.TAB_SWITCHER));

        // Ensure focus tracking continues to work.
        when(mMockPaneManager.focusPane(eq(PaneId.TAB_SWITCHER))).thenReturn(true);

        mMockPaneManagerPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        mMockPaneManagerPaneSupplier.set(mIncognitoTabSwitcherPane);
        assertTrue(mBackStackHandler.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.SUCCESS, mBackStackHandler.handleBackPress());
        assertFalse(mBackStackHandler.getHandleBackPressChangedSupplier().get());
    }

    private boolean hasObservers(ObservableSupplier<Pane> paneSupplier) {
        return ((ObservableSupplierImpl<Pane>) paneSupplier).hasObservers();
    }
}
