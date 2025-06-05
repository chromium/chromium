// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.base.ViewUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * LinearLayout with helpers to add and visit tile Views (TileView). A container View for tiles can
 * extend this class, and add a mixture of {tile, non-tile} Views as children. To visit tile Views
 * only, one would use {@link #getTileCount()} and {@link #getTileAt()} instead of {@link
 * #getChildCount()} and {@link #getChildAt()}.
 */
@NullMarked
public class TilesLinearLayout extends LinearLayout {
    private final List<TileView> mTileList = new ArrayList<TileView>();

    protected float mNonTileViewsTotalWidthDp;

    public TilesLinearLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void removeAllViews() {
        mTileList.clear();
        mNonTileViewsTotalWidthDp = 0f;
        super.removeAllViews();
    }

    /**
     * Updates the start margin of all children except the first, to control spacing between.
     *
     * @param margin The margin to set.
     */
    void setIntervalMargins(int margin) {
        int childCount = getChildCount();
        for (int i = 1; i < childCount; i++) {
            updateViewStartMargin(getChildAt(i), margin);
            updateViewEndMargin(getChildAt(i - 1), 0);
        }
    }

    /**
     * Updates the start (end) margin the first (last) child, to control spacing around content.
     *
     * @param margin The margin to set.
     */
    void setEdgeMargins(int margin) {
        int childCount = getChildCount();
        if (childCount > 0) {
            updateViewStartMargin(getChildAt(0), margin);
            updateViewEndMargin(getChildAt(childCount - 1), margin);
        }
    }

    /**
     * Specialized addView() for tiles.
     *
     * @param tileView The tile View to add.
     */
    public void addTile(TileView tileView) {
        super.addView(tileView);
        mTileList.add(tileView);
    }

    /**
     * Specialized getChildCount() for tiles.
     *
     * @return The count of TileView children, added from addTile().
     */
    public int getTileCount() {
        return mTileList.size();
    }

    /**
     * Specialized getChildAt() for tiles.
     *
     * @return The (index+1)-th TileView added via addTile().
     */
    public TileView getTileAt(int index) {
        return mTileList.get(index);
    }

    /**
     * Specialized addView() for non-tile Views.
     *
     * @param view The View to add.
     * @param widthDp The width of the View to add, in dp.
     */
    public void addNonTileViewWithWidth(View view, float widthDp) {
        super.addView(view);
        mNonTileViewsTotalWidthDp += widthDp;
    }

    /** Returns the total width of non-tile Views added, in pixel. */
    public int getNonTileViewsTotalWidthPx() {
        return ViewUtils.dpToPx(getContext(), mNonTileViewsTotalWidthDp);
    }

    private void updateViewStartMargin(View view, int newStartMarginPx) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newStartMarginPx != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newStartMarginPx);
            view.setLayoutParams(layoutParams);
        }
    }

    private void updateViewEndMargin(View view, int newEndMarginPx) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newEndMarginPx != layoutParams.getMarginEnd()) {
            layoutParams.setMarginEnd(newEndMarginPx);
            view.setLayoutParams(layoutParams);
        }
    }
}
