// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Coordinator for the Tips Opt In bottom sheet. */
@NullMarked
public class TipsOptInCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final TipsOptInSheetContent mSheetContent;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     */
    public TipsOptInCoordinator(Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;

        View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.tips_opt_in_bottom_sheet, /* root= */ null);
        mSheetContent = new TipsOptInSheetContent(contentView);
    }

    /** Cleans up resources. */
    public void destroy() {}

    /** Shows the promo. The caller is responsible for all eligibility checks. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);

        // Mark that the promo has been shown.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, true);
    }

    @NullMarked
    private class TipsOptInSheetContent implements BottomSheetContent {
        private final View mContentView;

        TipsOptInSheetContent(View contentView) {
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
        public void destroy() {
            TipsOptInCoordinator.this.destroy();
        }

        @Override
        public int getPriority() {
            return BottomSheetContent.ContentPriority.HIGH;
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public void onBackPressed() {}

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public String getSheetContentDescription(Context context) {
            return context.getString(R.string.tips_opt_in_bottom_sheet_content_description);
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.tips_opt_in_bottom_sheet_closed_content_description;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.tips_opt_in_bottom_sheet_half_height_content_description;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.tips_opt_in_bottom_sheet_full_height_content_description;
        }
    }
}
