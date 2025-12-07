// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.ValueAnimator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/** Utilities related to {@link RoundedCornerImageView} animations. */
@NullMarked
public class RoundedCornerAnimatorUtil {
    /**
     * Returns a {@link ValueAnimator} for {@link RoundedCornerImageView#setRoundedCorners}
     *
     * @param imageView The {@link RoundedCornerImageView} for animating the corners.
     * @param startCorners Radii start values.
     * @param endCorners Radii end values.
     * @return The {@link ValueAnimator} for the rounded corner animation.
     */
    public static ValueAnimator createRoundedCornerAnimator(
            RoundedCornerImageView imageView, int[] startCorners, int[] endCorners) {
        assert startCorners.length == 4 && endCorners.length == 4;
        ValueAnimator cornerAnimator = ValueAnimator.ofFloat(0f, 1f);
        int[] delta =
                new int[] {
                    endCorners[0] - startCorners[0],
                    endCorners[1] - startCorners[1],
                    endCorners[2] - startCorners[2],
                    endCorners[3] - startCorners[3]
                };
        cornerAnimator.addUpdateListener(
                animation -> {
                    float fraction = animation.getAnimatedFraction();
                    int[] currentCorners = new int[4];
                    for (int i = 0; i < 4; i++) {
                        currentCorners[i] = startCorners[i] + Math.round(delta[i] * fraction);
                    }
                    imageView.setRoundedCorners(
                            currentCorners[0],
                            currentCorners[1],
                            currentCorners[2],
                            currentCorners[3]);
                    imageView.invalidate();
                });
        return cornerAnimator;
    }
}
