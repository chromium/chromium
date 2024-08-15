// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSessionState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

/** Helper class to transition between uma sessions as the android activity type changes. */
public class UmaActivityObserver {

    private final UmaSessionStats mUmaSessionStats;
    private boolean mIsSessionActive;
    private static @ActivityType int sCurrentActivityType = ActivityType.PRE_FIRST_TAB;

    public UmaActivityObserver(Context context) {
        mUmaSessionStats = new UmaSessionStats(context);
    }

    /**
     * Call when an android activity has resumed, with native code loaded.
     *
     * <p>This function can safely be called multiple times in a given activity's start/resume
     * sequence because it checks whether it is already tracking the current activity.
     *
     * @param activityType The type of the Activity.
     * @param tabModelSelector A TabModelSelector instance for recording tab counts on page loads.
     *     If null, UmaActivityObserver does not record page loads and tab counts.
     * @param permissionDelegate The AndroidPermissionDelegate used for querying permission status.
     *     If null, UmaActivityObserver will not record permission status.
     */
    public void startUmaSession(
            @ActivityType int activityType,
            TabModelSelector tabModelSelector,
            AndroidPermissionDelegate permissionDelegate) {
        if (mIsSessionActive) {
            if (activityType == sCurrentActivityType) {
                return;
            }
            endUmaSession();
        }
        mIsSessionActive = true;

        // Stage the activity type value such that it can be picked up when the new
        // UMA record is opened as a part of the subsequent session resume.
        ChromeSessionState.setActivityType(activityType);
        sCurrentActivityType = activityType;

        UmaSessionStats.updateMetricsServiceState();
        mUmaSessionStats.startNewSession(activityType, tabModelSelector, permissionDelegate);
    }

    /**
     * Call when a android activity has paused, with native code loaded.
     *
     * <p>The activity is expected to have previously started with nativve code loaded.
     */
    public void endUmaSession() {
        if (!mIsSessionActive) {
            return;
        }
        mIsSessionActive = false;

        // Record session metrics.
        mUmaSessionStats.logAndEndSession();
    }

    /** Returns the current activity type being used for UMA logging. */
    public static @ActivityType int getCurrentActivityType() {
        return sCurrentActivityType;
    }
}
