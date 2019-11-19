// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Integrates native PowerMonitor with the java side.
 */
@JNINamespace("base::android")
public class PowerMonitor  {
    private static PowerMonitor sInstance;

    private boolean mIsBatteryPower;

    public static void createForTests() {
        // Applications will create this once the JNI side has been fully wired up both sides. For
        // tests, we just need native -> java, that is, we don't need to notify java -> native on
        // creation.
        sInstance = new PowerMonitor();
    }

    /**
     * Create a PowerMonitor instance if none exists.
     */
    public static void create() {
        ThreadUtils.assertOnUiThread();

        if (sInstance != null) return;

        Context context = ContextUtils.getApplicationContext();
        sInstance = new PowerMonitor();
        IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatusIntent = context.registerReceiver(null, ifilter);
        if (batteryStatusIntent != null) onBatteryChargingChanged(batteryStatusIntent);

        IntentFilter powerConnectedFilter = new IntentFilter();
        powerConnectedFilter.addAction(Intent.ACTION_POWER_CONNECTED);
        powerConnectedFilter.addAction(Intent.ACTION_POWER_DISCONNECTED);
        context.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                PowerMonitor.onBatteryChargingChanged(intent);
            }
        }, powerConnectedFilter);
    }

    private PowerMonitor() {
    }

    private static void onBatteryChargingChanged(Intent intent) {
        assert sInstance != null;
        int chargePlug = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
        // If we're not plugged, assume we're running on battery power.
        sInstance.mIsBatteryPower = chargePlug != BatteryManager.BATTERY_PLUGGED_USB
                && chargePlug != BatteryManager.BATTERY_PLUGGED_AC;
        PowerMonitorJni.get().onBatteryChargingChanged();
    }

    @CalledByNative
    private static boolean isBatteryPower() {
        // Creation of the PowerMonitor can be deferred based on the browser startup path.  If the
        // battery power is requested prior to the browser triggering the creation, force it to be
        // created now.
        if (sInstance == null) create();

        return sInstance.mIsBatteryPower;
    }

    @NativeMethods
    interface Natives {
        void onBatteryChargingChanged();
    }
}
