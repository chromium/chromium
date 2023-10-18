// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.view.View;
import android.view.animation.Interpolator;

import androidx.annotation.NonNull;

import org.chromium.ui.interpolators.Interpolators;

/** Implementation of interface exposed by {@link FadeHubLayoutAnimationFactory}. */
public class FadeHubLayoutAnimationFactoryImpl {
    /** See {@link FadeHubLayoutAnimationFactory#createFadeInAnimator(HubContainerView, long)}. */
    public static HubLayoutAnimator createFadeInAnimator(
            @NonNull HubContainerView hubContainerView, long durationMs) {
        return createFadeAnimator(
                HubLayoutAnimationType.FADE_IN,
                hubContainerView,
                /* initialAlpha= */ 0.0f,
                /* finalAlpha= */ 1.0f,
                Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR,
                durationMs);
    }

    /** See {@link FadeHubLayoutAnimationFactory#createFadeOutAnimator(HubContainerView, long)}. */
    public static HubLayoutAnimator createFadeOutAnimator(
            @NonNull HubContainerView hubContainerView, long durationMs) {
        return createFadeAnimator(
                HubLayoutAnimationType.FADE_OUT,
                hubContainerView,
                /* initialAlpha= */ 1.0f,
                /* finalAlpha= */ 0.0f,
                Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR,
                durationMs);
    }

    /**
     * See {@link FadeHubLayoutAnimationFactory#createFadeInAnimatorProvider(HubContainerView,
     * long)}.
     */
    public static HubLayoutAnimatorProvider createFadeInAnimatorProvider(
            @NonNull HubContainerView hubContainerView, long durationMs) {
        return new PresetHubLayoutAnimatorProvider(
                createFadeInAnimator(hubContainerView, durationMs));
    }

    /**
     * See {@link FadeHubLayoutAnimationFactory#createFadeOutAnimatorProvider(HubContainerView,
     * long)}.
     */
    public static HubLayoutAnimatorProvider createFadeOutAnimatorProvider(
            @NonNull HubContainerView hubContainerView, long durationMs) {
        return new PresetHubLayoutAnimatorProvider(
                createFadeOutAnimator(hubContainerView, durationMs));
    }

    private static HubLayoutAnimator createFadeAnimator(
            @HubLayoutAnimationType int animationType,
            @NonNull HubContainerView hubContainerView,
            float initialAlpha,
            float finalAlpha,
            Interpolator interpolator,
            long durationMs) {
        ObjectAnimator animator =
                ObjectAnimator.ofFloat(hubContainerView, View.ALPHA, initialAlpha, finalAlpha);
        animator.setDuration(durationMs);
        animator.setInterpolator(interpolator);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.play(animator);

        HubLayoutAnimationListener listener =
                new HubLayoutAnimationListener() {
                    @Override
                    public void beforeStart() {
                        hubContainerView.setAlpha(initialAlpha);
                        hubContainerView.setVisibility(View.VISIBLE);
                    }

                    @Override
                    public void afterEnd() {
                        // Reset alpha for the next animation. If the animation is hiding the view
                        // will already be INVISIBLE. If the animation is showing this is a noop.
                        hubContainerView.setAlpha(1f);
                    }
                };
        return new HubLayoutAnimator(animationType, animatorSet, listener);
    }
}
