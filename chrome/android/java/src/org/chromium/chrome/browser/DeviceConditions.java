// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Device network and power conditions that can be either checked individually with the specific
 * static methods or gathered all at once using {@link.getCurrent}.
 */
public class DeviceConditions {
    // Battery and power related variables.
    private boolean mPowerConnected;
    private int mBatteryPercentage;
    private boolean mPowerSaveOn;
    private boolean mScreenOnAndUnlocked;

    // Network related variables.
    private @ConnectionType int mNetConnectionType = ConnectionType.CONNECTION_UNKNOWN;
    private boolean mActiveNetworkMetered;

    // If true, getCurrentNetConnectionType() will always return CONNECTION_NONE.
    @VisibleForTesting
    public static boolean sForceNoConnectionForTesting;

    /**
     * Creates a DeviceConditions instance that stores a snapshot of the current set of device
     * network and power conditions. Also used when setting up tests simulating specific conditions.
     */
    @VisibleForTesting
    public DeviceConditions(boolean powerConnected, int batteryPercentage, int netConnectionType,
            boolean powerSaveOn, boolean activeNetworkMetered, boolean screenOnAndUnlocked) {
        mPowerConnected = powerConnected;
        mBatteryPercentage = batteryPercentage;
        mPowerSaveOn = powerSaveOn;
        mNetConnectionType = netConnectionType;
        mActiveNetworkMetered = activeNetworkMetered;
        mScreenOnAndUnlocked = screenOnAndUnlocked;
    }

    @VisibleForTesting
    DeviceConditions() {}

    /**
     * Returns the current device conditions if the device supports obtaining battery status.
     * Otherwise it will return null.
     */
    public static DeviceConditions getCurrent(Context context) {
        Intent batteryStatus = getBatteryStatus(context);
        if (batteryStatus == null) return null;

        return new DeviceConditions(isCurrentlyPowerConnected(context, batteryStatus),
                getCurrentBatteryPercentage(context, batteryStatus),
                getCurrentNetConnectionType(context), isCurrentlyInPowerSaveMode(context),
                isCurrentActiveNetworkMetered(context), isCurrentlyScreenOnAndUnlocked(context));
    }

    /** @return Whether the device is connected to a power source. */
    public static boolean isCurrentlyPowerConnected(Context context) {
        Intent batteryStatus = getBatteryStatus(context);
        if (batteryStatus == null) return false;

        return isCurrentlyPowerConnected(context, batteryStatus);
    }

    private static boolean isCurrentlyPowerConnected(Context context, Intent batteryStatus) {
        int status = batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        boolean isConnected = (status == BatteryManager.BATTERY_STATUS_CHARGING
                || status == BatteryManager.BATTERY_STATUS_FULL);
        return isConnected;
    }

    /** @return The battery percentage or 0 if the device can't provide that information. */
    public static int getCurrentBatteryPercentage(Context context) {
        Intent batteryStatus = getBatteryStatus(context);
        if (batteryStatus == null) return 0;

        return getCurrentBatteryPercentage(context, batteryStatus);
    }

    private static int getCurrentBatteryPercentage(Context context, Intent batteryStatus) {
        int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (scale == 0) return 0;

        int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int percentage = Math.round(100 * level / (float) scale);
        return percentage;
    }

    /**
     * @return Whether the device is in power save mode. This feature is only available in Lollipop
     * and later versions of Android devices so it will return false for earlier versions.
     */
    public static boolean isCurrentlyInPowerSaveMode(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return false;
        }
        return isCurrentlyInPowerSaveModeLollipop(context);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static boolean isCurrentlyInPowerSaveModeLollipop(Context context) {
        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return powerManager.isPowerSaveMode();
    }

