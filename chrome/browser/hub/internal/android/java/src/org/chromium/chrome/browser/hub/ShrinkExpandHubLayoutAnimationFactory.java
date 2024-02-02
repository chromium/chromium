// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.SyncOneshotSupplier;

import java.util.function.DoubleConsumer;

/**
 * Factory for creating {@link HubLayoutAnimatorProvider}s for shrink, expand, and new tab
 * animations. These will fallback to fade animations if dependencies aren't fulfilled in time.
 */
public class ShrinkExpandHubLayoutAnimationFactory {
    /**
     * Creates an animation to use when creating a non-background new tab from Hub. This animation
     * is similar to the {@link #createExpandTabAnimatorProvider}, but has a different animation
     * type, doesn't use a bitmap, and has no fallback animator.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param animationDataSupplier The supplier for {@link ShrinkExpandAnimationData} to use for
     *     the animation.
     * @param backgroundColor The background color to use for the animation.
     * @param durationMs The duration in milliseconds of the animation.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     */
    public static HubLayoutAnimatorProvider createNewTabAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull SyncOneshotSupplier<ShrinkExpandAnimationData> animationDataSupplier,
            @ColorInt int backgroundColor,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return new ShrinkExpandHubLayoutAnimatorProvider(
                HubLayoutAnimationType.EXPAND_NEW_TAB,
                /* needsBitmap= */ false,
                hubContainerView,
                animationDataSupplier,
                backgroundColor,
                durationMs,
                onAlphaChange);
    }

    /**
     * Creates an animation to use when shrinking from a tab to the tab switcher in the Hub.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param animationDataSupplier The supplier for {@link ShrinkExpandAnimationData} to use for
     *     the animation.
     * @param backgroundColor The background color to use if the thumbnail doesn't cover the full
     *     animating area (this is unlikely to happen for Shrink animations).
     * @param durationMs The duration in milliseconds of the animation.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     */
    public static HubLayoutAnimatorProvider createShrinkTabAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull SyncOneshotSupplier<ShrinkExpandAnimationData> animationDataSupplier,
            @ColorInt int backgroundColor,
            long durationMs,
            @NonNull DoubleConsumer onAlphaChange) {
        return new ShrinkExpandHubLayoutAnimatorProvider(
                HubLayoutAnimationType.SHRINK_TAB,
                /* needsBitmap= */ true,
                hubContainerView,
                animationDataSupplier,
                backgroundColor,
                durationMs,
                onAlphaChange);
    }

    /**
     * Creates an animation to use when expanding from the tab switcher in the Hub to a tab.
     *
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param animationDataSupplier The supplier for {@link ShrinkExpandAnimationData} to use for
     *     the animation.
     * @param backgroundColor The background color to use if the thumbnail doesn't cover the full
     *     animating area. This is possible if the thumbnail was captured for a different aspect
     *     ratio than the one that will be shown i.e. different orientation or multi-window.
     * @param durationMs The duration in milliseconds of the animation.
     * @param mOnAlphaChange Observer to notify when alpha changes during animations.
     */
    public static HubLayoutAnimatorProvider createExpandTabAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull SyncOneshotSupplier<ShrinkExpandAnimationData> animationDataSupplier,
            @ColorInt int backgroundColor,
            long durationMs,
            @NonNull DoubleConsumer mOnAlphaChange) {
        return new ShrinkExpandHubLayoutAnimatorProvider(
                HubLayoutAnimationType.EXPAND_TAB,
                /* needsBitmap= */ true,
                hubContainerView,
                animationDataSupplier,
                backgroundColor,
                durationMs,
                mOnAlphaChange);
    }
}
