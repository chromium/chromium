// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordManagerUnavailableException;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;

/**
 * Tests that metric reporter correctly writes the histograms depending on the operation and error.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class PasswordCheckupClientMetricsRecorderTest {
    private static final String PASSWORD_CHECKUP_HISTOGRAM_BASE = "PasswordManager.PasswordCheckup";

    private String getSuffixForOperation(@PasswordCheckOperation int operation) {
        switch (operation) {
            case PasswordCheckOperation.RUN_PASSWORD_CHECKUP:
                return "RunPasswordCheckup";
            case PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT:
                return "GetBreachedCredentialsCount";
            case PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT:
                return "GetIntent";
            case PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT:
                return "GetWeakCredentialsCount";
            case PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT:
                return "GetReusedCredentialsCount";
            default:
                throw new AssertionError();
        }
    }

    private void checkHistogramsOnSuccess(@PasswordCheckOperation int operation) {
        final String nameWithSuffix =
                PASSWORD_CHECKUP_HISTOGRAM_BASE + "." + getSuffixForOperation(operation);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffix + ".Success", 1));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffix + ".Latency", 0));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".ErrorLatency"));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".Error"));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".ApiError"));
    }

    private void checkHistogramsOnFailure(
            @PasswordCheckOperation int operation, int errorCode, @Nullable Integer apiErrorCode) {
        final String nameWithSuffix =
                PASSWORD_CHECKUP_HISTOGRAM_BASE + "." + getSuffixForOperation(operation);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffix + ".Success", 0));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".Latency"));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".ErrorLatency", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffix + ".Error", errorCode));
        if (apiErrorCode != null) {
            assertEquals(
                    1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            nameWithSuffix + ".APIError", apiErrorCode));
        } else {
            assertEquals(
                    0,
                    RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffix + ".APIError"));
        }
    }

    @Test
    public void testRecordsSuccessHistogramForRunPasswordCheckup() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.RUN_PASSWORD_CHECKUP;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(null);
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsSuccessHistogramForGetBreachedCredentialsCount() {
        @PasswordCheckOperation
        int operation = PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(null);
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsSuccessHistogramForGetWeakCredentialsCount() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(null);
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsSuccessHistogramForGetReusedCredentialsCount() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(null);
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsSuccessHistogramForGetPasswordCheckupIntentIntent() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(null);
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsBasicErrorHistogramForRunPasswordCheckup() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.RUN_PASSWORD_CHECKUP;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(new PasswordManagerUnavailableException());
        checkHistogramsOnFailure(
                operation, CredentialManagerError.PASSWORD_MANAGER_NOT_AVAILABLE, null);
    }

    @Test
    public void testRecordsBasicErrorHistogramForGetBreachedCredentialsCount() {
        @PasswordCheckOperation
        int operation = PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(new PasswordManagerUnavailableException());
        checkHistogramsOnFailure(
                operation, CredentialManagerError.PASSWORD_MANAGER_NOT_AVAILABLE, null);
    }

    @Test
    public void testRecordsBasicErrorHistogramForGetWeakCredentialsCount() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(new PasswordManagerUnavailableException());
        checkHistogramsOnFailure(
                operation, CredentialManagerError.PASSWORD_MANAGER_NOT_AVAILABLE, null);
    }

    @Test
    public void testRecordsBasicErrorHistogramForGetReusedCredentialsCount() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(new PasswordManagerUnavailableException());
        checkHistogramsOnFailure(
                operation, CredentialManagerError.PASSWORD_MANAGER_NOT_AVAILABLE, null);
    }

    @Test
    public void testRecordsBasicErrorHistogramForGetPasswordCheckupIntentIntent() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(new PasswordManagerUnavailableException());
        checkHistogramsOnFailure(
                operation, CredentialManagerError.PASSWORD_MANAGER_NOT_AVAILABLE, null);
    }

    @Test
    public void testRecordsApiErrorHistogramForRunPasswordCheckup() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.RUN_PASSWORD_CHECKUP;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));
        checkHistogramsOnFailure(
                operation, CredentialManagerError.API_EXCEPTION, CommonStatusCodes.DEVELOPER_ERROR);
    }

    @Test
    public void testRecordsApiErrorHistogramForGetBreachedCredentialsCount() {
        @PasswordCheckOperation
        int operation = PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));
        checkHistogramsOnFailure(
                operation, CredentialManagerError.API_EXCEPTION, CommonStatusCodes.DEVELOPER_ERROR);
    }

    @Test
    public void testRecordsApiErrorHistogramForGetWeakCredentialsCount() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_WEAK_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));
        checkHistogramsOnFailure(
                operation, CredentialManagerError.API_EXCEPTION, CommonStatusCodes.DEVELOPER_ERROR);
    }

    @Test
    public void testRecordsApiErrorHistogramForGetReusedCredentialsCount() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_REUSED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));
        checkHistogramsOnFailure(
                operation, CredentialManagerError.API_EXCEPTION, CommonStatusCodes.DEVELOPER_ERROR);
    }

    @Test
    public void testRecordsApiErrorHistogramForGetPasswordCheckupIntentIntent() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR)));
        checkHistogramsOnFailure(
                operation, CredentialManagerError.API_EXCEPTION, CommonStatusCodes.DEVELOPER_ERROR);
    }

    @Test
    public void testRecordsOtherApiError() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(new NullPointerException());
        checkHistogramsOnFailure(operation, CredentialManagerError.OTHER_API_ERROR, null);
    }
}