    /**
     * @return Whether the device is in idle / doze mode. This feature is only available in M and
     * later versions of Android devices so it will return false for earlier versions.
     */
    public static boolean isCurrentlyInIdleMode(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return false;
        }
        return isCurrentlyInIdleModeM(context);
    }

    @TargetApi(Build.VERSION_CODES.M)
    private static boolean isCurrentlyInIdleModeM(Context context) {
        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return powerManager.isDeviceIdleMode();
    }

    /**
     * @return Network connection type, where possible values are defined by
     *     org.chromium.net.ConnectionType.
     */
    public static int getCurrentNetConnectionType(Context context) {
        int connectionType = ConnectionType.CONNECTION_NONE;
        if (sForceNoConnectionForTesting) {
            return connectionType;
        }

        // If we are starting in the background, native portion might not be initialized.
        if (NetworkChangeNotifier.isInitialized()) {
            connectionType = NetworkChangeNotifier.getInstance().getCurrentConnectionType();
        }

        // Sometimes the NetworkConnectionNotifier lags the actual connection type, especially when
        // the GCM NM wakes us from doze state.  If we are really connected, report the connection
        // type from android.
        if (connectionType == ConnectionType.CONNECTION_NONE) {
            // Get the connection type from android in case chromium's type is not yet set.
            ConnectivityManager cm =
                    (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
            NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
            boolean isConnected = activeNetwork != null && activeNetwork.isConnectedOrConnecting();
            if (isConnected) {
                connectionType = convertAndroidNetworkTypeToConnectionType(activeNetwork.getType());
            }
        }
        return connectionType;
    }

    /**
     * @return true if the active network is a metered network
     */
    public static boolean isCurrentActiveNetworkMetered(Context context) {
        ConnectivityManager cm =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        return cm.isActiveNetworkMetered();
    }

    /**
     * @return Whether the screen is currently on and unlocked.
     */
    public static boolean isCurrentlyScreenOnAndUnlocked(Context context) {
        KeyguardManager keyguardManager =
                (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        return keyguardManager != null && !keyguardManager.isKeyguardLocked()
                && ApiCompatibilityUtils.isInteractive();
    }

    private static Intent getBatteryStatus(Context context) {
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        // Note this is a sticky intent, so we aren't really registering a receiver, just getting
        // the sticky intent.  That means that we don't need to unregister the filter later.
        return context.registerReceiver(null, filter);
    }

    /** Returns the NCN network type corresponding to the connectivity manager network type */
    private static int convertAndroidNetworkTypeToConnectionType(
            int connectivityManagerNetworkType) {
        if (connectivityManagerNetworkType == ConnectivityManager.TYPE_WIFI) {
            return ConnectionType.CONNECTION_WIFI;
        }
        // for mobile, we don't know if it is 2G, 3G, or 4G, default to worst case of 2G.
        if (connectivityManagerNetworkType == ConnectivityManager.TYPE_MOBILE) {
            return ConnectionType.CONNECTION_2G;
        }
        if (connectivityManagerNetworkType == ConnectivityManager.TYPE_BLUETOOTH) {
            return ConnectionType.CONNECTION_BLUETOOTH;
        }
        // Since NetworkConnectivityManager doesn't understand the other types, call them UNKNOWN.
        return ConnectionType.CONNECTION_UNKNOWN;
    }

    /** Returns whether power is connected. */
    public boolean isPowerConnected() {
        return mPowerConnected;
    }

    /** Returns the remaining battery power percentage (0-100). */
    public int getBatteryPercentage() {
        return mBatteryPercentage;
    }

    /** Returns whether the device is in power save mode. */
    public boolean isInPowerSaveMode() {
        return mPowerSaveOn;
    }

    /**
     * Returns the network connection type based on the values defined in
     * org.chromium.net.ConnectionType.
     */
    public int getNetConnectionType() {
        return mNetConnectionType;
    }

    /**
     * Sets the network connection type.
     */
    @VisibleForTesting
    void setNetworkConnectionType(@ConnectionType int netConnectionType) {
        mNetConnectionType = netConnectionType;
    }

    /** Returns whether network connection is metered. */
    public boolean isActiveNetworkMetered() {
        return mActiveNetworkMetered;
    }

    /** Returns whether the screen is on and unlocked. */
    public boolean isScreenOnAndUnlocked() {
        return mScreenOnAndUnlocked;
    }
}
