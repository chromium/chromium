// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.ui.interpolators.Interpolators;

/** Implementation of {@link TranslateHubLayoutAnimationFactory}. */
public class TranslateHubLayoutAnimationFactoryImpl {

    /**
     * See {@link
     * TranslateHubLayoutAnimationFactory#createTranslateUpAnimatorProvider(HubContainerView,
     * long)}.
     */
    public static HubLayoutAnimatorProvider createTranslateUpAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull ScrimController scrimController,
            long durationMs,
            float yOffset) {
        AnimatorSet animatorSet = new AnimatorSet();

        HubLayoutAnimationListener listener =
                new HubLayoutAnimationListener() {
                    @Override
                    public void beforeStart() {
                        hubContainerView.setY(hubContainerView.getHeight());
                        hubContainerView.setVisibility(View.VISIBLE);

                        // Defer setting an animation until beforeStart() because we need to be
                        // certain the hubContainerView will have a dimension before starting the
                        // animation.
                        ObjectAnimator animator =
                                ObjectAnimator.ofFloat(
                                        hubContainerView,
                                        View.TRANSLATION_Y,
                                        hubContainerView.getHeight(),
                                        yOffset);
                        animator.setInterpolator(Interpolators.EMPHASIZED_DECELERATE);
                        animator.setDuration(durationMs);
                        animatorSet.play(animator);

                        scrimController.startShowingScrim();
                    }

                    @Override
                    public void afterEnd() {
                        // Carried over from the legacy implementation in TabSwitcherLayout.
                        hubContainerView.setY(yOffset);
                    }
                };

        return new PresetHubLayoutAnimatorProvider(
                new HubLayoutAnimator(HubLayoutAnimationType.TRANSLATE_UP, animatorSet, listener));
    }

    /**
     * See {@link
     * TranslateHubLayoutAnimationFactory#createTranslateDownAnimatorProvider(HubContainerView,
     * long)}.
     */
    public static HubLayoutAnimatorProvider createTranslateDownAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull ScrimController scrimController,
            long durationMs,
            float yOffset) {
        ObjectAnimator animator =
                ObjectAnimator.ofFloat(
                        hubContainerView,
                        View.TRANSLATION_Y,
                        yOffset,
                        hubContainerView.getHeight());
        animator.setInterpolator(Interpolators.EMPHASIZED_ACCELERATE);
        animator.setDuration(durationMs);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.play(animator);

        HubLayoutAnimationListener listener =
                new HubLayoutAnimationListener() {
                    @Override
                    public void beforeStart() {
                        scrimController.startHidingScrim();
                    }

                    @Override
                    public void afterEnd() {
                        // Reset the Y offset for the next animation.
                        hubContainerView.setY(yOffset);
                    }
                };
        return new PresetHubLayoutAnimatorProvider(
                new HubLayoutAnimator(
                        HubLayoutAnimationType.TRANSLATE_DOWN, animatorSet, listener));
    }
}
