// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;

import androidx.annotation.IntDef;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Tracks metrics caused by a particular Activity stopping.
 */
public class ActivityStopMetrics {
    // NUM_ENTRIES is intentionally included into @IntDef.
    @IntDef({StopReason.UNKNOWN, StopReason.BACK_BUTTON,
            StopReason.OTHER_CHROME_ACTIVITY_IN_FOREGROUND, StopReason.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StopReason {
        /** Activity stopped for unknown reasons. */
        int UNKNOWN = 0;
        /** Activity stopped after the user hit the back button. */
        int BACK_BUTTON = 1;
        // Obsolete -- Activity stopped after the user hit the close/return UI button.
        // int RETURN_BUTTON = 2;
        // Obsolete --  Activity stopped because it launched a {@link CustomTabActivity} on top of
        //              itself.
        // int CUSTOM_TAB_STARTED = 3;
        // Obsolete -- Activity stopped because its child {@link CustomTabActivity} stopped itself.
        // int CUSTOM_TAB_STOPPED = 4;
        /** Activity stopped because another of Chrome Activities came into focus. */
        int OTHER_CHROME_ACTIVITY_IN_FOREGROUND = 5;

        /** Boundary. Shouldn't ever be passed to the metrics service. */
        int NUM_ENTRIES = 6;
    }

    /** Name of the histogram that will be recorded. */
    private static final String HISTOGRAM_NAME = "Android.Activity.ChromeTabbedActivity.StopReason";

    /** Why the Activity is being stopped. */
    @StopReason private int mStopReason;

    /**
     * Constructs an {@link ActivityStopMetrics} instance.
     */
    public ActivityStopMetrics() {
        mStopReason = StopReason.NUM_ENTRIES;
    }

    /**
     * Records the reason that the parent Activity was stopped.
     * @param parent Activity that owns this {@link ActivityStopMetrics} instance.
     */
    public void onStopWithNative(Activity parent) {
        if (mStopReason == StopReason.NUM_ENTRIES) {
            if (parent != ApplicationStatus.getLastTrackedFocusedActivity()
                    && ApplicationStatus.hasVisibleActivities()) {
                mStopReason = StopReason.OTHER_CHROME_ACTIVITY_IN_FOREGROUND;
            } else {
                mStopReason = StopReason.UNKNOWN;
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_NAME, mStopReason, StopReason.NUM_ENTRIES);
        mStopReason = StopReason.NUM_ENTRIES;
    }

    /**
     * Tracks the reason that the parent Activity was stopped.
     * @param reason Reason the Activity was stopped (see {@link StopReason}).
     */
    public void setStopReason(@StopReason int reason) {
        if (mStopReason != StopReason.NUM_ENTRIES) return;
        mStopReason = reason;
    }
}
