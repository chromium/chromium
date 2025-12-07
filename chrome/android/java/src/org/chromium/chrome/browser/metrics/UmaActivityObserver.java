// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
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
    private @Nullable TabModelSelector mLatestTabModelSelector;
    private @Nullable AndroidPermissionDelegate mLatestAndroidPermissionDelegate;

    /** Activities that implement this interface manage their own UMA Session starting/ending. */
    public interface UmaSessionAwareActivity {}

    private static ApplicationStatus.@Nullable ActivityStateListener sAppActivityListener;

    static {
        doStaticInit();
    }

    private static void doStaticInit() {
        // Handles the case where we open a non-UMA aware activity like Bookmarks over CTA, and then
        // the user hides the Bookmarks Activity (which should end the session).
        sAppActivityListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (activity instanceof UmaSessionAwareActivity) return;
                        if (newState != ActivityState.STOPPED
                                && newState != ActivityState.DESTROYED) {
                            return;
                        }
                        if (sActiveObserver == null) return;
                        if (ApplicationStatus.getStateForApplication()
                                == ApplicationState.HAS_RUNNING_ACTIVITIES) {
                            return;
                        }
                        sActiveObserver.endUmaSessionInternal(false, true);
                    }
                };
        ApplicationStatus.registerStateListenerForAllActivities(sAppActivityListener);
    }

    public UmaActivityObserver(
            Context context,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            @ActivityType int activityType) {
        assert context instanceof UmaSessionAwareActivity;
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
            @Nullable TabModelSelector tabModelSelector,
            @Nullable AndroidPermissionDelegate permissionDelegate) {
        mLatestTabModelSelector = tabModelSelector;
        mLatestAndroidPermissionDelegate = permissionDelegate;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES)) {
            sVisibleObservers.add(this);

            if (sActiveObserver != null) {
                if (sActiveObserver.mActivityType == mActivityType) {
                    sActiveObserver = this;
                    return;
                }
                sActiveObserver.endUmaSessionInternal(false, false);
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
     * Should be called whenever an Activity is paused, in case the Activity is killed before the
     * Activity is stopped and the session is ended.
     */
    public void flushUmaSession() {
        mUmaSessionStats.flushSession();
    }

    /**
     * Call when a android activity has become hidden, with native code loaded.
     *
     * <p>The activity is expected to have previously started with nativve code loaded.
     */
    public void endUmaSession() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES)) {
            for (Activity activity : ApplicationStatus.getRunningActivities()) {
                if (activity instanceof UmaSessionAwareActivity) continue;
                if (ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED) {
                    // Don't end the session if an Activity like Settings/Bookmarks is still
                    // visible.
                    return;
                }
            }
            endUmaSessionInternal(true, true);
        } else {
            if (sActiveObserver == null) {
                return;
            }
            sActiveObserver = null;

            // Record session metrics.
            mUmaSessionStats.logAndEndSession();
        }
    }

    private void endUmaSessionInternal(boolean startNextVisibleSession, boolean removeObserver) {
        if (removeObserver) {
            sVisibleObservers.remove(this);
        }
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
            endUmaSessionInternal(true, true);
        }
    }
}
