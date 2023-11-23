// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.compat.ApiHelperForQ;

/** Integrates native PowerMonitor with the java side. */
@JNINamespace("base::android")
public class PowerMonitor {
    private static PowerMonitor sInstance;

    private boolean mIsBatteryPower;

    public static void createForTests() {
        // Applications will create this once the JNI side has been fully wired up both sides. For
        // tests, we just need native -> java, that is, we don't need to notify java -> native on
        // creation.
        sInstance = new PowerMonitor();
    }

    /** Create a PowerMonitor instance if none exists. */
    public static void create() {
        ThreadUtils.assertOnUiThread();

        if (sInstance != null) return;

        Context context = ContextUtils.getApplicationContext();
        sInstance = new PowerMonitor();
        IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatusIntent =
                ContextUtils.registerProtectedBroadcastReceiver(context, null, ifilter);
        if (batteryStatusIntent != null) {
            // Default to 0, which the EXTRA_PLUGGED docs indicate means "on battery power".  There
            // is no symbolic constant.  Nonzero values indicate we have some external power source.
            int chargePlug = batteryStatusIntent.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0);
            // If we're not plugged, assume we're running on battery power.
            onBatteryChargingChanged(chargePlug == 0);
        }

        IntentFilter powerConnectedFilter = new IntentFilter();
        powerConnectedFilter.addAction(Intent.ACTION_POWER_CONNECTED);
        powerConnectedFilter.addAction(Intent.ACTION_POWER_DISCONNECTED);
        ContextUtils.registerProtectedBroadcastReceiver(
                context,
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        PowerMonitor.onBatteryChargingChanged(
                                intent.getAction().equals(Intent.ACTION_POWER_DISCONNECTED));
                    }
                },
                powerConnectedFilter);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            PowerManager powerManager =
                    (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            if (powerManager != null) {
                PowerMonitorForQ.addThermalStatusListener(powerManager);
            }
        }
    }

    private PowerMonitor() {}

    private static void onBatteryChargingChanged(boolean isBatteryPower) {
        assert sInstance != null;
        sInstance.mIsBatteryPower = isBatteryPower;
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

    @CalledByNative
    private static int getRemainingBatteryCapacity() {
        // Creation of the PowerMonitor can be deferred based on the browser startup path.  If the
        // battery power is requested prior to the browser triggering the creation, force it to be
        // created now.
        if (sInstance == null) create();

        return getRemainingBatteryCapacityImpl();
    }

    private static int getRemainingBatteryCapacityImpl() {
        return ((BatteryManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.BATTERY_SERVICE))
                .getIntProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER);
    }

    @CalledByNative
    private static int getCurrentThermalStatus() {
        // Return invalid code that will get mapped to unknown in the native library.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return -1;

        // Creation of the PowerMonitor can be deferred based on the browser startup path.  If the
        // battery power is requested prior to the browser triggering the creation, force it to be
        // created now.
        if (sInstance == null) create();

        PowerManager powerManager =
                (PowerManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.POWER_SERVICE);
        if (powerManager == null) return -1;
        return ApiHelperForQ.getCurrentThermalStatus(powerManager);
    }

    @NativeMethods
    interface Natives {
        void onBatteryChargingChanged();

        void onThermalStatusChanged(int thermalStatus);
    }
}
