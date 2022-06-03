// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.LaunchSourceType;

/**
 * Logs to UMA the amount of time user spends in a CCT for CCTs launched from webapps.
 *
 * TODO(https://crbug.com/883402): Rename this to CustomTabTimeSpentLogger and refactor into the
 * customtabs package.
 */
public class WebappCustomTabTimeSpentLogger {
    private long mStartTime;
    private @LaunchSourceType int mActivityType;

    private WebappCustomTabTimeSpentLogger(@LaunchSourceType int activityType) {
        mActivityType = activityType;
        mStartTime = SystemClock.elapsedRealtime();
    }

    /**
     * Create {@link WebappCustomTabTimeSpentLogger} instance and starts timer.
     * @param type of the activity that opens the CCT.
     * @return {@link WebappCustomTabTimeSpentLogger} instance.
     */
    public static WebappCustomTabTimeSpentLogger createInstanceAndStartTimer(
            @LaunchSourceType int activityType) {
        return new WebappCustomTabTimeSpentLogger(activityType);
    }

    /**
     * Stop timer and log UMA.
     */
    public void onPause() {
        long timeSpent = SystemClock.elapsedRealtime() - mStartTime;
        String umaSuffix;
        // TODO(peconn): Combine this with TrustedWebActivityOpenTimeRecorder.
        switch (mActivityType) {
            case LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY:
                umaSuffix = ".MediaLauncherActivity";
                break;
            default:
                umaSuffix = ".Other";
                break;
        }
        RecordHistogram.recordLongTimesHistogram(
                "CustomTab.SessionDuration" + umaSuffix, timeSpent);
    }
}
