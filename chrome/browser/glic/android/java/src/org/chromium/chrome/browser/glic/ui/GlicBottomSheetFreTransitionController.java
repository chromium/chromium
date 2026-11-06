// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Controls the opacity crossfade and skeleton loading transition for the Glic FRE bottom sheet. */
@NullMarked
class GlicBottomSheetFreTransitionController {

    @VisibleForTesting static final long CROSSFADE_DURATION_MS = 380;
    private static final long PULSE_DURATION_MS = 900;
    private static final float PULSE_MIN_ALPHA = 0.4f;

    @IntDef({
        State.IDLE,
        State.WAITING_SKELETON,
        State.CROSSFADING_OUT,
        State.REVEALED,
        State.DESTROYED,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int IDLE = 0;
        int WAITING_SKELETON = 1;
        int CROSSFADING_OUT = 2;
        int REVEALED = 3;
        int DESTROYED = 4;
    }

    @IntDef({RevealResult.READY, RevealResult.LOAD_ERROR, RevealResult.TIMED_OUT})
    @Retention(RetentionPolicy.SOURCE)
    @interface RevealResult {
        int READY = 0;
        int LOAD_ERROR = 1;
        int TIMED_OUT = 2;
    }

    interface RevealCompleteCallback {
        void onRevealComplete(boolean wasSkeletonShown, @RevealResult int result);
    }

    private final View mFreRootView;
    private final View mContentContainer;
    private final View mSkeletonContainer;
    private final RevealCompleteCallback mOnRevealComplete;
    private final Handler mHandler;

    private @State int mState = State.IDLE;
    private boolean mWebUiSettled;
    private @RevealResult int mResult = RevealResult.READY;
    private boolean mWasSkeletonShown;
    private @Nullable ObjectAnimator mPulseAnimator;
    private @Nullable ObjectAnimator mCrossfadeAnimator;

    GlicBottomSheetFreTransitionController(
            View freRootView,
            View contentContainer,
            View skeletonContainer,
            boolean isWebUiReady,
            boolean isWebUiLoadError,
            RevealCompleteCallback onRevealComplete) {
        mFreRootView = freRootView;
        mContentContainer = contentContainer;
        mSkeletonContainer = skeletonContainer;
        mOnRevealComplete = onRevealComplete;
        mHandler = new Handler(Looper.getMainLooper());
        updateWebUiState(isWebUiReady, isWebUiLoadError);
    }

    private void updateWebUiState(boolean isReady, boolean isError) {
        mWebUiSettled = isReady || isError;
        mResult = isError ? RevealResult.LOAD_ERROR : RevealResult.READY;
    }

    void onAccepted() {
        if (mState != State.IDLE) {
            return;
        }
        if (mWebUiSettled) {
            startCrossfadeOut();
            return;
        }

        mState = State.WAITING_SKELETON;
        mWasSkeletonShown = true;
        mContentContainer.setVisibility(View.GONE);
        mSkeletonContainer.setVisibility(View.VISIBLE);
        mSkeletonContainer.setAlpha(1.0f);

        if (ValueAnimator.areAnimatorsEnabled()) {
            mPulseAnimator =
                    ObjectAnimator.ofFloat(mSkeletonContainer, View.ALPHA, 1.0f, PULSE_MIN_ALPHA);
            mPulseAnimator.setDuration(PULSE_DURATION_MS);
            mPulseAnimator.setRepeatMode(ValueAnimator.REVERSE);
            mPulseAnimator.setRepeatCount(ValueAnimator.INFINITE);
            mPulseAnimator.start();
        }

        int timeoutMs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.GLIC_NATIVE_FRE_ANDROID,
                        ChromeFeatureList.GLIC_NATIVE_FRE_ANDROID_SKELETON_TIMEOUT_MS);
        mHandler.postDelayed(this::onSkeletonTimeout, timeoutMs);
    }

    void onWebUiStateChanged(boolean isReady, boolean isError) {
        if (mState != State.IDLE && mState != State.WAITING_SKELETON) {
            return;
        }
        updateWebUiState(isReady, isError);
        if (mState == State.WAITING_SKELETON && mWebUiSettled) {
            stopWaitingSkeleton();
            startCrossfadeOut();
        }
    }

    private void onSkeletonTimeout() {
        if (mState != State.WAITING_SKELETON) {
            return;
        }
        mResult = RevealResult.TIMED_OUT;
        stopWaitingSkeleton();
        startCrossfadeOut();
    }

    private void stopWaitingSkeleton() {
        mHandler.removeCallbacksAndMessages(null);
        if (mPulseAnimator != null) {
            mPulseAnimator.cancel();
            mPulseAnimator = null;
        }
    }

    private void startCrossfadeOut() {
        mState = State.CROSSFADING_OUT;
        mFreRootView.setClickable(false);
        mFreRootView.setImportantForAccessibility(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        if (!ValueAnimator.areAnimatorsEnabled()) {
            mFreRootView.setAlpha(0f);
            mFreRootView.setVisibility(View.GONE);
            // Post so the callback never runs inside onAccepted().
            mHandler.post(this::finishReveal);
            return;
        }

        mCrossfadeAnimator = ObjectAnimator.ofFloat(mFreRootView, View.ALPHA, 1.0f, 0.0f);
        mCrossfadeAnimator.setDuration(CROSSFADE_DURATION_MS);
        mCrossfadeAnimator.setInterpolator(Interpolators.LEGACY_ACCELERATE);
        mCrossfadeAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mFreRootView.setLayerType(View.LAYER_TYPE_NONE, null);
                        if (mState != State.CROSSFADING_OUT) {
                            return;
                        }
                        mFreRootView.setVisibility(View.GONE);
                        finishReveal();
                    }
                });
        mFreRootView.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        mCrossfadeAnimator.start();
    }

    private void finishReveal() {
        mState = State.REVEALED;
        mCrossfadeAnimator = null;
        mOnRevealComplete.onRevealComplete(mWasSkeletonShown, mResult);
    }

    void destroy() {
        if (mState == State.DESTROYED) {
            return;
        }
        mState = State.DESTROYED;
        stopWaitingSkeleton();
        if (mCrossfadeAnimator != null) {
            mCrossfadeAnimator.cancel();
            mCrossfadeAnimator = null;
        }
    }
}
