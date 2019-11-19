// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.PowerManager;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;

/**
 * Observes and keeps a record of whether the system power saving mode is on.
 */
public class PowerSavingModeMonitor {
    private static PowerSavingModeMonitor sInstance;

    /** Returns the instance of this singleton. */
    public static PowerSavingModeMonitor getInstance() {
        if (sInstance == null) {
            sInstance = new PowerSavingModeMonitor();
        }
        return sInstance;
    }

    private final ObserverList<Runnable> mObservers = new ObserverList<>();
    @Nullable private final PowerManager mPowerManager;
    @Nullable private BroadcastReceiver mPowerModeReceiver;

    private boolean mPowerSavingIsOn;

    /** Returns whether power saving mode is currently on. */
    public boolean powerSavingIsOn() {
        return mPowerSavingIsOn;
    }

    /** Adds an observer of power saving mode changes. */
    public void addObserver(Runnable observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer of power saving mode changes. */
    public void removeObserver(Runnable observer) {
        mObservers.removeObserver(observer);
    }

    private PowerSavingModeMonitor() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            // Power manager not available before Lollipop. mPowerSavingIsOn is false forever.
            mPowerManager = null;
            return;
        }
        mPowerManager = (PowerManager) ContextUtils.getApplicationContext().getSystemService(
                Context.POWER_SERVICE);

        updatePowerSaveMode();
        updateAccordingToAppState();
        ApplicationStatus.registerApplicationStateListener(state -> updateAccordingToAppState());
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updateAccordingToAppState() {
        final int applicationState = ApplicationStatus.getStateForApplication();
        if (applicationState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || applicationState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            start();
        } else {
            stop();
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void start() {
        if (mPowerModeReceiver == null) {
            mPowerModeReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    updatePowerSaveMode();
                }
            };
            ContextUtils.getApplicationContext().registerReceiver(mPowerModeReceiver,
                    new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
        }
        updatePowerSaveMode();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void stop() {
        if (mPowerModeReceiver != null) {
            ContextUtils.getApplicationContext().unregisterReceiver(mPowerModeReceiver);
            mPowerModeReceiver = null;
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updatePowerSaveMode() {
        boolean newValue = mPowerManager != null && mPowerManager.isPowerSaveMode();
        if (newValue == mPowerSavingIsOn) return;

        mPowerSavingIsOn = newValue;
        for (Runnable observer : mObservers) {
            observer.run();
        }
    }
}
