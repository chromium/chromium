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
    private List<TileView> mTileList = new ArrayList<TileView>();

    public TilesLinearLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void removeAllViews() {
        mTileList.clear();
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

    private void updateViewStartMargin(View view, int newStartMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newStartMargin != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newStartMargin);
            view.setLayoutParams(layoutParams);
        }
    }

    private void updateViewEndMargin(View view, int newEndMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newEndMargin != layoutParams.getMarginEnd()) {
            layoutParams.setMarginEnd(newEndMargin);
            view.setLayoutParams(layoutParams);
        }
    }
}
