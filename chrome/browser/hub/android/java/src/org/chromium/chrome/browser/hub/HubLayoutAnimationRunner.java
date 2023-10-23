// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface for running a {@link HubLayoutAnimator}.
 *
 * <p>One {@link HubLayoutAnimationRunner} will be created per {@link HubLayoutAnimatorProvider} and
 * the runner will not be re-used. The {@link HubLayout} will create a new {@link
 * HubLayoutAnimationRunner} for each transition animation.
 *
 * <p>The {@link HubLayoutAnimatorProvider} is responsible for setting up a {@link
 * HubLayoutAnimator}, possibly asynchronously; this is the setup portion of the {@link AnimatorSet}
 * API.
 *
 * <p>The {@link HubLayoutAnimationRunner} is responsible for:
 *
 * <ul>
 *   <li>Tracking animation progress.
 *   <li>Waiting for a {@link HubLayoutAnimatorProvider} to provide a {@link HubLayoutAnimator}. The
 *       wait may timeout resulting in the {@link HubLayoutAnimatorProvider} being asked to supply a
 *       fallback {@link HubLayoutAnimator}.
 *   <li>Setting up and managing {@link HubLayoutAnimationListener}s using animations listeners on
 *       {@link AnimatorSet}
 *   <li>Forcing animation completion.
 * </ul>
 *
 * This is the listening and execution portion of the {@link AnimatorSet} API, with additional
 * functionality required for {@link HubLayout} interaction.
 */
public interface HubLayoutAnimationRunner {
    @IntDef({
        AnimationState.INITIALIZING,
        AnimationState.WAITING_FOR_ANIMATOR,
        AnimationState.STARTED,
        AnimationState.FINISHED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnimationState {
        /** The animation runner hasn't received {@link #runWithTimeout(long)}. */
        int INITIALIZING = 0;

        /** The animation runner is waiting for an animator. */
        int WAITING_FOR_ANIMATOR = 1;

        /** The animation has started. */
        int STARTED = 2;

        /** The animation has finished. */
        int FINISHED = 3;
    }

    /** Returns the current state of the runner. */
    @AnimationState
    int getAnimationState();

    /** Returns the {@link HubLayoutAnimationType} of the animation this is running. */
    @HubLayoutAnimationType
    int getAnimationType();

    /**
     * Runs the provided animation when it is ready or forces the animation to start after a
     * timeout.
     *
     * @param timeoutMillis The timeout after which to force the animation to start.
     */
    void runWithWaitForAnimatorTimeout(long timeoutMillis);

    /**
     * Synchronously forces the current animation to run to completion. The animation should reach
     * its end state immediately while sending all relevant {@link HubLayoutAnimationListener}
     * events.
     */
    void forceAnimationToFinish();

    /**
     * Adds a {@link HubLayoutAnimatorListener} for the animation.
     *
     * @param animationListener The {@link HubLayoutAnimationListener} to add.
     */
    void addListener(@NonNull HubLayoutAnimationListener animationListener);
}
