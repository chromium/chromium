// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSessionState;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.util.HashSet;
import java.util.Set;

/** Helper class to transition between uma sessions as the android activity type changes. */
@NullMarked
public class UmaActivityObserver implements DestroyObserver {
    private static @Nullable UmaActivityObserver sActiveObserver;
    private static final Set<UmaActivityObserver> sVisibleObservers = new HashSet<>();

    private final UmaSessionStats mUmaSessionStats;
    private final @ActivityType int mActivityType;
    private TabModelSelector mLatestTabModelSelector;
    private AndroidPermissionDelegate mLatestAndroidPermissionDelegate;

    public UmaActivityObserver(
            Context context,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            @ActivityType int activityType) {
        mUmaSessionStats = new UmaSessionStats(context);
        lifecycleDispatcher.register(this);
        mActivityType = activityType;
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
    @Initializer
    public void startUmaSession(
            TabModelSelector tabModelSelector, AndroidPermissionDelegate permissionDelegate) {
        mLatestTabModelSelector = tabModelSelector;
        mLatestAndroidPermissionDelegate = permissionDelegate;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES)) {
            sVisibleObservers.add(this);

            if (sActiveObserver != null) {
                if (sActiveObserver.mActivityType == mActivityType) {
                    sActiveObserver = this;
                    return;
                }
                sActiveObserver.endUmaSessionInternal(false);
            }
            sActiveObserver = this;

            // Stage the activity type value such that it can be picked up when the new
            // UMA record is opened as a part of the subsequent session resume.
            ChromeSessionState.setActivityType(mActivityType);

            UmaSessionStats.updateMetricsServiceState();
            mUmaSessionStats.startNewSession(
                    mActivityType, mLatestTabModelSelector, mLatestAndroidPermissionDelegate);
        } else {
            if (sActiveObserver != null) {
                if (mActivityType == sActiveObserver.mActivityType) {
                    return;
                }
                endUmaSession();
            }
            // Stage the activity type value such that it can be picked up when the new
            // UMA record is opened as a part of the subsequent session resume.
            ChromeSessionState.setActivityType(mActivityType);
            sActiveObserver = this;

            UmaSessionStats.updateMetricsServiceState();
            mUmaSessionStats.startNewSession(mActivityType, tabModelSelector, permissionDelegate);
        }
    }

    /**
     * Call when a android activity has become hidden, with native code loaded.
     *
     * <p>The activity is expected to have previously started with nativve code loaded.
     */
    public void endUmaSession() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES)) {
            sVisibleObservers.remove(this);
            endUmaSessionInternal(true);
        } else {
            if (sActiveObserver == null) {
                return;
            }
            sActiveObserver = null;

            // Record session metrics.
            mUmaSessionStats.logAndEndSession();
        }
    }

    private void endUmaSessionInternal(boolean startNextVisibleSession) {
        if (sActiveObserver != this) return;
        // Record session metrics.
        mUmaSessionStats.logAndEndSession();
        sActiveObserver = null;
        if (!sVisibleObservers.isEmpty() && startNextVisibleSession) {
            // Switch active session to an arbitrary visible window if this session ends.
            UmaActivityObserver observer = sVisibleObservers.iterator().next();
            observer.startUmaSession(
                    observer.mLatestTabModelSelector, observer.mLatestAndroidPermissionDelegate);
        }
    }

    /** Returns the current activity type being used for UMA logging. */
    public static @ActivityType int getCurrentActivityType() {
        if (sActiveObserver == null) return ActivityType.PRE_FIRST_TAB;
        return sActiveObserver.mActivityType;
    }

    @Override
    public void onDestroy() {
        if (sActiveObserver == null) return; // Ensures native library has been initialized.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES)) {
            endUmaSession();
        }
    }
}
