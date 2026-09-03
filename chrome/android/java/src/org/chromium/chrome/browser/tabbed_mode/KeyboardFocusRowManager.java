// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType.APP;

import android.view.View;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarCoordinator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.messages.MessageContainerCoordinator;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.ui.accessibility.KeyboardFocusRow;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Controls the keyboard focus location for top controls and side UI (messages, tab strip, omnibox,
 * bookmarks bar, vertical tabs, side panel) on Chrome for Android.
 *
 * <p>See {@link org.chromium.chrome.browser.KeyboardShortcuts.KeyboardShortcutsSemanticMeaning}
 */
@NullMarked
/* package */ class KeyboardFocusRowManager {

    // Alphabetical order by field name
    private final Supplier<@Nullable BookmarkBarCoordinator> mBookmarkBarCoordinatorSupplier;
    private final Supplier<@Nullable CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<@Nullable MessageContainerCoordinator>
            mMessageContainerCoordinatorSupplier;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<@Nullable SidePanelContainerCoordinator> mSidePanelContainerSupplier;
    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier;
    private final Supplier<@Nullable StripLayoutHelperManager> mStripLayoutHelperManagerSupplier;
    private final TabObscuringHandler mTabObscuringHandler;
    private final Supplier<@Nullable ToolbarManager> mToolbarManagerSupplier;
    private final Supplier<Boolean> mUrlBarVisibleSupplier;
    private final Supplier<@Nullable VerticalTabsSideUiCoordinator>
            mVerticalTabsSideUiCoordinatorSupplier;

    /**
     * Constructs a {@link KeyboardFocusRowManager}, which controls the keyboard focus location for
     * messages, tab strip, omnibox, bookmarks bar, vertical tabs, and side panel on Chrome for
     * Android.
     *
     * <p>See {@link org.chromium.chrome.browser.KeyboardShortcuts.KeyboardShortcutsSemanticMeaning}
     *
     * @param bookmarkBarCoordinatorSupplier Supplies the {@link BookmarkBarCoordinator} (or null,
     *     if the bookmarks bar is not visible) that will be used to get/set keyboard focus on the
     *     bookmarks bar.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder} that will be
     *     used to request focus on the tab contents.
     * @param messageContainerCoordinatorSupplier Supplies the {@link MessageContainerCoordinator}
     *     (or null, if messages are not supported or initialized) that will be used to get/set
     *     keyboard focus on messages.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager} that will be used
     *     to determine if an app modal dialog is showing (in which case the keyboard shortcuts
     *     should not do anything).
     * @param sidePanelContainerSupplier Supplies the {@link SidePanelContainerCoordinator} (or
     *     null, if the side panel is not visible) that will be used to get/set keyboard focus on
     *     the side panel.
     * @param sideUiStateProviderSupplier Supplies the {@link SideUiStateProvider} that will be used
     *     to get/set keyboard focus on the side panel.
     * @param stripLayoutHelperManagerSupplier Supplies the {@link StripLayoutHelperManager} (or
     *     null, if the tab strip is not visible) that will be used to get/set keyboard focus on the
     *     tab strip.
     * @param tabObscuringHandler The {@link TabObscuringHandler} that will be used to determine if
     *     the tab is obscured (in which case the keyboard shortcuts should not do anything).
     * @param toolbarManagerSupplier Supplies the {@link ToolbarManager} (or null, if the toolbar is
     *     not visible) that will be used to get/set keyboard focus on the omnibox.
     * @param urlBarVisibleSupplier Supplies a boolean indicating whether the URL bar is currently
     *     visible, used to determine if it can receive keyboard focus.
     * @param verticalTabsSideUiCoordinatorSupplier Supplies the {@link
     *     VerticalTabsSideUiCoordinator} (or null, if vertical tabs is not initialized or
     *     supported) that will be used to get/set keyboard focus on vertical tabs.
     */
    KeyboardFocusRowManager(
            Supplier<@Nullable BookmarkBarCoordinator> bookmarkBarCoordinatorSupplier,
            Supplier<@Nullable CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<@Nullable MessageContainerCoordinator> messageContainerCoordinatorSupplier,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            Supplier<@Nullable SidePanelContainerCoordinator> sidePanelContainerSupplier,
            OneshotSupplierImpl<SideUiStateProvider> sideUiStateProviderSupplier,
            Supplier<@Nullable StripLayoutHelperManager> stripLayoutHelperManagerSupplier,
            TabObscuringHandler tabObscuringHandler,
            Supplier<@Nullable ToolbarManager> toolbarManagerSupplier,
            Supplier<Boolean> urlBarVisibleSupplier,
            Supplier<@Nullable VerticalTabsSideUiCoordinator>
                    verticalTabsSideUiCoordinatorSupplier) {
        mBookmarkBarCoordinatorSupplier = bookmarkBarCoordinatorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mMessageContainerCoordinatorSupplier = messageContainerCoordinatorSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSidePanelContainerSupplier = sidePanelContainerSupplier;
        mSideUiStateProviderSupplier = sideUiStateProviderSupplier;
        mStripLayoutHelperManagerSupplier = stripLayoutHelperManagerSupplier;
        mTabObscuringHandler = tabObscuringHandler;
        mToolbarManagerSupplier = toolbarManagerSupplier;
        mUrlBarVisibleSupplier = urlBarVisibleSupplier;
        mVerticalTabsSideUiCoordinatorSupplier = verticalTabsSideUiCoordinatorSupplier;
    }

    /**
     * Returns whether the keyboard focus row can be switched (true when the toolbar is visible and
     * no app-modal dialog is showing).
     */
    private boolean canSwitchKeyboardFocusRow() {
        var modalDialogManager = mModalDialogManagerSupplier.get();
        if (mTabObscuringHandler.isToolbarObscured()
                || (modalDialogManager != null
                        && modalDialogManager.isShowing()
                        && modalDialogManager.getCurrentType() == APP)) {
            return false;
        }
        return true;
    }

    /**
     * Called when the user switches which row of the top controls should have keyboard focus.
     *
     * @param forward True if cycling forward, false if cycling reverse.
     */
    /* package */ void onKeyboardFocusRowSwitch(boolean forward) {
        if (!canSwitchKeyboardFocusRow()) return;

        @KeyboardFocusRow int oldKeyboardFocusRow = getKeyboardFocusRow();
        @KeyboardFocusRow
        int newKeyboardFocusRow = getNewKeyboardFocusRow(oldKeyboardFocusRow, forward);
        if (oldKeyboardFocusRow == KeyboardFocusRow.OMNIBOX) {
            var toolbarManager = mToolbarManagerSupplier.get();
            if (toolbarManager != null) {
                toolbarManager.endFuseboxInput();
            }
        }

        switch (newKeyboardFocusRow) {
            case KeyboardFocusRow.NONE -> {
                var compositorViewHolder = mCompositorViewHolderSupplier.get();
                if (compositorViewHolder != null) {
                    compositorViewHolder.setFocusOnFirstContentViewItem();
                }
            }
            case KeyboardFocusRow.MESSAGE -> {
                var messageContainerCoordinator = mMessageContainerCoordinatorSupplier.get();
                if (messageContainerCoordinator != null) {
                    messageContainerCoordinator.requestKeyboardFocus();
                }
            }
            case KeyboardFocusRow.OMNIBOX -> {
                var toolbarManager = mToolbarManagerSupplier.get();
                if (toolbarManager != null) {
                    toolbarManager.beginFuseboxInput(
                            new AutocompleteInput(OmniboxFocusReason.MENU_OR_KEYBOARD_ACTION));
                }
            }
            case KeyboardFocusRow.TAB_STRIP -> {
                var stripLayoutHelperManager = mStripLayoutHelperManagerSupplier.get();
                if (stripLayoutHelperManager != null) {
                    stripLayoutHelperManager.requestKeyboardFocus();
                }
            }
            case KeyboardFocusRow.VERTICAL_TABS -> {
                var verticalTabsCoordinator = mVerticalTabsSideUiCoordinatorSupplier.get();
                if (verticalTabsCoordinator != null) {
                    verticalTabsCoordinator.requestKeyboardFocus();
                }
            }
            case KeyboardFocusRow.BOOKMARKS_BAR -> {
                var bookmarkBarCoordinator = mBookmarkBarCoordinatorSupplier.get();
                if (bookmarkBarCoordinator != null) bookmarkBarCoordinator.requestFocus();
            }

            case KeyboardFocusRow.SIDE_PANEL -> {
                var sidePanelContainer = mSidePanelContainerSupplier.get();
                if (sidePanelContainer != null) {
                    View contentView = sidePanelContainer.getContentView();
                    if (contentView != null) {
                        contentView.requestFocus();
                    }
                }
            }
        }
    }

    private @KeyboardFocusRow int getKeyboardFocusRow() {
        var messageContainerCoordinator = mMessageContainerCoordinatorSupplier.get();
        if (messageContainerCoordinator != null
                && messageContainerCoordinator.containsKeyboardFocus()) {
            return KeyboardFocusRow.MESSAGE;
        }

        var toolbarManager = mToolbarManagerSupplier.get();
        if (toolbarManager != null && toolbarManager.isUrlBarFocused()) {
            return KeyboardFocusRow.OMNIBOX;
        }

        var stripLayoutHelperManager = mStripLayoutHelperManagerSupplier.get();
        if (stripLayoutHelperManager != null && stripLayoutHelperManager.containsKeyboardFocus()) {
            return KeyboardFocusRow.TAB_STRIP;
        }

        var verticalTabsCoordinator = mVerticalTabsSideUiCoordinatorSupplier.get();
        if (verticalTabsCoordinator != null && verticalTabsCoordinator.containsKeyboardFocus()) {
            return KeyboardFocusRow.VERTICAL_TABS;
        }

        var bookmarkBarCoordinator = mBookmarkBarCoordinatorSupplier.get();
        if (bookmarkBarCoordinator != null && bookmarkBarCoordinator.hasKeyboardFocus()) {
            return KeyboardFocusRow.BOOKMARKS_BAR;
        }

        var sidePanelContainer = mSidePanelContainerSupplier.get();
        if (sidePanelContainer != null) {
            View contentView = sidePanelContainer.getContentView();
            if (contentView != null && contentView.hasFocus()) {
                return KeyboardFocusRow.SIDE_PANEL;
            }
        }

        return KeyboardFocusRow.NONE;
    }

    /**
     * Given {@code oldKeyboardFocusRow}, returns what the new keyboard focus row should be. This
     * method assumes that the toolbar is visible and not obscured by other content.
     *
     * @param oldKeyboardFocusRow The old {@link KeyboardFocusRow}.
     * @param forward True if cycling forward, false if cycling reverse.
     * @return What the new keyboard focus row should be.
     */
    private @KeyboardFocusRow int getNewKeyboardFocusRow(
            @KeyboardFocusRow int oldKeyboardFocusRow, boolean forward) {
        // NONE is always an option.
        List<Integer> keyboardFocusRows = new ArrayList<>(List.of(KeyboardFocusRow.NONE));

        // The next item in the focus cycle order is MESSAGE, if it is present.
        var messageContainerCoordinator = mMessageContainerCoordinatorSupplier.get();
        if (messageContainerCoordinator != null && messageContainerCoordinator.isVisible()) {
            keyboardFocusRows.add(KeyboardFocusRow.MESSAGE);
        }

        var toolbarManager = mToolbarManagerSupplier.get();
        if (toolbarManager != null) {
            boolean isUrlBarVisible = mUrlBarVisibleSupplier.get();
            if (isUrlBarVisible) {
                keyboardFocusRows.add(KeyboardFocusRow.OMNIBOX);
            }
        }

        // The next item in the focus cycle order is TAB_STRIP, if it is present.
        var stripLayoutHelperManager = mStripLayoutHelperManagerSupplier.get();
        if (stripLayoutHelperManager != null
                && stripLayoutHelperManager.getStripVisibilityStateSupplier().get()
                        == StripVisibilityState.VISIBLE) {
            keyboardFocusRows.add(KeyboardFocusRow.TAB_STRIP);
        }

        // The next item in the focus cycle order is VERTICAL_TABS, if it is present.
        var sideUiStateProvider = mSideUiStateProviderSupplier.get();
        if (sideUiStateProvider != null
                && sideUiStateProvider.isSideUiShowing(SideUiId.VERTICAL_TABS)) {
            keyboardFocusRows.add(KeyboardFocusRow.VERTICAL_TABS);
        }

        // The next item in the focus cycle order is BOOKMARKS_BAR, if it is present.
        var bookmarkBarCoordinator = mBookmarkBarCoordinatorSupplier.get();
        if (bookmarkBarCoordinator != null && bookmarkBarCoordinator.isVisible()) {
            keyboardFocusRows.add(KeyboardFocusRow.BOOKMARKS_BAR);
        }

        // The next item in the focus cycle order is the SIDE_PANEL, if it is shown.
        if (AndroidSidePanelEnabledFn.isEnabled()) {
            if (sideUiStateProvider != null
                    && sideUiStateProvider.isSideUiShowing(SideUiId.SIDE_PANEL)) {
                keyboardFocusRows.add(KeyboardFocusRow.SIDE_PANEL);
            }
        }

        int currentFocusIndex = keyboardFocusRows.indexOf(oldKeyboardFocusRow);
        if (currentFocusIndex == -1) return KeyboardFocusRow.NONE;
        int delta = forward ? 1 : -1;
        int newFocusIndex =
                (currentFocusIndex + delta + keyboardFocusRows.size()) % keyboardFocusRows.size();
        return keyboardFocusRows.get(newFocusIndex);
    }

    /* package */ @KeyboardFocusRow
    int getKeyboardFocusRowForTesting() {
        return getKeyboardFocusRow();
    }
}
