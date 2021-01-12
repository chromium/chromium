// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Computes and records metrics for what caused Chrome to be launched.
 */
public abstract class LaunchCauseMetrics {
    // Static to avoid recording launch metrics when transitioning between Activities without
    // Chrome leaving the foreground.
    private static boolean sRecordedLaunchCause;

    @VisibleForTesting
    public static final String LAUNCH_CAUSE_HISTOGRAM = "MobileStartup.Experimental.LaunchCause";

    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({LaunchCause.OTHER, LaunchCause.CUSTOM_TAB, LaunchCause.TWA})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchCause {
        int OTHER = 0;
        int CUSTOM_TAB = 1;
        int TWA = 2;

        int NUM_ENTRIES = 3;
    }

    public LaunchCauseMetrics() {
        ApplicationStatus.registerApplicationStateListener(newState -> {
            if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
                reset();
            }
        });
    }

    /**
     * Resets state used to compute launch cause when Chrome is backgrounded.
     */
    @CallSuper
    protected void reset() {
        sRecordedLaunchCause = false;
    }

    /**
     * Computes and returns what the cause of the Chrome launch was.
     */
    protected abstract @LaunchCause int computeLaunchCause();

    /**
     * Called after Chrome has launched and all information necessary to compute why Chrome was
     * launched is available.
     *
     * Records UMA metrics for what caused Chrome to launch.
     */
    public void recordLaunchCause() {
        if (sRecordedLaunchCause) return;
        sRecordedLaunchCause = true;

        @LaunchCause
        int cause = computeLaunchCause();
        RecordHistogram.recordEnumeratedHistogram(
                LAUNCH_CAUSE_HISTOGRAM, cause, LaunchCause.NUM_ENTRIES);
    }

    @VisibleForTesting
    public static void resetForTests() {
        ThreadUtils.assertOnUiThread();
        sRecordedLaunchCause = false;
    }
}
