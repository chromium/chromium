// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** Data to asynchronously supply to {@link ShrinkExpandHubLayoutAnimatorProvider}. */
public class ShrinkExpandAnimationData {
    private final @NonNull Rect mInitialRect;
    private final @NonNull Rect mFinalRect;
    private final @Nullable Size mThumbnailSize;
    private final boolean mUseFallbackAnimation;

    /**
     * @param initialRect The initial {@link Rect} for the view to start at.
     * @param finalRect The final {@link Rect} for the view to end at.
     * @param thumbnailSize The size of a thumbnail. This is used if the {@code initialRect} is
     *     clipped at the top to make the animation of the image smooth.
     * @param useFallbackAnimation Whether the fallback animation should be used. If this is true
     *     the fallback animation is forced. Useful when something happened while preparing this
     *     data that suggests the shrink or expand animation can no longer proceed.
     */
    // TODO(crbug.com/40285429): Try to get rid of useFallbackAnimation, it is a holdover from
    // performance ssues with TabSwitcherLayout on low-end devices or when the recycler view
    // model needs to be rebuilt.
    public ShrinkExpandAnimationData(
            @NonNull Rect initialRect,
            @NonNull Rect finalRect,
            @Nullable Size thumbnailSize,
            boolean useFallbackAnimation) {
        mInitialRect = initialRect;
        mFinalRect = finalRect;
        mThumbnailSize = thumbnailSize;
        mUseFallbackAnimation = useFallbackAnimation;
    }

    /** Returns the starting rect of the animation. */
    public @NonNull Rect getInitialRect() {
        return mInitialRect;
    }

    /** Returns the final rect of the animation. */
    public @NonNull Rect getFinalRect() {
        return mFinalRect;
    }

    /** Returns the thumbnail size. */
    public @Nullable Size getThumbnailSize() {
        return mThumbnailSize;
    }

    /** Returns whether to use the fallback animation. */
    public boolean shouldUseFallbackAnimation() {
        return mUseFallbackAnimation;
    }
}
