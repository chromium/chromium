// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** The bottom sheet content for the tab bottom sheet. */
@NullMarked
public abstract class TabBottomSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final float mFullHeightRatio;
    private final @ColorInt int mBackgroundColor;
    private final @Px int mPeekViewHeight;
    private final @IdRes int mEmptyPlaceholderContainerId;
    private final boolean mUsePlaceholder;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     * @param fullHeightRatio The full height ratio for the bottom sheet.
     * @param backgroundColor The background color for the bottom sheet.
     * @param peekViewHeight The height of the peek view in pixels.
     * @param peekViewContainerId The resource ID for the peek view container.
     * @param emptyPlaceholderContainerId The resource ID for the empty placeholder container.
     */
    public TabBottomSheetContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            @IdRes int emptyPlaceholderContainerId) {
        mContentView = contentView;
        mFullHeightRatio = fullHeightRatio;
        mBackgroundColor = backgroundColor;
        mPeekViewHeight = peekViewHeight;
        mEmptyPlaceholderContainerId = emptyPlaceholderContainerId;

        View view = mContentView.findViewById(peekViewContainerId);
        View peekContainer = NullUtil.assertNonNull(view);
        peekContainer.setBackgroundColor(mBackgroundColor);

        TextViewWithCompoundDrawables placeholder =
                NullUtil.assertNonNull(mContentView.findViewById(mEmptyPlaceholderContainerId));
        mUsePlaceholder = setupPlaceholder(placeholder);
    }

    /**
     * Sets up the visual properties of the inactive state placeholder.
     *
     * @param placeholder The empty placeholder text view with compound drawables support.
     * @return true if the placeholder should be visible, false otherwise.
     */
    protected boolean setupPlaceholder(TextViewWithCompoundDrawables placeholder) {
        return false;
    }

    /** Returns whether the content uses an empty placeholder. */
    public boolean usePlaceholder() {
        return mUsePlaceholder;
    }

    @Override
    public abstract @Nullable GlowSpec getSheetBackgroundGlowSpecOverride();

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

    @Override
    public abstract @StringRes int getSheetHalfHeightAccessibilityStringId();

    @Override
    public abstract @StringRes int getSheetFullHeightAccessibilityStringId();

    @Override
    public abstract @StringRes int getSheetClosedAccessibilityStringId();

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

    @Override
    public boolean shouldRestoreStateOnUnsuppress() {
        return false;
    }

    public @Nullable TextViewWithCompoundDrawables getPlaceholderViewForTesting() {
        return (TextViewWithCompoundDrawables)
                mContentView.findViewById(mEmptyPlaceholderContainerId);
    }
}
