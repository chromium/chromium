// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;

import com.google.ipc.invalidation.ticl.android2.channel.AndroidGcmController;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.invalidation.InvalidationClientService;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.notifier.InvalidationIntentProtocol;

import java.util.HashSet;

/**
 * Controller used to send start, stop, and registration-change commands to the invalidation
 * client library used by Sync.
 */
public class InvalidationController implements ApplicationStatus.ApplicationStateListener {
    private static final String TAG = "cr_invalidation";

    /**
     * Timer which can be paused. When the timer is paused, the execution of its scheduled task is
     * delayed till the timer is resumed.
     */
    private static class Timer {
        private Handler mHandler;

        /**
         * Runnable which is added to the handler's message queue.
         */
        private Runnable mHandlerRunnable;

        /**
         * User provided task.
         */
        private Runnable mRunnable;

        /**
         * Time at which the task is scheduled.
         */
        private long mScheduledTime;

        public Timer() {
            mHandler = new Handler();
        }

        /**
         * Sets the task to run. The task will run after the delay or once {@link #resume()} is
         * called, whichever occurs last. The previously scheduled task, if any, is cancelled.
         * @param r Task to run.
         * @param delayMs Delay in milliseconds after which to run the task.
         */
        public void setRunnable(Runnable r, long delayMs) {
            cancel();
            mRunnable = r;
            mScheduledTime = SystemClock.elapsedRealtime() + delayMs;
        }

        /**
         * Blocks the task from being run.
         */
        public void pause() {
            if (mHandlerRunnable == null) return;

            mHandler.removeCallbacks(mHandlerRunnable);
            mHandlerRunnable = null;
        }

        /**
         * Unblocks the task from being run. If the task was scheduled for a time in the past, runs
         * the task. Does nothing if no task is scheduled.
         */
        public void resume() {
            if (mRunnable == null || mHandlerRunnable != null) return;

            long delayMs = Math.max(mScheduledTime - SystemClock.elapsedRealtime(), 0);
            mHandlerRunnable = new Runnable() {
                @Override
                public void run() {
                    Runnable r = mRunnable;
                    mRunnable = null;
                    mHandlerRunnable = null;
                    r.run();
                }
            };
            mHandler.postDelayed(mHandlerRunnable, delayMs);
        }

        /**
         * Cancels the scheduled task, if any.
         */
        public void cancel() {
            pause();
            mRunnable = null;
        }
    }

    /**
     * The amount of time after the RecentTabsPage is opened to register for session sync
     * invalidations. The delay is designed so that only users who linger on the RecentTabsPage
     * register for session sync invalidations. How long users spend on the RecentTabsPage is
     * measured by the NewTabPage.RecentTabsPage.TimeVisibleAndroid UMA metric.
     */
    private static final int REGISTER_FOR_SESSION_SYNC_INVALIDATIONS_DELAY_MS = 20000;

    /**
     * The amount of time after the RecentTabsPage is closed to unregister for session sync
     * invalidations. The delay is long to avoid registering and unregistering a lot if the user
     * visits the RecentTabsPage a lot.
     */
    private static final int UNREGISTER_FOR_SESSION_SYNC_INVALIDATIONS_DELAY_MS = 3600000; // 1hr

    private static final Object LOCK = new Object();

    @SuppressLint("StaticFieldLeak")
    private static InvalidationController sInstance;

    /**
     * Whether the controller was started.
     */
    private boolean mStarted;

    /**
     * Used to schedule tasks to enable and disable session sync invalidations.
     */
    private Timer mEnableSessionInvalidationsTimer;

    /**
     *  Whether session sync invalidations are enabled.
     */
    private boolean mSessionInvalidationsEnabled;

    /**
     * The number of open RecentTabsPages
     */
    private int mNumRecentTabPages;

    /**
     * Whether GCM has been initialized for Invalidations.
     */
    private boolean mGcmInitialized;

    /**
     * Updates the sync invalidation types that the client is registered for based on the preferred
     * sync types.  Starts the client if needed.
     */
    public void ensureStartedAndUpdateRegisteredTypes() {
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService == null) return;

        mStarted = true;

        // Ensure GCM has been initialized.
        ensureGcmIsInitialized();

        // Do not apply changes to {@link #mSessionInvalidationsEnabled} yet because the timer task
        // may be scheduled far into the future.
        mEnableSessionInvalidationsTimer.resume();

        HashSet<Integer> typesToRegister = new HashSet<Integer>();
        typesToRegister.addAll(syncService.getPreferredDataTypes());
        if (!mSessionInvalidationsEnabled) {
            typesToRegister.remove(ModelType.SESSIONS);
            typesToRegister.remove(ModelType.FAVICON_TRACKING);
            typesToRegister.remove(ModelType.FAVICON_IMAGES);
        }

