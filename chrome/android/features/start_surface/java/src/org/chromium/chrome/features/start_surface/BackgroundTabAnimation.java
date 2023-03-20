// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.TimeInterpolator;
import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.util.Property;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.Interpolators;

import java.util.ArrayList;
import java.util.Collection;

/**
 * Creates an Animator for the tab-opened-in-background animation on Start Surface.
 * Based on {@link SimpleAnimationLayout#tabCreatedInBackground}.
 */
public class BackgroundTabAnimation {
    /** Duration of the first step of the background animation: zooming out, rotating in */
    private static final long BACKGROUND_STEP1_DURATION = 300;
    /** Duration of the second step of the background animation: pause */
    private static final long BACKGROUND_STEP2_DURATION = 150;
    /** Duration of the third step of the background animation: zooming in, sliding out */
    private static final long BACKGROUND_STEP3_DURATION = 300;
    /** Percentage of the screen covered by the new tab */
    private static final float BACKGROUND_COVER_PCTG = 0.5f;

    /** Factor by which to scale tasks surface when it briefly shrinks to the center. */
    private static final float TASKS_SURFACE_SCALE = 0.9f;

    private static final float TASKS_SURFACE_Z = 12f;

    /**
     * Create an animation to show that a tab has been created in the background.
     * @param layout {@link Layout} from which the tab was created.
     * @param view View that will appear to momentarily shrink.
     * @param lastTapX X position of the last touch down event in px.
     * @param lastTapY Y position of the last touch down event in px.
     * @param portrait True for portrait mode, false for landscape.
     * @return An @{link Animator} for the background tab animation.
     */
    public static Animator create(
            Layout layout, ViewGroup view, float lastTapX, float lastTapY, boolean portrait) {
        Context context = layout.getContext();
        final float margin = Math.min(layout.getWidth(), layout.getHeight())
                * (1.0f - TASKS_SURFACE_SCALE) / 2.0f;

        Collection<Animator> animationList = new ArrayList<>(5);

        // Step 1: zoom out the source tab and bring in the new tab
        animationList.add(
                animate(view, View.SCALE_X, 1f, TASKS_SURFACE_SCALE, BACKGROUND_STEP1_DURATION));
        animationList.add(
                animate(view, View.SCALE_Y, 1f, TASKS_SURFACE_SCALE, BACKGROUND_STEP1_DURATION));

        animationList.add(animate(view, View.X, 0f, margin, BACKGROUND_STEP1_DURATION));
        animationList.add(animate(view, View.Y, 0f, margin, BACKGROUND_STEP1_DURATION));
        animationList.add(animate(view, View.Z, 0f, TASKS_SURFACE_Z, BACKGROUND_STEP1_DURATION));

        AnimatorSet step1Source = new AnimatorSet();
        step1Source.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        step1Source.playTogether(animationList);

        float pauseX = margin;
        float pauseY = margin;

        if (portrait) {
            pauseY = BACKGROUND_COVER_PCTG * layout.getHeight();
        } else {
            pauseX = BACKGROUND_COVER_PCTG * layout.getWidth();
        }

        // Make a rectangle to represent the new tab.
        GradientDrawable rectDrawable = new GradientDrawable();
        rectDrawable.setShape(GradientDrawable.RECTANGLE);
        rectDrawable.setSize(view.getWidth(), view.getHeight());
        rectDrawable.setColor(
                ChromeColors.getPrimaryBackgroundColor(context, /*isIncognito=*/false));

        ImageView newTabView = new AppCompatImageView(context);
        newTabView.setImageDrawable(rectDrawable);

        view.addView(newTabView);

        animationList = new ArrayList<>(5);
        animationList.add(animate(newTabView, View.ALPHA, 0f, 1f, BACKGROUND_STEP1_DURATION / 2));
        animationList.add(animate(
                newTabView, View.SCALE_X, 0f, TASKS_SURFACE_SCALE, BACKGROUND_STEP1_DURATION));
        animationList.add(animate(
                newTabView, View.SCALE_Y, 0f, TASKS_SURFACE_SCALE, BACKGROUND_STEP1_DURATION));
        animationList.add(animate(newTabView, View.X, lastTapX, pauseX, BACKGROUND_STEP1_DURATION));
        animationList.add(animate(newTabView, View.Y, lastTapY, pauseY, BACKGROUND_STEP1_DURATION));

        AnimatorSet step1New = new AnimatorSet();
        step1New.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        step1New.playTogether(animationList);

        AnimatorSet step1 = new AnimatorSet();
        step1.playTogether(step1New, step1Source);

        // step 2: pause

        // step 3: zoom in the source tab and slide down the new tab
        animationList = new ArrayList<>(7);

        TimeInterpolator interpolator = Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR;

        animationList.add(animate(view, View.SCALE_X, TASKS_SURFACE_SCALE, 1f,
                BACKGROUND_STEP3_DURATION, interpolator));
        animationList.add(animate(view, View.SCALE_Y, TASKS_SURFACE_SCALE, 1f,
                BACKGROUND_STEP3_DURATION, interpolator));
        animationList.add(
                animate(view, View.X, margin, 0f, BACKGROUND_STEP3_DURATION, interpolator));
        animationList.add(
                animate(view, View.Y, margin, 0f, BACKGROUND_STEP3_DURATION, interpolator));
        animationList.add(animate(
                view, View.Z, TASKS_SURFACE_Z, 0f, BACKGROUND_STEP3_DURATION, interpolator));

        animationList.add(animate(newTabView, View.ALPHA, 1f, 0f, BACKGROUND_STEP3_DURATION));

        // Rectangle flies away toward the tab switcher button.
        if (portrait) {
            animationList.add(animate(newTabView, View.Y, pauseY, -view.getHeight(),
                    BACKGROUND_STEP3_DURATION, Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR));
        } else {
            animationList.add(animate(newTabView, View.X, pauseX, view.getWidth(),
                    BACKGROUND_STEP3_DURATION, Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR));
        }

        AnimatorSet step3 = new AnimatorSet();
        step3.setStartDelay(BACKGROUND_STEP2_DURATION);
        step3.playTogether(animationList);

        AnimatorSet tabCreatedBackgroundAnimation = new AnimatorSet();
        tabCreatedBackgroundAnimation.playSequentially(step1, step3);

        // Remove the flying rectangle view when the animation ends so they don't accumulate.
        tabCreatedBackgroundAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                view.removeView(newTabView);
            }
        });
        return tabCreatedBackgroundAnimation;
    }

    private static Animator animate(
            View view, Property<View, Float> property, float a, float b, long duration) {
        return animate(view, property, a, b, duration, /*interpolator=*/null);
    }

    private static Animator animate(View view, Property<View, Float> property, float a, float b,
            long duration, TimeInterpolator interpolator) {
        ObjectAnimator animator =
                ObjectAnimator.ofFloat(view, property, a, b).setDuration(duration);
        if (interpolator != null) {
            animator.setInterpolator(interpolator);
        }
        return animator;
    }
}