// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.display.DisplayAndroid;

import java.util.List;

/** Monitors the position, size, and z-order of Chrome windows for occlusion tracking. */
@NullMarked
public class WindowOcclusionTracker implements ViewTreeObserver.OnGlobalLayoutListener {
    // |TAG| can be at most 20 characters, so the class name itself is too long.
    private static final String TAG = "OcclusionTracking";
    private static final boolean DEBUG_LOGGING = false;
    private static @Nullable WindowOcclusionTracker sInstance;

    private final WindowZOrderTracker mWindowZOrderTracker;
    private final int mMinimumVisibilitySizeThreshold;
    private final int mCalculateOcclusionRateLimitMs;
    private long mLastCalculateOcclusionTimeMs;
    private boolean mCalculateOcclusionPending;

    @VisibleForTesting
    WindowOcclusionTracker(@Nullable WindowZOrderTracker windowZOrderTracker) {
        mMinimumVisibilitySizeThreshold =
                UiAndroidFeatureList.sAndroidWindowOcclusionMinimumVisibilitySizeThreshold
                        .getValue();
        mCalculateOcclusionRateLimitMs =
                UiAndroidFeatureList.sAndroidWindowOcclusionCalculateOcclusionRateLimitMs
                        .getValue();

        if (windowZOrderTracker == null) {
            mWindowZOrderTracker = new WindowZOrderTracker(this::calculateOcclusionRateLimited);
        } else {
            mWindowZOrderTracker = windowZOrderTracker;
        }
    }

