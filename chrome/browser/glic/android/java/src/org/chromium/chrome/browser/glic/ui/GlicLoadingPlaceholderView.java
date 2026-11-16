// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.R;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.List;

/**
 * Native loading skeleton for the Glic bottom sheet and side panel. Pulses the skeleton blocks with
 * a staggered opacity animation while visible.
 */
@NullMarked
public class GlicLoadingPlaceholderView extends ConstraintLayout {
    @VisibleForTesting static final float HIGH_OPACITY = 1.0f;
    @VisibleForTesting static final float LOW_OPACITY = 0.6f;
    private static final int PULSE_HALF_DURATION_MS = 750;
    private static final long[] PULSE_STAGGER_DELAYS_MS = {0L, 100L, 300L, 500L};
    private static final AccelerateDecelerateInterpolator PULSE_INTERPOLATOR =
            new AccelerateDecelerateInterpolator();

    private final AnimationHandler mAnimationHandler = new AnimationHandler();
    private List<View> mBlocks;
    private ChromeImageButton mCloseButton;

    /**
     * Inflates a loading placeholder.
     *
     * @param context The context to inflate with.
     * @param backgroundColor The background color of the placeholder.
     * @param placeholderElemColor The fill color of the skeleton blocks.
     * @param onCloseClicked Run when the close button is clicked.
     */
    public static GlicLoadingPlaceholderView create(
            Context context,
            @ColorInt int backgroundColor,
            @ColorInt int placeholderElemColor,
            Runnable onCloseClicked) {
        GlicLoadingPlaceholderView view =
                (GlicLoadingPlaceholderView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.glic_loading_placeholder, null, false);
        view.setBackgroundColor(backgroundColor);
        for (View block : view.mBlocks) {
            ((GradientDrawable) block.getBackground().mutate()).setColor(placeholderElemColor);
        }
        view.mCloseButton.setOnClickListener(v -> onCloseClicked.run());
        return view;
    }

    public GlicLoadingPlaceholderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Initializer
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mCloseButton = findViewById(R.id.glic_loading_placeholder_close_button);
        mBlocks =
                List.of(
                        findViewById(R.id.glic_loading_placeholder_header_start),
                        findViewById(R.id.glic_loading_placeholder_header_end),
                        findViewById(R.id.glic_loading_placeholder_body_top),
                        findViewById(R.id.glic_loading_placeholder_body_bottom));
    }

    @Override
    public void onVisibilityAggregated(boolean isVisible) {
        super.onVisibilityAggregated(isVisible);
        if (isVisible && ValueAnimator.areAnimatorsEnabled()) {
            if (!mAnimationHandler.isAnimationPresent()) {
                mAnimationHandler.startAnimation(createPulseAnimatorSet());
            }
        } else {
            mAnimationHandler.forceFinishAnimation();
            for (View block : mBlocks) {
                block.setAlpha(HIGH_OPACITY);
            }
        }
    }

    private AnimatorSet createPulseAnimatorSet() {
        List<Animator> animators = new ArrayList<>(mBlocks.size());
        for (int i = 0; i < mBlocks.size(); i++) {
            View block = mBlocks.get(i);
            ValueAnimator pulse = ValueAnimator.ofFloat(HIGH_OPACITY, LOW_OPACITY);
            pulse.addUpdateListener(
                    animation -> block.setAlpha((float) animation.getAnimatedValue()));
            pulse.setStartDelay(PULSE_STAGGER_DELAYS_MS[i]);
            pulse.setDuration(PULSE_HALF_DURATION_MS);
            pulse.setInterpolator(PULSE_INTERPOLATOR);
            pulse.setRepeatCount(ValueAnimator.INFINITE);
            pulse.setRepeatMode(ValueAnimator.REVERSE);
            animators.add(pulse);
        }

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(animators);
        return animatorSet;
    }

    List<View> getPlaceholderBlocksForTesting() {
        return mBlocks;
    }

    ChromeImageButton getCloseButtonForTesting() {
        return mCloseButton;
    }

    boolean isAnimatingForTesting() {
        return mAnimationHandler.isAnimationPresent();
    }
}
