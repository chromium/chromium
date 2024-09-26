// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sort_ui;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.Transformation;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.base.ViewUtils;

/** View class representing an expandable/collapsible view holding option chips for the feed. */
public class FeedOptionsView extends LinearLayout {
    private LinearLayout mChipsContainer;
    private static final int ANIMATION_DURATION_MS = 200;

    public FeedOptionsView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mChipsContainer = findViewById(R.id.chips_container);
    }

    public ChipView createNewChip() {
        ChipView chip = new ChipView(getContext(), null, 0, R.style.SuggestionChip);

        mChipsContainer.addView(chip);
        ViewGroup.MarginLayoutParams marginParams =
                (ViewGroup.MarginLayoutParams) chip.getLayoutParams();
        marginParams.setMarginEnd(
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.feed_options_chip_margin));
        chip.setLayoutParams(marginParams);
        return chip;
    }

    void setVisibility(boolean isVisible) {
        boolean currentVisibility = getVisibility() == VISIBLE;
        if (currentVisibility == isVisible) return;
        if (isVisible) {
            expand();
        } else {
            collapse();
        }
    }

    /** Expands this view to full height. */
    private void expand() {
        // If the view's parent is not shown, we want to set this view as VISIBLE without the
        // animation, and reset the height if it was previously set by collapse() animation.
        if (getParent() == null || !((View) getParent()).isShown()) {
            setVisibility(VISIBLE);
            setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
            return;
        }

        // Width is match_parent and height is wrap_content.
        int widthMeasureSpec =
                View.MeasureSpec.makeMeasureSpec(
                        ((ViewGroup) getParent()).getWidth(), View.MeasureSpec.EXACTLY);
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
                        ViewUtils.requestLayout(
                                FeedOptionsView.this,
                                "FeedOptionsView.expand.Animation.applyTransformation");
                    }

                    @Override
                    public boolean willChangeBounds() {
                        return true;
                    }
                };

        animation.setDuration(ANIMATION_DURATION_MS);
        startAnimation(animation);
    }

    /** Collapses this view to 0 height and then marks it GONE. */
    private void collapse() {
        // If the view's parent is not shown, we want to set this view as GONE without the
        // animation.
        if (getParent() == null || !((View) getParent()).isShown()) {
            setVisibility(GONE);
            return;
        }

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
                            ViewUtils.requestLayout(
                                    FeedOptionsView.this,
                                    "FeedOptionsView.collapse.Animation.applyTransformation");
                        }
                    }

                    @Override
                    public boolean willChangeBounds() {
                        return true;
                    }
                };

        animation.setDuration(ANIMATION_DURATION_MS);
        startAnimation(animation);
    }
}
