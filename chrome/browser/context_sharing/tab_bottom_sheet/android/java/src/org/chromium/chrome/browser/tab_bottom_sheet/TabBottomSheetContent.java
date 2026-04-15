// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;

/** The bottom sheet content for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final float mFullHeightRatio;
    private final GlowSpec mGlowSpec;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     * @param fullHeightRatio The full height ratio for the bottom sheet.
     */
    public TabBottomSheetContent(View contentView, float fullHeightRatio) {
        mContentView = contentView;
        mFullHeightRatio = fullHeightRatio;
        // TODO(crbug.com/502611927): Remove or tweak this for AIM.
        mGlowSpec =
                new GlowSpec(
                        mContentView.getContext().getColor(R.color.default_bg_color_blue),
                        GlowSpec.ShadowSize.LONG);
    }

    @Override
    public @Nullable GlowSpec getSheetBackgroundGlowSpecOverride() {
        return mGlowSpec;
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
    public boolean hasCustomLifecycle() {
        return true;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // No scrim.
        return true;
    }

    @Override
    public @ColorInt int getSheetBackgroundColorOverride() {
        // TODO(crbug.com/502611927): This may need to be different for AIM.
        return mContentView.getContext().getColor(R.color.tab_bottom_sheet_bg);
    }

    @Override
    public void onBackPressed() {
        handleBackPress();
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        // TODO(crbug.com/489070365): Update min height based on java toolbar or webUi header.
        return Math.round(mContentView.getHeight() * 0.1f);
    }

    @Override
    public float getHalfHeightRatio() {
        // TODO(crbug.com/502611927): Update this for AIM.
        return ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()
                ? mFullHeightRatio
                : HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // TODO(crbug.com/502611927): Update this for AIM.
        return ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()
                ? HeightMode.DEFAULT
                : mFullHeightRatio;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return "";
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    // TODO(crbug.com/502611927): These strings may need to be different for different clients.

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.tab_bottom_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.tab_bottom_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.tab_bottom_sheet_closed;
    }

    @Override
    public boolean canBeSuppressed(BottomSheetContent nextContent) {
        return true;
    }

    @Override
    public boolean actsAsBrowserControls() {
        // TODO(crbug.com/491512853): Revisit if this is correct for landscape mode or in non-PEEK
        // states. For now, always returning true seems most correct.
        return true;
    }

    @Override
    public boolean allowInSheetContentSnackbars() {
        return false;
    }
}
