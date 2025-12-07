// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** BottomSheetContent implementation for app filter bottom sheet. */
@NullMarked
class AppFilterSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final View mToolbarView;
    private final RecyclerView mListView;
    private final Runnable mCloseRunnable;

    /** Construct a new AppFilterSheet. */
    AppFilterSheetContent(
            Context context, View contentView, RecyclerView listView, Runnable closeRunnable) {
        var layoutInflater = LayoutInflater.from(context);
        mToolbarView = layoutInflater.inflate(R.layout.appfilter_header, null);
        mContentView = contentView;
        mListView = listView;
        mCloseRunnable = closeRunnable;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mListView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mCloseRunnable.run();
    }

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getHalfHeightRatio() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.history_app_filter_sheet_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return Resources.ID_NULL; // disabled
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.history_app_filter_sheet_opened;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.history_app_filter_sheet_closed;
    }
}
