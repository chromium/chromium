// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.OutcomeReceiver;
import android.os.PowerManager;
import android.os.PowerMonitorReadings;
import android.os.health.SystemHealthManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.power_monitor.BatteryPowerStatus;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Integrates native PowerMonitor with the java side. */
@NullMarked
@JNINamespace("base::android")
public class PowerMonitor {
    private static boolean sIsInitRequested;

    @PowerStatus private static int sBatteryPowerStatus = PowerStatus.UNKNOWN;

    private static final int SYSTEM_HEALTH_MANAGER_API_TIMEOUT_MS = 1000;
    private static final String SYSTEM_HEALTH_MANAGER_ERROR_HISTOGRAM =
            "Power.SystemHealthManagerError";

    private static final List<android.os.PowerMonitor> sPowerMonitors = new ArrayList<>();
    private static @Nullable SystemHealthManager sSystemHealthManager;
    private static @Nullable SequencedTaskRunner sTaskRunner;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @Retention(RetentionPolicy.SOURCE)
    @interface SystemHealthManagerError {
        // LINT.IfChange(SystemHealthManagerError)
        int NO_POWER_MONITORS = 0;
        int GET_POWER_MONITOR_READINGS_INTERRUPTED = 1;
        int GET_POWER_MONITOR_READINGS_TIMEOUT = 2;
        int COUNT = 3;
        // LINT.ThenChange(/tools/metrics/histograms/metadata/power/enums.xml:SystemHealthManagerError)
    };

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
                                Intent.ACTION_POWER_DISCONNECTED.equals(intent.getAction()));
                    }
                },
                powerConnectedFilter);

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (powerManager != null) {
            powerManager.addThermalStatusListener(
                    new PowerManager.OnThermalStatusChangedListener() {
                        @Override
                        public void onThermalStatusChanged(int status) {
                            PowerMonitorJni.get().onThermalStatusChanged(status);
                        }
                    });
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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            sTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);
            sSystemHealthManager =
                    (SystemHealthManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.SYSTEM_HEALTH_SERVICE);
            if (sSystemHealthManager != null) {
                sSystemHealthManager.getSupportedPowerMonitors(
                        Runnable::run,
                        (monitors) -> {
                            synchronized (sPowerMonitors) {
                                for (android.os.PowerMonitor monitor : monitors) {
                                    // Ignore direct rails measurements. Their meaning is
                                    // effectively unknown.
                                    // https://developer.android.com/reference/android/os/PowerMonitor#POWER_MONITOR_TYPE_CONSUMER
                                    // https://developer.android.com/reference/android/os/PowerMonitor#POWER_MONITOR_TYPE_MEASUREMENT
                                    if (monitor.getType()
                                            == android.os.PowerMonitor
                                                    .POWER_MONITOR_TYPE_CONSUMER) {
                                        sPowerMonitors.add(monitor);
                                    }
                                }
                            }
                        });
            }
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
    private static @JniType("std::vector<PowerMonitorReading>") List<PowerMonitorReading>
            getTotalEnergyConsumed() {
        ThreadUtils.assertOnBackgroundThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            return Collections.emptyList();
        }
        CountDownLatch ready = new CountDownLatch(1);
        class TotalEnergyReceiver
                implements OutcomeReceiver<PowerMonitorReadings, RuntimeException> {
            private final List<PowerMonitorReading> mTotalEnergyConsumed = new ArrayList<>();

            @Override
            public void onResult(PowerMonitorReadings readings) {
                ThreadUtils.assertOnBackgroundThread();
                // No need to synchronize sPowerMonitors because it is never mutated once populated.
                for (android.os.PowerMonitor monitor : sPowerMonitors) {
                    long consumed = readings.getConsumedEnergy(monitor);
                    if (consumed != PowerMonitorReadings.ENERGY_UNAVAILABLE) {
                        mTotalEnergyConsumed.add(
                                new PowerMonitorReading(monitor.getName(), consumed));
                    }
                }
                ready.countDown();
            }

            public List<PowerMonitorReading> getTotalEnergyConsumed() {
                return mTotalEnergyConsumed;
            }
        }

        TotalEnergyReceiver receiver = new TotalEnergyReceiver();
        // We start monitoring power use only after we know the battery status. Thus we don't need
        // to call `create` here.
        synchronized (sPowerMonitors) {
            if (sPowerMonitors.isEmpty()) {
                RecordHistogram.recordEnumeratedHistogram(
                        SYSTEM_HEALTH_MANAGER_ERROR_HISTOGRAM,
                        SystemHealthManagerError.NO_POWER_MONITORS,
                        SystemHealthManagerError.COUNT);
                return Collections.emptyList();
            }
            // If power monitors are not empty, the system health manager and the task runner are
            // initialized.
            assumeNonNull(sSystemHealthManager);
            assumeNonNull(sTaskRunner);
            sSystemHealthManager.getPowerMonitorReadings(sPowerMonitors, sTaskRunner, receiver);
        }
        boolean isReady = false;
        try {
            isReady = ready.await(SYSTEM_HEALTH_MANAGER_API_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            RecordHistogram.recordEnumeratedHistogram(
                    SYSTEM_HEALTH_MANAGER_ERROR_HISTOGRAM,
                    SystemHealthManagerError.GET_POWER_MONITOR_READINGS_INTERRUPTED,
                    SystemHealthManagerError.COUNT);
            return Collections.emptyList();
        }
        if (!isReady) {
            RecordHistogram.recordEnumeratedHistogram(
                    SYSTEM_HEALTH_MANAGER_ERROR_HISTOGRAM,
                    SystemHealthManagerError.GET_POWER_MONITOR_READINGS_TIMEOUT,
                    SystemHealthManagerError.COUNT);
            return Collections.emptyList();
        }
        return receiver.getTotalEnergyConsumed();
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
