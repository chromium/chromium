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
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper.PasswordCheckOperation;

import java.util.Optional;
import java.util.OptionalInt;

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
            @PasswordCheckOperation int operation, int errorCode, OptionalInt apiErrorCode) {
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
        apiErrorCode.ifPresentOrElse(
                apiError ->
                        assertEquals(
                                1,
                                RecordHistogram.getHistogramValueCountForTesting(
                                        nameWithSuffix + ".APIError", apiError)),
                () ->
                        assertEquals(
                                0,
                                RecordHistogram.getHistogramTotalCountForTesting(
                                        nameWithSuffix + ".APIError")));
    }

    @Test
    public void testRecordsSuccessHistogramForRunPasswordCheckup() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.RUN_PASSWORD_CHECKUP;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(Optional.empty());
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsSuccessHistogramForGetBreachedCredentialsCount() {
        @PasswordCheckOperation
        int operation = PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(Optional.empty());
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsSuccessHistogramForGetPasswordCheckupIntentIntent() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(Optional.empty());
        checkHistogramsOnSuccess(operation);
    }

    @Test
    public void testRecordsBasicErrorHistogramForRunPasswordCheckup() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.RUN_PASSWORD_CHECKUP;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                Optional.of(
                        new PasswordCheckBackendException("", CredentialManagerError.NO_CONTEXT)));
        checkHistogramsOnFailure(operation, CredentialManagerError.NO_CONTEXT, OptionalInt.empty());
    }

    @Test
    public void testRecordsBasicErrorHistogramForGetBreachedCredentialsCount() {
        @PasswordCheckOperation
        int operation = PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                Optional.of(
                        new PasswordCheckBackendException("", CredentialManagerError.NO_CONTEXT)));
        checkHistogramsOnFailure(operation, CredentialManagerError.NO_CONTEXT, OptionalInt.empty());
    }

    @Test
    public void testRecordsBasicErrorHistogramForGetPasswordCheckupIntentIntent() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                Optional.of(
                        new PasswordCheckBackendException("", CredentialManagerError.NO_CONTEXT)));
        checkHistogramsOnFailure(operation, CredentialManagerError.NO_CONTEXT, OptionalInt.empty());
    }

    @Test
    public void testRecordsApiErrorHistogramForRunPasswordCheckup() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.RUN_PASSWORD_CHECKUP;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                Optional.of(new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR))));
        checkHistogramsOnFailure(
                operation,
                CredentialManagerError.API_EXCEPTION,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
    }

    @Test
    public void testRecordsApiErrorHistogramForGetBreachedCredentialsCount() {
        @PasswordCheckOperation
        int operation = PasswordCheckOperation.GET_BREACHED_CREDENTIALS_COUNT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                Optional.of(new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR))));
        checkHistogramsOnFailure(
                operation,
                CredentialManagerError.API_EXCEPTION,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
    }

    @Test
    public void testRecordsApiErrorHistogramForGetPasswordCheckupIntentIntent() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(
                Optional.of(new ApiException(new Status(CommonStatusCodes.DEVELOPER_ERROR))));
        checkHistogramsOnFailure(
                operation,
                CredentialManagerError.API_EXCEPTION,
                OptionalInt.of(CommonStatusCodes.DEVELOPER_ERROR));
    }

    @Test
    public void testRecordsOtherApiError() {
        @PasswordCheckOperation int operation = PasswordCheckOperation.GET_PASSWORD_CHECKUP_INTENT;
        PasswordCheckupClientMetricsRecorder metricsRecorder =
                new PasswordCheckupClientMetricsRecorder(operation);
        metricsRecorder.recordMetrics(Optional.of(new NullPointerException()));
        checkHistogramsOnFailure(
                operation, CredentialManagerError.OTHER_API_ERROR, OptionalInt.empty());
    }
}
