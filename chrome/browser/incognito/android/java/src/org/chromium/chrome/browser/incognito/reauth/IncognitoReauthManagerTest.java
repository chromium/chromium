// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthAvailabilityCheckerBridge;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Mock tests for testing the functionality of {@link IncognitoReauthManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoReauthManagerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private IncognitoReauthManager mIncognitoReauthManager;

    @Mock private ReauthenticatorBridge mReauthenticatorBridgeMock;

    @Mock private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;

    @Before
    public void setUp() {
        mIncognitoReauthManager = new IncognitoReauthManager(mReauthenticatorBridgeMock);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DEVICE_REAUTH_REFACTOR)
    public void
            testIncognitoReauthManager_WhenCantUseAuthentication_FiresCallbackWithNotPossible() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthNotPossible();
        verify(mReauthenticatorBridgeMock, never()).reauthenticate(any());
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    public void testIncognitoReauthManager_WhenFeatureDisabled_FiresCallbackWithNotPossible() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthNotPossible();
        verify(mReauthenticatorBridgeMock, never()).reauthenticate(any());
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DEVICE_REAUTH_REFACTOR)
    public void
            testIncognitoReauthManager_WhenReauthenticationSucceeded_FiresCallbackWithSuccess() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        doAnswer(
                        invocationOnMock -> {
                            Callback<Boolean> callback = invocationOnMock.getArgument(0);
                            callback.onResult(true);
                            return true;
                        })
                .when(mReauthenticatorBridgeMock)
                .reauthenticate(notNull());

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthSuccess();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DEVICE_REAUTH_REFACTOR)
    public void testIncognitoReauthManager_WhenReauthenticationFailed_FiresCallbackWithFailed() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        doAnswer(
                        invocationOnMock -> {
                            Callback<Boolean> callback = invocationOnMock.getArgument(0);
                            callback.onResult(false);
                            return false;
                        })
                .when(mReauthenticatorBridgeMock)
                .reauthenticate(notNull());

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthFailure();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEVICE_REAUTH_REFACTOR)
    public void testIncognitoReauthManager_refactorFlagEnabled_biometricsUnavailable() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        DeviceAuthAvailabilityCheckerBridge.setBiometricAvailabilityStatusForTesting(
                BiometricStatus.UNAVAILABLE);

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthNotPossible();
        verify(mReauthenticatorBridgeMock, never()).getBiometricAvailabilityStatus();
        verifyNoInteractions(mReauthenticatorBridgeMock);
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEVICE_REAUTH_REFACTOR)
    public void testIncognitoReauthManager_refactorFlagEnabled_biometricsAvailable() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        DeviceAuthAvailabilityCheckerBridge.setBiometricAvailabilityStatusForTesting(
                BiometricStatus.BIOMETRICS_AVAILABLE);
        doAnswer(
                        invocationOnMock -> {
                            Callback<Boolean> callback = invocationOnMock.getArgument(0);
                            callback.onResult(true);
                            return true;
                        })
                .when(mReauthenticatorBridgeMock)
                .reauthenticate(notNull());

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthSuccess();
        verify(mReauthenticatorBridgeMock, never()).getBiometricAvailabilityStatus();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DEVICE_REAUTH_REFACTOR)
    public void testIncognitoReauthManager_refactorFlagEnabled_reauthenticationFailed() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        DeviceAuthAvailabilityCheckerBridge.setBiometricAvailabilityStatusForTesting(
                BiometricStatus.BIOMETRICS_AVAILABLE);
        doAnswer(
                        invocationOnMock -> {
                            Callback<Boolean> callback = invocationOnMock.getArgument(0);
                            callback.onResult(false);
                            return false;
                        })
                .when(mReauthenticatorBridgeMock)
                .reauthenticate(notNull());

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthFailure();
        verify(mReauthenticatorBridgeMock, never()).getBiometricAvailabilityStatus();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }
}
