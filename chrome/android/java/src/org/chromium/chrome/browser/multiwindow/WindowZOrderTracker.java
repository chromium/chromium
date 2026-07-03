// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.util.SparseArray;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tracks the z-order of Chrome windows. This class listens to window focus and activity state
 * changes, maintaining a map of windows ordered by their z-index per display.
 *
 * <p>The tracker maintains a relationship between an {@link ActivityWindowAndroid} and its z-order.
 * This is necessary because focus and state changes from the Android framework are reported at the
 * {@link Activity} level. We map the Activity to its ActivityWindowAndroid, and then promote it to
 * the top of the z-order for the display it is on.
 *
 * <p>Assumptions:
 *
 * <ul>
 *   <li>An {@link ActivityWindowAndroid} exists on exactly one display at a time.
 *   <li>When onActivityTopResumedChanged is called for an Activity, its associated
 *       ActivityWindowAndroid is promoted to the top of the z-order for its display.
 * </ul>
 */
@NullMarked
class WindowZOrderTracker {
    private static final String TAG = "WindowZOrderTracker";
    private static final boolean DEBUG_LOGGING = false;
    private static final String METRIC_NAMESPACE = "Android.MultiWindow.WindowZOrder.";
    private static final long METRIC_INTERVAL_MS = 5 * 60 * 1000;

    // Map of display ID to a list of all known windows in z-order (bottom to top).
    // Note: a window cannot be split between multiple displays and will exist in only one list.
    private final SparseArray<List<ActivityWindowAndroid>> mZOrder;

    private final Map<ActivityWindowAndroid, WindowAndroid.ActivityStateObserver> mObservers;

    private final Runnable mZOrderChangedCallback;

    // Counts activity top resumed state transitions handled per metric interval.
    private int mResumedCount;
    // Counts actual deduplicated Z-order promotions performed per metric interval.
    private int mPromotionCount;

