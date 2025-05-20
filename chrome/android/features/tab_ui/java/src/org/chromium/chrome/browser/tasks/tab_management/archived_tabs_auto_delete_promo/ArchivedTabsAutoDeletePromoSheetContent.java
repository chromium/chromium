// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** The bottom sheet content for the Auto Delete Archived Tabs Decision Promo. */
@NullMarked
public class ArchivedTabsAutoDeletePromoSheetContent implements BottomSheetContent {
    private final View mContentView;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     */
    public ArchivedTabsAutoDeletePromoSheetContent(View contentView) {
        mContentView = contentView;
    }

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
    public void onBackPressed() {
        handleBackPress();
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.archived_tabs_auto_delete_promo_description);
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.archived_tabs_auto_delete_promo_description;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.archived_tabs_auto_delete_promo_description;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.archived_tabs_auto_delete_promo_description;
    }
}
