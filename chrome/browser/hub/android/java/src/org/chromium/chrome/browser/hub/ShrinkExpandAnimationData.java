// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.graphics.Rect;
import android.util.Size;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Data to asynchronously supply to {@link ShrinkExpandHubLayoutAnimatorProvider}. */
@NullMarked
public class ShrinkExpandAnimationData {
    private final Rect mInitialRect;
    private final Rect mFinalRect;
    private final @Nullable Size mThumbnailSize;
    private final int[] mInitialCornerRadii;
    private final int[] mFinalCornerRadii;
    private final boolean mIsTopToolbar;
    private final boolean mUseFallbackAnimation;

    /**
     * Creates a {@link ShrinkExpandAnimationData} for {@link
     * HubLayoutAnimationType#EXPAND_NEW_TAB}.
     *
     * @param initialRect The initial {@link Rect} for the view to start at.
     * @param finalRect The final {@link Rect} for the view to end at.
     * @param cornerRadius The corner radius for the {@link ShrinkExpandImageView}.
     * @param useFallbackAnimation Whether the fallback animation should be used. If this is true
     *     the fallback animation is forced. Useful when something happened while preparing this
     *     data that suggests the shrink or expand animation can no longer proceed.
     */
    public static ShrinkExpandAnimationData createHubNewTabAnimationData(
            Rect initialRect, Rect finalRect, int cornerRadius, boolean useFallbackAnimation) {
        // We can assume the top toolbar exists for this animation as either we will cover the hub
        // toolbar with the animation (incognito) or the new tab has a dedicated top toolbar.
        return new ShrinkExpandAnimationData(
                initialRect,
                finalRect,
                new int[] {0, cornerRadius, cornerRadius, cornerRadius},
                new int[] {0, cornerRadius, cornerRadius, cornerRadius},
                /* thumbnailSize= */ null,
                /* isTopToolbar= */ true,
                useFallbackAnimation);
    }

    /**
     * Creates a {@link ShrinkExpandAnimationData} for {@link HubLayoutAnimationType#SHRINK_TAB} and
     * {@link HubLayoutAnimationType#EXPAND_TAB}.
     *
     * @param initialRect The initial {@link Rect} for the view to start at.
     * @param finalRect The final {@link Rect} for the view to end at.
     * @param initialTopCornerRadius The initial radius for the {@link ShrinkExpandImageView} top
     *     corners.
     * @param initialBottomCornerRadius The initial radius for the {@link ShrinkExpandImageView}
     *     bottom corners.
     * @param finalTopCornerRadius The final radius for the {@link ShrinkExpandImageView} top
     *     corners.
     * @param finalBottomCornerRadius The final radius for the {@link ShrinkExpandImageView} bottom
     *     corners.
     * @param thumbnailSize The size of a thumbnail. This is used if the {@code initialRect} is
     *     clipped at the top to make the animation of the image smooth.
     * @param isTopToolbar Whether the top toolbar will be shown behind the shrink.
     * @param useFallbackAnimation Whether the fallback animation should be used. If this is true
     *     the fallback animation is forced. Useful when something happened while preparing this
     *     data that suggests the shrink or expand animation can no longer proceed.
     */
    public static ShrinkExpandAnimationData createHubShrinkExpandAnimationData(
            Rect initialRect,
            Rect finalRect,
            int initialTopCornerRadius,
            int initialBottomCornerRadius,
            int finalTopCornerRadius,
            int finalBottomCornerRadius,
            @Nullable Size thumbnailSize,
            boolean isTopToolbar,
            boolean useFallbackAnimation) {
        return new ShrinkExpandAnimationData(
                initialRect,
                finalRect,
                new int[] {
                    initialTopCornerRadius,
                    initialTopCornerRadius,
                    initialBottomCornerRadius,
                    initialBottomCornerRadius
                },
                new int[] {
                    finalTopCornerRadius,
                    finalTopCornerRadius,
                    finalBottomCornerRadius,
                    finalBottomCornerRadius
                },
                thumbnailSize,
                isTopToolbar,
                useFallbackAnimation);
    }

    /**
     * @param initialRect The initial {@link Rect} for the view to start at.
     * @param finalRect The final {@link Rect} for the view to end at.
     * @param initialCornerRadii The initial radii for the {@link ShrinkExpandImageView} corners.
     * @param finalCornerRadii The final radii for the {@link ShrinkExpandImageView} corners. Its
     *     values will be scaled by {@code scaleFactor}.
     * @param thumbnailSize The size of a thumbnail. This is used if the {@code initialRect} is
     *     clipped at the top to make the animation of the image smooth.
     * @param isTopToolbar Whether the top toolbar will be shown behind the shrink.
     * @param useFallbackAnimation Whether the fallback animation should be used. If this is true
     *     the fallback animation is forced. Useful when something happened while preparing this
     *     data that suggests the shrink or expand animation can no longer proceed.
     */
    // TODO(crbug.com/40285429): Try to get rid of useFallbackAnimation, it is a holdover from
    // performance issues with TabSwitcherLayout on low-end devices or when the recycler view
    // model needs to be rebuilt.
    public ShrinkExpandAnimationData(
            Rect initialRect,
            Rect finalRect,
            int[] initialCornerRadii,
            int[] finalCornerRadii,
            @Nullable Size thumbnailSize,
            boolean isTopToolbar,
            boolean useFallbackAnimation) {
        assert initialCornerRadii.length == 4 && finalCornerRadii.length == 4
                : "Corner Radii should be equal to 4";
        float scaleFactor = (float) initialRect.width() / finalRect.width();
        assert scaleFactor >= 0f : "Unexpected width results in a negative scale factor";
        for (int i = 0; i < 4; ++i) {
            finalCornerRadii[i] = Math.round(finalCornerRadii[i] * scaleFactor);
        }

        mInitialRect = initialRect;
        mFinalRect = finalRect;
        mInitialCornerRadii = initialCornerRadii;
        mFinalCornerRadii = finalCornerRadii;
        mThumbnailSize = thumbnailSize;
        mIsTopToolbar = isTopToolbar;
        mUseFallbackAnimation = useFallbackAnimation;
    }

    /** Returns the starting rect of the animation. */
    public Rect getInitialRect() {
        return mInitialRect;
    }

    /** Returns the final rect of the animation. */
    public Rect getFinalRect() {
        return mFinalRect;
    }

    /** Returns the initial corner radii for {@link ShrinkExpandImageView}. */
    public int[] getInitialCornerRadii() {
        return mInitialCornerRadii;
    }

    /** Returns the final corner radii for {@link ShrinkExpandImageView}. */
    public int[] getFinalCornerRadii() {
        return mFinalCornerRadii;
    }

    /** Returns the thumbnail size. */
    public @Nullable Size getThumbnailSize() {
        return mThumbnailSize;
    }

    /** Whether the top toolbar will be shown behind the animation. */
    public boolean isTopToolbar() {
        return mIsTopToolbar;
    }

    /** Returns whether to use the fallback animation. */
    public boolean shouldUseFallbackAnimation() {
        return mUseFallbackAnimation;
    }
}