    /** Returns the singleton instance of the WindowOcclusionTracker. */
    public static WindowOcclusionTracker getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new WindowOcclusionTracker(null);
        }
        return sInstance;
    }

    /**
     * Starts tracking the given window.
     *
     * @param windowAndroid The window to track.
     */
    public void track(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (mWindowZOrderTracker.track(windowAndroid)) {
            View view = getPrimaryView(windowAndroid);
            if (view == null) {
                WindowOcclusionMetrics.recordTrackResult(false);
                return;
            }

            view.getViewTreeObserver().addOnGlobalLayoutListener(this);
            WindowOcclusionMetrics.recordTrackResult(true);

            windowAndroid.setIsOcclusionTracked(true);

            // No need to forward the occlusion state here, as it will be done shortly by the
            // WindowZOrderTracker once the new window receives the initial focus change event.
        }
    }

    /**
     * Stops tracking the given window.
     *
     * @param windowAndroid The window to stop tracking.
     */
    public void untrack(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (mWindowZOrderTracker.untrack(windowAndroid)) {
            View view = getPrimaryView(windowAndroid);
            if (view == null) {
                WindowOcclusionMetrics.recordUntrackResult(false);
                return;
            }

            view.getViewTreeObserver().removeOnGlobalLayoutListener(this);
            WindowOcclusionMetrics.recordUntrackResult(true);

            // Calculate occlusion state here in case the window was removed for some reason that
            // didn't trigger a focus change or position update (such as a crash or kill).
            calculateOcclusionRateLimited();
        }
    }

    // State listener callbacks.
    @Override
    public void onGlobalLayout() {
        calculateOcclusionRateLimited();
    }

    // End of state listener callbacks.

    private @Nullable View getPrimaryView(ActivityWindowAndroid windowAndroid) {
        Window window = windowAndroid.getWindow();
        return window == null ? null : window.getDecorView();
    }

    @VisibleForTesting
    void calculateOcclusionRateLimited() {
        ThreadUtils.assertOnUiThread();

        // Calculate occlusion immediately if the rate limit is not set.
        if (mCalculateOcclusionRateLimitMs <= 0) {
            calculateOcclusion();
            return;
        }

        if (mCalculateOcclusionPending) {
            // We already have a pending calculation, so we don't need to schedule another one
            // or perform one now.
            return;
        }

        long now = TimeUtils.uptimeMillis();
        long nextCalculateOcclusionTimeMs =
                mLastCalculateOcclusionTimeMs + mCalculateOcclusionRateLimitMs;

        if (now < nextCalculateOcclusionTimeMs) {
            mCalculateOcclusionPending = true;
            ThreadUtils.postOnUiThreadDelayed(
                    () -> {
                        mCalculateOcclusionPending = false;
                        calculateOcclusion();
                    },
                    nextCalculateOcclusionTimeMs - now);
            return;
        }

        calculateOcclusion();
    }

    @VisibleForTesting
    void calculateOcclusion() {
        WindowOcclusionMetrics.onCalculateOcclusion();
        mLastCalculateOcclusionTimeMs = TimeUtils.uptimeMillis();

        try (TimingMetric t = WindowOcclusionMetrics.getCalculateDurationTimer()) {
            final SparseArray<List<ActivityWindowAndroid>> zOrder =
                    mWindowZOrderTracker.getWindowZOrder();

            for (int i = 0; i < zOrder.size(); i++) {
                int displayId = zOrder.keyAt(i);
                List<ActivityWindowAndroid> windows = zOrder.valueAt(i);
                if (windows.size() < 2) {
                    if (windows.size() == 1) {
                        windows.get(0).setOccluded(false, null, null);
                    }
                    continue;
                }

                DisplayAndroid displayAndroid = windows.get(0).getDisplay();
                int displayWidth = displayAndroid.getDisplayWidth();
                int displayHeight = displayAndroid.getDisplayHeight();

                if (DEBUG_LOGGING) {
                    Log.i(
                            TAG,
                            "Display size for display ID %d: %d x %d",
                            displayId,
                            displayWidth,
                            displayHeight);
                }

                calculateOcclusionForDisplay(windows, displayWidth, displayHeight);
            }
        }
    }

    private void calculateOcclusionForDisplay(
            List<ActivityWindowAndroid> windows, int displayWidth, int displayHeight) {
        Region cumulativeOccludedRegion = new Region();
        Region viewVisibleRegion = new Region();

        // Iterate in reverse z-order (top to bottom).
        for (int i = windows.size() - 1; i >= 0; i--) {
            ActivityWindowAndroid window = windows.get(i);
            View view = getPrimaryView(window);
            if (view == null) {
                window.setOccluded(false, null, null);
                WindowOcclusionMetrics.recordCalculateResult(
                        WindowOcclusionMetrics.CalculateResult.VIEW_NOT_FOUND);
                continue;
            }

            // Get the current screen rect for the view. We cannot cache this value across
            // calculations as it could change.
            Rect viewScreenRect = getScreenVisibleRect(view, displayWidth, displayHeight);

            if (viewScreenRect.isEmpty()) {
                // In case of an error, continue the calculation for subsequent windows. Since the
                // occluded area is cumulative, any zero-sized view will not affect the correctness
                // of the occlusion state for subsequent windows.
                // Setting null for the bounds and visible region causes the window to be considered
                // fully visible which is the safest fallback in this case.
                window.setOccluded(false, null, null);
                WindowOcclusionMetrics.recordCalculateResult(
                        WindowOcclusionMetrics.CalculateResult.VISIBLE_RECT_EMPTY);
                continue;
            }

            WindowOcclusionMetrics.recordCalculateResult(
                    WindowOcclusionMetrics.CalculateResult.SUCCESS);

            if (cumulativeOccludedRegion.quickContains(viewScreenRect)) {
                // The window is fully occluded.
                if (DEBUG_LOGGING) {
                    Log.i(TAG, "Window: %s is occluded: true", window);
                }
                window.setOccluded(true, viewScreenRect, null);
                continue;
            }

            if (cumulativeOccludedRegion.quickReject(viewScreenRect)) {
                // The window does not overlap with the occluded region.
                if (DEBUG_LOGGING) {
                    Log.i(TAG, "Window: %s is occluded: false", window);
                }
                window.setOccluded(false, viewScreenRect, null);
                cumulativeOccludedRegion.op(viewScreenRect, Region.Op.UNION);
                continue;
            }

            // Subtract the cumulative occluded region from the view screen rect to see if any
            // part is visible.
            viewVisibleRegion.set(viewScreenRect);
            viewVisibleRegion.op(cumulativeOccludedRegion, Region.Op.DIFFERENCE);

            // Add the view to the cumulative occluded region.
            cumulativeOccludedRegion.op(viewScreenRect, Region.Op.UNION);

            boolean isOccluded = !isNoticeablyVisible(viewVisibleRegion);
            if (DEBUG_LOGGING) {
                Log.i(TAG, "Window: %s is occluded: %b", window, isOccluded);
            }
            window.setOccluded(isOccluded, viewScreenRect, viewVisibleRegion);
        }
    }

    private boolean isNoticeablyVisible(Region visibleRegion) {
        RegionIterator iterator = new RegionIterator(visibleRegion);
        Rect rect = new Rect();
        while (iterator.next(rect)) {
            if (rect.width() >= mMinimumVisibilitySizeThreshold
                    && rect.height() >= mMinimumVisibilitySizeThreshold) {
                return true;
            }
        }

        if (DEBUG_LOGGING && !visibleRegion.isEmpty()) {
            Log.i(TAG, "Setting partially visible view as occluded: %s", visibleRegion);
        }

        return false;
    }

    /**
     * Returns the visible rect of the view in absolute screen coordinates.
     *
     * <p>We cannot just use {@link View#getGlobalVisibleRect} because it sets the rect to the
     * "coordinates of the non-clipped area of this view in the coordinate space of the view's root
     * view." We must combine it with {@link View#getLocationOnScreen} on the root view to get the
     * absolute position of the window in terms of the screen.
     */
    private Rect getScreenVisibleRect(View view, int displayWidth, int displayHeight) {
        Rect r = new Rect();

        if (!view.getGlobalVisibleRect(r)) {
            return new Rect();
        }

        int[] loc = new int[2];
        view.getRootView().getLocationOnScreen(loc);
        r.offset(loc[0], loc[1]);

        if (!r.intersect(0, 0, displayWidth, displayHeight)) {
            return new Rect();
        }
        return r;
    }
}
