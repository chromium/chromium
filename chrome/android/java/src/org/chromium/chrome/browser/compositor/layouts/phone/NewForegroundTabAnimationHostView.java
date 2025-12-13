// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.RectEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.RoundedCornerAnimatorUtil;
import org.chromium.chrome.browser.hub.ShrinkExpandAnimator;
import org.chromium.chrome.browser.hub.ShrinkExpandImageView;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.animation.RunOnNextLayout;
import org.chromium.ui.animation.RunOnNextLayoutDelegate;
import org.chromium.ui.interpolators.Interpolators;

/**
 * Host view for the new foreground tab animation.
 *
 * <p>This custom FrameLayout is designed to be a completely non-clipping container that covers the
 * entire parent view. This is to force the whole view to be a "drawing" zone and avoid {@link
 * org.chromium.chrome.browser.hub.ShrinkExpandImageView} to be clipped when using view properties
 * like scale and translation. This problem is mostly present in multi-window mode.
 *
 * <p>This view is also responsible for running the new tab foreground animation.
 */
@NullMarked
public class NewForegroundTabAnimationHostView extends FrameLayout implements RunOnNextLayout {
    /** Interface for listening to events during the new foreground tab animation. */
    public interface Listener {
        /**
         * Called only on the successful completion of {@link #mExpandAnimatorSet}. This is
         * responsible for performing the tab selection (updating the tab ID and model in {@link
         * NewTabAnimationLayout}) and signaling the compositor to show the new tab.
         *
         * <p>Note: This method is intentionally not called if {@link #mExpandAnimatorSet} is
         * cancelled. This prevents race conditions in tab selection that can be observed when
         * multiple foreground tab creations are interrupted before the animation finishes.
         */
        void onExpandAnimationFinished();

        /**
         * Called when {@link #mFadeAnimator} is finished to indicate that the animation is done and
         * the view can be removed. If the animation is cancelled, then {@link
         * NewTabAnimationLayout#forceAnimationToFinish} removes the view instead.
         */
        void onForegroundAnimationFinished();
    }

    private static final String TAG = "NTAnimForeground";
    private static final long EXPAND_DURATION_MS = 300L;
    private static final long FADE_DURATION_MS = 150L;
    private final boolean mLogsEnabled;
    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;
    private final int[] mStartRadii;
    private final Rect mInitialRect;
    private final Listener mListener;
    private final ShrinkExpandImageView mRectView;

    private @Nullable ObjectAnimator mFadeAnimator;
    // Retains a strong reference to the {@link ShrinkExpandAnimator} on the class to prevent it
    // from being prematurely GC'd when using {@link ObjectAnimator}.
    private @Nullable ShrinkExpandAnimator mExpandAnimator;
    private @Nullable ValueAnimator mRectAnimator;
    private @Nullable ValueAnimator mCornerAnimator;
    private @Nullable AnimatorSet mExpandAnimatorSet;

