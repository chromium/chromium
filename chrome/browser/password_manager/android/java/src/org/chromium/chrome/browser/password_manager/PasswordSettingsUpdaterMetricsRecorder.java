// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerSetting.AUTO_SIGN_IN;
import static org.chromium.chrome.browser.password_manager.PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS;

import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Records metrics for an asynchronous job or a series of jobs. The job is expected to have started
 * when the {@link PasswordSettingsUpdaterMetricsRecorder} instance is created. Latency is reported
 * in {@link #recordMetrics(Exception) recordMetrics} under that assumption.
 */
class PasswordSettingsUpdaterMetricsRecorder {
    private static final String PASSWORD_SETTINGS_HISTOGRAM_BASE =
            "PasswordManager.PasswordSettings";
    static final String SET_VALUE_FUNCTION_SUFFIX = "SetSettingValue";
    static final String GET_VALUE_FUNCTION_SUFFIX = "GetSettingValue";

    private final @PasswordManagerSetting int mSetting;
    private final String mFunctionSuffix;
    private final long mStartTimeMs;

    PasswordSettingsUpdaterMetricsRecorder(
            @PasswordManagerSetting int setting, String functionSuffix) {
        mSetting = setting;
        mFunctionSuffix = functionSuffix;
        mStartTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * Records the metrics depending on {@link Exception} provided.
     * Success metric is always reported. Latency is reported separately for
     * successful and failed operations.
     * Error codes are reported for failed operations only.
     *
     * @param exception {@link Exception} instance corresponding to the occurred error
     */
    void recordMetrics(@Nullable Exception exception) {
        RecordHistogram.recordBooleanHistogram(getHistogramName("Success"), exception == null);
        RecordHistogram.recordTimesHistogram(
                getHistogramName(exception == null ? "Latency" : "ErrorLatency"),
                SystemClock.elapsedRealtime() - mStartTimeMs);
        if (exception != null) {
            reportErrorMetrics(exception);
        }
    }

    /** Returns histogram suffix (infix in real) for a given setting. */
    private static String getSuffixForSetting(@PasswordManagerSetting int setting) {
        switch (setting) {
            case OFFER_TO_SAVE_PASSWORDS:
                return "OfferToSavePasswords";
            case AUTO_SIGN_IN:
                return "AutoSignIn";
            default:
                assert false : "All settings need to be handled.";
                return "";
        }
    }

    private String getHistogramName(String metric) {
        return PASSWORD_SETTINGS_HISTOGRAM_BASE
                + "."
                + mFunctionSuffix
                + "."
                + getSuffixForSetting(mSetting)
                + "."
                + metric;
    }

    private void reportErrorMetrics(Exception exception) {
        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramName("ErrorCode"), error, AndroidBackendErrorType.MAX_VALUE + 1);
        if (error == AndroidBackendErrorType.EXTERNAL_ERROR) {
            int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
            RecordHistogram.recordSparseHistogram(getHistogramName("APIError1"), apiErrorCode);
        }
    }

    String getFunctionSuffixForTesting() {
        return mFunctionSuffix;
    }

    @VisibleForTesting
    @PasswordManagerSetting
    int getSettingForTesting() {
        return mSetting;
    }
}
