// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.components.browser_ui.widget.tile.TileView;

/**
 * The layout for the container of MV tiles on the Start surface.
 */
public class MvTilesLayout extends LinearLayout {
    private final Context mContext;
    private final int mTileViewPaddingPortrait;
    private final int mTileViewPaddingLandscape;

    public MvTilesLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        Resources resources = mContext.getResources();
        mTileViewPaddingPortrait = resources.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_portrait);
        mTileViewPaddingLandscape = resources.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.tile_view_padding_landscape);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateTilesViewLayout(newConfig.orientation);
    }

    /**
     * Updates the paddings of all of the MV tiles according to the current screen orientation.
     * @param orientation The new orientation that paddings should be adjusted to.
     */
    void updateTilesViewLayout(int orientation) {
        int childCount = getChildCount();
        if (childCount == 0) return;

        int newMargin = orientation == Configuration.ORIENTATION_PORTRAIT
                ? mTileViewPaddingPortrait
                : mTileViewPaddingLandscape;
        for (int i = 0; i < childCount; i++) {
            TileView tileView = (TileView) getChildAt(i);
            updateSingleTileViewLayout(tileView, newMargin);
        }
    }

    private void updateSingleTileViewLayout(TileView tileView, int newMarginStart) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) tileView.getLayoutParams();

        if (newMarginStart != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newMarginStart);
            tileView.setLayoutParams(layoutParams);
        }
    }
}
