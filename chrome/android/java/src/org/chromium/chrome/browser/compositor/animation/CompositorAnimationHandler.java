// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.animation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;

import java.util.ArrayList;

/**
 * The handler responsible for managing and pushing updates to all of the active
 * CompositorAnimators.
 */
public class CompositorAnimationHandler {
    /** Whether or not testing mode is enabled. In this mode, animations end immediately. */
    private static boolean sIsInTestingMode;

    /** A list of all the handler's animators. */
    private final ArrayList<CompositorAnimator> mAnimators = new ArrayList<>();

    /** This handler's update host. */
    private final LayoutUpdateHost mUpdateHost;

    /**
     * A cached copy of the list of {@link CompositorAnimator}s to prevent allocating a new list
     * every update.
     */
    private final ArrayList<CompositorAnimator> mCachedList = new ArrayList<>();

    /**
     * Whether or not an update has already been requested for the next frame due to an animation
     * starting.
     */
    private boolean mWasUpdateRequestedForAnimationStart;

    /** The last time that an update was pushed to animations. */
    private long mLastUpdateTimeMs;

    /**
     * Default constructor.
     * @param host A {@link LayoutUpdateHost} responsible for requesting frames when an animation
     *             updates.
     */
    public CompositorAnimationHandler(@NonNull LayoutUpdateHost host) {
        assert host != null;
        mUpdateHost = host;
    }

    /**
     * Add an animator to the list of known animators to start receiving updates.
     * @param animator The animator to start.
     */
    final void registerAndStartAnimator(final CompositorAnimator animator) {
        // If animations are currently running, the last updated time is being updated. If not,
        // reset the value here. This prevents gaps in animations from breaking timing.
        if (getActiveAnimationCount() <= 0) mLastUpdateTimeMs = System.currentTimeMillis();

        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                mAnimators.remove(animator);
                animator.removeListener(this);
            }
        });
        mAnimators.add(animator);

        if (!mWasUpdateRequestedForAnimationStart) {
            mUpdateHost.requestUpdate();
            mWasUpdateRequestedForAnimationStart = true;
        }

        // If in testing mode, immediately push an update and end the animation.
        if (sIsInTestingMode) pushUpdate(Long.MAX_VALUE);
    }

    /**
     * Push an update to all the currently running animators.
     * @return True if all animations controlled by this handler have completed.
     */
    public final boolean pushUpdate() {
        long currentTime = System.currentTimeMillis();
        long deltaTimeMs = currentTime - mLastUpdateTimeMs;
        mLastUpdateTimeMs = currentTime;

        return pushUpdate(deltaTimeMs);
    }

    /**
     * Push an update to all the currently running animators.
     * @param deltaTimeMs The time since the previous update in ms.
     * @return True if all animations controlled by this handler have completed.
     */
    final boolean pushUpdate(long deltaTimeMs) {
        mWasUpdateRequestedForAnimationStart = false;
        if (mAnimators.isEmpty()) return true;

        // Do updates to the animators. Use a cloned list so the original list can be modified in
        // the update loop.
        mCachedList.addAll(mAnimators);
        for (int i = 0; i < mCachedList.size(); i++) {
            CompositorAnimator currentAnimator = mCachedList.get(i);
            currentAnimator.doAnimationFrame(deltaTimeMs);
            // Once the animation ends, it no longer needs to receive updates; remove it from the
            // handler's list of animations. Restarting the animation will re-add the animation to
            // this handler.
            if (currentAnimator.hasEnded()) mAnimators.remove(currentAnimator);
        }
        mCachedList.clear();

        mUpdateHost.requestUpdate();

        return mAnimators.isEmpty();
    }

    /**
     * Clean up this handler.
     */
    public final void destroy() {
        mAnimators.clear();
    }

    /**
     * @return The number of animations that are active inside this handler.
     */
    @VisibleForTesting
    int getActiveAnimationCount() {
        return mAnimators.size();
    }

    /**
     * Enable or disable testing mode. This causes any animations to end immediately.
     * @param enabled Whether testing mode is enabled or disabled.
     */
    @VisibleForTesting
    public static void setTestingMode(boolean enabled) {
        sIsInTestingMode = enabled;
    }

    /**
     * @return Whether we are in testing mode or not.
     */
    @VisibleForTesting
    public static boolean isInTestingMode() {
        return sIsInTestingMode;
    }

    /**
     * Provides update for animation in testing mode.
     * @return Whether update was successful or not.
     */
    @VisibleForTesting
    final boolean pushUpdateInTestingMode(long deltaTimeMs) {
        return sIsInTestingMode ? pushUpdate(deltaTimeMs) : false;
    }
}
