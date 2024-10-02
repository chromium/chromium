// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;

import java.util.Optional;

/**
 * Records metrics for an asynchronous job or a series of jobs. The job is expected to have started
 * when the {@link PasswordCheckupClientMetricsRecorder} instance is created. Latency is reported
 * in {@link #recordMetrics(Optional<Exception>) recordMetrics} under that assumption.
 */
class PasswordCheckupClientMetricsRecorder {
    private static final String PASSWORD_CHECKUP_HISTOGRAM_BASE = "PasswordManager.PasswordCheckup";
    private static final String RUN_PASSWORD_CHECKUP_OPERATION_SUFFIX = "RunPasswordCheckup";
    private static final String GET_BREACHED_CREDENTIALS_COUNT_OPERATION_SUFFIX =
            "GetBreachedCredentialsCount";
    private static final String GET_PASSWORD_CHECKUP_INTENT_OPERATION_SUFFIX = "GetIntent";

    private final @PasswordCheckOperation int mOperation;
    private final long mStartTimeMs;

    PasswordCheckupClientMetricsRecorder(@PasswordCheckOperation int operation) {
        mOperation = operation;
        mStartTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * Records the metrics depending on {@link Exception} provided.
     * Success metric is always reported. Latency is reported separately for
     * successful and failed operations.
     * Error codes are reported for failed operations only. For GMS errors, API error code is
     * additionally reported.
     *
     * @param exception {@link Optional<Exception>} instance corresponding to the occurred error
     */
    void recordMetrics(Optional<Exception> exception) {
        RecordHistogram.recordBooleanHistogram(getHistogramName("Success"), !exception.isPresent());
        RecordHistogram.recordTimesHistogram(
                getHistogramName(exception.isPresent() ? "ErrorLatency" : "Latency"),
                SystemClock.elapsedRealtime() - mStartTimeMs);
        if (exception.isPresent()) {
            recordErrorMetrics(exception.get());
        }
    }

    private void recordErrorMetrics(Exception exception) {
        @CredentialManagerError
        int error = PasswordManagerAndroidBackendUtil.getPasswordCheckupBackendError(exception);
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramName("Error"), error, CredentialManagerError.COUNT);

        if (error == CredentialManagerError.API_EXCEPTION) {
            int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
            RecordHistogram.recordSparseHistogram(getHistogramName("APIError"), apiErrorCode);
        }
    }

    private String getHistogramName(String metric) {
        return PASSWORD_CHECKUP_HISTOGRAM_BASE
                + "."
                + getSuffixForOperation(mOperation)
                + "."
                + metric;
    }

    /** Returns histogram suffix (infix in real) for a given operation. */
    private static String getSuffixForOperation(@PasswordCheckOperation int operation) {
        switch (operation) {
            case PasswordCheckOperation.RUN_PASSWORD_CHECKUP:
                return RUN_PASSWORD_CHECKUP_OPERATION_SUFFIX;
            case PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT:
                return GET_BREACHED_CREDENTIALS_COUNT_OPERATION_SUFFIX;
            case PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT:
                return GET_PASSWORD_CHECKUP_INTENT_OPERATION_SUFFIX;
            default:
                throw new AssertionError("All operations need to be handled.");
        }
    }
}
