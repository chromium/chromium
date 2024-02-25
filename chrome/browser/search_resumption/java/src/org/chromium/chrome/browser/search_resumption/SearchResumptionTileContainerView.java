// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.Transformation;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

/** The view for the section of a set of search suggestions on the search resumption module. */
public class SearchResumptionTileContainerView extends LinearLayout {
    private static final int ANIMATION_DURATION_MS = 200;

    private boolean mIsExpanded;

    public SearchResumptionTileContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Inflates a {@link SearchResumptionTileView} instance. */
    SearchResumptionTileView buildTileView() {
        return (SearchResumptionTileView)
                LayoutInflater.from(getContext())
                        .inflate(R.layout.search_resumption_module_tile_layout, this, false);
    }

    boolean isExpanded() {
        return mIsExpanded;
    }

    void destroy() {
        for (int i = 0; i < getChildCount(); i++) {
            ((SearchResumptionTileView) getChildAt(i)).destroy();
        }
        removeAllViews();
    }

    /** Expands or collapses the view, with an animation if enabled. */
    void configureExpandedCollapsed(boolean expand, boolean isAnimationEnabled) {
        if (mIsExpanded == expand) return;

        mIsExpanded = expand;
        if (expand) {
            expand(isAnimationEnabled);
        } else {
            collapse(isAnimationEnabled);
        }
    }

    /** Expands this view to full height. */
    private void expand(boolean isAnimationEnabled) {
        // Width is match_parent and height is wrap_content.
        int widthMeasureSpec =
                View.MeasureSpec.makeMeasureSpec(getWidth(), View.MeasureSpec.EXACTLY);
        int heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        measure(widthMeasureSpec, heightMeasureSpec);
        int targetHeight = getMeasuredHeight();

        // Older (pre-API21) Android versions cancel animations with height of 0.
        getLayoutParams().height = 1;
        setVisibility(VISIBLE);

        Animation animation =
                new Animation() {
                    @Override
                    protected void applyTransformation(float interpolatedTime, Transformation t) {
                        int height;
                        if (interpolatedTime == 1) {
                            height = ViewGroup.LayoutParams.WRAP_CONTENT;
                        } else {
                            height = (int) (targetHeight * interpolatedTime);
                        }
                        getLayoutParams().height = height;
                        requestLayout();
                    }

                    @Override
                    public boolean willChangeBounds() {
                        return true;
                    }
                };

        animation.setDuration(isAnimationEnabled ? ANIMATION_DURATION_MS : 0);
        startAnimation(animation);
    }

    /** Collapses this view to 0 height and then marks it GONE. */
    private void collapse(boolean isAnimationEnabled) {
        int initialHeight = getMeasuredHeight();

        Animation animation =
                new Animation() {
                    @Override
                    protected void applyTransformation(float interpolatedTime, Transformation t) {
                        if (interpolatedTime == 1) {
                            setVisibility(GONE);
                        } else {
                            getLayoutParams().height =
                                    initialHeight - (int) (initialHeight * interpolatedTime);
                            requestLayout();
                        }
                    }

                    @Override
                    public boolean willChangeBounds() {
                        return true;
                    }
                };

        animation.setDuration(isAnimationEnabled ? ANIMATION_DURATION_MS : 0);
        startAnimation(animation);
    }
}
