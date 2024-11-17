// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.os.SystemClock;

import org.jni_zero.CalledByNative;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Records metrics related to saving passwords in CCTs, such as whether a PendingIntent */
public class CctPasswordSavingMetricsRecorderBridge
        implements UnownedUserData, WindowAndroid.ActivityStateObserver {
    public static final UnownedUserDataKey<CctPasswordSavingMetricsRecorderBridge> KEY =
            new UnownedUserDataKey<>(CctPasswordSavingMetricsRecorderBridge.class);

    static final String SUBMISSION_TO_REDIRECT_TIME_HISTOGRAM =
            "PasswordManager.CctFormSubmissionToRedirectTime";
    static final String SUBMISSION_TO_ACTIVITY_STOP_TIME_HISTOGRAM =
            "PasswordManager.CctFormSubmissionToActivityStopTime";

    static final String REDIRECT_TO_ACTIVITY_STOP_TIME_HISTOGRAM =
            "PasswordManager.CctRedirectToActivityStopTime";

    private final WeakReference<WindowAndroid> mWeakWindowAndroid;
    private Long mStartTimeMs;
    private Long mRedirectTimeMs;

    @CalledByNative
    CctPasswordSavingMetricsRecorderBridge(WindowAndroid windowAndroid) {
        mWeakWindowAndroid = new WeakReference<>(windowAndroid);
    }

    @CalledByNative
    void onPotentialSaveFormSubmitted() {
        mStartTimeMs = SystemClock.elapsedRealtime();
        mRedirectTimeMs = null;

        WindowAndroid windowAndroid = mWeakWindowAndroid.get();
        if (windowAndroid == null) {
            mStartTimeMs = null;
            return;
        }

        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), this);
        windowAndroid.addActivityStateObserver(this);
    }

    @CalledByNative
    void destroy() {
        // It's possible that the recorder gets destroyed before it records anything if
        // the metric is no longer relevant (e.g. the successful form submission actually
        // resulted in a save prompt being shown, rather than a redirect to the app).
        WindowAndroid windowAndroid = mWeakWindowAndroid.get();
        if (windowAndroid == null) {
            return;
        }
        KEY.detachFromHost(windowAndroid.getUnownedUserDataHost());
        windowAndroid.removeActivityStateObserver(this);
    }

    public void onExternalNavigation() {
        if (mStartTimeMs == null) {
            return;
        }
        mRedirectTimeMs = SystemClock.elapsedRealtime();
        RecordHistogram.recordTimesHistogram(
                SUBMISSION_TO_REDIRECT_TIME_HISTOGRAM, mRedirectTimeMs - mStartTimeMs);
    }

    @Override
    public void onActivityStopped() {
        if (mStartTimeMs == null) {
            return;
        }

        if (mRedirectTimeMs == null) {
            // Only an Activity stop that is a direct result of a password form submission
            // which results in a redirect to another app is relevant for these metrics.
            return;
        }
        RecordHistogram.recordTimesHistogram(
                SUBMISSION_TO_ACTIVITY_STOP_TIME_HISTOGRAM,
                SystemClock.elapsedRealtime() - mStartTimeMs);
        RecordHistogram.recordTimesHistogram(
                REDIRECT_TO_ACTIVITY_STOP_TIME_HISTOGRAM,
                SystemClock.elapsedRealtime() - mRedirectTimeMs);
    }
}
