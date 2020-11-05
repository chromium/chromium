// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * The coordinator used to show the bookmark bottom sheet when trying to add a bookmark. The bottom
 * sheet contains a list of folders that the bookmark can be added to.
 */
public class BookmarkBottomSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private BookmarkBottomSheetContent mBottomSheetContent;

    /**
     * Constructs the bookmark bottom sheet.
     * @param context The Android context that contains the bookmark bottom sheet.
     * @param bottomSheetController The controller to perform operations on the bottom sheet.
     */
    public BookmarkBottomSheetCoordinator(
            Context context, @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
    }

    /**
     * Shows the bookmark bottom sheet.
     */
    public void show() {
        View contentView = LayoutInflater.from(mContext).inflate(
                org.chromium.chrome.R.layout.bookmark_bottom_sheet, /*root=*/null);
        RecyclerView sheetItemListView =
                contentView.findViewById(org.chromium.chrome.R.id.sheet_item_list);
        // TODO(xingliu): Load actual top level bookmark folders.
        sheetItemListView.setAdapter(new SimpleRecyclerViewAdapter(new ModelList()));
        mBottomSheetContent = new BookmarkBottomSheetContent(
                contentView, sheetItemListView::computeHorizontalScrollOffset);
        mBottomSheetController.requestShowContent(mBottomSheetContent, /*animate=*/false);
    }
}
