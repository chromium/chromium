// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.interpolators.Interpolators;

/**
 * Implementation of {@link NativePage.SmoothTransitionDelegate} for the New Tab Page. This delegate
 * handles the alpha animation transition, optionally waiting for the feed surface to be restored
 * before starting.
 */
@NullMarked
@VisibleForTesting
public class NtpSmoothTransitionDelegate implements NativePage.SmoothTransitionDelegate {
    private static final int SMOOTH_TRANSITION_DURATION_MS = 100;

    private final View mView;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private Animator mAnimator;
    private NonNullObservableSupplier<Integer> mRestoringState;
    private boolean mAnimatorStarted;

    final Callback<Integer> mOnScrollStateChanged =
            new Callback<>() {
                @Override
                public void onResult(Integer restoreState) {
                    if (restoreState == FeedSurfaceProvider.RestoringState.NO_STATE_TO_RESTORE
                            || restoreState == FeedSurfaceProvider.RestoringState.RESTORED) {
                        mAnimator.start();
                        mRestoringState.removeObserver(this);
                        mAnimatorStarted = true;
                        mHandler.removeCallbacks(mFallback);
                        BackPressMetrics.recordNTPSmoothTransitionMethod(false);
                    }
                }
            };
    private final Runnable mFallback =
            () -> {
                if (!mAnimatorStarted) {
                    mAnimator.start();
                    mAnimatorStarted = true;
                    mRestoringState.removeObserver(mOnScrollStateChanged);
                    BackPressMetrics.recordNTPSmoothTransitionMethod(true);
                }
            };

    public NtpSmoothTransitionDelegate(
            View view, NonNullObservableSupplier<Integer> restoringState) {
        mView = view;
        mAnimator = buildSmoothTransition(view);
        mRestoringState = restoringState;

        // Fallback added for metric records only.
        restoringState.addSyncObserverAndPostIfNonNull(
                new Callback<Integer>() {
                    long mStart;

                    @Override
                    public void onResult(@Nullable Integer result) {
                        assumeNonNull(result);
                        if (result == FeedSurfaceProvider.RestoringState.WAITING_TO_RESTORE) {
                            mStart = TimeUtils.currentTimeMillis();
                        } else if (result == FeedSurfaceProvider.RestoringState.RESTORED) {
                            BackPressMetrics.recordNTPFeedRestorationDuration(
                                    TimeUtils.currentTimeMillis() - mStart);
                        }
                    }
                });
    }

    @Override
    public void prepare() {
        assert !mAnimator.isRunning() : "Previous animation should not be running";
        assert !mAnimatorStarted : "Previous animation should not be finished or cancelled.";
        cancel();
        mView.setAlpha(0f);
    }

    @Override
    public void start(Runnable onEnd) {
        assert !mAnimator.isRunning() : "Previous animation have been done or cancelled";
        mAnimatorStarted = false;

        mAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        onEnd.run();
                    }
                });
        mRestoringState.addSyncObserverAndPostIfNonNull(mOnScrollStateChanged);
        mHandler.postDelayed(mFallback, BackPressMetrics.MAX_FALLBACK_DELAY_NTP_SMOOTH_TRANSITION);
    }

    @Override
    public void cancel() {
        mRestoringState.removeObserver(mOnScrollStateChanged);
        mHandler.removeCallbacks(mFallback);
        mAnimator.cancel();
        mView.setAlpha(1f);
    }

    private static Animator buildSmoothTransition(View view) {
        var animator = ObjectAnimator.ofFloat(view, View.ALPHA, 0f, 1f);
        animator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        animator.setDuration(SMOOTH_TRANSITION_DURATION_MS);
        return animator;
    }

    public Animator getAnimatorForTesting() {
        return mAnimator;
    }
}
