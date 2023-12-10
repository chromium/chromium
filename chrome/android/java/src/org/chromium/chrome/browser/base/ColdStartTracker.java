// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Activity;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Process;
import android.os.SystemClock;

import androidx.annotation.RequiresApi;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.base.SplitCompatAppComponentFactory.ProcessCreationReason;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;

/**
 * Utility class to guess whether the first Activity was created during application cold start.
 *
 * <p>
 * Cold startup is most important in cases when an activity is created and needs to become
 * interactive as fast as possible, while the browser is still initializing. Android OS can
 * create the app process in many ways without an Activity and keep it "cached" for a long time
 * before the first Activity gets created. This quite common course of events runs faster and is
 * therefore considered to be a "warm" start.
 *
 * <p>
 * For this class, the "cold start" is a time interval closely related to the lifecycle of the first
 * Activity created in the app process lifetime.
 *
 * <p>
 * The cold start interval is _empty_ if the process creation was not _caused_ by the creation of an
 * Activity. In other words, when the process is instantiated due to receiving a broadcast,
 * instantiating a service or a content provider, Chrome gets initialized during that time and can
 * remain idle (frozen) for a long time before the first Activity is created. Otherwise, the cold
 * start interval starts at process creation and ends when the first activity returns from
 * onCreate().
 *
 * <p>
 * It is possible to confirm that it was cold start when the first Activity started. Detecting that
 * no other Activities intervened since then is more difficult and is not always possible using the
 * ApplicationStatus because activity state changes arrive when the activity calls corresponding
 * methods of the superclass (e.g. performs super.onPause()).
 *
 * <p>
 * Note: This class is not ideal for use when recording UMA metrics filtered by cold startup because
 * it applies a time-based heuristic on Android releases prior to P. On these systems the app may
 * sometimes appear as cold even if it has been fully initialized a little bit in advance. UMA
 * histograms based on this heuristic may feature an unwanted population-biased hump.
 */
public class ColdStartTracker implements ActivityStateListener {
    private static ColdStartTracker sColdStartTracker;

    private Boolean mStartedAsCold;

    private ColdStartTracker() {
        assert ApplicationStatus.isInitialized();
        ApplicationStatus.registerStateListenerForAllActivities(this);
    }

    /** Must be called after {@link ApplicationStatus} is initialized. */
    public static void initialize() {
        assert sColdStartTracker == null;
        sColdStartTracker = new ColdStartTracker();
    }

    /**
     * Robolectric JUnit tests create a new application between each test without resetting static
     * state. This method allows to reset the state and then {@link #initialize()} again.
     */
    public static void resetInstanceForTesting() {
        sColdStartTracker = null;
    }

    public static void setStartedAsColdForTesting() {
        if (sColdStartTracker == null) initialize();
        sColdStartTracker.mStartedAsCold = true;
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.CREATED) {
            detectStartedAsCold();
            ApplicationStatus.unregisterActivityStateListener(this);
        }
    }

    // Must be called during onCreate() (or earlier) of the first activity created in the process.
    private void detectStartedAsCold() {
        if (mStartedAsCold != null) return;
        if (VERSION.SDK_INT >= VERSION_CODES.P) {
            mStartedAsCold = isColdStartupOnP();
            return;
        }

        // Fallback: treat recently started process as cold.
        mStartedAsCold = (SystemClock.elapsedRealtime() - Process.getStartElapsedRealtime() < 500);
    }

    @RequiresApi(Build.VERSION_CODES.P)
    private boolean isColdStartupOnP() {
        @ProcessCreationReason
        int creationReason = SplitCompatAppComponentFactory.getProcessCreationReason();
        if (creationReason <= ProcessCreationReason.PENDING) {
            assert BuildConfig.IS_FOR_TEST;
            // The process creation hooks have not been run, therefore this is a cold start.
            // This condition was observed in robolectric tests. Unlikely to happen on real devices.
            return true;
        }

        // Service connections, content providers and broadcast receivers could have created the
        // process long before the activity was created. These other process creation reasons likely
        // make it a warm start.
        return creationReason == ProcessCreationReason.ACTIVITY;
    }

    /**
     * Tells whether it was cold start when the first Activity was created. If called before any
     * Activity is fully created, considers that the call is made from the first Activity.onCreate()
     * and tells whether it is starting as cold.
     *
     * <p>
     * It is highly recommended to combine this heuristic with
     * {@link SimpleStartupForegroundSessionDetector#runningCleanForegroundSession()} to filter out
     * cases when another Activity was created.
     *
     * <p>
     * This method must *not* be called from code running before the first Activity (such as in
     * Application).
     */
    public static boolean wasColdOnFirstActivityCreationOrNow() {
        if (BuildConfig.IS_FOR_TEST && sColdStartTracker == null) return false;
        return sColdStartTracker.firstActivityWasColdOrDidNotGetCreatedYet();
    }

    private boolean firstActivityWasColdOrDidNotGetCreatedYet() {
        detectStartedAsCold();
        return mStartedAsCold;
    }
}
