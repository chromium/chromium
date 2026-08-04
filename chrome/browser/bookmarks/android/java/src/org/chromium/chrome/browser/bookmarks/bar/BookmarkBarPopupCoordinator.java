// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Point;
import android.os.SystemClock;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.function.Supplier;

/** Coordinates the display and synchronization of popup menus anchored to the Bookmarks Bar. */
@NullMarked
public class BookmarkBarPopupCoordinator {
    /**
     * Grace period in milliseconds during which a touch on the folder popup suppresses dismissing
     * the folder popup when the context menu is dismissed. This value (500ms) matches the standard
     * Android ViewConfiguration long press timeout to ensure accidental touches do not immediately
     * dismiss the popup.
     */
    private static final long FOLDER_TOUCH_DISMISSAL_GRACE_PERIOD_MS = 500;

    @VisibleForTesting final BookmarkBarPopup mFolderPopup;
    @VisibleForTesting final BookmarkBarPopup mContextMenuPopup;

    private boolean mIsSwitchingContextMenu;
    private long mLastFolderTouchTime;

    private @Nullable View mFolderAnchorView;
    private @Nullable View mContextMenuAnchorView;

    public BookmarkBarPopupCoordinator(
            Activity activity,
            View bookmarkBarView,
            Supplier<Pair<Integer, Integer>> controlsHeightSupplier) {
        mFolderPopup = new BookmarkBarPopup(activity, controlsHeightSupplier);
        mContextMenuPopup = new BookmarkBarPopup(activity, controlsHeightSupplier);
    }

    /**
     * Shows a popup window listing the bookmarks and subfolders inside a bookmarks bar folder.
     *
     * @param anchorView The {@link View} used as the positioning anchor for the popup.
     * @param folderMenuModel A {@link ModelList} containing the property models of the bookmark
     *     items.
     * @param isIncognito Whether the current profile session is incognito.
     */
    @SuppressLint("ClickableViewAccessibility")
    public void showFolderItemsPopup(
            View anchorView, ModelList folderMenuModel, boolean isIncognito) {
        dismiss();
        highlightFolderAnchor(anchorView);
        mFolderPopup.show(
                anchorView,
                /* offset= */ null,
                folderMenuModel,
                isIncognito,
                this::dismiss,
                () -> onFolderPopupDismiss(anchorView),
                (v, event) -> {
                    int action = event.getActionMasked();
                    if (action == MotionEvent.ACTION_DOWN
                            || action == MotionEvent.ACTION_BUTTON_PRESS) {
                        mLastFolderTouchTime = SystemClock.uptimeMillis();
                    }
                    return false;
                },
                this::onPopupTouchIntercept);
    }

    /** Dismisses both folder and context menu popup windows. */
    public void dismiss() {
        dismissFolderPopup();
        dismissContextMenuPopup();
    }

    /** Dismisses the folder popup window. */
    public void dismissFolderPopup() {
        mFolderPopup.dismiss();
    }

    /** Dismisses the context menu popup window. */
    public void dismissContextMenuPopup() {
        mContextMenuPopup.dismiss();
    }

    /**
     * Shows a context menu popup window for a bookmark item. If the context menu is being shown for
     * an item within an already-open folder popup, this ensures the folder popup is not dismissed.
     */
    public void showContextMenuPopup(
            ModelList contextMenuModel,
            View anchorView,
            @Nullable Point offset,
            boolean isIncognito) {
        mIsSwitchingContextMenu = true;
        try {
            dismissContextMenuPopup();
        } finally {
            mIsSwitchingContextMenu = false;
        }

        highlightContextMenuAnchor(anchorView);
        mContextMenuPopup.show(
                anchorView,
                offset,
                contextMenuModel,
                isIncognito,
                this::dismiss,
                () -> onContextMenuPopupDismiss(anchorView),
                /* touchListener= */ null,
                this::onPopupTouchIntercept);
    }

    private void highlightFolderAnchor(View anchorView) {
        clearFolderAnchorHighlight();
        mFolderAnchorView = anchorView;
        anchorView.setSelected(true);
    }

    private void clearFolderAnchorHighlight() {
        if (mFolderAnchorView != null) {
            mFolderAnchorView.setSelected(false);
            mFolderAnchorView = null;
        }
    }

    private void highlightContextMenuAnchor(View anchorView) {
        clearContextMenuAnchorHighlight();
        // Do not apply highlighting if anchor is the bookmark bar empty space.
        if (anchorView instanceof BookmarkBar) {
            return;
        }
        mContextMenuAnchorView = anchorView;
        anchorView.setSelected(true);
    }

    private void clearContextMenuAnchorHighlight() {
        if (mContextMenuAnchorView != null) {
            mContextMenuAnchorView.setSelected(false);
            mContextMenuAnchorView = null;
        }
    }

    private void onFolderPopupDismiss(View anchorView) {
        if (mFolderAnchorView == anchorView) {
            clearFolderAnchorHighlight();
        }
        dismissContextMenuPopup();
    }

    private void onContextMenuPopupDismiss(View anchorView) {
        if (mContextMenuAnchorView == anchorView) {
            clearContextMenuAnchorHighlight();
        }
        if (!mIsSwitchingContextMenu
                && SystemClock.uptimeMillis() - mLastFolderTouchTime
                        > FOLDER_TOUCH_DISMISSAL_GRACE_PERIOD_MS) {
            dismissFolderPopup();
        }
    }

    /**
     * Intercepts touches on the context menu popup to manage the state between the overlapping
     * folder popup and context menu popup. This manages a 'grace period' where touching the
     * underlying folder popup prevents the context menu dismissal from also closing the folder
     * popup, ensuring smooth interactions when navigating between menus.
     */
    private boolean onPopupTouchIntercept(View v, MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_BUTTON_PRESS) {
            if (mFolderPopup.containsTouch(event.getRawX(), event.getRawY())) {
                mLastFolderTouchTime = SystemClock.uptimeMillis();
            }
        }
        if (event.getAction() == MotionEvent.ACTION_OUTSIDE) {
            if (mFolderPopup.containsTouch(event.getRawX(), event.getRawY())) {
                mLastFolderTouchTime = SystemClock.uptimeMillis();
                dismissContextMenuPopup();
            } else {
                dismiss();
            }
            return true;
        }
        return false;
    }

    public void onBrowserControlsChanged(int topControlsHeight, int bottomControlsHeight) {
        mFolderPopup.onBrowserControlsChanged(topControlsHeight, bottomControlsHeight);
        mContextMenuPopup.onBrowserControlsChanged(topControlsHeight, bottomControlsHeight);
    }
}
