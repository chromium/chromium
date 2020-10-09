// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Exposes radio related information about the current device.
 */
@JNINamespace("base::android")
public class RadioUtils {
    private RadioUtils() {}

    /**
     * Return whether the current SDK supports necessary functions.
     * @return True or false.
     */
    @CalledByNative
    private static boolean isSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P;
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
        Network network = connectivityManager.getActiveNetwork();
        if (network == null) return false;
        NetworkCapabilities networkCapabilities =
                connectivityManager.getNetworkCapabilities(network);
        if (networkCapabilities == null) return false;
        return networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI);
    }

    /**
     * Return current cell signal level.
     * @return Signal level from 0 (no signal) to 4 (good signal).
     */
    @CalledByNative
    @TargetApi(Build.VERSION_CODES.P)
    private static int getCellSignalLevel() {
        assert isSupported();
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);
        SignalStrength signalStrength = telephonyManager.getSignalStrength();
        if (signalStrength == null) return -1;
        return signalStrength.getLevel();
    }
}
