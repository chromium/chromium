// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.PendingIntent;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.ResolvableApiException;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.Optional;

/**
 * Tests that settings updater callbacks invoke the right native callbacks.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
public class PasswordSettingsUpdaterReceiverBridgeTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final long sDummyNativePointer = 7;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private PasswordSettingsUpdaterReceiverBridge.Natives mReceiverBridgeJniMock;
    @Mock
    private PasswordSettingsUpdaterMetricsRecorder mMetricsRecorderMock;

    private PasswordSettingsUpdaterReceiverBridge mReceiverBridge;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                PasswordSettingsUpdaterReceiverBridgeJni.TEST_HOOKS, mReceiverBridgeJniMock);
        mReceiverBridge = new PasswordSettingsUpdaterReceiverBridge(sDummyNativePointer);
    }

    @Test
    public void testOnSettingValueFetchedCalled() {
        mReceiverBridge.onSettingValueFetched(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                Optional.of(true), mMetricsRecorderMock);
        verify(mReceiverBridgeJniMock)
                .onSettingValueFetched(
                        sDummyNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
        verify(mMetricsRecorderMock).recordMetrics(isNull());
    }

    @Test
    public void testOnSettingValueAbsentCalled() {
        mReceiverBridge.onSettingValueFetched(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                Optional.empty(), mMetricsRecorderMock);
        verify(mReceiverBridgeJniMock)
                .onSettingValueAbsent(
                        sDummyNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        verify(mMetricsRecorderMock).recordMetrics(isNull());
    }

    @Test
    public void testOnSettingFetchingErrorCalled() {
        Exception expectedException = new ApiException(new Status(CommonStatusCodes.NETWORK_ERROR));
        mReceiverBridge.handleFetchingException(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException, mMetricsRecorderMock);

        verify(mReceiverBridgeJniMock)
                .onSettingFetchingError(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR, CommonStatusCodes.NETWORK_ERROR);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testOnSuccessfulSettingChangeCalled() {
        mReceiverBridge.onSettingValueSet(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, mMetricsRecorderMock);
        verify(mReceiverBridgeJniMock)
                .onSuccessfulSettingChange(
                        sDummyNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        verify(mMetricsRecorderMock).recordMetrics(isNull());
    }

    @Test
    public void testOnFailedSettingChangeCalled() {
        Exception expectedException = new ApiException(new Status(CommonStatusCodes.NETWORK_ERROR));
        mReceiverBridge.handleSettingException(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException, mMetricsRecorderMock);

        verify(mReceiverBridgeJniMock)
                .onFailedSettingChange(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR, CommonStatusCodes.NETWORK_ERROR);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableFetchingError()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException = new ResolvableApiException(
                new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        mReceiverBridge.handleFetchingException(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException, mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock)
                .onSettingFetchingError(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableSettingError()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException = new ResolvableApiException(
                new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        mReceiverBridge.handleSettingException(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException, mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock)
                .onFailedSettingChange(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableFetchingErrorIfDestroyed()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException = new ResolvableApiException(
                new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        // Simulate native bridge destruction. No resolution or native bridge interaction should
        // happen after this.
        mReceiverBridge.destroyForTesting();

        mReceiverBridge.handleFetchingException(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException, mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock, never())
                .onSettingFetchingError(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableSettingErrorIfDestroyed()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException = new ResolvableApiException(
                new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        // Simulate native bridge destruction. No resolution or native bridge interaction should
        // happen after this.
        mReceiverBridge.destroyForTesting();

        mReceiverBridge.handleSettingException(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException, mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock, never())
                .onFailedSettingChange(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }
}
