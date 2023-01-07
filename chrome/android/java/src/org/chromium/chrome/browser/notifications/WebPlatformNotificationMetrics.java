// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

/**
 * This class collects metrics about what happens when the user interacts with web platform
 * notifications, for example how long it takes to show an Activity after a notification is clicked,
 * or if an Activity is shown at all.
 *
 * Unfortunately, it would be very difficult to follow the code execution from the point a
 * notification is clicked all the way to when an Activity is launched, so we use static state and
 * call methods on this class from seemingly disparate places. A consequence of this is that we
 * don't know for sure whether an Activity being focused is the result of a notification, so we use
 * the heuristic that if an Activity focus occurs within 5 seconds after a notification click, that
 * counts.
 */
public class WebPlatformNotificationMetrics {
    private static final String METRIC_PREFIX = "Notifications.WebPlatformV2.";
    private static final int ATTRIBUTION_THRESHOLD_MS = 10_000;
    private static final long INVALID_TIME = -1;
    private static WebPlatformNotificationMetrics sInstance;

    private final Clock mClock;
    private final Recorder mRecorder;

    // These variables carry data about the notification click.
    private long mNotificationClickTimeMs = INVALID_TIME;
    private boolean mActionButtonClicked;
    private boolean mNewTabLaunched;
    // These variables prevent a single notification click from triggering multiple recordings.
    private boolean mNotificationClosed;
    private boolean mTabFocused;

    /** Interface to provide the time. */
    interface Clock {
        /** Gets the current time. */
        long getTime();
    }

    /** Delegate to record metrics. */
    interface Recorder {
        /** Records an action. */
        void recordAction(String action);

        /** Records a duration. */
        void recordDuration(String name, long durationMs);
    }

    /**
     * Gets an instance of this class suitable for prod - it uses the system time and records to
     * metrics.
     */
    public static WebPlatformNotificationMetrics getInstance() {
        if (sInstance == null) {
            sInstance = new WebPlatformNotificationMetrics(
                    SystemClock::elapsedRealtime, new Recorder() {
                        @Override
                        public void recordAction(String action) {
                            RecordUserAction.record(action);
                        }

                        @Override
                        public void recordDuration(String name, long durationMs) {
                            RecordHistogram.recordTimesHistogram(name, durationMs);
                        }
                    });
        }

        return sInstance;
    }

    /** Creates an instance of this class allowing for dependency injection for testing. */
    WebPlatformNotificationMetrics(Clock clock, Recorder recorder) {
        mClock = clock;
        mRecorder = recorder;
    }

    /** To be called when a notification is clicked. */
    public void onNotificationClicked(boolean actionButton) {
        mNotificationClickTimeMs = mClock.getTime();
        mActionButtonClicked = actionButton;
        mNewTabLaunched = false;
        mNotificationClosed = false;
        mTabFocused = false;

        recordAction("Click");
    }

    /** To be called when a new tab is created. */
    public void onNewTabLaunched() {
        // The tab is going to be focused after being created, so the actual logging is done in
        // onTabFocused.
        mNewTabLaunched = true;
    }

    /** To be called when a tab is focused. */
    public void onTabFocused() {
        if (shouldIgnore()) return;

        // We don't want to record twice for the same notification.
        if (mTabFocused) return;
        mTabFocused = true;

        recordAction(mNewTabLaunched ? "NewActivity" : "FocusActivity");
        recordTime("TimeToActivity");
    }

    /** To be called when a notification is closed. */
    public void onNotificationClosed() {
        if (shouldIgnore()) return;

        // We don't want to record twice for the same notification.
        if (mNotificationClosed) return;
        mNotificationClosed = true;

        recordAction("Close");
        recordTime("TimeToClose");
    }

    private void recordAction(String action) {
        String target = mActionButtonClicked ? "ActionButton." : "Body.";
        mRecorder.recordAction(METRIC_PREFIX + target + action);
    }

    private void recordTime(String action) {
        String target = mActionButtonClicked ? "ActionButton." : "Body.";
        long now = mClock.getTime();

        mRecorder.recordDuration(METRIC_PREFIX + target + action, now - mNotificationClickTimeMs);
    }

    private boolean shouldIgnore() {
        if (mNotificationClickTimeMs == INVALID_TIME) return true;

        long now = mClock.getTime();
        return (now - mNotificationClickTimeMs) > ATTRIBUTION_THRESHOLD_MS;
    }
}
