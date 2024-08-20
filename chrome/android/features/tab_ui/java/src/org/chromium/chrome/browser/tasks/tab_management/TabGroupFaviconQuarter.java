// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.UNSET;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DimenRes;
import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;

import java.util.Arrays;

/**
 * A quarter of the combined start image element for tab group rows. It should display in one of the
 * corners of the start image element. The parent of this must be a ConstraintLayout.
 */
public class TabGroupFaviconQuarter extends FrameLayout {
    private GradientDrawable mBackground;
    private ImageView mImageView;
    private TextView mTextView;
    private float mInnerRadius;
    private float mOuterRadius;

    /** Constructor for inflation. */
    public TabGroupFaviconQuarter(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mBackground = (GradientDrawable) getBackground();
        mImageView = findViewById(R.id.favicon_image);
        mTextView = findViewById(R.id.hidden_tab_count);
        mInnerRadius = getResources().getDimension(R.dimen.tab_group_quarter_inner_radius);
        mOuterRadius = getResources().getDimension(R.dimen.tab_group_quarter_outer_radius);
    }

    void adjustPositionForCorner(@Corner int corner, @IdRes int parentId) {
        mBackground.setCornerRadii(buildCornerRadii(corner));
        ConstraintLayout.LayoutParams params = (ConstraintLayout.LayoutParams) getLayoutParams();
        setConstraintForCorner(params, corner, parentId);
        setLayoutParams(params);
    }

    /** The displayed image is exclusive with the plus count. */
    void setImage(Drawable image) {
        mImageView.setVisibility(View.VISIBLE);
        mImageView.setImageDrawable(image);
        hideText();
        updateBackgroundColor(R.dimen.default_elevation_0);
    }

    /** The displayed plus count is exclusive with the image. */
    void setPlusCount(int plusCount) {
        hideImage();
        mTextView.setVisibility(View.VISIBLE);
        String text = getResources().getString(R.string.plus_hidden_tab_count, plusCount);
        mTextView.setText(text);
        updateBackgroundColor(R.dimen.default_elevation_1);
    }

    void clear() {
        hideImage();
        hideText();
        updateBackgroundColor(R.dimen.default_elevation_1);
    }

    private void hideImage() {
        mImageView.setVisibility(View.INVISIBLE);
        mImageView.setImageDrawable(null);
    }

    private void hideText() {
        mTextView.setVisibility(View.INVISIBLE);
        mTextView.setText(null);
    }

    private void setConstraintForCorner(
            ConstraintLayout.LayoutParams params, @Corner int corner, @IdRes int parentId) {
        switch (corner) {
            case Corner.TOP_LEFT:
                setConstraints(params, parentId, parentId, UNSET, UNSET);
                break;
            case Corner.TOP_RIGHT:
                setConstraints(params, UNSET, parentId, parentId, UNSET);
                break;
            case Corner.BOTTOM_RIGHT:
                setConstraints(params, UNSET, UNSET, parentId, parentId);
                break;
            case Corner.BOTTOM_LEFT:
                setConstraints(params, parentId, UNSET, UNSET, parentId);
                break;
        }
    }

    private void setConstraints(
            ConstraintLayout.LayoutParams params,
            @IdRes int left,
            @IdRes int top,
            @IdRes int right,
            @IdRes int bottom) {
        params.leftToLeft = left;
        params.topToTop = top;
        params.rightToRight = right;
        params.bottomToBottom = bottom;
    }

    private float[] buildCornerRadii(@Corner int corner) {
        // Each corner takes two values, the x and y value, though we'll always make them match.
        float[] radii = new float[8];
        Arrays.fill(radii, mInnerRadius);
        int cornerStartIndex = corner * 2;
        radii[cornerStartIndex] = mOuterRadius;
        radii[cornerStartIndex + 1] = mOuterRadius;
        return radii;
    }

    private void updateBackgroundColor(@DimenRes int elevation) {
        mBackground.setColor(ChromeColors.getSurfaceColor(getContext(), elevation));
    }
}
