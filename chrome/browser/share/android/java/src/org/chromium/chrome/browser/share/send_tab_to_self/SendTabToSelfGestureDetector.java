// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;
import java.util.function.Supplier;

/**
 * Detects a physical double-tap gesture of the phone against a surface. Uses the linear
 * acceleration sensor to detect sharp spikes in acceleration.
 */
@NullMarked
public class SendTabToSelfGestureDetector implements SensorEventListener {
    private static final String TAG = "STTSGestureDetector";

    // Time window for a double tap (in milliseconds).
    private static final long MIN_DELAY_MS = 100;
    private static final long MAX_DELAY_MS = 500;

    // Acceleration threshold values (m/s^2) for different sensitivity levels.
    // Higher sensitivity requires less force (lower threshold) to trigger.
    private static final float THRESHOLD_LOW_SENSITIVITY = 20.0f;
    private static final float THRESHOLD_MEDIUM_SENSITIVITY = 15.0f;
    private static final float THRESHOLD_HIGH_SENSITIVITY = 10.0f;

    private final SensorManager mSensorManager;
    private final @Nullable Sensor mAccelerometer;
    private final Supplier<Tab> mTabSupplier;
    private final Supplier<Profile> mProfileSupplier;
    // Threshold for considering an acceleration event as a "tap" (in m/s^2).
    // Dynamically configured based on the gesture sensitivity level.
    private final float mAccelerationThreshold;

    private long mLastTapTimeMs;
    private boolean mListening;

    public SendTabToSelfGestureDetector(
            Context context, Supplier<Tab> tabSupplier, Supplier<Profile> profileSupplier) {
        mSensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        mAccelerometer = mSensorManager.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION);
        mTabSupplier = tabSupplier;
        mProfileSupplier = profileSupplier;
        mAccelerationThreshold = getAccelerationThreshold();
    }

    public void start() {
        if (mAccelerometer != null && !mListening) {
            mSensorManager.registerListener(
                    this, mAccelerometer, SensorManager.SENSOR_DELAY_NORMAL);
            mListening = true;
            Log.d(TAG, "Gesture detector started");
        } else if (mAccelerometer == null) {
            Log.d(TAG, "Linear acceleration sensor not available");
        }
    }

    public void stop() {
        if (mListening) {
            mSensorManager.unregisterListener(this);
            mListening = false;
            Log.d(TAG, "Gesture detector stopped");
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() != Sensor.TYPE_LINEAR_ACCELERATION) {
            return;
        }
        onSensorValuesChanged(event.values, System.currentTimeMillis());
    }

    void onSensorValuesChanged(float[] values, long currentTimeMs) {
        // `values` is bound to be a 3-dimensional vector
        // (https://developer.android.com/reference/android/hardware/SensorEvent#sensor.type_linear_acceleration:).
        float x = values[0];
        float y = values[1];
        float z = values[2];

        // Calculate the magnitude of the acceleration vector.
        double magnitude = Math.sqrt(x * x + y * y + z * z);

        if (magnitude > mAccelerationThreshold) {
            long delay = currentTimeMs - mLastTapTimeMs;

            Log.d(TAG, "Peak detected: magnitude=%f, delay=%d", magnitude, delay);

            if (delay > MIN_DELAY_MS && delay < MAX_DELAY_MS) {
                Log.i(TAG, "Double tap detected against surface!");
                onGestureDetected();
                // Reset to avoid counting a third tap as a new double tap immediately
                mLastTapTimeMs = 0;
            } else {
                mLastTapTimeMs = currentTimeMs;
            }
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // Not needed for this implementation.
    }

    void onGestureDetected() {
        Tab tab = mTabSupplier.get();
        if (tab == null || tab.isOffTheRecord()) return;

        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        @EntryPointDisplayReason
        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                        profile, tab.getUrl().getSpec());
        // The model is starting up, ignore the gesture.
        if (displayReason == null) return;

        switch (displayReason) {
            case EntryPointDisplayReason.OFFER_SIGN_IN:
                Log.d(TAG, "User is not signed in for Send Tab to Self");
                return;
            case EntryPointDisplayReason.INFORM_NO_TARGET_DEVICE:
                Log.d(TAG, "No target devices found for Send Tab to Self");
                return;
            case EntryPointDisplayReason.OFFER_FEATURE:
                break;
        }

        List<TargetDeviceInfo> devices =
                SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(profile);
        // This should ideally have been caught by the switch case above, but in some rarest edge
        // cases, the device list may become empty between the check and now.
        if (devices.isEmpty()) {
            Log.d(TAG, "No target devices found for Send Tab to Self");
            return;
        }

        // `devices` is sorted based on the most recently used timestamp. Grab the device with the
        // freshest timestamp as the target.
        TargetDeviceInfo target = devices.get(0);
        SendTabToSelfAndroidBridge.sendTabToDevice(
                profile,
                tab.getWebContents(),
                target.cacheGuid,
                target.deviceName,
                tab.getUrl().getSpec(),
                tab.getTitle(),
                result -> Log.i(TAG, "Send tab result: %s", result));
    }

    private static float getAccelerationThreshold() {
        // Map gesture sensitivity parameters to acceleration thresholds (m/s^2).
        // - "low": harder to trigger, requires sharp tap (20 m/s^2)
        // - "medium" (or default): standard trigger threshold (15 m/s^2)
        // - "high": easier to trigger, requires light tap (10 m/s^2)
        String sensitivity =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.SEND_TAB_TO_SELF_GESTURE, "sensitivity");
        if ("low".equals(sensitivity)) {
            return THRESHOLD_LOW_SENSITIVITY;
        } else if ("high".equals(sensitivity)) {
            return THRESHOLD_HIGH_SENSITIVITY;
        }
        return THRESHOLD_MEDIUM_SENSITIVITY;
    }
}
