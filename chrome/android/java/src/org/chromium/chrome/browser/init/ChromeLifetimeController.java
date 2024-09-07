// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;

import androidx.core.os.BuildCompat;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.BrowserRestartActivity;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;

/**
 * Answers requests to kill and (potentially) restart Chrome's main browser process.
 *
 * <p>This class fires an Intent to start the {@link BrowserRestartActivity}, which will ultimately
 * kill the main browser process from its own process.
 *
 * <p>https://crbug.com/515919 details why another Activity is used instead of using the
 * AlarmManager. https://crbug.com/545453 details why the BrowserRestartActivity handles the process
 * killing.
 */
class ChromeLifetimeController
        implements ApplicationLifetime.Observer, ApplicationStatus.ActivityStateListener {
    /** Amount of time to wait for Chrome to destroy all the activities of the main process. */
    private static final long WATCHDOG_DELAY_MS = 1000;

    /** Singleton instance of the class. */
    private static ChromeLifetimeController sInstance;

    /** Handler to post tasks to. */
    private final Handler mHandler;

    /** Restarts the process. */
    private final Runnable mRestartRunnable;

    /** Whether or not killing the process was already initiated. */
    private boolean mIsWaitingForProcessDeath;

    /** Whether or not Chrome should be restarted after the process is killed. */
    private boolean mRestartChromeOnDestroy;

    /** How many Chrome Activities are still alive. */
    private int mRemainingActivitiesCount;

    /** Initialize the ChromeLifetimeController; */
    public static void initialize() {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) return;
        sInstance = new ChromeLifetimeController();
        ApplicationLifetime.addObserver(sInstance);
    }

    private ChromeLifetimeController() {
        mHandler = new Handler(Looper.getMainLooper());
        mRestartRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        fireBrowserRestartActivityIntent();
                    }
                };
    }

    @Override
    public void onTerminate(boolean restart) {
        mRestartChromeOnDestroy = restart;

        // Tell all Chrome Activities to finish themselves.
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
            mRemainingActivitiesCount++;
            activity.finish();
        }

        if (BuildCompat.isAtLeastV()) {
            // Background activity launches are prohibited on newer versions of Android, so if the
            // restart intent isn't fired right away then Chrome won't restart. See b/331370736.
            mHandler.post(mRestartRunnable);
        } else {
            // Kick off a timer to kill the process after a delay, which fires only if the
            // Activities
            // take too long to be finished.
            mHandler.postDelayed(mRestartRunnable, WATCHDOG_DELAY_MS);
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        assert mRemainingActivitiesCount > 0;
        if (newState == ActivityState.DESTROYED) {
            mRemainingActivitiesCount--;
            if (mRemainingActivitiesCount == 0) {
                fireBrowserRestartActivityIntent();
            }
        }
    }

    /** Start the Activity that will ultimately kill this process. */
    private void fireBrowserRestartActivityIntent() {
        ThreadUtils.assertOnUiThread();

        if (mIsWaitingForProcessDeath) return;
        mIsWaitingForProcessDeath = true;
        mHandler.removeCallbacks(mRestartRunnable);

        // The {@link BrowserRestartActivity} starts in its own process.
        Context context = ContextUtils.getApplicationContext();
        Intent intent = BrowserRestartActivity.createIntent(context, mRestartChromeOnDestroy);
        context.startActivity(intent);
    }
}
