// Copyright 2018 The Chromium Authors
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

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.base.ViewUtils;

/**
 * The View representing a single explore sites category.
 * Consists of a large image icon over descriptive text.
 */
public class ExperimentalExploreSitesCategoryTileView extends LinearLayout {
    /** The data represented by this tile. */
    private ExploreSitesCategoryTile mCategoryData;

    private final Context mContext;
    private RoundedIconGenerator mIconGenerator;

    private TextView mTitleView;
    private ImageView mIconView;

    private int mIconWidthPx;
    private int mIconHeightPx;

    /** Constructor for inflating from XML. */
    public ExperimentalExploreSitesCategoryTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.experimental_explore_sites_category_tile_title);
        mIconView = findViewById(R.id.experimental_explore_sites_category_tile_icon);
    }

    public void initialize(ExploreSitesCategoryTile category, int widthPx) {
        Resources resources = mContext.getResources();
        mCategoryData = category;
        mIconWidthPx = widthPx
                - (2 * resources.getDimensionPixelSize(R.dimen.experimental_explore_sites_padding));
        mIconHeightPx = mIconWidthPx * 2 / 3;
        mIconGenerator = new RoundedIconGenerator(mIconWidthPx, mIconHeightPx,
                resources.getDimensionPixelSize(R.dimen.experimental_explore_sites_radius),
                mContext.getColor(R.color.default_favicon_background_color),
                resources.getDimensionPixelSize(R.dimen.tile_view_icon_text_size));
        updateIcon(null);
        mTitleView.setText(mCategoryData.getCategoryName());
    }

    public void updateIcon(Bitmap bitmap) {
        Resources resources = mContext.getResources();
        Drawable drawable;
        if (bitmap == null) {
            drawable = new BitmapDrawable(
                    resources, mIconGenerator.generateIconForText(mCategoryData.getCategoryName()));
        } else {
            drawable = ViewUtils.createRoundedBitmapDrawable(resources,
                    Bitmap.createScaledBitmap(bitmap, mIconWidthPx, mIconHeightPx, false),
                    resources.getDimensionPixelSize(R.dimen.experimental_explore_sites_radius));
        }
        mCategoryData.setIconDrawable(drawable);
        mIconView.setImageDrawable(drawable);
    }
}
