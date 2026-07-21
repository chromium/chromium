// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.graphics.Point;
import android.util.Pair;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.function.Supplier;

/** Coordinates the display of all popup menus anchored to the Bookmarks Bar. */
@NullMarked
public class BookmarkBarPopupCoordinator {
    @VisibleForTesting final BookmarkBarPopup mPopup;

    public BookmarkBarPopupCoordinator(
            Activity activity,
            View bookmarkBarView,
            Supplier<Pair<Integer, Integer>> controlsHeightSupplier) {
        mPopup = new BookmarkBarPopup(activity, controlsHeightSupplier);
    }

    /**
     * Shows a popup window listing the bookmarks and subfolders inside a bookmarks bar folder.
     *
     * @param anchorView The {@link View} used as the positioning anchor for the popup. The popup
     *     will be aligned relative to this view.
     * @param folderMenuModel A {@link ModelList} containing the property models of the bookmark
     *     items to display. {@link BookmarkBarMediator} is expected to construct this model list
     *     and populate it with the direct children of the folder before invoking this method.
     * @param isIncognito Whether the current profile session is incognito.
     */
    public void showFolderItemsPopup(
            View anchorView, ModelList folderMenuModel, boolean isIncognito) {
        showPopup(folderMenuModel, anchorView, null, isIncognito);
    }

    /** Dismisses the active popup window. */
    public void dismiss() {
        mPopup.dismiss();
    }

    public void showPopup(
            ModelList bookmarkItems, View anchorView, @Nullable Point offset, boolean isIncognito) {
        mPopup.show(anchorView, offset, bookmarkItems, isIncognito);
    }

    public void onBrowserControlsChanged(int topControlsHeight, int bottomControlsHeight) {
        mPopup.onBrowserControlsChanged(topControlsHeight, bottomControlsHeight);
    }
}
