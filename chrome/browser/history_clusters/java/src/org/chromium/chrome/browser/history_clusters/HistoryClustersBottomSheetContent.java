// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * Bottom sheet content implementation for the HistoryClusters UI. This enables the bottom sheet
 * system to render our bottom sheet content and e.g. announce accessibility events correctly.
 */
class HistoryClustersBottomSheetContent implements BottomSheetContent {
    private View mContentView;

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.history_clusters_journeys_tab_label;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.history_clusters_journeys_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.history_clusters_journeys_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.history_clusters_journeys_closed;
    }

    void setContentView(View contentView) {
        mContentView = contentView;
    }
}