    /**
     * @param context The Android{@link Context}.
     * @param initialRect The starting {@link Rect} for {@link #mRectView}.
     * @param startRadii The initial corner radii for {@link #mRectView}.
     * @param backgroundColor The background color for {@link #mRectView}.
     * @param isRtl Whether the layout is RTL.
     * @param listener The {@link Listener} to notify animation status.
     * @param logsEnabled Whether to enable logging.
     */
    public NewForegroundTabAnimationHostView(
            Context context,
            Rect initialRect,
            int[] startRadii,
            @ColorInt int backgroundColor,
            boolean isRtl,
            Listener listener,
            boolean logsEnabled) {
        super(context);
        setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setClipChildren(false);
        setClipToPadding(false);

        // Setting a background color forces the view to be considered a "drawing" view by Android.
        // This makes sure that the transformations (scale, translation) in ShrinkExpandImageView
        // are rendered correctly and not optimized away or improperly clipped.
        setBackgroundColor(ContextCompat.getColor(getContext(), android.R.color.transparent));

        mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
        mStartRadii = startRadii;
        mInitialRect = new Rect(initialRect);
        mListener = listener;
        mLogsEnabled = logsEnabled;

        mRectView = new ShrinkExpandImageView(context);
        // {@link View#INVISIBLE} is needed to generate the geometry information.
        mRectView.setVisibility(View.INVISIBLE);
        mRectView.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);
        addView(mRectView);
        mRectView.setRoundedFillColor(backgroundColor);
        mRectView.reset(mInitialRect);
        mRectView.setRoundedCorners(startRadii[0], startRadii[1], startRadii[2], startRadii[3]);
    }

    /**
     * Starts the new foreground tab animation.
     *
     * <p>The {@link Listener#onExpandAnimationFinished()} is called upon {@link
     * #mExpandAnimatorSet}.end().
     *
     * <p>{@link Listener#onForegroundAnimationFinished()} is called when the entire animation
     * finishes either by {@link #mFadeAnimator}.end(), {@link #mFadeAnimator}.cancel() or {@link
     * #mExpandAnimatorSet}.cancel().
     *
     * @param finalRect The final {@link Rect} for {@link #mRectView}
     * @param endRadii The final corner radii for {@link #mRectView}
     */
    /* package */ void startAnimation(Rect finalRect, int[] endRadii) {
        mExpandAnimator =
                new ShrinkExpandAnimator(
                        mRectView, mInitialRect, finalRect, /* searchBoxHeight= */ 0);
        mExpandAnimator.setRect(mInitialRect);
        // Make copies just in case the rects get mutated.
        mRectAnimator =
                ValueAnimator.ofObject(
                        new RectEvaluator(), new Rect(mInitialRect), new Rect(finalRect));
        mRectAnimator.addUpdateListener(
                animation -> {
                    if (mExpandAnimator == null) return;
                    mExpandAnimator.setRect((Rect) animation.getAnimatedValue());
                });

        mCornerAnimator =
                RoundedCornerAnimatorUtil.createRoundedCornerAnimator(
                        mRectView, mStartRadii, endRadii);

        AnimationFreezeChecker rectChecker =
                new AnimationFreezeChecker(AnimationFreezeChecker.FOREGROUND_RECT_TAG);
        mRectAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animation) {
                        rectChecker.onAnimationStart();
                        if (mLogsEnabled) Log.i(TAG, "mRectAnimator#onStart");
                    }

                    @Override
                    public void onEnd(Animator animation) {
                        rectChecker.onAnimationEnd();
                        if (mLogsEnabled) Log.i(TAG, "mRectAnimator#onEnd");
                    }

                    @Override
                    public void onCancel(Animator animation) {
                        rectChecker.onAnimationCancel();
                        if (mLogsEnabled) Log.i(TAG, "mRectAnimator#onCancel");
                    }
                });

        AnimationFreezeChecker cornerChecker =
                new AnimationFreezeChecker(AnimationFreezeChecker.FOREGROUND_CORNER_TAG);
        mCornerAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animation) {
                        cornerChecker.onAnimationStart();
                        if (mLogsEnabled) Log.i(TAG, "mCornerAnimator#onStart");
                    }

                    @Override
                    public void onEnd(Animator animation) {
                        cornerChecker.onAnimationEnd();
                        if (mLogsEnabled) Log.i(TAG, "mCornerAnimator#onEnd");
                    }

                    @Override
                    public void onCancel(Animator animation) {
                        cornerChecker.onAnimationCancel();
                        if (mLogsEnabled) Log.i(TAG, "mCornerAnimator#onCancel");
                    }
                });

        mFadeAnimator = ObjectAnimator.ofFloat(mRectView, ShrinkExpandImageView.ALPHA, 1f, 0f);
        mFadeAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mFadeAnimator.setDuration(FADE_DURATION_MS);
        AnimationFreezeChecker fadeChecker =
                new AnimationFreezeChecker(AnimationFreezeChecker.FOREGROUND_FADE_TAG);
        mFadeAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onStart(Animator animation) {
                        fadeChecker.onAnimationStart();
                        if (mLogsEnabled) Log.i(TAG, "mFadeAnimator#onStart");
                    }

                    @Override
                    public void onEnd(Animator animation) {
                        fadeChecker.onAnimationEnd();
                        if (mLogsEnabled) Log.i(TAG, "mFadeAnimator#onEnd");
                        mListener.onForegroundAnimationFinished();
                        mFadeAnimator = null;
                    }

                    @Override
                    public void onCancel(Animator animation) {
                        fadeChecker.onAnimationCancel();
                        if (mLogsEnabled) Log.i(TAG, "mFadeAnimator#onCancel");
                        mFadeAnimator = null;
                    }
                });

        mExpandAnimatorSet = new AnimatorSet();
        mExpandAnimatorSet.playTogether(mRectAnimator, mCornerAnimator);
        mExpandAnimatorSet.setDuration(EXPAND_DURATION_MS);
        mExpandAnimatorSet.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        AnimationFreezeChecker expandChecker =
                new AnimationFreezeChecker(AnimationFreezeChecker.FOREGROUND_EXPAND_TAG);
        mExpandAnimatorSet.addListener(
                new CancelAwareAnimatorListener() {
                    private void clearAnimators() {
                        mExpandAnimatorSet = null;
                        mRectAnimator = null;
                        mExpandAnimator = null;
                        mCornerAnimator = null;
                    }

                    @Override
                    public void onStart(Animator animation) {
                        expandChecker.onAnimationStart();
                        if (mLogsEnabled) Log.i(TAG, "mExpandAnimatorSet#onStart");
                    }

                    @Override
                    public void onEnd(Animator animation) {
                        expandChecker.onAnimationEnd();
                        if (mLogsEnabled) Log.i(TAG, "mExpandAnimatorSet#onEnd");
                        mListener.onExpandAnimationFinished();
                        clearAnimators();
                        assumeNonNull(mFadeAnimator);
                        mFadeAnimator.start();
                    }

                    @Override
                    public void onCancel(Animator animation) {
                        // Intentionally skip {@link mListener.onExpandAnimationFinished()} to
                        // prevent race conditions during tab selection (see Javadoc).
                        expandChecker.onAnimationCancel();
                        if (mLogsEnabled) Log.i(TAG, "mExpandAnimatorSet#onCancel");
                        clearAnimators();
                        mFadeAnimator = null;
                    }
                });
        mRectView.setVisibility(View.VISIBLE);
        mExpandAnimatorSet.start();
    }

    @Override
    public void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        runOnNextLayoutRunnables();
    }

    @Override
    public void runOnNextLayout(Runnable runnable) {
        mRunOnNextLayoutDelegate.runOnNextLayout(runnable);
    }

    @Override
    public void runOnNextLayoutRunnables() {
        mRunOnNextLayoutDelegate.runOnNextLayoutRunnables();
    }

    /** Returns whether the expand animation is currently running. */
    /* package */ boolean isExpandAnimationRunning() {
        return mExpandAnimatorSet != null && mExpandAnimatorSet.isRunning();
    }

    /**
     * Forces the animation to finish. If the expand animation is running, it will be canceled, and
     * the fade-out animation will be skipped. If the fade-out animation is running, it will be
     * cancelled.
     */
    /* package */ void forceAnimationToFinish() {
        if (mExpandAnimatorSet != null) {
            if (mLogsEnabled) Log.i(TAG, "forceAnimationToFinish: mExpandAnimatorSet#cancel");
            mExpandAnimatorSet.cancel();
        } else if (mFadeAnimator != null) {
            if (mLogsEnabled) Log.i(TAG, "forceAnimationToFinish: mFadeAnimator#cancel");
            mFadeAnimator.cancel();
        }
    }
}
