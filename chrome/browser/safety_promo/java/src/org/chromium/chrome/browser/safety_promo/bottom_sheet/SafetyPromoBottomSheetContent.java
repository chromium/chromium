// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;

/** {@link BottomSheetContent} of the Safety Promo "Show me how" bottom sheet. */
@NullMarked
class SafetyPromoBottomSheetContent implements BottomSheetContent {
    private static final BottomSheetType SHEET_TYPE =
            new BottomSheetType.Builder().setUserInitiated(/* isUserInitiated= */ true).build();

    private final View mContentView;
    private final Runnable mOnDestroyRunnable;

    /**
     * @param contentView The view of the sheet.
     * @param onDestroy Called when the controller destroys the content, i.e. after the sheet is
     *     closed for any reason.
     */
    SafetyPromoBottomSheetContent(View contentView, Runnable onDestroy) {
        mContentView = contentView;
        mOnDestroyRunnable = onDestroy;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    // TODO(crbug.com/534388538): Return the scroll offset of the content once it is wrapped in a
    // scrollable container.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        mOnDestroyRunnable.run();
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public BottomSheetType getSheetType() {
        return SHEET_TYPE;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    // TODO(crbug.com/534388538): Replace the placeholder accessibility strings below, borrowed from
    // the NTP setup list sheet, with Safety FRE strings.
    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // The half height state is disabled, so this is never announced.
        return R.string.educational_tip_see_more_bottom_sheet_accessibility_opened;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.educational_tip_see_more_bottom_sheet_accessibility_opened;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.educational_tip_see_more_bottom_sheet_accessibility_closed;
    }
}