        Intent registerIntent = InvalidationIntentProtocol.createRegisterIntent(
                ChromeSigninController.get().getSignedInUser(), typesToRegister);
        registerIntent.setClass(ContextUtils.getApplicationContext(),
                InvalidationClientService.getRegisteredClass());
        startServiceIfPossible(registerIntent);
    }

    /**
     * Registers for Google Cloud Messaging (GCM) for Invalidations.
     */
    private void ensureGcmIsInitialized() {
        if (mGcmInitialized) return;
        mGcmInitialized = true;
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                boolean useGcmUpstream = true;
                AndroidGcmController.get(ContextUtils.getApplicationContext())
                        .initializeGcm(useGcmUpstream);
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    public boolean isGcmInitialized() {
        return mGcmInitialized;
    }

    /**
     * Starts the invalidation client without updating the registered invalidation types.
     */
    private void start() {
        mStarted = true;
        mEnableSessionInvalidationsTimer.resume();
        Intent intent = new Intent(ContextUtils.getApplicationContext(),
                InvalidationClientService.getRegisteredClass());
        startServiceIfPossible(intent);
    }

    /**
     * Stops the invalidation client.
     */
    public void stop() {
        mStarted = false;
        mEnableSessionInvalidationsTimer.pause();
        Intent intent = new Intent(ContextUtils.getApplicationContext(),
                InvalidationClientService.getRegisteredClass());
        intent.putExtra(InvalidationIntentProtocol.EXTRA_STOP, true);
        startServiceIfPossible(intent);
    }

    private void startServiceIfPossible(Intent intent) {
        // The use of background services is restricted when the application is not in foreground
        // for O. See crbug.com/680812.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                ContextUtils.getApplicationContext().startService(intent);
            } catch (IllegalStateException exception) {
                Log.e(TAG, "Failed to start service from exception: ", exception);
            }
        } else {
            ContextUtils.getApplicationContext().startService(intent);
        }
    }

    /**
     * Returns whether the invalidation client has been started.
     */
    public boolean isStarted() {
        return mStarted;
    }

    /**
     * Called when a RecentTabsPage is opened.
     */
    public void onRecentTabsPageOpened() {
        ++mNumRecentTabPages;
        if (mNumRecentTabPages == 1) {
            setSessionInvalidationsEnabled(true, REGISTER_FOR_SESSION_SYNC_INVALIDATIONS_DELAY_MS);
        }
    }

    /**
     * Called when a RecentTabsPage is closed.
     */
    public void onRecentTabsPageClosed() {
        --mNumRecentTabPages;
        if (mNumRecentTabPages == 0) {
            setSessionInvalidationsEnabled(
                    false, UNREGISTER_FOR_SESSION_SYNC_INVALIDATIONS_DELAY_MS);
        }
    }

    /**
     * Returns the instance that will use {@code context} to issue intents.
     *
     * Calling this method will create the instance if it does not yet exist.
     */
    public static InvalidationController get() {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new InvalidationController();
            }
            return sInstance;
        }
    }

    /**
     * Schedules a task to enable/disable session sync invalidations. Cancels any previously
     * scheduled tasks to enable/disable session sync invalidations.
     * @param enabled whether to enable or disable session sync invalidations.
     * @param delayMs Delay in milliseconds after which to apply change.
     */
    private void setSessionInvalidationsEnabled(final boolean enabled, long delayMs) {
        mEnableSessionInvalidationsTimer.cancel();
        if (mSessionInvalidationsEnabled == enabled) return;

        mEnableSessionInvalidationsTimer.setRunnable(new Runnable() {
            @Override
            public void run() {
                mSessionInvalidationsEnabled = enabled;
                ensureStartedAndUpdateRegisteredTypes();
            }
        }, delayMs);
        if (mStarted) {
            mEnableSessionInvalidationsTimer.resume();
        }
    }

    /**
     * Creates an instance using {@code context} to send intents.
     */
    @VisibleForTesting
    InvalidationController() {
        if (ContextUtils.getApplicationContext() == null)
            throw new NullPointerException("Unable to get application context");
        mSessionInvalidationsEnabled = false;
        mEnableSessionInvalidationsTimer = new Timer();

        ApplicationStatus.registerApplicationStateListener(this);
    }

    @Override
    public void onApplicationStateChange(int newState) {
        // The isSyncEnabled() check is used to check whether the InvalidationController would be
        // started if it did not stop itself when the application is paused.
        if (AndroidSyncSettings.isSyncEnabled()) {
            if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
                start();
            } else if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
                stop();
            }
        }
    }
}
