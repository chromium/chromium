// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ui.widget.MaterialProgressBar;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;

import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Wrapper around {@link MaterialProgressBar} to animate progress changes and enable/disable
 * pulsing.
 */
class AnimatedProgressBar {
    // The number of ms the progress bar would take to go from 0 to 100%.
    private static final int PROGRESS_BAR_SPEED_MS = 3_000;

    private final MaterialProgressBar mProgressBar;

    private boolean mIsRunningProgressAnimation;
    private int mLastProgress;
    private Queue<ValueAnimator> mPendingIncreaseAnimations = new ArrayDeque<>();
    private int mProgressBarSpeedMs = PROGRESS_BAR_SPEED_MS;

    AnimatedProgressBar(MaterialProgressBar progressBar) {
        mProgressBar = progressBar;
    }

    public void show() {
        mProgressBar.setVisibility(View.VISIBLE);
    }

    public void hide() {
        mProgressBar.setVisibility(View.INVISIBLE);
    }

    /**
     * Set the progress to {@code progress}. The transition to the new progress value is
     * animated.
     */
    public void setProgress(int progress) {
        if (progress == mLastProgress) {
            return;
        }
        ValueAnimator progressAnimation = ValueAnimator.ofInt(mLastProgress, progress);
        progressAnimation.setDuration(
                mProgressBarSpeedMs * Math.abs(progress - mLastProgress) / 100);
        progressAnimation.setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR);
        progressAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (mPendingIncreaseAnimations.isEmpty()) {
                    mIsRunningProgressAnimation = false;
                } else {
                    mIsRunningProgressAnimation = true;
                    mPendingIncreaseAnimations.poll().start();
                }
            }
        });
        progressAnimation.addUpdateListener(
                animation -> mProgressBar.setProgress((int) animation.getAnimatedValue()));
        mLastProgress = progress;

        if (mIsRunningProgressAnimation) {
            mPendingIncreaseAnimations.offer(progressAnimation);
        } else {
            mIsRunningProgressAnimation = true;
            progressAnimation.start();
        }
    }

    @VisibleForTesting
    void disableAnimationsForTesting(boolean disable) {
        mProgressBarSpeedMs = disable ? 0 : PROGRESS_BAR_SPEED_MS;
    }
}
