// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Mock tests for testing the functionality of {@link IncognitoReauthManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoReauthManagerTest {
    private IncognitoReauthManager mIncognitoReauthManager;

    @Mock private ReauthenticatorBridge mReauthenticatorBridgeMock;

    @Mock private IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallbackMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mIncognitoReauthManager = new IncognitoReauthManager(mReauthenticatorBridgeMock);
    }

    @Test
    @MediumTest
    public void
            testIncognitoReauthManager_WhenCantUseAuthentication_FiresCallbackWithNotPossible() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthNotPossible();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @MediumTest
    public void testIncognitoReauthManager_WhenFeatureDisabled_FiresCallbackWithNotPossible() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallbackMock);
        verify(mIncognitoReauthCallbackMock).onIncognitoReauthNotPossible();
        verifyNoMoreInteractions(mIncognitoReauthCallbackMock);
    }

    @Test
    @MediumTest
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
    @MediumTest
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
}
