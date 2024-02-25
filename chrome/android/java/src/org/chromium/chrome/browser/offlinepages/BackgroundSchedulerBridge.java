// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.text.format.DateUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.device.DeviceConditions;

/**
 * Provides Java scheduling support from native offlining code as
 * well as JNI interface to tell native code to start processing
 * queued requests.
 */
@JNINamespace("offline_pages::android")
public class BackgroundSchedulerBridge {
    // Starts processing of one or more queued background requests.
    // Returns whether processing was started and that caller should
    // expect a callback (once processing has completed or terminated).
    // If processing was already active or not able to process for
    // some other reason, returns false and this calling instance will
    // not receive a callback.
    // TODO(dougarnett): consider adding policy check api to let caller
    //     separately determine if not allowed by policy.
    public static boolean startScheduledProcessing(
            DeviceConditions deviceConditions, Callback<Boolean> callback) {
        return BackgroundSchedulerBridgeJni.get()
                .startScheduledProcessing(
                        deviceConditions.isPowerConnected(),
                        deviceConditions.getBatteryPercentage(),
                        deviceConditions.getNetConnectionType(),
                        callback);
    }

    /**
     * Stops scheduled processing.
     * @return true, as it always expects to be rescheduled.
     */
    public static boolean stopScheduledProcessing() {
        BackgroundSchedulerBridgeJni.get().stopScheduledProcessing();
        return true;
    }

    @CalledByNative
    private static void schedule(TriggerConditions triggerConditions) {
        BackgroundScheduler.getInstance().schedule(triggerConditions);
    }

    @CalledByNative
    private static void backupSchedule(TriggerConditions triggerConditions, long delayInSeconds) {
        BackgroundScheduler.getInstance()
                .scheduleBackup(triggerConditions, DateUtils.SECOND_IN_MILLIS * delayInSeconds);
    }

    @CalledByNative
    private static void unschedule() {
        BackgroundScheduler.getInstance().cancel();
    }

    @CalledByNative
    private static boolean getPowerConditions() {
        return DeviceConditions.isCurrentlyPowerConnected(ContextUtils.getApplicationContext());
    }

    @CalledByNative
    private static int getBatteryConditions() {
        return DeviceConditions.getCurrentBatteryPercentage(ContextUtils.getApplicationContext());
    }

    @CalledByNative
    private static int getNetworkConditions() {
        return DeviceConditions.getCurrentNetConnectionType(ContextUtils.getApplicationContext());
    }

    /**
     * Used by native code to create and pass up Java object encapsulating the
     * trigger conditions.
     */
    @CalledByNative
    private static TriggerConditions createTriggerConditions(
            boolean requirePowerConnected,
            int minimumBatteryPercentage,
            boolean requireUnmeteredNetwork) {
        return new TriggerConditions(
                requirePowerConnected, minimumBatteryPercentage, requireUnmeteredNetwork);
    }

    @NativeMethods
    interface Natives {
        /** Instructs the native RequestCoordinator to start processing. */
        boolean startScheduledProcessing(
                boolean powerConnected,
                int batteryPercentage,
                int netConnectionType,
                Callback<Boolean> callback);

        /** Instructs the native RequestCoordinator to stop processing. */
        void stopScheduledProcessing();
    }
}
