// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarCoordinator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.messages.MessageContainerCoordinator;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.ui.accessibility.KeyboardFocusRow;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link KeyboardFocusRowManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class KeyboardFocusRowManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkBarCoordinator mBookmarkBarCoordinator;
    @Mock private CompositorViewHolder mCompositorViewHolder;
    @Mock private MessageContainerCoordinator mMessageContainerCoordinator;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SidePanelContainerCoordinator mSidePanelContainerCoordinator;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Mock private StripLayoutHelperManager mStripLayoutHelperManager;
    @Mock private TabObscuringHandler mTabObscuringHandler;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private VerticalTabsSideUiCoordinator mVerticalTabsSideUiCoordinator;

    private final SettableNonNullObservableSupplier<@StripVisibilityState Integer>
            mStripVisibilityStateSupplier =
                    ObservableSuppliers.createNonNull(StripVisibilityState.OBSCURED);
    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private KeyboardFocusRowManager mKeyboardFocusRowManager;

    @Before
    public void setUp() {
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        when(mStripLayoutHelperManager.getStripVisibilityStateSupplier())
                .thenReturn(mStripVisibilityStateSupplier);

        mKeyboardFocusRowManager =
                new KeyboardFocusRowManager(
                        () -> mBookmarkBarCoordinator,
                        () -> mCompositorViewHolder,
                        () -> mMessageContainerCoordinator,
                        () -> mModalDialogManager,
                        () -> mSidePanelContainerCoordinator,
                        mSideUiStateProviderSupplier,
                        () -> mStripLayoutHelperManager,
                        mTabObscuringHandler,
                        () -> mToolbarManager,
                        () -> true,
                        () -> mVerticalTabsSideUiCoordinator);
    }

    @Test
    @SmallTest
    public void testGetKeyboardFocusRow_verticalTabs() {
        when(mVerticalTabsSideUiCoordinator.containsKeyboardFocus()).thenReturn(true);
        assertEquals(
                KeyboardFocusRow.VERTICAL_TABS,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    public void testGetKeyboardFocusRow_tabStrip() {
        when(mStripLayoutHelperManager.containsKeyboardFocus()).thenReturn(true);
        assertEquals(
                KeyboardFocusRow.TAB_STRIP,
                mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    public void testSwitchKeyboardFocusRow_withVerticalTabs() {
        when(mSideUiStateProvider.isSideUiShowing(SideUiId.VERTICAL_TABS)).thenReturn(true);

        // Initial state: NONE. Switching moves to OMNIBOX.
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ true);
        verify(mToolbarManager).beginFuseboxInput(any());

        // Focus is on OMNIBOX.
        when(mToolbarManager.isUrlBarFocused()).thenReturn(true);

        // Next switch moves to VERTICAL_TABS.
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ true);
        verify(mToolbarManager).endFuseboxInput();
        verify(mVerticalTabsSideUiCoordinator).requestKeyboardFocus();

        // Focus is on VERTICAL_TABS.
        when(mToolbarManager.isUrlBarFocused()).thenReturn(false);
        when(mVerticalTabsSideUiCoordinator.containsKeyboardFocus()).thenReturn(true);

        // Next switch moves to NONE.
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ true);
        verify(mCompositorViewHolder).setFocusOnFirstContentViewItem();
    }

    @Test
    @SmallTest
    public void testSwitchKeyboardFocusRow_horizontalTabStripNotVisible_whenVerticalTabsShowing() {
        when(mSideUiStateProvider.isSideUiShowing(SideUiId.VERTICAL_TABS)).thenReturn(true);
        mStripVisibilityStateSupplier.set(StripVisibilityState.OBSCURED);

        // Focus is on OMNIBOX.
        when(mToolbarManager.isUrlBarFocused()).thenReturn(true);

        // Next switch moves to VERTICAL_TABS, bypassing TAB_STRIP.
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ true);
        verify(mVerticalTabsSideUiCoordinator).requestKeyboardFocus();
        verify(mStripLayoutHelperManager, never()).requestKeyboardFocus();
    }

    @Test
    @SmallTest
    public void testSwitchKeyboardFocusRowBackward_withVerticalTabs() {
        when(mSideUiStateProvider.isSideUiShowing(SideUiId.VERTICAL_TABS)).thenReturn(true);

        // Initial state: NONE. Backward switch moves to the last element (VERTICAL_TABS).
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ false);
        verify(mVerticalTabsSideUiCoordinator).requestKeyboardFocus();

        // Focus is on VERTICAL_TABS. Backward switch moves to OMNIBOX.
        when(mVerticalTabsSideUiCoordinator.containsKeyboardFocus()).thenReturn(true);
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ false);
        verify(mToolbarManager).beginFuseboxInput(any());

        // Focus is on OMNIBOX. Backward switch moves to NONE.
        when(mVerticalTabsSideUiCoordinator.containsKeyboardFocus()).thenReturn(false);
        when(mToolbarManager.isUrlBarFocused()).thenReturn(true);
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ false);
        verify(mToolbarManager).endFuseboxInput();
        verify(mCompositorViewHolder).setFocusOnFirstContentViewItem();
    }

    @Test
    @SmallTest
    public void testGetKeyboardFocusRow_message() {
        when(mMessageContainerCoordinator.containsKeyboardFocus()).thenReturn(true);
        assertEquals(
                KeyboardFocusRow.MESSAGE, mKeyboardFocusRowManager.getKeyboardFocusRowForTesting());
    }

    @Test
    @SmallTest
    public void testSwitchKeyboardFocusRow_withMessage() {
        when(mMessageContainerCoordinator.isVisible()).thenReturn(true);

        // Initial state: NONE. Switching moves to MESSAGE.
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ true);
        verify(mMessageContainerCoordinator).requestKeyboardFocus();

        // Focus is on MESSAGE.
        when(mMessageContainerCoordinator.containsKeyboardFocus()).thenReturn(true);

        // Next switch moves to OMNIBOX.
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ true);
        verify(mToolbarManager).beginFuseboxInput(any());
    }

    @Test
    @SmallTest
    public void testSwitchKeyboardFocusRowBackward_withMessage() {
        when(mMessageContainerCoordinator.isVisible()).thenReturn(true);

        // Focus is on OMNIBOX. Backward switch moves to MESSAGE.
        when(mToolbarManager.isUrlBarFocused()).thenReturn(true);
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ false);
        verify(mToolbarManager).endFuseboxInput();
        verify(mMessageContainerCoordinator).requestKeyboardFocus();

        // Focus is on MESSAGE. Backward switch moves to NONE.
        when(mToolbarManager.isUrlBarFocused()).thenReturn(false);
        when(mMessageContainerCoordinator.containsKeyboardFocus()).thenReturn(true);
        mKeyboardFocusRowManager.onKeyboardFocusRowSwitch(/* forward= */ false);
        verify(mCompositorViewHolder).setFocusOnFirstContentViewItem();
    }
}
