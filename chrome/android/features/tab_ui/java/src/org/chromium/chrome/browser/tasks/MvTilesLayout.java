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
    private int mScreenOrientation;

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
    protected void onFinishInflate() {
        super.onFinishInflate();
        updateTilesViewLayout(mContext.getResources().getConfiguration().orientation);
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
    private void updateTilesViewLayout(int orientation) {
        if (mScreenOrientation == orientation) return;

        mScreenOrientation = orientation;
        int childCount = getChildCount();
        if (childCount == 0) return;

        int newMargin = mScreenOrientation == Configuration.ORIENTATION_PORTRAIT
                ? mTileViewPaddingPortrait
                : mTileViewPaddingLandscape;
        for (int i = 0; i < childCount; i++) {
            TileView tileView = (TileView) getChildAt(i);
            updateSingleTileViewLayout(tileView, newMargin);
        }
    }

    /**
     * Updates the given TileView's layout according to the current screen orientation.
     * @param tileView The TileView to update.
     */
    void updateSingleTileViewLayout(TileView tileView) {
        if (tileView == null) return;

        // Updates the spacing between tiles according to the screen orientation.
        int newPadding = mScreenOrientation == Configuration.ORIENTATION_PORTRAIT
                ? mTileViewPaddingPortrait
                : mTileViewPaddingLandscape;
        updateSingleTileViewLayout(tileView, newPadding);
    }

    private void updateSingleTileViewLayout(TileView tileView, int newMarginStart) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) tileView.getLayoutParams();

        if (newMarginStart != layoutParams.getMarginStart()) {
            layoutParams.setMarginStart(newMarginStart);
            tileView.setLayoutParams(layoutParams);
        }
    }
}
