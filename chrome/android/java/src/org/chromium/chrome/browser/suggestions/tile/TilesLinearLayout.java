// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.tile.TileView;

import java.util.ArrayList;
import java.util.List;

/**
 * LinearLayout with helpers to add and visit tile Views (TileView) mixed with other Views:
 *
 * <ul>
 *   <li>Tiles: MVT tiles with known width, separated by interval margin.
 *   <li>Dividers: Non-tiles that squeezes inside an interval margin, thus consuming no width.
 *   <li>UI Views: Non-tiles with arbitary width, separated by interval margin.
 * </ul>
 *
 * A container View for tiles can extend this class. To visit tile Views only, one would use {@link
 * #getTileCount()} and {@link #getTileAt()} instead of {@link #getChildCount()} and {@link
 * #getChildAt()}.
 */
@NullMarked
public class TilesLinearLayout extends LinearLayout {
    private final List<TileView> mTileList = new ArrayList<>();

    protected int mTileAndUiViewCount;
    protected @Px float mUiViewsTotalWidth;

    public TilesLinearLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void removeAllViews() {
        mTileList.clear();
        mTileAndUiViewCount = 0;
        mUiViewsTotalWidth = 0f;
        super.removeAllViews();
    }

    /**
     * Updates the start margin of all children except the first, to control spacing between.
     *
     * @param margin The margin to set.
     */
    void setIntervalMargins(@Px int margin) {
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
    void setEdgeMargins(@Px int margin) {
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
        addView(tileView);
        mTileList.add(tileView);
        ++mTileAndUiViewCount;
    }

    /**
     * Specialized addView() for dividers.
     *
     * @param divider The divider View to add.
     */
    public void addDivider(SuggestionsTileVerticalDivider divider) {
        addView(divider);
    }

    /**
     * Specialized addView() for UI Views.
     *
     * @param view The UI View to add.
     * @param width The width of the added View, in Px.
     */
    public void addUiView(View uiView, @Px float width) {
        addView(uiView);
        mUiViewsTotalWidth += width;
        ++mTileAndUiViewCount;
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

    protected void updateViewStartMargin(View view, @Px int newStartMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newStartMargin != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newStartMargin);
            view.setLayoutParams(layoutParams);
        }
    }

    protected void updateViewEndMargin(View view, @Px int newEndMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
        if (newEndMargin != layoutParams.getMarginEnd()) {
            layoutParams.setMarginEnd(newEndMargin);
            view.setLayoutParams(layoutParams);
        }
    }
}
