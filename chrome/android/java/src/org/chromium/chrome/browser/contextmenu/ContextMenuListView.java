// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.ui.UiUtils;

/**
 * A custom ListView to be able to set width and height using the contents. Width and height are
 * constrained to make sure the view fits the screen size with margins.
 */
@NullMarked
public class ContextMenuListView extends ListView {
    // Whether the max width of this list view is limited by screen width.
    private final boolean mLimitedByScreenWidth;
    private boolean mIsFlyout;

    public ContextMenuListView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mLimitedByScreenWidth = ContextMenuUtils.isPopupSupported(context);
    }

    /**
     * Whether the {@link ListView} is a flyout. For flyout popups, the width has to wrap the
     * content up to the maximum width.
     */
    public void setIsFlyout(boolean isFlyout) {
        mIsFlyout = isFlyout;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        widthMeasureSpec = MeasureSpec.makeMeasureSpec(calculateWidth(), MeasureSpec.EXACTLY);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * The value returned by this method is used to set the width of the context menu in {@link
     * onMeasure()}
     *
     * @return The width of the context menu in pixels
     */
    private int calculateWidth() {
        final int windowWidthPx = getResources().getDisplayMetrics().widthPixels;
        final int maxWidthFromRes =
                getResources().getDimensionPixelSize(R.dimen.context_menu_max_width);
        final int lateralMargin =
                getResources().getDimensionPixelSize(R.dimen.context_menu_lateral_margin);

        // This ListView should be inside a FrameLayout with the menu_bg_tinted background. Since
        // the background is a 9-patch, it gets some extra padding automatically, and we should
        // take it into account when calculating the width here. This flow is not applicable if the
        // background does not comes with a 9-patch (e.g. frame's width is 0).
        final View frame = ((View) getParent().getParent());
        assert frame.getId() == R.id.context_menu_frame;
        final int frameWidth = frame.getMeasuredWidth();
        final int parentLateralPadding = frame.getPaddingLeft() + frame.getPaddingRight();
        final int maxWidth =
                (frameWidth == 0 ? maxWidthFromRes : Math.min(maxWidthFromRes, frameWidth))
                        - parentLateralPadding;

        // When context menu is a popup, the max width with windowWidth - 2 * lateralMargin does not
        // apply since it is presented in a popup window. See https://crbug.com/1314675.
        int calculatedWidth;
        if (mLimitedByScreenWidth) {
            if (mIsFlyout) {
                int maxFlyoutWidth =
                        getResources().getDimensionPixelSize(R.dimen.context_menu_small_width)
                                - parentLateralPadding;
                int contentWidth =
                        UiUtils.computeListAdapterContentDimensions(getAdapter(), this)[0];
                calculatedWidth = Math.min(maxFlyoutWidth, contentWidth);
            } else {
                calculatedWidth = maxWidth;
            }
        } else {
            calculatedWidth = windowWidthPx - 2 * lateralMargin - parentLateralPadding;
        }

        return Math.min(calculatedWidth, maxWidth);
    }
}
