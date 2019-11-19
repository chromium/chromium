// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * This view is used to obscure content and bring focus to a foreground view (i.e. the bottom sheet
 * or the omnibox suggestions).
 *
 * If the view is disabled, then its alpha will be set to 0f and it will not receive touch events.
 *
 * To use the scrim, {@link #showScrim(ScrimParams)} must be called to set the params for
 * how the scrim will behave. After that, users can either allow the default animation to run or
 * change the view's alpha manually using {@link #setViewAlpha(float)}. Once the scrim is done being
 * used, {@link #hideScrim(boolean)} should be called.
 */
public class ScrimView extends View implements View.OnClickListener {
    /** Params that define the behavior of the scrim for a single user. */
    public static class ScrimParams {
        /**
         * The top margin of the scrim. This can be used to shrink the scrim to show items at the
         * top of the screen.
         */
        public final int topMargin;

        /** Whether the scrim should affect the status bar color. */
        public final boolean affectsStatusBar;

        /** The view that the scrim is using to place itself in the hierarchy. */
        public final View anchorView;

        /** Whether the scrim should show in front of the anchor view. */
        public final boolean showInFrontOfAnchorView;

        /** An observer for visibility and input related events. */
        public final ScrimObserver observer;

        /**
         * The background color for the {@link ScrimView}. If null, a default color will be set as
         * the background, unless {@link #backgroundDrawable} is set.
         */
        @Nullable
        public Integer backgroundColor;

        /**
         * Background of the {@link ScrimView}.
         *
         * <p>When this is set, no default background color applies and {@link #backgroundColor} is
         * ignored.
         *
         * <p>The drawable is responsible for filling in the background with the appropriate color.
         * When the scrim should cover the status bar, the background color drawn by this drawable
         * must be consistent with the status bar's color.
         */
        @Nullable
        public Drawable backgroundDrawable;

        /**
         * A filter for touch event that happen on this view.
         *
         * <p>The filter intercepts click events, which means that {@link
         * ScrimObserver#onScrimClick} will not be called when an event filter is set.
         */
        @Nullable
        public EventFilter eventFilter;

        /**
         * Build a new set of params to control the scrim.
         * @param anchorView The view that the scrim is using to place itself in the hierarchy.
         * @param showInFrontOfAnchorView Whether the scrim should show in front of the anchor view.
         * @param affectsStatusBar Whether the scrim should affect the status bar color.
         * @param topMargin The top margin of the scrim. This can be used to shrink the scrim to
         *                  show items at the top of the screen.
         * @param observer n observer for visibility and input related events.
         */
        public ScrimParams(View anchorView, boolean showInFrontOfAnchorView,
                boolean affectsStatusBar, int topMargin, ScrimObserver observer) {
            this.topMargin = topMargin;
            this.affectsStatusBar = affectsStatusBar;
            this.anchorView = anchorView;
            this.showInFrontOfAnchorView = showInFrontOfAnchorView;
            this.observer = observer;
        }
    }

    /**
     * A delegate to expose functionality that changes the scrim over the status bar. This will only
     * affect Android versions >= M.
     */
    public interface StatusBarScrimDelegate {
        /**
         * Set the amount of scrim over the status bar. The implementor may choose to not respect
         * the value provided to this method.
         * @param scrimFraction The scrim fraction over the status bar. 0 is completely hidden, 1 is
         *                      completely shown.
         */
        void setStatusBarScrimFraction(float scrimFraction);
    }

    /**
     * An interface for listening to events on the fading view.
     */
    public interface ScrimObserver {
        /**
         * An event that triggers when the view is clicked.
         */
        void onScrimClick();

        /**
         * An event that triggers when the visibility of the overlay has changed. Visibility is true
         * if the overlay's opacity is > 0f.
         * @param visible True if the overlay has become visible.
         */
        void onScrimVisibilityChanged(boolean visible);
    }

    /** The duration for the fading animation. */
    private static final int FADE_DURATION_MS = 300;

    /** A means of changing the statusbar color. */
    private final StatusBarScrimDelegate mStatusBarScrimDelegate;

    /** The view that the scrim should exist in. */
    private final ViewGroup mParent;

    /** The default background color if {@link ScrimParams#backgroundColor} is not set. */
    private final int mDefaultBackgroundColor;

    /** The animator for fading the view out. */
    private ObjectAnimator mOverlayFadeInAnimator;

    /** The animator for fading the view in. */
    private ObjectAnimator mOverlayFadeOutAnimator;

    /** The active animator (if any). */
    private Animator mOverlayAnimator;

    /** The current set of params affecting the scrim. */
    private ScrimParams mActiveParams;

    /** The duration for the fading animation. This can be overridden for testing. */
    private int mFadeDurationMs;

    /** If true, {@code mActiveParams.eventFilter} is set, but was never called. */
    private boolean mIsNewEventFilter;

    /**
     * @param context An Android {@link Context} for creating the view.
     * @param scrimDelegate A means of changing the scrim over the status bar.
     * @param parent The {@link ViewGroup} the scrim should exist in.
     */
    public ScrimView(
            Context context, @Nullable StatusBarScrimDelegate scrimDelegate, ViewGroup parent) {
        super(context);
        mStatusBarScrimDelegate = scrimDelegate;
        mParent = parent;
        mDefaultBackgroundColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.omnibox_focused_fading_background_color);
        mFadeDurationMs = FADE_DURATION_MS;
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);

        setAlpha(0.0f);
        setVisibility(View.GONE);
        setOnClickListener(this);
        setBackgroundColor(mDefaultBackgroundColor);
    }

    /**
     * Place the scrim in the view hierarchy.
     * @param view The view the scrim should be placed in front of or behind.
     * @param inFrontOf If true, the scrim is placed in front of the specified view, otherwise it is
     *                  placed behind it.
     */
    private void placeScrimInHierarchy(View view, boolean inFrontOf) {
        // Climb the view hierarchy until we reach the target parent.
        while (view.getParent() != mParent) {
            if (!(view instanceof ViewGroup)) {
                assert false : "Focused view must be part of the hierarchy!";
            }
            view = (View) view.getParent();
        }
        UiUtils.removeViewFromParent(this);
        if (inFrontOf) {
            UiUtils.insertAfter(mParent, this, view);
        } else {
            UiUtils.insertBefore(mParent, this, view);
        }
    }

    /**
     * Set the alpha for the fading view. This specifically does not override
     * {@link #setAlpha(float)} so animations can be canceled if this is called.
     * @param alpha The desired alpha for this view.
     */
    public void setViewAlpha(float alpha) {
        assert mActiveParams != null : "#showScrim must be called before setting alpha!";

        if (!isEnabled() || MathUtils.areFloatsEqual(alpha, getAlpha())) return;

        setAlpha(alpha);

        if (mOverlayAnimator != null) mOverlayAnimator.cancel();
    }

    /**
     * A notification that the set of params that affect the scrim changed.
     * @param params The scrim's params.
     */
    private void onParamsChanged(ScrimParams params) {
        mActiveParams = params;
        UiUtils.removeViewFromParent(this);
        if (params != null && params.backgroundDrawable != null) {
            setBackgroundDrawable(params.backgroundDrawable);
        } else {
            setBackgroundColor(params != null && params.backgroundColor != null
                            ? params.backgroundColor
                            : mDefaultBackgroundColor);
        }
        if (params == null || params.anchorView == null) return;

        placeScrimInHierarchy(params.anchorView, params.showInFrontOfAnchorView);
        getLayoutParams().width = LayoutParams.MATCH_PARENT;
        getLayoutParams().height = LayoutParams.MATCH_PARENT;
        assert getLayoutParams() instanceof MarginLayoutParams;
        ((MarginLayoutParams) getLayoutParams()).topMargin = params.topMargin;
        mIsNewEventFilter = params.eventFilter != null;
    }

    @Override
    public void setEnabled(boolean isEnabled) {
        super.setEnabled(isEnabled);

        if (!isEnabled) {
            if (mOverlayAnimator != null) mOverlayAnimator.cancel();
            setAlpha(0f);
        }
    }

    /**
     * Sets the alpha for this view and alters visibility based on that value.
     * WARNING: This method should not be called externally for this view! Use setViewAlpha instead.
     * @param alpha The alpha to set the view to.
     */
    @Override
    public void setAlpha(float alpha) {
        super.setAlpha(alpha);

        if (mActiveParams != null && mActiveParams.affectsStatusBar
                && mStatusBarScrimDelegate != null) {
            mStatusBarScrimDelegate.setStatusBarScrimFraction(alpha);
        }
    }

    /**
     * Triggers a fade in of the omnibox results background creating a new animation if necessary.
     */
    public void showScrim(@NonNull ScrimParams params) {
        onParamsChanged(params);
        setVisibility(View.VISIBLE);
        if (mActiveParams.observer != null) {
            mActiveParams.observer.onScrimVisibilityChanged(true);
        }
        if (mOverlayFadeInAnimator == null) {
            mOverlayFadeInAnimator = ObjectAnimator.ofFloat(this, ALPHA, 1f);
            mOverlayFadeInAnimator.setDuration(mFadeDurationMs);
            mOverlayFadeInAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        }

        runFadeAnimation(mOverlayFadeInAnimator);
    }

    /**
     * Triggers a fade out of the omnibox results background creating a new animation if necessary.
     */
    public void hideScrim(boolean fadeOut) {
        if (mOverlayFadeOutAnimator == null) {
            mOverlayFadeOutAnimator = ObjectAnimator.ofFloat(this, ALPHA, 0f);
            mOverlayFadeOutAnimator.setDuration(mFadeDurationMs);
            mOverlayFadeOutAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            mOverlayFadeOutAnimator.addListener(new CancelAwareAnimatorListener() {
                @Override
                public void onEnd(Animator animation) {
                    setVisibility(View.GONE);
                    if (mActiveParams != null && mActiveParams.observer != null) {
                        mActiveParams.observer.onScrimVisibilityChanged(false);
                    }
                    onParamsChanged(null);
                }
            });
        }

        mOverlayFadeOutAnimator.setFloatValues(getAlpha(), 0f);
        runFadeAnimation(mOverlayFadeOutAnimator);
        if (!fadeOut) mOverlayFadeOutAnimator.end();
    }

    /**
     * Runs an animation for this view. If one is running, the existing one will be canceled.
     * @param fadeAnimation The animation to run.
     */
    private void runFadeAnimation(Animator fadeAnimation) {
        if (mOverlayAnimator == fadeAnimation && mOverlayAnimator.isRunning()) {
            return;
        } else if (mOverlayAnimator != null) {
            mOverlayAnimator.cancel();
        }
        mOverlayAnimator = fadeAnimation;
        mOverlayAnimator.start();
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        EventFilter eventFilter = mActiveParams == null ? null : mActiveParams.eventFilter;
        if (eventFilter == null) return super.onTouchEvent(e);

        // Make sure the first event that goes through the filter is an ACTION_DOWN, even in the
        // case where the filter is added while a gesture is already in progress.
        if (mIsNewEventFilter && e.getActionMasked() != MotionEvent.ACTION_DOWN) {
            MotionEvent downEvent = MotionEvent.obtain(e);
            downEvent.setAction(MotionEvent.ACTION_DOWN);
            if (!eventFilter.onTouchEvent(downEvent)) return false;
        }
        mIsNewEventFilter = false;
        return eventFilter.onTouchEvent(e);
    }

    @Override
    public void onClick(View view) {
        if (mActiveParams == null || mActiveParams.observer == null) return;
        mActiveParams.observer.onScrimClick();
    }

    @VisibleForTesting
    public void disableAnimationForTesting(boolean disable) {
        mFadeDurationMs = disable ? 0 : FADE_DURATION_MS;
    }
}
