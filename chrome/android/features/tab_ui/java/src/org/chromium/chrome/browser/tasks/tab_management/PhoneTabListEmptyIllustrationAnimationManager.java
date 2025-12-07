// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.animation.DrawableFadeInAnimatorFactory;
import org.chromium.ui.animation.DrawableTranslationAnimatorFactory;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
import java.util.List;

/** Manages animation for the Empty Tab List Illustration for Phones. */
@NullMarked
public class PhoneTabListEmptyIllustrationAnimationManager
        implements TabListEmptyIllustrationAnimationManager {
    private final int mDeltaPx;
    private final ImageView mImage;
    private final TextView mHeading;
    private final TextView mSubheading;
    private final AnimationHandler mAnimationHandler = new AnimationHandler();

    /**
     * @param image the {@link ImageView} representing the illustration.
     * @param heading the heading which appears when the tab list is empty.
     * @param subheading the subheading which appears when the tab list is empty.
     */
    public PhoneTabListEmptyIllustrationAnimationManager(
            ImageView image, TextView heading, TextView subheading) {
        mImage = image;
        mHeading = heading;
        mSubheading = subheading;

        Resources resources = mImage.getContext().getResources();
        mDeltaPx = resources.getDimensionPixelSize(R.dimen.phone_tab_list_empty_animation_delta);
    }

    /**
     * Animates the Empty Tab List Illustration for Phones.
     *
     * @param durationMs The duration of the animation in milliseconds.
     */
    @Override
    public void animate(long durationMs) {
        LayerDrawable layers = (LayerDrawable) mImage.getDrawable();

        Drawable topWindow =
                layers.findDrawableByLayerId(
                        R.id.phone_tab_switcher_empty_state_illustration_top_window);
        Drawable bottomWindow =
                layers.findDrawableByLayerId(
                        R.id.phone_tab_switcher_empty_state_illustration_bottom_window);
        Drawable leftCloud =
                layers.findDrawableByLayerId(
                        R.id.phone_tab_switcher_empty_state_illustration_cloud_left);
        Drawable rightCloud =
                layers.findDrawableByLayerId(
                        R.id.phone_tab_switcher_empty_state_illustration_cloud_right);

        List<Animator> animators = new ArrayList<>();
        animators.add(
                DrawableTranslationAnimatorFactory.build(leftCloud, new Rect(), -mDeltaPx, 0));
        animators.add(
                DrawableTranslationAnimatorFactory.build(rightCloud, new Rect(), mDeltaPx, 0));

        animators.add(
                PhoneTabListIllustrationWindowAnimation.build(topWindow, -mDeltaPx, -mDeltaPx));
        animators.add(
                PhoneTabListIllustrationWindowAnimation.build(bottomWindow, mDeltaPx, mDeltaPx));

        animators.add(PhoneTabListEmptyTextAnimation.build(mHeading, mDeltaPx));
        animators.add(PhoneTabListEmptyTextAnimation.build(mSubheading, mDeltaPx));

        AnimatorSet animator = new AnimatorSet();
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        transformationOnAnimationStart(leftCloud, rightCloud);
                    }
                });
        animator.playTogether(animators);
        animator.setDuration(durationMs);
        animator.setInterpolator(Interpolators.DECELERATE_INTERPOLATOR);
        mAnimationHandler.startAnimation(animator);
    }

    /**
     * Applies the transformation to be run at the start of the animation for the Empty Tab List
     * Illustration.
     */
    private static void transformationOnAnimationStart(Drawable leftCloud, Drawable rightCloud) {
        leftCloud.setAlpha(255);
        rightCloud.setAlpha(255);
    }

    /** Applies an initial transformation on the Empty Tab List Illustration. */
    @Override
    public void initialTransformation() {
        LayerDrawable layers = (LayerDrawable) mImage.getDrawable();
        Drawable leftCloud =
                layers.findDrawableByLayerId(
                        R.id.phone_tab_switcher_empty_state_illustration_cloud_left);
        Drawable rightCloud =
                layers.findDrawableByLayerId(
                        R.id.phone_tab_switcher_empty_state_illustration_cloud_right);

        leftCloud.setAlpha(0);
        rightCloud.setAlpha(0);

        PhoneTabListInitialTransformation.applyCloudTransformation(leftCloud, -mDeltaPx);
        PhoneTabListInitialTransformation.applyCloudTransformation(rightCloud, mDeltaPx);
    }

    /** Initial transformation to be applied on elements on the empty tab list illustration. */
    private static class PhoneTabListInitialTransformation {
        public static void applyCloudTransformation(Drawable cloud, int dX) {
            cloud.setBounds(dX, 0, dX + cloud.getIntrinsicWidth(), cloud.getIntrinsicHeight());
        }
    }

    /**
     * Contains the logic for building animations for text which appears when the tab list is empty.
     */
    private static class PhoneTabListEmptyTextAnimation {
        public static Animator build(View view, int dY) {
            AnimatorSet animator = new AnimatorSet();
            ObjectAnimator translateY = ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, dY, 0);
            ObjectAnimator fade = ObjectAnimator.ofFloat(view, View.ALPHA, 0, 1);

            animator.playTogether(translateY, fade);
            return animator;
        }
    }

    /**
     * Contains the logic for building animations for windows which appear in the empty tab list
     * illustration.
     */
    private static class PhoneTabListIllustrationWindowAnimation {
        public static Animator build(Drawable drawable, int dX, int dY) {
            AnimatorSet animator = new AnimatorSet();
            Animator translate = DrawableTranslationAnimatorFactory.build(drawable, dX, dY);
            Animator fadeIn = DrawableFadeInAnimatorFactory.build(drawable);
            animator.playTogether(translate, fadeIn);
            return animator;
        }
    }
}
