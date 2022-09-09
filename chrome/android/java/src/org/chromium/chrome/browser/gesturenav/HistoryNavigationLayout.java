// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.gesture.GesturePoint;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.gesturenav.NavigationBubble.CloseTarget;

/**
 * FrameLayout that supports side-wise slide gesture for history navigation.
 */
class HistoryNavigationLayout extends FrameLayout implements ViewGroup.OnHierarchyChangeListener {
    // {@link NavigationGlow} object for rendered pages.
    private final NavigationGlow mCompositorGlowEffect;

    // Whether the current tab shows a native or rendered page.
    private final Supplier<Boolean> mIsNativePage;

    // Callback that performs navigation action in response to UI.,
    private final Callback<Boolean> mNavigateCallback;

    // Frame layout hosting the arrow puck UI.
    private SideSlideLayout mSideSlideLayout;

    // {@link NavigationGlow} object for native pages. Lazily created.
    private NavigationGlow mJavaGlowEffect;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopNavigatingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure
    // it does not conflict with pending Android draws.
    private Runnable mDetachLayoutRunnable;

    public HistoryNavigationLayout(Context context, Supplier<Boolean> isNativePage,
            NavigationGlow compositorGlowEffect, Callback<Boolean> navigateCallback) {
        super(context);
        mIsNativePage = isNativePage;
        mCompositorGlowEffect = compositorGlowEffect;
        mNavigateCallback = navigateCallback;
        setOnHierarchyChangeListener(this);
        setVisibility(View.INVISIBLE);
    }

    @Override
    public void onChildViewAdded(View parent, View child) {
        if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
    }

    @Override
    public void onChildViewRemoved(View parent, View child) {
        // TODO(jinsukkim): Replace INVISIBLE with GONE to avoid performing layout/measurements.
        if (getChildCount() == 0) setVisibility(View.INVISIBLE);
    }

    /**
     * Creates a view hosting the gesture navigation UI.
     * @return The created view.
     */
    private SideSlideLayout createLayout() {
        mSideSlideLayout = new SideSlideLayout(getContext());
        mSideSlideLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        return mSideSlideLayout;
    }

    /**
     * Start showing arrow widget for navigation back/forward.
     * @param forward {@code true} for forward navigation, or {@code false} for back.
     * @param closeIndicator
     */
    void showBubble(boolean forward, @CloseTarget int closeIndicator) {
        if (mSideSlideLayout == null) {
            SideSlideLayout sideSlideLayout = createLayout();
            sideSlideLayout.setOnNavigationListener((direction) -> {
                mNavigateCallback.onResult(direction);
                cancelStopNavigatingRunnable();
                sideSlideLayout.post(getStopNavigatingRunnable());
            });
            sideSlideLayout.setOnResetListener(() -> {
                if (getDetachLayoutRunnable() != null) return;
                sideSlideLayout.post(createDetachLayoutRunnable());
            });
        }
        mSideSlideLayout.setEnabled(true);
        mSideSlideLayout.setDirection(forward);
        mSideSlideLayout.setCloseIndicator(closeIndicator);
        attachLayoutIfNecessary();
        mSideSlideLayout.start();
    }

    /**
     * Create {@link NavigationGlow} object, lazily when possible.
     */
    private NavigationGlow getGlowEffect() {
        if (mIsNativePage.get()) {
            if (mJavaGlowEffect == null) mJavaGlowEffect = new AndroidUiNavigationGlow(this);
            return mJavaGlowEffect;
        } else {
            return mCompositorGlowEffect;
        }
    }

    /**
     * Start showing edge glow effect.
     * @param p Current position of the touch event.
     */
    void showGlow(GesturePoint p) {
        getGlowEffect().prepare(p.x, p.y);
    }

    /**
     * Signals a pull update.
     * @param offset The change in horizontal pull distance (positive if toward right,
     *         negative if left).
     */
    void pullBubble(float offset) {
        if (mSideSlideLayout == null) return;
        mSideSlideLayout.pull(offset);
    }

    /**
     * Signals a pull update for glow effect.
     * @param offset The change in horizontal pull distance.
     */
    void pullGlow(float offset) {
        getGlowEffect().onScroll(offset);
    }

    /**
     * Release the active pull. If no pull has started, the release will be ignored.
     * If the pull was sufficiently large, the navigation sequence will be initiated.
     * @param allowNav {@code true} if release action is supposed to trigger navigation.
     */
    void releaseBubble(boolean allowNav) {
        if (mSideSlideLayout == null) return;
        cancelStopNavigatingRunnable();
        mSideSlideLayout.release(allowNav);
    }

    /**
     * Release the glow effect.
     */
    void releaseGlow() {
        getGlowEffect().release();
    }

    /**
     * Reset navigation bubble UI in action.
     */
    void resetBubble() {
        if (mSideSlideLayout == null) return;
        cancelStopNavigatingRunnable();
        mSideSlideLayout.reset();
    }

    /**
     * Reset the glow effect.
     */
    void resetGlow() {
        getGlowEffect().reset();
    }

    /**
     * @return {@link SideSlideLayout} object.
     */
    SideSlideLayout getSideSlideLayout() {
        return mSideSlideLayout;
    }

    /**
     * Cancel navigation operation by removing the runnable in the queue.
     */
    void cancelStopNavigatingRunnable() {
        if (mStopNavigatingRunnable != null) {
            mSideSlideLayout.removeCallbacks(mStopNavigatingRunnable);
            mStopNavigatingRunnable = null;
        }
    }

    Runnable getDetachLayoutRunnable() {
        return mDetachLayoutRunnable;
    }

    Runnable createDetachLayoutRunnable() {
        mDetachLayoutRunnable = () -> {
            mDetachLayoutRunnable = null;
            detachLayoutIfNecessary();
        };
        return mDetachLayoutRunnable;
    }

    /**
     * Cancel the operation detaching the layout from view hierarchy.
     */
    void cancelDetachLayoutRunnable() {
        if (mDetachLayoutRunnable != null) {
            mSideSlideLayout.removeCallbacks(mDetachLayoutRunnable);
            mDetachLayoutRunnable = null;
        }
    }

    Runnable getStopNavigatingRunnable() {
        if (mStopNavigatingRunnable == null) {
            mStopNavigatingRunnable = () -> mSideSlideLayout.stopNavigating();
        }
        return mStopNavigatingRunnable;
    }

    /**
     * Attach {@link SideSlideLayout} to view hierarchy when UI is activated.
     */
    private void attachLayoutIfNecessary() {
        // The animation view is attached/detached on-demand to minimize overlap
        // with composited SurfaceView content.
        cancelDetachLayoutRunnable();
        if (isLayoutDetached()) addView(mSideSlideLayout);
    }

    private void detachLayoutIfNecessary() {
        if (isLayoutDetached()) return;
        cancelDetachLayoutRunnable();
        removeView(mSideSlideLayout);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean isLayoutDetached() {
        return mSideSlideLayout == null || mSideSlideLayout.getParent() == null;
    }
}
