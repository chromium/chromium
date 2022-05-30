// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import com.google.common.base.Optional;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;

/**
 * Records metrics for an asynchronous job or a series of jobs. The job is expected to have started
 * when the {@link PasswordCheckupClientMetricsRecorder} instance is created. Histograms are
 * recorded in {@link #recordMetrics(Exception) recordMetrics}.
 *
 * TODO(crbug.com/1326151): Report success and latency metrics.
 */
class PasswordCheckupClientMetricsRecorder {
    private static final String PASSWORD_CHECKUP_HISTOGRAM_BASE = "PasswordManager.PasswordCheckup";
    private static final String RUN_PASSWORD_CHECKUP_OPERATION_SUFFIX = "RunPasswordCheckup";
    private static final String GET_BREACHED_CREDENTIALS_COUNT_OPERATION_SUFFIX =
            "GetBreachedCredentialsCount";
    private static final String GET_PASSWORD_CHECKUP_INTENT_OPERATION_SUFFIX = "GetIntent";

    private final @PasswordCheckOperation int mOperation;

    PasswordCheckupClientMetricsRecorder(@PasswordCheckOperation int operation) {
        mOperation = operation;
    }

    /**
     * Records the metrics depending on {@link Exception} provided.
     * Error codes are reported for all errors. For GMS errors, API error code is additionally
     * reported.
     *
     * @param exception {@link Exception} instance corresponding to the occurred error
     */
    void recordMetrics(Optional<Exception> exception) {
        // TODO(crbug.com/1326506): Record success and latency metrics.
        if (exception.isPresent()) {
            recordErrorMetrics(exception.get());
        }
    }

    private void recordErrorMetrics(Exception exception) {
        @CredentialManagerError
        int error = PasswordManagerAndroidBackendUtil.getPasswordCheckupBackendError(exception);
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramName("Error"), error, CredentialManagerError.COUNT);

        if (error == CredentialManagerError.API_ERROR) {
            int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
            RecordHistogram.recordSparseHistogram(getHistogramName("APIError"), apiErrorCode);
        }
    }

    private String getHistogramName(String metric) {
        return PASSWORD_CHECKUP_HISTOGRAM_BASE + "." + getSuffixForOperation(mOperation) + "."
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
