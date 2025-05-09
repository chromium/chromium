// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.base.BaseFeatures;
import org.chromium.base.TimeUtils;
import org.chromium.base.jank_tracker.FrameMetricsListener;
import org.chromium.base.jank_tracker.FrameMetricsStore;
import org.chromium.base.jank_tracker.JankReportingScheduler;
import org.chromium.base.jank_tracker.JankScenario;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.jank_tracker.JankTrackerImpl;
import org.chromium.base.jank_tracker.JankTrackerStateController;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

/**
 * A Webview class that implements the listener part of the JankTracker requirement. It mirrors
 * JankActivityTracker in starting and stopping the listener and collection.
 */
class AwFrameMetricsListener {

    private static final WeakHashMap<Window, AwFrameMetricsListener> sWindowMap =
            new WeakHashMap<>();

    private boolean mAttached;
    private final JankTrackerStateController mController;
    private final JankTracker mJankTracker;
    private WeakReference<Window> mWindow;
    private int mAttachedWebviews;
    private int mVisibleWebviews;

    private static final WeakHashMap<Window, Integer> sNumActiveScrolls = new WeakHashMap<>();

    private AwFrameMetricsListener() {
        FrameMetricsStore metricsStore = new FrameMetricsStore();
        mController =
                new JankTrackerStateController(
                        new FrameMetricsListener(metricsStore),
                        new JankReportingScheduler(metricsStore));
        mJankTracker = new JankTrackerImpl(mController);
        mAttached = false;
    }

    private void attachListener(Window window) {
        if (mAttached) {
            return;
        }
        mWindow = new WeakReference<Window>(window);
        mController.startMetricCollection(window);
        mAttached = true;
    }

    private void detachListener(Window window) {
        if (!mAttached || window != mWindow.get()) {
            return;
        }
        mController.stopMetricCollection(window);
        mAttached = false;
    }

    private void incrementAttachedWebViews() {
        mAttachedWebviews++;
    }

    private void decrementAttachedWebViews() {
        mAttachedWebviews--;
        assert mAttachedWebviews >= 0;
    }

    private int getAttachedWebViews() {
        return mAttachedWebviews;
    }

    @Nullable
    static AwFrameMetricsListener maybeCreate(
            ViewGroup containerView, WindowAndroid mWindowAndroid) {
        if (AwFeatureMap.isEnabled(BaseFeatures.COLLECT_ANDROID_FRAME_TIMELINE_METRICS)) {
            Window window = mWindowAndroid.getWindow();
            if (window != null && containerView.isHardwareAccelerated()) {
                return onAttachedToWindow(window);
            }
        }
        return null;
    }

    @Nullable
    static AwFrameMetricsListener maybeClear(
            AwFrameMetricsListener awFrameMetricsListener,
            ViewGroup containerView,
            WindowAndroid windowAndroid) {
        if (awFrameMetricsListener != null) {
            Window window = windowAndroid.getWindow();
            if (window != null && containerView.isHardwareAccelerated()) {
                onDetachedFromWindow(window);
                return null;
            }
        }

        return awFrameMetricsListener;
    }

    private static AwFrameMetricsListener onAttachedToWindow(Window window) {
        AwFrameMetricsListener listener = sWindowMap.get(window);
        if (listener == null) {
            listener = new AwFrameMetricsListener();
            listener.attachListener(window);
            sWindowMap.put(window, listener);
        }
        listener.incrementAttachedWebViews();
        return listener;
    }

    private static void onDetachedFromWindow(Window window) {
        AwFrameMetricsListener listener = sWindowMap.get(window);
        listener.decrementAttachedWebViews();
        if (listener.getAttachedWebViews() >= 1) {
            return;
        }
        listener.detachListener(window);
        sWindowMap.remove(window);
    }

    public void onWebViewVisible() {
        if (!mAttached) {
            return;
        }
        mVisibleWebviews++;
        if (mVisibleWebviews > 1) {
            return;
        }
        mController.startPeriodicReporting();
        mController.startMetricCollection(null);
    }

    public void onWebViewHidden() {
        if (!mAttached) {
            return;
        }
        mVisibleWebviews--;
        assert mVisibleWebviews >= 0;
        if (mVisibleWebviews == 0) {
            mController.stopMetricCollection(null);
            mController.stopPeriodicReporting();
        }
    }

    public void onWebContentsScrollStateUpdate(boolean isScrolling, long scrollId) {
        if (!mAttached) {
            return;
        }
        // scrollIds are unique across multiple webviews in a window.
        Window window = mWindow.get();
        if (window == null) {
            return;
        }
        int numActiveScrolls = sNumActiveScrolls.getOrDefault(window, 0);
        if (isScrolling) {
            numActiveScrolls += 1;
            mJankTracker.startTrackingScenario(
                    new JankScenario(JankScenario.Type.WEBVIEW_SCROLLING, scrollId));
        } else {
            assert numActiveScrolls >= 1;
            numActiveScrolls -= 1;
            mJankTracker.finishTrackingScenario(
                    new JankScenario(JankScenario.Type.WEBVIEW_SCROLLING, scrollId),
                    TimeUtils.uptimeMillis() * TimeUtils.NANOSECONDS_PER_MILLISECOND);
        }

        if (numActiveScrolls == 0) {
            mJankTracker.finishTrackingScenario(
                    JankScenario.COMBINED_WEBVIEW_SCROLLING,
                    TimeUtils.uptimeMillis() * TimeUtils.NANOSECONDS_PER_MILLISECOND);
            sNumActiveScrolls.remove(window);
            return;
        }
        if (numActiveScrolls == 1 && isScrolling) {
            mJankTracker.startTrackingScenario(JankScenario.COMBINED_WEBVIEW_SCROLLING);
        }
        sNumActiveScrolls.put(window, numActiveScrolls);
    }
}
