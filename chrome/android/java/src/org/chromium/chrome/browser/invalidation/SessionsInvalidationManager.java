// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;

/**
 * Class responsible for managing registration for invalidations for noisy sync
 * data types such as SESSIONS on Android. It keeps track of how many recent
 * pages tab open and register/unregister for SESSIONS invalidation accordingly.
 * It should be used only from the UI thread.
 */
public class SessionsInvalidationManager implements ApplicationStatus.ApplicationStateListener {
    /**
     * The amount of time after the RecentTabsPage is opened to register for session sync
     * invalidations. The delay is designed so that only users who linger on the RecentTabsPage
     * register for session sync invalidations. How long users spend on the RecentTabsPage is
     * measured by the NewTabPage.RecentTabsPage.TimeVisibleAndroid UMA metric.
     */
    static final int REGISTER_FOR_SESSION_SYNC_INVALIDATIONS_DELAY_MS =
            (int) DateUtils.SECOND_IN_MILLIS * 20;

    /** Used to schedule tasks to enable and disable session sync invalidations. */
    private final ResumableDelayedTaskRunner mEnableSessionInvalidationsRunner;

    /** Used to call native code that enables and disables session invalidations. */
    private final Profile mProfile;

    private static SessionsInvalidationManager sInstance;

    /** Whether session sync invalidations are enabled. */
    private boolean mIsSessionInvalidationsEnabled;

    /** The number of open RecentTabsPages */
    private int mNumRecentTabPages;

    /**
     * Returns the a singleton SessionsInvalidationManager.
     *
     * Calling this method will create the instance if it does not yet exist.
     */
    public static SessionsInvalidationManager get(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SessionsInvalidationManager(profile, new ResumableDelayedTaskRunner());
        }
        return sInstance;
    }

    @VisibleForTesting
    SessionsInvalidationManager(Profile profile, ResumableDelayedTaskRunner runner) {
        mProfile = profile;
        mIsSessionInvalidationsEnabled = false;
        mEnableSessionInvalidationsRunner = runner;
        ApplicationStatus.registerApplicationStateListener(this);
    }

    /** Called when a RecentTabsPage is opened. */
    public void onRecentTabsPageOpened() {
        ++mNumRecentTabPages;
        if (mNumRecentTabPages == 1) {
            setSessionInvalidationsEnabled(true, REGISTER_FOR_SESSION_SYNC_INVALIDATIONS_DELAY_MS);
        }
    }

    /** Called when a RecentTabsPage is closed. */
    public void onRecentTabsPageClosed() {
        --mNumRecentTabPages;
        if (mNumRecentTabPages == 0) {
            setSessionInvalidationsEnabled(false, 0);
        }
    }

    /**
     * Schedules a task to enable/disable session sync invalidations. Cancels any previously
     * scheduled tasks to enable/disable session sync invalidations.
     * @param isEnabled whether to enable or disable session sync invalidations.
     * @param delayMs Delay in milliseconds after which to apply change.
     */
    private void setSessionInvalidationsEnabled(boolean isEnabled, long delayMs) {
        mEnableSessionInvalidationsRunner.cancel();

        if (mIsSessionInvalidationsEnabled == isEnabled) {
            return;
        }

        mEnableSessionInvalidationsRunner.setRunnable(
                () -> {
                    mIsSessionInvalidationsEnabled = isEnabled;
                    ForeignSessionHelper foreignSessionHelper = new ForeignSessionHelper(mProfile);
                    foreignSessionHelper.setInvalidationsForSessionsEnabled(isEnabled);
                    foreignSessionHelper.destroy();
                },
                delayMs);
        mEnableSessionInvalidationsRunner.resume();
    }

    @Override
    public void onApplicationStateChange(int newState) {
        // Registering for receiving invalidation requires calling native code. Therefore, we should
        // pause the runner when Chrome goes to the background to postpone calling up native code
        // until next time Chrome is running again.
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            mEnableSessionInvalidationsRunner.resume();
        } else if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            mEnableSessionInvalidationsRunner.pause();
        }
    }

    @VisibleForTesting
    public boolean isSessionInvalidationsEnabled() {
        return mIsSessionInvalidationsEnabled;
    }
}
