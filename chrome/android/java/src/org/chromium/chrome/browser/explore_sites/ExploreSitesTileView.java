// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.tile.TileWithTextView;

/**
 * View for a category name and site tiles.
 */
public class ExploreSitesTileView extends TileWithTextView {
    private static final int TITLE_LINES = 2;
    private final int mIconCornerRadius;

    // Used to generate textual icons.
    private RoundedIconGenerator mIconGenerator;

    public ExploreSitesTileView(Context ctx, AttributeSet attrs) {
        super(ctx, attrs);
        TypedArray styleAttrs = ctx.obtainStyledAttributes(attrs, R.styleable.ExploreSitesTileView);
        mIconCornerRadius = styleAttrs.getDimensionPixelSize(
                R.styleable.ExploreSitesTileView_iconCornerRadius,
                getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius));
        styleAttrs.recycle();
    }

    public void initialize(RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }

    public void updateIcon(Bitmap iconImage, String text) {
        setIconDrawable(getDrawableForBitmap(iconImage, text));
    }

    public void setTitle(String titleText) {
        setTitle(titleText, TITLE_LINES);
    }

    public Drawable getDrawableForBitmap(Bitmap image, String text) {
        if (image == null) {
            return new BitmapDrawable(getResources(), mIconGenerator.generateIconForText(text));
        }
        // Icon corner radius must be scaled to the current size of the image from the final size,
        // because an arbitrary sized icon may be passed to the RoundedBitmapDrawableFactory, which
        // expects the radius to be scaled to the image being passed in, not the final view. This is
        // why we cannot use ViewUtils.createRoundedBitmapDrawable.
        float scaledIconCornerRadius;
        float iconSize = View.MeasureSpec.getSize(mIconView.getLayoutParams().width);
        if (iconSize == 0) {
            scaledIconCornerRadius = mIconCornerRadius;
        } else {
            scaledIconCornerRadius = image.getWidth() / iconSize * mIconCornerRadius;
        }
        RoundedBitmapDrawable roundedIcon =
                RoundedBitmapDrawableFactory.create(getResources(), image);
        roundedIcon.setCornerRadius(scaledIconCornerRadius);
        return roundedIcon;
    }
}
