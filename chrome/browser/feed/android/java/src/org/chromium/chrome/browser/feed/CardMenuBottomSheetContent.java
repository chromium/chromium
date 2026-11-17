// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;

/** Provide data that the bottom sheet manager needs to show a bottom sheet. */
@NullMarked
public class CardMenuBottomSheetContent implements BottomSheetContent {
    private static final BottomSheetType BOTTOM_SHEET_TYPE =
            new BottomSheetType.Builder().setUserInitiated(true).build();

    private final View mContentView;
    private final Runnable mOnDestroy;

    /**
     * Creates bottom sheet content for a card context menu.
     *
     * @param view The content view shown inside the bottom sheet.
     * @param onDestroy Invoked when the sheet content is destroyed.
     */
    public CardMenuBottomSheetContent(View view, Runnable onDestroy) {
        mContentView = view;
        mOnDestroy = onDestroy;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        mOnDestroy.run();
    }

    @Override
    public BottomSheetType getSheetType() {
        return BOTTOM_SHEET_TYPE;
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.feed_card_menu_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.feed_card_menu_opened_half;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.feed_card_menu_opened_full;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.feed_card_menu_closed;
    }
}
