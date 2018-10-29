// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

/**
 * The View representing a single explore sites category.
 * Consists of a large image icon over descriptive text.
 */
public class ExperimentalExploreSitesCategoryTileView extends LinearLayout {
    /** The data represented by this tile. */
    private ExploreSitesCategoryTile mCategoryData;

    private Resources mResources;
    private RoundedIconGenerator mIconGenerator;

    private TextView mTitleView;
    private ImageView mIconView;

    private int mIconWidthPx;
    private int mIconHeightPx;

    /** Constructor for inflating from XML. */
    public ExperimentalExploreSitesCategoryTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mResources = context.getResources();
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.experimental_explore_sites_category_tile_title);
        mIconView = findViewById(R.id.experimental_explore_sites_category_tile_icon);
    }

    public void initialize(ExploreSitesCategoryTile category, int widthPx) {
        mCategoryData = category;
        mIconWidthPx = widthPx
                - (2
                          * mResources.getDimensionPixelSize(
                                    R.dimen.experimental_explore_sites_padding));
        mIconHeightPx = mIconWidthPx * 2 / 3;
        mIconGenerator = new RoundedIconGenerator(mIconWidthPx, mIconHeightPx,
                mResources.getDimensionPixelSize(R.dimen.experimental_explore_sites_radius),
                ApiCompatibilityUtils.getColor(
                        mResources, R.color.default_favicon_background_color),
                mResources.getDimensionPixelSize(R.dimen.tile_view_icon_text_size));
        updateIcon(null);
        mTitleView.setText(mCategoryData.getCategoryName());
    }

    public void updateIcon(Bitmap bitmap) {
        Drawable drawable;
        if (bitmap == null) {
            drawable = new BitmapDrawable(mResources,
                    mIconGenerator.generateIconForText(mCategoryData.getCategoryName()));
        } else {
            drawable = ViewUtils.createRoundedBitmapDrawable(
                    Bitmap.createScaledBitmap(bitmap, mIconWidthPx, mIconHeightPx, false),
                    mResources.getDimensionPixelSize(R.dimen.experimental_explore_sites_radius));
        }
        mCategoryData.setIconDrawable(drawable);
        mIconView.setImageDrawable(drawable);
    }
}
