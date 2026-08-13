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
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ContextMenuItemType;
import org.chromium.ui.UiUtils;

import java.util.Set;

/**
 * A custom ListView to be able to set width and height using the contents. Width and height are
 * constrained to make sure the view fits the screen size with margins.
 */
@NullMarked
public class ContextMenuListView extends ListView {
    private final int mMinWidth;
    private final int mMaxWidth;
    private final int mFlyoutMaxWidth;
    private final int mLateralMargin;

    // Measured width of list items. Used by popup, dialog, and flyout context menus.
    private int mCalculatedItemWidth;

    // Whether this ListView is used for a flyout submenu.
    private boolean mIsFlyout;

    public ContextMenuListView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMinWidth = getResources().getDimensionPixelSize(R.dimen.menu_width_min);
        mMaxWidth = getResources().getDimensionPixelSize(R.dimen.menu_width_max);
        mFlyoutMaxWidth = getResources().getDimensionPixelSize(R.dimen.flyout_menu_max_width);
        mLateralMargin = getResources().getDimensionPixelSize(R.dimen.menu_horizontal_margin);
    }

    /** Sets whether this ListView represents a flyout submenu. */
    public void setIsFlyout(boolean isFlyout) {
        mIsFlyout = isFlyout;
        if (mIsFlyout && getAdapter() != null && mCalculatedItemWidth == 0) {
            mCalculatedItemWidth =
                    UiUtils.computeListAdapterContentDimensions(
                            getAdapter(), this, Set.of(ContextMenuItemType.HEADER))[0];
        }
    }

    @Override
    public void setAdapter(ListAdapter adapter) {
        super.setAdapter(adapter);
        if (adapter != null) {
            mCalculatedItemWidth =
                    UiUtils.computeListAdapterContentDimensions(
                            getAdapter(), this, Set.of(ContextMenuItemType.HEADER))[0];
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

        // This ListView is inside a FrameLayout (context_menu_frame) with a background drawable.
        // The background may have padding that we need to account for when calculating width.
        final View frame = ((View) getParent().getParent());
        assert frame.getId() == R.id.context_menu_frame;
        final int parentLateralPadding = frame.getPaddingLeft() + frame.getPaddingRight();

        int contentWidth = mCalculatedItemWidth + parentLateralPadding;
        int maxAllowedWidth = mIsFlyout ? mFlyoutMaxWidth : mMaxWidth;
        int menuWidth =
                UiUtils.computeMenuWidth(
                        contentWidth, mMinWidth, maxAllowedWidth, mLateralMargin, windowWidthPx);

        return Math.max(0, menuWidth - parentLateralPadding);
    }
}
