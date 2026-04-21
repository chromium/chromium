// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.util.SparseArray;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tracks the z-order of Chrome windows. This class listens to window focus changes and maintains a
 * map of windows ordered by their z-index per display.
 *
 * <p>The tracker maintains a relationship between a {@link ActivityWindowAndroid} and its z-order.
 * This is necessary because focus changes from the Android framework are reported at the {@link
 * Activity} level. We map the Activity to its ActivityWindowAndroid, and then promote it to the top
 * of the z-order for the display it is on.
 *
 * <p>Assumptions:
 *
 * <ul>
 *   <li>An {@link ActivityWindowAndroid} exists on exactly one display at a time.
 *   <li>When an Activity gains focus, its associated ActivityWindowAndroid is promoted to the top
 *       of the z-order for its display.
 * </ul>
 */
@NullMarked
class WindowZOrderTracker implements ApplicationStatus.WindowFocusChangedListener {
    private static final String TAG = "WindowZOrderTracker";
    private static final boolean DEBUG_LOGGING = false;
    private static final String METRIC_NAMESPACE = "Android.MultiWindow.WindowZOrder.";
    private static final long METRIC_INTERVAL_MS = 5 * 60 * 1000;

    // Map of display ID to a list of windows in z-order (bottom to top).
    // Note: a window cannot be split between multiple displays and will exist in only one list.
    private final SparseArray<List<ActivityWindowAndroid>> mZOrder;

    private final Set<ActivityWindowAndroid> mTrackedWindows;

    private final Runnable mZOrderChangedCallback;

    private int mFocusChangedCount;

    private final Runnable mEmitMetricsRunnable =
            new Runnable() {
                @Override
                public void run() {
                    recordTrackedWindowStats();
                    recordDisplayStats();
                    recordFocusChangedCount();

                    PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, this, METRIC_INTERVAL_MS);
                }
            };

    /**
     * @param zOrderChangedCallback A callback to be invoked when the z-order changes.
     */
    public WindowZOrderTracker(Runnable zOrderChangedCallback) {
        mZOrder = new SparseArray<>();
        mTrackedWindows = new HashSet<>();
        mZOrderChangedCallback = zOrderChangedCallback;

        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, mEmitMetricsRunnable, METRIC_INTERVAL_MS);
    }

    /**
     * Starts tracking the given ActivityWindowAndroid.
     *
     * <p>If this is the first window being tracked, it registers this tracker as a window focus
     * changed listener to start monitoring focus events.
     *
     * @param windowAndroid The window to track.
     * @return {@code true} if the window was not previously tracked, {@code false} otherwise.
     */
    public boolean track(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG_LOGGING) Log.i(TAG, "Tracking window: %s", windowAndroid);

        if (mTrackedWindows.isEmpty()) {
            ApplicationStatus.registerWindowFocusChangedListener(this);
        }

        // Note: new activities will receive a focus change event and be promoted to the top of the
        // z-order shortly after initialization so there is no need to explicitly promote them here.

        return mTrackedWindows.add(windowAndroid);
    }

    /**
     * Stops tracking the given ActivityWindowAndroid and removes it from the z-order.
     *
     * <p>If this was the last window being tracked, it unregisters this tracker from window focus
     * events.
     *
     * @param windowAndroid The window to stop tracking.
     * @return {@code true} if the window was being tracked and was removed, {@code false}
     *     otherwise.
     */
    public boolean untrack(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG_LOGGING) Log.i(TAG, "Untracking window: %s", windowAndroid);
        boolean removed = mTrackedWindows.remove(windowAndroid);

        final int displayId = windowAndroid.getDisplay().getDisplayId();

        List<ActivityWindowAndroid> windows = mZOrder.get(displayId);
        if (windows == null) {
            return removed;
        }

        if (windows.remove(windowAndroid)) {
            if (windows.isEmpty()) {
                mZOrder.delete(displayId);
            }
        }

        if (removed && mTrackedWindows.isEmpty()) {
            ApplicationStatus.unregisterWindowFocusChangedListener(this);
        }

        return removed;
    }

    /**
     * Returns the current z-order of tracked windows per display.
     *
     * <p>Note: The returned SparseArray and its lists are mutable internal state. Do not modify
     * them directly.
     *
     * @return A SparseArray mapping display IDs to a list of windows ordered from bottom to top.
     */
    public SparseArray<List<ActivityWindowAndroid>> getWindowZOrder() {
        ThreadUtils.assertOnUiThread();
        return mZOrder;
    }

    @Override
    public void onWindowFocusChanged(Activity activity, boolean hasFocus) {
        ThreadUtils.assertOnUiThread();
        mFocusChangedCount++;
        if (!hasFocus) {
            return;
        }

        for (ActivityWindowAndroid window : mTrackedWindows) {
            if (window.getActivity().get() == activity) {
                promoteToTopOfZOrder(window);
                mZOrderChangedCallback.run();
                return;
            }
        }
    }

    private void promoteToTopOfZOrder(ActivityWindowAndroid window) {
        assert mTrackedWindows.contains(window);

        int currentDisplayId = window.getDisplay().getDisplayId();

        if (DEBUG_LOGGING) {
            Log.i(
                    TAG,
                    "Promoting window (%s) to top of z-order for display %d",
                    window,
                    currentDisplayId);
        }

        List<ActivityWindowAndroid> currentDisplayZOrder = mZOrder.get(currentDisplayId);
        if (currentDisplayZOrder == null) {
            currentDisplayZOrder = new ArrayList<>();
            mZOrder.put(currentDisplayId, currentDisplayZOrder);
        }

        // Remove the window from its current position and add it to the top.
        // Note that when changing displays windows are destroyed and recreated so there is no need
        // to handle the case where the window is on a different display.
        currentDisplayZOrder.remove(window);
        currentDisplayZOrder.add(window);

        if (DEBUG_LOGGING) {
            logZOrder();
        }
    }

    private void logZOrder() {
        for (int i = 0; i < mZOrder.size(); i++) {
            Log.i(TAG, "Z-order for display %d: ", mZOrder.keyAt(i));
            List<ActivityWindowAndroid> displayZOrder = mZOrder.valueAt(i);
            for (int j = displayZOrder.size() - 1; j >= 0; j--) {
                Log.i(TAG, "%s", displayZOrder.get(j));
            }
        }
    }

    private void recordTrackedWindowStats() {
        RecordHistogram.recordCount100Histogram(
                getMetricName("TrackedWindowsCount"), mTrackedWindows.size());
    }

    private void recordDisplayStats() {
        RecordHistogram.recordCount100Histogram(getMetricName("DisplaysCount"), mZOrder.size());
    }

    private void recordFocusChangedCount() {
        RecordHistogram.recordCount100Histogram(
                getMetricName("FocusChangedCount"), mFocusChangedCount);
        mFocusChangedCount = 0;
    }

    private static String getMetricName(String metricName) {
        return METRIC_NAMESPACE + metricName;
    }
}
