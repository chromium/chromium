// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.omaha.OmahaBase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Monitors the event that indicates the screen is turning on.  Used to run actions that shouldn't
 * occur while the phone's screen is off (i.e. when the user expects the phone to be "asleep").
 *
 * When conditions are right, code in {@link PowerBroadcastReceiver.ServiceRunnable#runActions()}
 * is executed.
 */
public class PowerBroadcastReceiver extends BroadcastReceiver {
    private final AtomicBoolean mIsRegistered = new AtomicBoolean(false);

    private PowerManagerHelper mPowerManagerHelper;
    private ServiceRunnable mServiceRunnable;

    /**
     * Stubs out interaction with the PowerManager.
     */
    @VisibleForTesting
    static class PowerManagerHelper {
        /** @return whether the screen is on or not. */
        public boolean isScreenOn(Context context) {
            return ApiCompatibilityUtils.isInteractive();
        }
    }

    /**
     * Defines a set of actions to perform when the conditions are met.
     */
    @VisibleForTesting
    public static class ServiceRunnable implements Runnable {
        @IntDef({State.UNINITIALIZED, State.POSTED, State.CANCELED, State.COMPLETED})
        @Retention(RetentionPolicy.SOURCE)
        public @interface State {
            int UNINITIALIZED = 0;
            int POSTED = 1;
            int CANCELED = 2;
            int COMPLETED = 3;
        }

        /**
         * ANRs are triggered if the app fails to respond to a touch event within 5 seconds. Posting
         * this runnable after 5 seconds lets ChromeTabbedActivity.onResume() perform whatever more
         * important tasks are necessary: http://b/5864891
         */
        private static final long MS_DELAY_TO_RUN = 5000;
        private final Handler mHandler = new Handler(Looper.getMainLooper());

        private @State int mState = State.UNINITIALIZED;

        public int getState() {
            return mState;
        }

        public void post() {
            if (mState == State.POSTED) return;
            setState(State.POSTED);
            mHandler.postDelayed(this, getDelayToRun());
        }

        public void cancel() {
            if (mState != State.POSTED) return;
            setState(State.CANCELED);
            mHandler.removeCallbacks(this);
        }

        /** Unless testing, do not override this function. */
        @Override
        public void run() {
            if (mState != State.POSTED) return;
            setState(State.COMPLETED);
            runActions();
        }

        public void setState(@State int state) {
            mState = state;
        }

        /**
         * Executed when all of the system conditions are met.
         */
        public void runActions() {
            Context context = ContextUtils.getApplicationContext();
            OmahaBase.onForegroundSessionStart(context);
        }

        public long getDelayToRun() {
            return MS_DELAY_TO_RUN;
        }
    }

    public PowerBroadcastReceiver() {
        mServiceRunnable = new ServiceRunnable();
        mPowerManagerHelper = new PowerManagerHelper();
    }

    /** See {@link ChromeApplication#onForegroundSessionStart()}. */
    public void onForegroundSessionStart() {
        ThreadUtils.assertOnUiThread();
        assert Looper.getMainLooper() == Looper.myLooper();

        if (mPowerManagerHelper.isScreenOn(ContextUtils.getApplicationContext())) {
            mServiceRunnable.post();
        } else {
            registerReceiver();
        }
    }

    /** See {@link ChromeApplication#onForegroundSessionEnd()}. */
    public void onForegroundSessionEnd() {
        assert Looper.getMainLooper() == Looper.myLooper();

        mServiceRunnable.cancel();
        unregisterReceiver();
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())
                && ApplicationStatus.hasVisibleActivities()) {
            mServiceRunnable.post();
            unregisterReceiver();
        }
    }

    /**
     * @return Whether or not this is registered with a context.
     */
    @VisibleForTesting
    public boolean isRegistered() {
        return mIsRegistered.get();
    }

    /**
     * Unregisters this broadcast receiver so it no longer receives Intents.
     * Also cancels any Runnables waiting to be executed.
     */
    private void unregisterReceiver() {
        if (mIsRegistered.getAndSet(false)) {
            ContextUtils.getApplicationContext().unregisterReceiver(this);
        }
    }

    /**
     * Registers this broadcast receiver so it receives Intents.
     */
    private void registerReceiver() {
        assert Looper.getMainLooper() == Looper.myLooper();
        if (mIsRegistered.getAndSet(true)) return;
        ContextUtils.getApplicationContext().registerReceiver(
                this, new IntentFilter(Intent.ACTION_SCREEN_ON));
    }

    /**
     * Sets the runnable that contains the actions to do when the screen is on.
     */
    @VisibleForTesting
    void setServiceRunnableForTests(ServiceRunnable runnable) {
        assert mServiceRunnable != null;
        mServiceRunnable.cancel();
        mServiceRunnable = runnable;
    }

    /**
     * Sets the PowerManagerHelper that will be used to check if the screen is on.
     */
    @VisibleForTesting
    void setPowerManagerHelperForTests(PowerManagerHelper helper) {
        mPowerManagerHelper = helper;
    }
}
