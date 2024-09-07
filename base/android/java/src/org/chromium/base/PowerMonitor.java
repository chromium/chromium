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

import org.chromium.base.power_monitor.BatteryPowerStatus;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Integrates native PowerMonitor with the java side. */
@JNINamespace("base::android")
public class PowerMonitor {
    private static boolean sIsInitRequested;

    @PowerStatus private static int sBatteryPowerStatus = PowerStatus.UNKNOWN;

    @Retention(RetentionPolicy.SOURCE)
    @interface PowerStatus {
        int UNKNOWN = 0;
        int BATTERY_POWER = 1;
        int EXTERNAL_POWER = 2;
    }

    public static void createForTests() {
        // Applications will create this once the JNI side has been fully wired up both sides. For
        // tests, we just need native -> java, that is, we don't need to notify java -> native on
        // creation.
        sIsInitRequested = true;
    }

    /** Create a PowerMonitor instance if none exists. */
    public static void create() {
        ThreadUtils.assertOnUiThread();
        if (sIsInitRequested) return;
        sIsInitRequested = true;
        if (BaseFeatureMap.isEnabled(
                BaseFeatures.POST_POWER_MONITOR_BROADCAST_RECEIVER_INIT_TO_BACKGROUND)) {
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, PowerMonitor::createInternal);
        } else {
            createInternal();
        }
    }

    private static void createInternal() {
        Context context = ContextUtils.getApplicationContext();
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

        IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatusIntent =
                ContextUtils.registerProtectedBroadcastReceiver(context, null, ifilter);

        // createInternal can be called from a background thread, we need to post the updates to the
        // main thread so that mIsBatteryPower is only gets called from UI thread.
        if (batteryStatusIntent != null) {
            // Default to 0, which the EXTRA_PLUGGED docs indicate means "on battery
            // power". There is no symbolic constant. Nonzero values indicate we have some external
            // power source.
            int chargePlug = batteryStatusIntent.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0);

            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // Only update if the status is the default.
                        if (sBatteryPowerStatus == PowerStatus.UNKNOWN) {
                            // If we're not plugged, assume we're running on battery power.
                            onBatteryChargingChanged(chargePlug == 0);
                        }
                    });
        }
    }

    private PowerMonitor() {}

    private static void onBatteryChargingChanged(boolean isBatteryPower) {
        ThreadUtils.assertOnUiThread();
        // We can't allow for updating battery status without requesting initialization.
        assert sIsInitRequested;
        sBatteryPowerStatus =
                isBatteryPower ? PowerStatus.BATTERY_POWER : PowerStatus.EXTERNAL_POWER;
        PowerMonitorJni.get().onBatteryChargingChanged();
    }

    @CalledByNative
    @BatteryPowerStatus
    private static int getBatteryPowerStatus() {
        // Creation of the PowerMonitor can be deferred based on the browser startup path.  If the
        // battery power is requested prior to the browser triggering the creation, force it to be
        // created now.
        if (!sIsInitRequested) {
            create();
        }

        return switch (sBatteryPowerStatus) {
                // UNKNOWN is for the default state, when we don't have value yet. That happens if
                // we call isBatteryPower() before the creation is done.
            case PowerStatus.UNKNOWN -> BatteryPowerStatus.UNKNOWN;
            case PowerStatus.EXTERNAL_POWER -> BatteryPowerStatus.EXTERNAL_POWER;
            case PowerStatus.BATTERY_POWER -> BatteryPowerStatus.BATTERY_POWER;
            default -> throw new IllegalStateException("Unexpected value: " + sBatteryPowerStatus);
        };
    }

    @CalledByNative
    private static int getRemainingBatteryCapacity() {
        // Creation of the PowerMonitor can be deferred based on the browser startup path.  If the
        // battery power is requested prior to the browser triggering the creation, force it to be
        // created now.
        if (!sIsInitRequested) {
            create();
        }
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
        if (!sIsInitRequested) {
            create();
        }
        PowerManager powerManager =
                (PowerManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.POWER_SERVICE);
        if (powerManager == null) return -1;
        return powerManager.getCurrentThermalStatus();
    }

    @NativeMethods
    interface Natives {
        void onBatteryChargingChanged();

        void onThermalStatusChanged(int thermalStatus);
    }
}
