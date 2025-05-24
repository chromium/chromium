// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.getPaneColorBlendInterpolator;
import static org.chromium.ui.util.ColorBlendAnimationFactory.createColorBlendAnimation;

import android.animation.Animator;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

/** Helper class to manage Animator object creation for views during a hub color scheme change. */
@NullMarked
public class SingleHubViewColorBlend implements HubViewColorBlend {

    private final long mDurationMs;
    private final ColorGetter mColorGetter;
    private final ColorSetter mColorSetter;

    /**
     * @param durationMs The duration of the animation in milliseconds.
     * @param colorGetter A method to get a ColorInt from a HubColorScheme.
     * @param colorSetter A method which updates the color of the view(s) to an interpolated color
     *     on an animator update.
     */
    public SingleHubViewColorBlend(
            long durationMs, ColorGetter colorGetter, ColorSetter colorSetter) {
        mDurationMs = durationMs;
        mColorGetter = colorGetter;
        mColorSetter = colorSetter;
    }

    /** Creates a color blend animation for a hub color scheme change. */
    @Override
    public Animator createAnimationForTransition(
            @HubColorScheme int startScheme, @HubColorScheme int endScheme) {
        @ColorInt int startColor = mColorGetter.colorIntFromColorScheme(startScheme);
        @ColorInt int endColor = mColorGetter.colorIntFromColorScheme(endScheme);
        Animator animation =
                createColorBlendAnimation(
                        mDurationMs, startColor, endColor, mColorSetter::setColorInt);
        animation.setInterpolator(getPaneColorBlendInterpolator());
        return animation;
    }
}
