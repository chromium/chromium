// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.build.annotations.NullMarked;

import java.util.List;
import java.util.function.DoubleConsumer;

/**
 * Factory for creating {@link HubLayoutAnimatorProvider} for fade-in tab list items animations. It
 * will fallback to fade animation of {@link HubContainerView} if dependencies aren't fulfilled in
 * time. Currently used on Android XR to animate {@link HubLayout} in XR full space mode.
 */
@NullMarked
public class TabListHubLayoutAnimationFactory {

    /**
     * Creates an animation to use when the tab list fades in.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param animationDataSupplier The supplier of List<View> to use for the animation.
     * @param durationMs The duration in milliseconds of the animation.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     */
    public static HubLayoutAnimatorProvider createFadeInTabListAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull SyncOneshotSupplier<List<View>> animationDataSupplier,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return new TabListHubLayoutAnimatorProvider(
                HubLayoutAnimationType.FADE_IN,
                hubContainerView,
                animationDataSupplier,
                durationMs,
                onAlphaChange);
    }
}
