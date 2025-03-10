// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.util.AttributeSet;
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

    void addTile(TileView tileView) {
        super.addView(tileView);
        mTileList.add(tileView);
    }

    int getTileCount() {
        return mTileList.size();
    }

    TileView getTileAt(int index) {
        return mTileList.get(index);
    }
}
