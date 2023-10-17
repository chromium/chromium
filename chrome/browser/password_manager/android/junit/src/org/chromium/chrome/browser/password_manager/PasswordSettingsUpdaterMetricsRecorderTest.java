// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.OptionalInt;

/**
 * Tests that metric reporter correctly writes the histograms depending on the function and setting.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class PasswordSettingsUpdaterMetricsRecorderTest {
    private static final String HISTOGRAM_NAME_BASE = "PasswordManager.PasswordSettings";

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
    }

    private void checkSuccessHistograms(String functionSuffix, String settingSuffix) {
        final String nameWithSuffixes =
                HISTOGRAM_NAME_BASE + "." + functionSuffix + "." + settingSuffix;
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Success", 1));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Latency", 0));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + ".ErrorLatency"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".ErrorCode"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".APIError1"));
    }

    private void checkFailureHistograms(
            String functionSuffix, String settingSuffix, int errorCode, OptionalInt apiErrorCode) {
        final String nameWithSuffixes =
                HISTOGRAM_NAME_BASE + "." + functionSuffix + "." + settingSuffix;
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Success", 0));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".Latency"));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + ".ErrorLatency", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + ".ErrorCode", errorCode));
        apiErrorCode.ifPresentOrElse(
                apiError ->
                        assertEquals(
                                1,
                                RecordHistogram.getHistogramValueCountForTesting(
                                        nameWithSuffixes + ".APIError1", apiError)),
                () ->
                        assertEquals(
                                0,
                                RecordHistogram.getHistogramTotalCountForTesting(
                                        nameWithSuffixes + ".APIError1")));
    }

    @Test
    public void testRecordsSuccessHistogramForGetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("GetSettingValue", "OfferToSavePasswords");
    }

    @Test
    public void testRecordsSuccessHistogramForGetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("GetSettingValue", "AutoSignIn");
    }

    @Test
    public void testRecordsErrorHistogramForGetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException = new Exception("Sample failure");

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "GetSettingValue",
                "OfferToSavePasswords",
                AndroidBackendErrorType.UNCATEGORIZED,
                OptionalInt.empty());
    }

    @Test
    public void testRecordsErrorHistogramForGetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException = new Exception("Sample failure");

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "GetSettingValue",
                "AutoSignIn",
                AndroidBackendErrorType.UNCATEGORIZED,
                OptionalInt.empty());
    }

    @Test
    public void testRecordsApiErrorHistogramForGetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "GetSettingValue",
                "OfferToSavePasswords",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }

    @Test
    public void testRecordsApiErrorHistogramForGetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "GetSettingValue",
                "AutoSignIn",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }

    @Test
    public void testRecordsSuccessHistogramForSetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("SetSettingValue", "OfferToSavePasswords");
    }

    @Test
    public void testRecordsSuccessHistogramForSetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("SetSettingValue", "AutoSignIn");
    }

    @Test
    public void testRecordsErrorHistogramForSetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException = new Exception("Sample failure");

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "SetSettingValue",
                "OfferToSavePasswords",
                AndroidBackendErrorType.UNCATEGORIZED,
                OptionalInt.empty());
    }

    @Test
    public void testRecordsErrorHistogramForSetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException = new Exception("Sample failure");

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "SetSettingValue",
                "AutoSignIn",
                AndroidBackendErrorType.UNCATEGORIZED,
                OptionalInt.empty());
    }

    @Test
    public void testRecordsApiErrorHistogramForSetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "SetSettingValue",
                "OfferToSavePasswords",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }

    @Test
    public void testRecordsApiErrorHistogramForSetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "SetSettingValue",
                "AutoSignIn",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }
}
