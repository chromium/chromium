// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Manages running an {@link AnimationRunner} after some conditions are met.
 *
 * Conditions:
 * <ol>
 *    <li>A {@link Bitmap} has been set with {@link #setBitmap(Bitmap)}. A null bitmap is allowed.
 *    <li>{@link #setLayoutCompleted()} is called indicating the layout is ready.
 *    <li>{@link #setTabListCanShowQuickly(boolean)} is called to indicate a decision has been made
 *        about whether the tab list can be shown quickly. Downsteam {@link AnimationRunner} may use
 *        this information to decide whether to skip the animation to avoid a low FPS animation.
 * </ol>
 */
public class ConditionalAnimationRunner {
    /** An animation runner that will run once conditions are met or a timeout has occurred. */
    @FunctionalInterface
    interface AnimationRunner {
        /**
         * @param bitmap The bitmap to show or null if a bitmap was not available.
         * @param tabListCanBeShownQuickly Whether the tab list can be shown quickly.
         */
        void run(@Nullable Bitmap bitmap, boolean tabListCanBeShownQuickly);
    }

    /**
     * Whether {@link #setBitmap(Bitmap)} has ever been called as a null bitmap may be used. For
     * example, the new tab animation uses a null bitmap.
     */
    private boolean mBitmapSet;

    private Bitmap mBitmap;

    /**
     * Whether the tab list can be shown quickly. This is a tri-state boolean as true and false
     * are both valid values and we want to wait for this to be set to non-null before proceeding.
     */
    private Boolean mTabListCanShowQuickly;

    private boolean mLayoutCompleted;

    private AnimationRunner mAnimationRunner;

    /**
     * Create a conditional runner that will invoke {@link AnimationRunner#run()} once all
     * conditions are met.
     * @param animationRunner The {@link AnimationRunner} to invoke once conditions are met.
     */
    ConditionalAnimationRunner(@NonNull AnimationRunner animationRunner) {
        mAnimationRunner = animationRunner;
    }

    /** Runs the {@link mAnimationRunner}. */
    void runAnimationDueToTimeout() {
        if (mAnimationRunner == null) return;

        runAnimation();
    }

    /**
     * Set the bitmap and may run the animation.
     * @param bitmap The bitmap to use for the animation or null.
     */
    void setBitmap(@Nullable Bitmap bitmap) {
        if (mAnimationRunner == null) return;

        mBitmapSet = true;
        mBitmap = bitmap;
        maybeRunAnimation();
    }

    /**
     * Set the whether the tab list can be shown quickly and may run the animation.
     * @param tabListCanBeShownQuickly Whether the tab switcher can be shown quickly.
     */
    void setTabListCanShowQuickly(boolean tabListCanBeShownQuickly) {
        if (mAnimationRunner == null) return;

        mTabListCanShowQuickly = tabListCanBeShownQuickly;
        maybeRunAnimation();
    }

    /** Set that the first layout completed and may run the animation. */
    void setLayoutCompleted() {
        if (mAnimationRunner == null) return;

        mLayoutCompleted = true;
        maybeRunAnimation();
    }

    /** Runs the animation if all the conditions are met. */
    private void maybeRunAnimation() {
        if (mBitmapSet && mTabListCanShowQuickly != null && mLayoutCompleted) {
            runAnimation();
        }
    }

    /** Unconditionally runs the animation with whatever is currently set. */
    private void runAnimation() {
        AnimationRunner animationRunner = mAnimationRunner;
        mAnimationRunner = null;
        animationRunner.run(
                mBitmap,
                mTabListCanShowQuickly == null ? false : mTabListCanShowQuickly.booleanValue());

        // Release the bitmap as it is expensive to keep around.
        mBitmap = null;
    }
}
