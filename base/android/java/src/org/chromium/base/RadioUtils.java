// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.Manifest;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Process;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.compat.ApiHelperForP;

/**
 * Exposes radio related information about the current device.
 */
@JNINamespace("base::android")
public class RadioUtils {
    // Cached value indicating if app has ACCESS_NETWORK_STATE permission.
    private static Boolean sHaveAccessNetworkState;
    // Cached value indicating if app has ACCESS_WIFI_STATE permission.
    private static Boolean sHaveAccessWifiState;

    private RadioUtils() {}

    /**
     * Return whether the current SDK supports necessary functions and the app
     * has necessary permissions.
     * @return True or false.
     */
    @CalledByNative
    private static boolean isSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P && haveAccessNetworkState()
                && haveAccessWifiState();
    }

    private static boolean haveAccessNetworkState() {
        // This could be racy if called on multiple threads, but races will
        // end in the same result so it's not a problem.
        if (sHaveAccessNetworkState == null) {
            sHaveAccessNetworkState =
                    ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                            Manifest.permission.ACCESS_NETWORK_STATE, Process.myPid(),
                            Process.myUid())
                    == PackageManager.PERMISSION_GRANTED;
        }
        return sHaveAccessNetworkState;
    }

    private static boolean haveAccessWifiState() {
        // This could be racy if called on multiple threads, but races will
        // end in the same result so it's not a problem.
        if (sHaveAccessWifiState == null) {
            sHaveAccessWifiState =
                    ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                            Manifest.permission.ACCESS_WIFI_STATE, Process.myPid(), Process.myUid())
                    == PackageManager.PERMISSION_GRANTED;
        }
        return sHaveAccessWifiState;
    }

    /**
     * Return whether the device is currently connected to a wifi network.
     * @return True or false.
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.P)
    private static boolean isWifiConnected() {
        assert isSupported();
        ConnectivityManager connectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        Network network = ApiHelperForM.getActiveNetwork(connectivityManager);
        if (network == null) return false;
        NetworkCapabilities networkCapabilities =
                connectivityManager.getNetworkCapabilities(network);
        if (networkCapabilities == null) return false;
        return networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI);
    }

    /**
     * Return current cell signal level.
     * @return Signal level from 0 (no signal) to 4 (good signal) or -1 in case of error.
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.P)
    private static int getCellSignalLevel() {
        assert isSupported();
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);
        int level = -1;
        try {
            SignalStrength signalStrength = ApiHelperForP.getSignalStrength(telephonyManager);
            if (signalStrength != null) {
                level = signalStrength.getLevel();
            }
        } catch (java.lang.SecurityException e) {
            // Sometimes SignalStrength.getLevel() requires extra permissions
            // that Chrome doesn't have. See crbug.com/1150536.
        }
        return level;
    }

    /**
     * Return current cell data activity.
     * @return 0 - none, 1 - in, 2 - out, 3 - in/out, 4 - dormant, or -1 in case of error.
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.P)
    private static int getCellDataActivity() {
        assert isSupported();
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);
        try {
            return telephonyManager.getDataActivity();
        } catch (java.lang.SecurityException e) {
            // Just in case getDataActivity() requires extra permissions.
            return -1;
        }
    }
}