    private final Runnable mEmitMetricsRunnable =
            new Runnable() {
                @Override
                public void run() {
                    recordTrackedWindowStats();
                    recordDisplayStats();
                    recordResumedCount();
                    recordPromotionCount();

                    PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, this, METRIC_INTERVAL_MS);
                }
            };

    /**
     * @param zOrderChangedCallback A callback to be invoked when the z-order changes.
     */
    public WindowZOrderTracker(Runnable zOrderChangedCallback) {
        mZOrder = new SparseArray<>();
        mObservers = new HashMap<>();
        mZOrderChangedCallback = zOrderChangedCallback;

        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, mEmitMetricsRunnable, METRIC_INTERVAL_MS);
    }

    /**
     * Starts tracking the given ActivityWindowAndroid.
     *
     * <p>If this is the first window being tracked, it registers this tracker as a window focus
     * changed listener.
     *
     * @param windowAndroid The window to track.
     */
    public void track(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG_LOGGING) Log.i(TAG, "Tracking window: %s", windowAndroid);

        int displayId = windowAndroid.getDisplay().getDisplayId();
        List<ActivityWindowAndroid> windows = mZOrder.get(displayId);
        if (windows == null) {
            windows = new ArrayList<>();
            mZOrder.put(displayId, windows);
        }
        if (!windows.contains(windowAndroid)) {
            windows.add(0, windowAndroid);
            WindowAndroid.ActivityStateObserver observer =
                    new WindowAndroid.ActivityStateObserver() {
                        @Override
                        public void onActivityTopResumedChanged(boolean isTopResumedActivity) {
                            if (DEBUG_LOGGING) {
                                Log.i(
                                        TAG,
                                        "onTopResumedActivityChanged: window=%s, isTop=%b",
                                        windowAndroid,
                                        isTopResumedActivity);
                            }
                            if (isTopResumedActivity) {
                                mResumedCount++;
                                if (promoteToTopOfZOrder(windowAndroid)) {
                                    mZOrderChangedCallback.run();
                                }
                            }
                        }
                    };
            assert !mObservers.containsKey(windowAndroid);
            mObservers.put(windowAndroid, observer);
            windowAndroid.addActivityStateObserver(observer);
            if (windowAndroid.isTopResumedActivity()) {
                if (promoteToTopOfZOrder(windowAndroid)) {
                    mZOrderChangedCallback.run();
                }
            }
        }
    }

    /**
     * Stops tracking the given ActivityWindowAndroid and removes it from the z-order.
     *
     * <p>If this was the last window being tracked, it unregisters this tracker from window focus
     * events.
     *
     * @param windowAndroid The window to stop tracking.
     */
    public void untrack(ActivityWindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG_LOGGING) Log.i(TAG, "Untracking window: %s", windowAndroid);

        int displayId = windowAndroid.getDisplay().getDisplayId();
        List<ActivityWindowAndroid> windows = mZOrder.get(displayId);
        if (windows != null) {
            windows.remove(windowAndroid);
            WindowAndroid.ActivityStateObserver observer = mObservers.remove(windowAndroid);
            if (observer != null) {
                windowAndroid.removeActivityStateObserver(observer);
            }
            if (windows.isEmpty()) {
                mZOrder.remove(displayId);
            }
        }
    }

    /**
     * Returns the current z-order of windows per display.
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

    private boolean promoteToTopOfZOrder(ActivityWindowAndroid window) {
        int currentDisplayId = window.getDisplay().getDisplayId();
        List<ActivityWindowAndroid> currentDisplayZOrder = mZOrder.get(currentDisplayId);
        if (currentDisplayZOrder == null) {
            currentDisplayZOrder = new ArrayList<>();
            mZOrder.put(currentDisplayId, currentDisplayZOrder);
        } else if (isWindowAtTop(window, currentDisplayZOrder)) {
            return false;
        }

        if (DEBUG_LOGGING) {
            Log.i(
                    TAG,
                    "Promoting window (%s) to top of z-order for display %d",
                    window,
                    currentDisplayId);
        }

        // Remove the window from its current position and add it to the top.
        // Note that when changing displays windows are destroyed and recreated so there is no need
        // to handle the case where the window is on a different display.
        currentDisplayZOrder.remove(window);
        currentDisplayZOrder.add(window);
        mPromotionCount++;

        if (DEBUG_LOGGING) {
            logZOrder();
        }
        return true;
    }

    private static boolean isWindowAtTop(
            ActivityWindowAndroid window, List<ActivityWindowAndroid> currentDisplayZOrder) {
        return !currentDisplayZOrder.isEmpty()
                && currentDisplayZOrder.get(currentDisplayZOrder.size() - 1) == window;
    }

    private void logZOrder() {
        for (int i = 0; i < mZOrder.size(); i++) {
            Log.i(TAG, "Z-order for display %d: ", mZOrder.keyAt(i));
            List<ActivityWindowAndroid> displayZOrder = mZOrder.valueAt(i);
            for (int j = displayZOrder.size() - 1; j >= 0; j--) {
                Log.i(
                        TAG,
                        "%s (state: %d)",
                        displayZOrder.get(j),
                        displayZOrder.get(j).getActivityState());
            }
        }
    }

    private void recordTrackedWindowStats() {
        int trackedCount = 0;
        for (int i = 0; i < mZOrder.size(); i++) {
            trackedCount += mZOrder.valueAt(i).size();
        }
        RecordHistogram.recordCount100Histogram(getMetricName("TrackedWindowsCount"), trackedCount);
    }

    private void recordDisplayStats() {
        RecordHistogram.recordCount100Histogram(getMetricName("DisplaysCount"), mZOrder.size());
    }

    private void recordResumedCount() {
        RecordHistogram.recordCount100Histogram(getMetricName("ResumedCount"), mResumedCount);
        mResumedCount = 0;
    }

    private void recordPromotionCount() {
        RecordHistogram.recordCount100Histogram(getMetricName("PromotionCount"), mPromotionCount);
        mPromotionCount = 0;
    }

    private static String getMetricName(String metricName) {
        return METRIC_NAMESPACE + metricName;
    }
}
