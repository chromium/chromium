// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;

import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;

import java.util.function.BooleanSupplier;

/** The bottom sheet content for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final float mFullHeightRatio;
    private final int mPeekViewHeight;
    private final @ColorInt int mBackgroundColor;
    private final @Nullable GlowSpec mGlowSpec;
    private final BooleanSupplier mCanNotBeSuppressedSupplier;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     * @param fullHeightRatio The full height ratio for the bottom sheet.
     * @param backgroundColor The background color for the bottom sheet.
     * @param clientType The client using the bottom sheet.
     * @param canNotBeSuppressedSupplier Supplier for whether the bottom sheet can be suppressed.
     */
    public TabBottomSheetContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @TabBottomSheetClientType int clientType,
            BooleanSupplier canNotBeSuppressedSupplier) {
        mContentView = contentView;
        mFullHeightRatio = fullHeightRatio;
        mBackgroundColor = backgroundColor;
        mCanNotBeSuppressedSupplier = canNotBeSuppressedSupplier;
        // TODO(crbug.com/502611927): Remove or tweak this for AIM.
        mGlowSpec =
                clientType == TabBottomSheetClientType.GLIC
                        ? new GlowSpec(
                                mContentView.getContext().getColor(R.color.default_bg_color_blue),
                                GlowSpec.ShadowSize.LONG)
                        : null;
        mPeekViewHeight =
                mContentView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total);

        View view = mContentView.findViewById(R.id.actor_control_container);
        View peekContainer = NullUtil.assertNonNull(view);
        peekContainer.setBackgroundColor(mBackgroundColor);
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
        return BottomSheetContent.ContentPriority.COBROWSE;
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
        return mBackgroundColor;
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
        return mPeekViewHeight;
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
                ? HeightMode.RESIZE_CONTENT
                : HeightMode.WRAP_CONTENT;
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
        return !mCanNotBeSuppressedSupplier.getAsBoolean();
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
