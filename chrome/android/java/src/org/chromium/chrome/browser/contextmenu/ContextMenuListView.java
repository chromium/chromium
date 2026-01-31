// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ListAdapter;
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
    // The minimum and maximum widths of the context menu frame by type (popup or dialog).
    private final int mMaxWidthFromRes;

    // Maximum width of items and padding. Used by popup context menu only.
    private int mCalculatedWidthPopup;

    // Margin between dialog type context menu and screen. Used by dialog context menu only.
    private final int mLateralMargin;

    public ContextMenuListView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mLimitedByScreenWidth = ContextMenuUtils.isPopupSupported(context);
        mMaxWidthFromRes =
                mLimitedByScreenWidth
                        ? getResources().getDimensionPixelSize(R.dimen.context_menu_popup_max_width)
                        : getResources().getDimensionPixelSize(R.dimen.context_menu_max_width);
        mLateralMargin = getResources().getDimensionPixelSize(R.dimen.context_menu_lateral_margin);
    }

    @Override
    public void setAdapter(ListAdapter adapter) {
        super.setAdapter(adapter);
        if (mLimitedByScreenWidth && adapter != null) {
            mCalculatedWidthPopup =
                    Math.max(
                            UiUtils.computeListAdapterContentDimensions(getAdapter(), this)[0],
                            getResources()
                                    .getDimensionPixelSize(R.dimen.context_menu_popup_min_width));
        }
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

        // This ListView should be inside a FrameLayout with the menu_bg_tinted background. Since
        // the background is a 9-patch, it gets some extra padding automatically, and we should
        // take it into account when calculating the width here. This flow is not applicable if the
        // background does not comes with a 9-patch (e.g. frame's width is 0).
        final View frame = ((View) getParent().getParent());
        assert frame.getId() == R.id.context_menu_frame;
        final int frameWidth = frame.getMeasuredWidth();
        final int parentLateralPadding = frame.getPaddingLeft() + frame.getPaddingRight();
        final int maxWidth =
                (frameWidth == 0 ? mMaxWidthFromRes : Math.min(mMaxWidthFromRes, frameWidth))
                        - parentLateralPadding;

        // When context menu is a dialog, width (with lateral margins) should fill out the window.
        // Otherwise (it is a popup), compute the width according to the list content. Note this
        // already includes the parent lateral padding (crbug.com/454336316).
        int calculatedWidth =
                mLimitedByScreenWidth
                        ? mCalculatedWidthPopup
                        : windowWidthPx - 2 * mLateralMargin - parentLateralPadding;

        // Clamp calculated width into the valid range.
        return Math.min(calculatedWidth, maxWidth);
    }
}
