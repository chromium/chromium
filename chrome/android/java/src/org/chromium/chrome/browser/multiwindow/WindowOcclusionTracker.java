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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.display.DisplayAndroid;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Monitors the position, size, and z-order of Chrome windows for occlusion tracking. */
@NullMarked
public class WindowOcclusionTracker implements ViewTreeObserver.OnGlobalLayoutListener {
    // |TAG| can be at most 20 characters, so the class name itself is too long.
    private static final String TAG = "OcclusionTracking";
    private static final boolean DEBUG_LOGGING = false;
    private static @Nullable WindowOcclusionTracker sInstance;

    private final WindowZOrderTracker mWindowZOrderTracker;
    private final int mMinimumVisibilitySizeThreshold;

    @VisibleForTesting
    WindowOcclusionTracker(@Nullable WindowZOrderTracker windowZOrderTracker) {
        mMinimumVisibilitySizeThreshold =
                UiAndroidFeatureList.sAndroidWindowOcclusionMinimumVisibilitySizeThreshold
                        .getValue();

        if (windowZOrderTracker == null) {
            mWindowZOrderTracker = new WindowZOrderTracker(this::forwardOcclusionState);
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
            forwardOcclusionState();
        }
    }

    // State listener callbacks.
    @Override
    public void onGlobalLayout() {
        forwardOcclusionState();
    }

    // End of state listener callbacks.

    private @Nullable View getPrimaryView(ActivityWindowAndroid windowAndroid) {
        Window window = windowAndroid.getWindow();
        return window == null ? null : window.getDecorView();
    }

    private void forwardOcclusionState() {
        Map<ActivityWindowAndroid, Boolean> occlusionState = calculateOcclusion();

        if (DEBUG_LOGGING) {
            for (Map.Entry<ActivityWindowAndroid, Boolean> entry : occlusionState.entrySet()) {
                Log.i(TAG, "Window: %s is occluded: %b", entry.getKey(), entry.getValue());
            }
        }

        for (Map.Entry<ActivityWindowAndroid, Boolean> entry : occlusionState.entrySet()) {
            entry.getKey().setOccluded(entry.getValue());
        }
    }

    @VisibleForTesting
    Map<ActivityWindowAndroid, Boolean> calculateOcclusion() {
        final SparseArray<List<ActivityWindowAndroid>> zOrder =
                mWindowZOrderTracker.getWindowZOrder();

        Map<ActivityWindowAndroid, Boolean> occlusionState = new HashMap<>();
        // Default to unoccluded for all windows. In the case of any error, the unoccluded state
        // will be forwarded to the window.
        for (ActivityWindowAndroid window : mWindowZOrderTracker.getAllWindowAndroids()) {
            occlusionState.put(window, false);
        }

        for (int i = 0; i < zOrder.size(); i++) {
            int displayId = zOrder.keyAt(i);
            List<ActivityWindowAndroid> windows = zOrder.valueAt(i);
            if (windows.size() < 2) continue;

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

            calculateOcclusionForDisplay(windows, displayWidth, displayHeight, occlusionState);
        }
        return occlusionState;
    }

    private void calculateOcclusionForDisplay(
            List<ActivityWindowAndroid> windows,
            int displayWidth,
            int displayHeight,
            Map<ActivityWindowAndroid, Boolean> occlusionState) {
        Region cumulativeOccludedRegion = new Region();
        Region viewVisibleRegion = new Region();

        // Iterate in reverse z-order (top to bottom).
        for (int i = windows.size() - 1; i >= 0; i--) {
            ActivityWindowAndroid window = windows.get(i);
            View view = getPrimaryView(window);
            if (view == null) {
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
                WindowOcclusionMetrics.recordCalculateResult(
                        WindowOcclusionMetrics.CalculateResult.VISIBLE_RECT_EMPTY);
                continue;
            }

            WindowOcclusionMetrics.recordCalculateResult(
                    WindowOcclusionMetrics.CalculateResult.SUCCESS);

            if (cumulativeOccludedRegion.quickContains(viewScreenRect)) {
                // The window is fully occluded.
                occlusionState.put(window, true);
                continue;
            }

            if (cumulativeOccludedRegion.quickReject(viewScreenRect)) {
                // The window does not overlap with the occluded region.
                occlusionState.put(window, false);
                cumulativeOccludedRegion.op(viewScreenRect, Region.Op.UNION);
                continue;
            }

            // Subtract the cumulative occluded region from the view screen rect to see if any
            // part is visible.
            viewVisibleRegion.set(viewScreenRect);
            viewVisibleRegion.op(cumulativeOccludedRegion, Region.Op.DIFFERENCE);

            // Add the view to the cumulative occluded region.
            cumulativeOccludedRegion.op(viewScreenRect, Region.Op.UNION);

            occlusionState.put(window, !isNoticeablyVisible(viewVisibleRegion));
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
