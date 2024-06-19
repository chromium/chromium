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
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;

import java.util.Optional;

/** Tests that settings updater callbacks invoke the right native callbacks. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordSettingsUpdaterReceiverBridgeTest {

    private static final long sFakeNativePointer = 7;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private PasswordSettingsUpdaterReceiverBridge.Natives mReceiverBridgeJniMock;
    @Mock private PasswordSettingsUpdaterMetricsRecorder mMetricsRecorderMock;

    private PasswordSettingsUpdaterReceiverBridge mReceiverBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                PasswordSettingsUpdaterReceiverBridgeJni.TEST_HOOKS, mReceiverBridgeJniMock);
        mReceiverBridge = new PasswordSettingsUpdaterReceiverBridge(sFakeNativePointer);
    }

    @Test
    public void testOnSettingValueFetchedCalled() {
        mReceiverBridge.onSettingValueFetched(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                Optional.of(true),
                mMetricsRecorderMock);
        verify(mReceiverBridgeJniMock)
                .onSettingValueFetched(
                        sFakeNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
        verify(mMetricsRecorderMock).recordMetrics(isNull());
    }

    @Test
    public void testOnSettingValueAbsentCalled() {
        mReceiverBridge.onSettingValueFetched(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                Optional.empty(),
                mMetricsRecorderMock);
        verify(mReceiverBridgeJniMock)
                .onSettingValueAbsent(
                        sFakeNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        verify(mMetricsRecorderMock).recordMetrics(isNull());
    }

    @Test
    public void testOnSettingFetchingErrorCalled() {
        Exception expectedException = new ApiException(new Status(CommonStatusCodes.NETWORK_ERROR));
        mReceiverBridge.handleFetchingException(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException,
                mMetricsRecorderMock);

        verify(mReceiverBridgeJniMock)
                .onSettingFetchingError(
                        sFakeNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.NETWORK_ERROR);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testOnSuccessfulSettingChangeCalled() {
        mReceiverBridge.onSettingValueSet(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, mMetricsRecorderMock);
        verify(mReceiverBridgeJniMock)
                .onSuccessfulSettingChange(
                        sFakeNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        verify(mMetricsRecorderMock).recordMetrics(isNull());
    }

    @Test
    public void testOnFailedSettingChangeCalled() {
        Exception expectedException = new ApiException(new Status(CommonStatusCodes.NETWORK_ERROR));
        mReceiverBridge.handleSettingException(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException,
                mMetricsRecorderMock);

        verify(mReceiverBridgeJniMock)
                .onFailedSettingChange(
                        sFakeNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.NETWORK_ERROR);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableFetchingError()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException =
                new ResolvableApiException(
                        new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        mReceiverBridge.handleFetchingException(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException,
                mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock)
                .onSettingFetchingError(
                        sFakeNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableSettingError()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException =
                new ResolvableApiException(
                        new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        mReceiverBridge.handleSettingException(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException,
                mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock)
                .onFailedSettingChange(
                        sFakeNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableFetchingErrorIfDestroyed()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException =
                new ResolvableApiException(
                        new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        // Simulate native bridge destruction. No resolution or native bridge interaction should
        // happen after this.
        mReceiverBridge.destroyForTesting();

        mReceiverBridge.handleFetchingException(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException,
                mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock, never())
                .onSettingFetchingError(
                        sFakeNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }

    @Test
    public void testResolutionNotLaunchedOnResolvableSettingErrorIfDestroyed()
            throws PendingIntent.CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception expectedException =
                new ResolvableApiException(
                        new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));

        // Simulate native bridge destruction. No resolution or native bridge interaction should
        // happen after this.
        mReceiverBridge.destroyForTesting();

        mReceiverBridge.handleSettingException(
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                expectedException,
                mMetricsRecorderMock);

        verify(pendingIntentMock, never()).send();
        verify(mReceiverBridgeJniMock, never())
                .onFailedSettingChange(
                        sFakeNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
        verify(mMetricsRecorderMock).recordMetrics(expectedException);
    }
}
