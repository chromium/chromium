// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseFeatureList;
import org.chromium.base.BaseFeatures;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;

/**
 * This class is similar in principle to MemoryPurgeManager in blink, but on the browser process
 * side. It triggers a critical memory pressure notification once the application has been in the
 * background for more than a few minutes.
 *
 * UI thread only.
 */
public class MemoryPurgeManager implements ApplicationStatus.ApplicationStateListener {
    private boolean mStarted;
    private long mLastBackgroundPeriodStart = NEVER;
    private boolean mDelayedPurgeTaskPending;
    private boolean mHasBeenInForeground;

    // Arbitrary delay, a few minutes is what is used for background renderer purge, and 5 minutes
    // for freezing.
    // TODO(crbug.com/1356242): Should ideally be tuned according to the distribution of background
    // time residency.
    @VisibleForTesting
    static final long PURGE_DELAY_MS = 4 * 60 * 1000;
    private static final long NEVER = -1;
    @VisibleForTesting
    static final String BACKGROUND_DURATION_HISTOGRAM_NAME =
            "Android.ApplicationState.TimeInBackgroundBeforeForegroundedAgain";

    private static final MemoryPurgeManager sInstance = new MemoryPurgeManager();

    @VisibleForTesting
    MemoryPurgeManager() {}

    public static MemoryPurgeManager getInstance() {
        return sInstance;
    }

    /**
     * Start the background memory purge, if enabled. May be called several times.
     *
     * This attempts to trigger a critical memory pressure notification after 4 continuous minutes
     * in background.
     */
    public void start() {
        ThreadUtils.assertOnUiThread();
        if (mStarted) return;
        mStarted = true;
        if (!BaseFeatureList.isEnabled(BaseFeatures.BROWSER_PROCESS_MEMORY_PURGE)) return;

        ApplicationStatus.registerApplicationStateListener(this);

        // We may already be in background, capture the initial state.
        onApplicationStateChange(getApplicationState());
    }

    @Override
    public void onApplicationStateChange(int state) {
        switch (state) {
            case ApplicationState.UNKNOWN:
            case ApplicationState.HAS_RUNNING_ACTIVITIES:
            case ApplicationState.HAS_PAUSED_ACTIVITIES:
                if (mLastBackgroundPeriodStart != NEVER && mHasBeenInForeground) {
                    long durationInBackgroundMs =
                            TimeUtils.elapsedRealtimeMillis() - mLastBackgroundPeriodStart;
                    RecordHistogram.recordLongTimesHistogram(
                            BACKGROUND_DURATION_HISTOGRAM_NAME, durationInBackgroundMs);
                }
                mHasBeenInForeground = true;
                mLastBackgroundPeriodStart = NEVER;
                break;
            case ApplicationState.HAS_STOPPED_ACTIVITIES:
                if (mLastBackgroundPeriodStart == NEVER) {
                    mLastBackgroundPeriodStart = TimeUtils.elapsedRealtimeMillis();
                    maybePostDelayedPurgingTask(PURGE_DELAY_MS);
                }
                break;
            case ApplicationState.HAS_DESTROYED_ACTIVITIES:
                // Ignored on purpose: the initial state of a process which never had any activity
                // is HAS_DESTROYED_ACTIVITIES, and we don't want to trigger in this case.
                break;
        }
    }

    private void delayedPurge() {
        // Came back to foreground in the meantime, do not repost a task, this will happen next time
        // we go to background.
        if (mLastBackgroundPeriodStart == NEVER) return;

        assert mLastBackgroundPeriodStart < TimeUtils.elapsedRealtimeMillis();
        long inBackgroundFor = TimeUtils.elapsedRealtimeMillis() - mLastBackgroundPeriodStart;
        if (inBackgroundFor < PURGE_DELAY_MS) {
            maybePostDelayedPurgingTask(PURGE_DELAY_MS - inBackgroundFor);
            return;
        }

        notifyMemoryPressure();
    }

    protected void notifyMemoryPressure() {
        MemoryPressureListener.notifyMemoryPressure(MemoryPressureLevel.CRITICAL);
    }

    protected int getApplicationState() {
        return ApplicationStatus.getStateForApplication();
    }

    private void maybePostDelayedPurgingTask(long delayMillis) {
        ThreadUtils.assertOnUiThread();
        if (mDelayedPurgeTaskPending) return;

        ThreadUtils.postOnUiThreadDelayed(() -> {
            mDelayedPurgeTaskPending = false;
            delayedPurge();
        }, delayMillis);
        mDelayedPurgeTaskPending = true;
    }
}
