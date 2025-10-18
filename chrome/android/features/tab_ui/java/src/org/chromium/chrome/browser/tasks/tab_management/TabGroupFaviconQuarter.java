// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.UNSET;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.tab_ui.R;

import java.util.Arrays;

/**
 * A quarter of the combined start image element for tab group rows. It should display in one of the
 * corners of the start image element. The parent of this must be a ConstraintLayout.
 */
@NullMarked
public class TabGroupFaviconQuarter extends FrameLayout {
    private GradientDrawable mBackground;
    private ImageView mImageView;
    private TextView mTextView;
    private float mInnerRadius;
    private float mOuterRadius;

    private boolean mHasImageOnBackground;
    private boolean mContaimentEnabled;

    /** Constructor for inflation. */
    public TabGroupFaviconQuarter(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // Mutable drawable so corner modifications (e.g. radii) don't get applied to all corners.
        mBackground = (GradientDrawable) getBackground().mutate();
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

    /** Set whether the quarter is displayed on a containment row. */
    void setContainmentEnabled(boolean isEnabled) {
        mContaimentEnabled = isEnabled;
        updateBackgroundColor();
    }

    /** The displayed image is exclusive with the plus count. */
    void setImage(Drawable image) {
        mHasImageOnBackground = true;
        mImageView.setVisibility(View.VISIBLE);
        mImageView.setImageDrawable(image);
        hideText();
        updateBackgroundColor();
    }

    /** The displayed plus count is exclusive with the image. */
    void setPlusCount(int plusCount) {
        mHasImageOnBackground = false;
        hideImage();
        mTextView.setVisibility(View.VISIBLE);
        String text = getResources().getString(R.string.plus_hidden_tab_count, plusCount);
        mTextView.setText(text);
        updateBackgroundColor();
    }

    void clear() {
        mHasImageOnBackground = false;
        hideImage();
        hideText();
        updateBackgroundColor();
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

    private void updateBackgroundColor() {
        @ColorInt
        int color =
                TabUiThemeProvider.getTabGroupFaviconQuarterFillColor(
                        getContext(), mHasImageOnBackground, mContaimentEnabled);
        mBackground.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
    }
}
