// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.password_manager.PasswordSettingsUpdaterMetricsRecorder.getStoreType;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;

import java.util.Arrays;
import java.util.Collection;
import java.util.OptionalInt;

/**
 * Tests that metric reporter correctly writes the histograms depending on the function and setting.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class PasswordSettingsUpdaterMetricsRecorderTest {
    @Parameters
    public static Collection testCases() {
        return Arrays.asList("test@gmail.com", null);
    }

    private static final String HISTOGRAM_NAME_BASE = "PasswordManager.PasswordSettings";

    private String mStoreType;

    public PasswordSettingsUpdaterMetricsRecorderTest(String account) {
        mStoreType = getStoreType(account);
    }

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
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".Success", 1));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Latency", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".Latency", 0));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + ".ErrorLatency"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".ErrorLatency"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".ErrorCode"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".ErrorCode"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".APIError1"));
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".APIError1"));
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
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".Latency"));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + ".ErrorLatency", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".ErrorLatency", 0));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + ".ErrorCode", errorCode));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + "." + mStoreType + ".ErrorCode", errorCode));
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
        apiErrorCode.ifPresentOrElse(
                apiError ->
                        assertEquals(
                                1,
                                RecordHistogram.getHistogramValueCountForTesting(
                                        nameWithSuffixes + "." + mStoreType + ".APIError1",
                                        apiError)),
                () ->
                        assertEquals(
                                0,
                                RecordHistogram.getHistogramTotalCountForTesting(
                                        nameWithSuffixes + "." + mStoreType + ".APIError1")));
    }

    @Test
    public void testRecordsSuccessHistogramForGetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("GetSettingValue", "OfferToSavePasswords");
    }

    @Test
    public void testRecordsSuccessHistogramForGetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("GetSettingValue", "AutoSignIn");
    }

    @Test
    public void testRecordsErrorHistogramForGetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("SetSettingValue", "OfferToSavePasswords");
    }

    @Test
    public void testRecordsSuccessHistogramForSetAutoSignIn() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.AUTO_SIGN_IN,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("SetSettingValue", "AutoSignIn");
    }

    @Test
    public void testRecordsErrorHistogramForSetSavePasswords() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

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
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "SetSettingValue",
                "AutoSignIn",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }

    @Test
    public void testRecordsSuccessHistogramForGetBiometricReauthBeforePwdFilling() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING,
                        PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        metricsRecorder.recordMetrics(null);
        checkSuccessHistograms("SetSettingValue", "BiometricReauthBeforePwdFilling");
    }

    @Test
    public void testRecordsErrorHistogramForGetBiometricReauthBeforePwdFilling() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        Exception expectedException = new Exception("Sample failure");

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "GetSettingValue",
                "BiometricReauthBeforePwdFilling",
                AndroidBackendErrorType.UNCATEGORIZED,
                OptionalInt.empty());
    }

    @Test
    public void testRecordsApiErrorHistogramForBiometricReauthBeforePwdFilling() {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING,
                        PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX,
                        mStoreType);

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));

        metricsRecorder.recordMetrics(expectedException);
        checkFailureHistograms(
                "GetSettingValue",
                "BiometricReauthBeforePwdFilling",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }
}
