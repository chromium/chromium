// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.DeviceConditions;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper Class for Click to Call UMA Collection.
 */
public class ClickToCallUma {
    // Keep in sync with the ClickToCallDeviceState enum in enums.xml.
    @IntDef({ClickToCallDeviceState.SCREEN_OFF_BACKGROUND,
            ClickToCallDeviceState.SCREEN_ON_BACKGROUND,
            ClickToCallDeviceState.SCREEN_OFF_FOREGROUND,
            ClickToCallDeviceState.SCREEN_ON_FOREGROUND})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ClickToCallDeviceState {
        int SCREEN_OFF_BACKGROUND = 0;
        int SCREEN_ON_BACKGROUND = 1;
        int SCREEN_OFF_FOREGROUND = 2;
        int SCREEN_ON_FOREGROUND = 3;
        int NUM_ENTRIES = 4;
    }

    private static @ClickToCallDeviceState int getDeviceState(Context context) {
        boolean isScreenOn = DeviceConditions.isCurrentlyScreenOnAndUnlocked(context);
        boolean isInForeground = ApplicationStatus.hasVisibleActivities();

        if (isInForeground) {
            return isScreenOn ? ClickToCallDeviceState.SCREEN_ON_FOREGROUND
                              : ClickToCallDeviceState.SCREEN_OFF_FOREGROUND;
        } else {
            return isScreenOn ? ClickToCallDeviceState.SCREEN_ON_BACKGROUND
                              : ClickToCallDeviceState.SCREEN_OFF_BACKGROUND;
        }
    }

    /**
     * Listens for outgoing phone calls for TIMEOUT_MS and adds metrics if there was one within that
     * time frame. This is only used to measure successful usages of the Click to Call feature and
     * does not contain any data other than the time it took from opening the dialer to a call being
     * initiated.
     */
    private static final class CallMetricListener extends PhoneStateListener {
        // Maximum time we wait for an outgoing call to be made.
        private static final long TIMEOUT_MS = 30000;

        // Current instance of a registered listener, or null if none running.
        private static CallMetricListener sListener;

        private final Handler mHandler = new Handler();
        private final Runnable mTimeoutRunnable = this::stopMetric;
        private final long mDialerOpenTime = SystemClock.uptimeMillis();
        private final TelephonyManager mTelephonyManager;

        private CallMetricListener(Context context) {
            mTelephonyManager =
                    (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
            mTelephonyManager.listen(this, PhoneStateListener.LISTEN_CALL_STATE);
            mHandler.postDelayed(mTimeoutRunnable, TIMEOUT_MS);
        }

        @Override
        public void onCallStateChanged(int state, String number) {
            // Note: |number| will always be empty as we don't have permissions to read it.
            if (sListener != this || state != TelephonyManager.CALL_STATE_OFFHOOK) return;

            // Record successful Click to Call journey and the time it took to initiate a call.
            recordCallMade(SystemClock.uptimeMillis() - mDialerOpenTime);

            stopMetric();
        }

        @MainThread
        public static void startMetric(Context context) {
            // We do not have READ_PHONE_STATE permissions which are required pre-M.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
            if (sListener != null) sListener.stopMetric();
            sListener = new CallMetricListener(context);
        }

        @MainThread
        private void stopMetric() {
            if (sListener != this) return;
            mHandler.removeCallbacks(mTimeoutRunnable);
            mTelephonyManager.listen(this, PhoneStateListener.LISTEN_NONE);
            sListener = null;
        }
    }

    private static void recordCallMade(long timeFromDialerToCallMs) {
        new CachedMetrics.MediumTimesHistogramSample("Sharing.ClickToCallPhoneCall")
                .record(timeFromDialerToCallMs);
    }

    public static void recordDialerShown(boolean emptyPhoneNumber) {
        CallMetricListener.startMetric(ContextUtils.getApplicationContext());
        new CachedMetrics.BooleanHistogramSample("Sharing.ClickToCallDialIntent")
                .record(emptyPhoneNumber);
    }

    public static void recordDialerPresent(boolean isDialerPresent) {
        new CachedMetrics.BooleanHistogramSample("Sharing.ClickToCallDialerPresent")
                .record(isDialerPresent);
    }

    public static void recordMessageReceived() {
        new CachedMetrics
                .EnumeratedHistogramSample(
                        "Sharing.ClickToCallReceiveDeviceState", ClickToCallDeviceState.NUM_ENTRIES)
                .record(getDeviceState(ContextUtils.getApplicationContext()));
    }
}
