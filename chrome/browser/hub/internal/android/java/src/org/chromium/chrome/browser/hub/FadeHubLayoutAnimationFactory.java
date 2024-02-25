// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

import java.util.function.DoubleConsumer;

/**
 * Factory for creating fade {@link HubLayoutAnimator}s and {@link HubLayoutAnimatorProvider}s to
 * use as {@link HubLayout} animations.
 *
 * <p>When using a fade animation as a primary animator provider use {@link
 * #createFadeInAnimatorProvider(HubContainerView, long)} and {@link
 * #createFadeOutAnimatorProvider(HubContainerView, long)}.
 *
 * <p>On devices that are not large form factor, the fade animation should be the default fallback
 * animation if an animation is missing dependencies or will take too long to prepare. When
 * supplying a fade animation as a fallback use {@link #createFadeInAnimator(HubContainerView,
 * long)} or {@link #createFadeOutAnimator(HubContainerView, long)} to prepare an animator when
 * {@link HubLayoutAnimatorProvider#supplyAnimatorNow()} is invoked.
 */
public class FadeHubLayoutAnimationFactory {
    /**
     * Create a fade in {@link HubLayoutAnimator}.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param durationMs The duration of the animation in milliseconds.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     * @return the requested animator.
     */
    public static HubLayoutAnimator createFadeInAnimator(
            @NonNull HubContainerView hubContainerView,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return FadeHubLayoutAnimationFactoryImpl.createFadeInAnimator(
                hubContainerView, durationMs, onAlphaChange);
    }

    /**
     * Create a fade out {@link HubLayoutAnimator}.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param durationMs The duration of the animation in milliseconds.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     * @return the requested animator.
     */
    public static HubLayoutAnimator createFadeOutAnimator(
            @NonNull HubContainerView hubContainerView,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return FadeHubLayoutAnimationFactoryImpl.createFadeOutAnimator(
                hubContainerView, durationMs, onAlphaChange);
    }

    /**
     * Create a fade in {@link HubLayoutAnimatorProvider}.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param durationMs The duration of the animation in milliseconds.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     * @return the requested animator provider
     */
    public static HubLayoutAnimatorProvider createFadeInAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return FadeHubLayoutAnimationFactoryImpl.createFadeInAnimatorProvider(
                hubContainerView, durationMs, onAlphaChange);
    }

    /**
     * Create a fade out {@link HubLayoutAnimatorProvider}.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param durationMs The duration of the animation in milliseconds.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     * @return the requested animator provider.
     */
    public static HubLayoutAnimatorProvider createFadeOutAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return FadeHubLayoutAnimationFactoryImpl.createFadeOutAnimatorProvider(
                hubContainerView, durationMs, onAlphaChange);
    }
}
