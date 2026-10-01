// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.KeyguardManager;
import android.content.Context;

import androidx.biometric.BiometricManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadows.ShadowKeyguardManager;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {DeviceAuthenticatorControllerTest.ShadowBiometricManager.class})
public class DeviceAuthenticatorControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private DeviceAuthenticatorController.Delegate mDelegate;

    private DeviceAuthenticatorController mController;

    @Implements(BiometricManager.class)
    public static class ShadowBiometricManager {
        private static int sCanAuthenticateResult = BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED;

        public static void setCanAuthenticateResult(int result) {
            sCanAuthenticateResult = result;
        }

        @Implementation
        public int canAuthenticate(int authenticators) {
            return sCanAuthenticateResult;
        }

        @Resetter
        public static void reset() {
            sCanAuthenticateResult = BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED;
        }
    }

    @Before
    public void setUp() {
        mController = new DeviceAuthenticatorController(/* activity= */ null, mDelegate);
    }

    private ShadowKeyguardManager getShadowKeyguardManager() {
        return shadowOf(
                (KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE));
    }

    @Test
    public void testCanAuthenticateWithBiometricWithoutActivity() {
        ShadowBiometricManager.setCanAuthenticateResult(
                BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED);
        assertEquals(
                BiometricsAvailability.NOT_ENROLLED, mController.canAuthenticateWithBiometric());

        ShadowBiometricManager.setCanAuthenticateResult(
                BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE);
        assertEquals(
                BiometricsAvailability.NO_HARDWARE, mController.canAuthenticateWithBiometric());

        ShadowBiometricManager.setCanAuthenticateResult(BiometricManager.BIOMETRIC_SUCCESS);
        getShadowKeyguardManager().setIsDeviceSecure(true);
        assertEquals(BiometricsAvailability.AVAILABLE, mController.canAuthenticateWithBiometric());

        getShadowKeyguardManager().setIsDeviceSecure(false);
        assertEquals(
                BiometricsAvailability.AVAILABLE_NO_FALLBACK,
                mController.canAuthenticateWithBiometric());
    }

    @Test
    public void testScreenLockIsDetectedWithoutActivity() {
        getShadowKeyguardManager().setIsDeviceSecure(true);

        assertTrue(mController.canAuthenticateWithBiometricOrScreenLock());
    }

    @Test
    public void testNoScreenLockIsDetectedWithoutActivity() {
        getShadowKeyguardManager().setIsDeviceSecure(false);

        assertFalse(mController.canAuthenticateWithBiometricOrScreenLock());
    }

    @Test
    public void testAuthenticateWithoutActivityFailsAsynchronously() {
        mController.authenticate();

        // The delegate must not be invoked re-entrantly: the native counterpart is still inside
        // DeviceAuthenticatorBridgeImpl::Authenticate at this point, and the embedder is allowed to
        // destroy the authenticator from within the completion callback.
        verify(mDelegate, never()).onAuthenticationCompleted(anyInt());

        ShadowLooper.runUiThreadTasks();

        verify(mDelegate).onAuthenticationCompleted(DeviceAuthUIResult.FAILED_NO_ACTIVITY);
    }

    @Test
    public void testCancelWithoutOngoingAuthenticationDoesNothing() {
        mController.cancel();

        ShadowLooper.runUiThreadTasks();

        verify(mDelegate, never()).onAuthenticationCompleted(anyInt());
    }
}
