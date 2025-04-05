// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType.APP;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarCoordinator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.tab_strip.StripVisibilityState;
import org.chromium.ui.accessibility.KeyboardFocusRow;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Controls the keyboard focus location for tab strip, toolbar, bookmarks bar on Chrome for Android.
 *
 * <p>See {@link org.chromium.chrome.browser.KeyboardShortcuts.KeyboardShortcutsSemanticMeaning}
 */
@NullMarked
/* package */ class KeyboardFocusRowManager {

    // Alphabetical order by field name
    private final Supplier</* @Nullable */ BookmarkBarCoordinator> mBookmarkBarCoordinatorSupplier;

    @SuppressWarnings("unused")
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;

    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier</* Nullable */ StripLayoutHelperManager>
            mStripLayoutHelperManagerSupplier;
    private final TabObscuringHandler mTabObscuringHandler;
    private final Supplier</* Nullable */ ToolbarManager> mToolbarManagerSupplier;

    /**
     * Constructs a {@link KeyboardFocusRowManager}, which controls the keyboard focus location for
     * tab strip, toolbar, bookmarks bar on Chrome for Android.
     *
     * <p>See {@link org.chromium.chrome.browser.KeyboardShortcuts.KeyboardShortcutsSemanticMeaning}
     *
     * @param bookmarkBarCoordinatorSupplier Supplies the {@link BookmarkBarCoordinator} (or null,
     *     if the bookmarks bar is not visible) that will be used to get/set keyboard focus on the
     *     bookmarks bar.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder} that will be
     *     used to request focus on the tab contents.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager} that will be used
     *     to determine if an app modal dialog is showing (in which case the keyboard shortcuts
     *     should not do anything).
     * @param stripLayoutHelperManagerSupplier Supplies the {@link StripLayoutHelperManager} (or
     *     null, if the tab strip is not visible) that will be used to get/set keyboard focus on the
     *     tab strip.
     * @param tabObscuringHandler The {@link TabObscuringHandler} that will be used to determine if
     *     the toolbar is obscured (in which case the keyboard shortcuts should not do anything).
     * @param toolbarManagerSupplier Supplies the {@link ToolbarManager} (or null, if the toolbar is
     *     not visible) that will be used to get/set keyboard focus on the toolbar.
     */
    KeyboardFocusRowManager(
            Supplier</* @Nullable */ BookmarkBarCoordinator> bookmarkBarCoordinatorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier</* @Nullable */ StripLayoutHelperManager> stripLayoutHelperManagerSupplier,
            TabObscuringHandler tabObscuringHandler,
            Supplier</* @Nullable */ ToolbarManager> toolbarManagerSupplier) {
        mBookmarkBarCoordinatorSupplier = bookmarkBarCoordinatorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mStripLayoutHelperManagerSupplier = stripLayoutHelperManagerSupplier;
        mTabObscuringHandler = tabObscuringHandler;
        mToolbarManagerSupplier = toolbarManagerSupplier;
    }

    /** Called when the user switches which row of the top controls should have keyboard focus. */
    /* package */ void onKeyboardFocusRowSwitch() {
        // If the toolbar is obscured, return early.
        var modalDialogManager = mModalDialogManagerSupplier.get();
        if (mTabObscuringHandler.isToolbarObscured()
                || (modalDialogManager.isShowing() && modalDialogManager.getCurrentType() == APP)) {
            return;
        }

        @KeyboardFocusRow int oldKeyboardFocusRow = getKeyboardFocusRow();
        @KeyboardFocusRow int newKeyboardFocusRow = getNewKeyboardFocusRow(oldKeyboardFocusRow);
        switch (newKeyboardFocusRow) {
            case KeyboardFocusRow.NONE -> {
                mCompositorViewHolderSupplier.get().setFocusOnFirstContentViewItem();
            }
            case KeyboardFocusRow.TAB_STRIP -> {
                var stripLayoutHelperManager = mStripLayoutHelperManagerSupplier.get();
                if (stripLayoutHelperManager != null) {
                    stripLayoutHelperManager.requestKeyboardFocus();
                }
            }
            case KeyboardFocusRow.TOOLBAR -> {
                var toolbarManager = mToolbarManagerSupplier.get();
                if (toolbarManager != null) toolbarManager.requestFocus();
            }
            case KeyboardFocusRow.BOOKMARKS_BAR -> {
                var bookmarkBarCoordinator = mBookmarkBarCoordinatorSupplier.get();
                if (bookmarkBarCoordinator != null) bookmarkBarCoordinator.requestFocus();
            }
        }
    }

    private @KeyboardFocusRow int getKeyboardFocusRow() {
        var stripLayoutHelperManager = mStripLayoutHelperManagerSupplier.get();
        if (stripLayoutHelperManager != null && stripLayoutHelperManager.containsKeyboardFocus()) {
            return KeyboardFocusRow.TAB_STRIP;
        }

        var toolbarManager = mToolbarManagerSupplier.get();
        if (toolbarManager != null && toolbarManager.containsKeyboardFocus()) {
            return KeyboardFocusRow.TOOLBAR;
        }

        var bookmarkBarCoordinator = mBookmarkBarCoordinatorSupplier.get();
        if (bookmarkBarCoordinator != null && bookmarkBarCoordinator.hasKeyboardFocus()) {
            return KeyboardFocusRow.BOOKMARKS_BAR;
        }

        return KeyboardFocusRow.NONE;
    }

    /**
     * Given {@param oldKeyboardFocusRow}, returns what the new keyboard focus row should be. This
     * method assumes that the toolbar is visible and not obscured by other content.
     *
     * @param oldKeyboardFocusRow The old {@link KeyboardFocusRow}.
     * @return What the new keyboard focus row should be.
     */
    private @KeyboardFocusRow int getNewKeyboardFocusRow(
            @KeyboardFocusRow int oldKeyboardFocusRow) {
        // NONE and TOOLBAR are always options.
        List<Integer> keyboardFocusRows =
                new ArrayList<>(List.of(KeyboardFocusRow.NONE, KeyboardFocusRow.TOOLBAR));

        // The next item in the focus cycle order is TAB_STRIP, if it is present.
        var stripLayoutHelperManager = mStripLayoutHelperManagerSupplier.get();
        if (stripLayoutHelperManager != null
                && stripLayoutHelperManager.getStripVisibilityState()
                        == StripVisibilityState.VISIBLE) {
            keyboardFocusRows.add(KeyboardFocusRow.TAB_STRIP);
        }

        // The next item in the focus cycle order is BOOKMARKS_BAR, if it is present.
        if (mBookmarkBarCoordinatorSupplier.hasValue()) {
            keyboardFocusRows.add(KeyboardFocusRow.BOOKMARKS_BAR);
        }

        int currentFocusIndex = keyboardFocusRows.indexOf(oldKeyboardFocusRow);
        if (currentFocusIndex == -1) return KeyboardFocusRow.NONE;
        int newFocusIndex = (currentFocusIndex + 1) % keyboardFocusRows.size();
        return keyboardFocusRows.get(newFocusIndex);
    }

    /* package */ @KeyboardFocusRow
    int getKeyboardFocusRowForTesting() {
        return getKeyboardFocusRow();
    }
}
