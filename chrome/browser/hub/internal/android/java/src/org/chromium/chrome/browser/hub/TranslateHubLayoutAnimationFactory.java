// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;

/**
 * Factory for creating {@link HubLayoutAnimatorProvider}s for translate animations. These
 * animations are primiarly used for large form factor devices.
 */
@NullMarked
public class TranslateHubLayoutAnimationFactory {

    /**
     * Creates a {@link HubLayoutAnimatorProvider} for translating up to show.
     *
     * @param colorMixer The color mixer for the Hub.
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param scrimController A controller to manage showing a scrim.
     * @param durationMs The duration in milliseconds of the animation.
     * @param yOffset The y-offset for the container view, in px.
     * @return a {@link HubLayoutAnimatorProvider} that provides the animation.
     */
    public static HubLayoutAnimatorProvider createTranslateUpAnimatorProvider(
            HubColorMixer colorMixer,
            HubContainerView hubContainerView,
            ScrimController scrimController,
            long durationMs,
            float yOffset) {
        return TranslateHubLayoutAnimationFactoryImpl.createTranslateUpAnimatorProvider(
                colorMixer, hubContainerView, scrimController, durationMs, yOffset);
    }

    /**
     * Creates a {@link HubLayoutAnimatorProvider} for translating down to hide.
     *
     * @param colorMixer The color mixer for the Hub.
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param scrimController A controller to manage hiding a scrim.
     * @param durationMs The duration in milliseconds of the animation.
     * @param yOffset The y-offset for the container view, in px.
     * @return a {@link HubLayoutAnimatorProvider} that provides the animation.
     */
    public static HubLayoutAnimatorProvider createTranslateDownAnimatorProvider(
            HubColorMixer colorMixer,
            HubContainerView hubContainerView,
            ScrimController scrimController,
            long durationMs,
            float yOffset) {
        return TranslateHubLayoutAnimationFactoryImpl.createTranslateDownAnimatorProvider(
                colorMixer, hubContainerView, scrimController, durationMs, yOffset);
    }
}
