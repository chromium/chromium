// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * Bottom sheet content for the Save Passwords instructional promo. Displays a layout with
 * instructional steps and an animation.
 */
@NullMarked
public class SavePasswordsInstructionalBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final String mContentDescription;
    private final @StringRes int mOpenedFullAccessibilityStringId;
    private final @StringRes int mClosedAccessibilityStringId;

    /**
     * @param view The inflated view to be shown in the bottom sheet.
     * @param contentDescription The accessibility description for the sheet.
     * @param openedFullAccessibilityStringId The accessibility string announced when opened.
     * @param closedAccessibilityStringId The accessibility string announced when closed.
     */
    public SavePasswordsInstructionalBottomSheetContent(
            View view,
            String contentDescription,
            @StringRes int openedFullAccessibilityStringId,
            @StringRes int closedAccessibilityStringId) {
        mContentView = view;
        mContentDescription = contentDescription;
        mOpenedFullAccessibilityStringId = openedFullAccessibilityStringId;
        mClosedAccessibilityStringId = closedAccessibilityStringId;
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
    public void destroy() {}

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
        return mContentDescription;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return mOpenedFullAccessibilityStringId;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return mClosedAccessibilityStringId;
    }
}
