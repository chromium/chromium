// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Bottom sheet content of the default browser promo card. */
@NullMarked
public class DefaultBrowserPromoBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    public DefaultBrowserPromoBottomSheetContent(View view) {
        mContentView = view;
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
        return context.getString(
                R.string.educational_tip_default_browser_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        assert false : "This method will not be called.";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.educational_tip_default_browser_bottom_sheet_accessibility_opened_full;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.educational_tip_default_browser_bottom_sheet_accessibility_closed;
    }
}
