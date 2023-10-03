// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/**
 * Base class for {@HubLayout} animations.
 *
 * TODO(crbug/1487209): Make this an abstract base class and expand on it.
 */
public class HubLayoutAnimation {
    private final @HubLayoutAnimationType int mAnimationType;

    /**
     * Base class for {@link HubLayout} animations.
     * @param animationType The {@link HubLayoutAnimationType} of this animation.
     */
    HubLayoutAnimation(@HubLayoutAnimationType int animationType) {
        mAnimationType = animationType;
    }

    /** Returns the {@link HubLayoutAnimationType} of this animation. */
    public @HubLayoutAnimationType int getAnimationType() {
        return mAnimationType;
    }

    /**
     * Forces the animation to finish. If the animation has not started any changes that it would
     * result happen immediately.
     */
    public void forceAnimationToFinish() {}
}
