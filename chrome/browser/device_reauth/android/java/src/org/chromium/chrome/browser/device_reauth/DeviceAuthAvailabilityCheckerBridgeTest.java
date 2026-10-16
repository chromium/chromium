// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.app.KeyguardManager;
import android.content.Context;

import androidx.biometric.BiometricManager;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowKeyguardManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link DeviceAuthAvailabilityCheckerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {DeviceAuthenticatorControllerTest.ShadowBiometricManager.class})
public class DeviceAuthAvailabilityCheckerBridgeTest {

    private ShadowKeyguardManager getShadowKeyguardManager() {
        return shadowOf(
                (KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE));
    }

    @Test
    public void testGetBiometricAvailabilityStatus_biometricsAvailable() {
        DeviceAuthenticatorControllerTest.ShadowBiometricManager.setCanAuthenticateResult(
                BiometricManager.BIOMETRIC_SUCCESS);
        getShadowKeyguardManager().setIsDeviceSecure(true);

        assertEquals(
                BiometricStatus.BIOMETRICS_AVAILABLE,
                DeviceAuthAvailabilityCheckerBridge.getBiometricAvailabilityStatus());
    }

    @Test
    public void testGetBiometricAvailabilityStatus_onlyLskfAvailable() {
        DeviceAuthenticatorControllerTest.ShadowBiometricManager.setCanAuthenticateResult(
                BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED);
        getShadowKeyguardManager().setIsDeviceSecure(true);

        assertEquals(
                BiometricStatus.ONLY_LSKF_AVAILABLE,
                DeviceAuthAvailabilityCheckerBridge.getBiometricAvailabilityStatus());
    }

    @Test
    public void testGetBiometricAvailabilityStatus_unavailable() {
        DeviceAuthenticatorControllerTest.ShadowBiometricManager.setCanAuthenticateResult(
                BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED);
        getShadowKeyguardManager().setIsDeviceSecure(false);

        assertEquals(
                BiometricStatus.UNAVAILABLE,
                DeviceAuthAvailabilityCheckerBridge.getBiometricAvailabilityStatus());
    }

    @Test
    public void testSetBiometricAvailabilityStatusForTesting_overridesResult() {
        DeviceAuthAvailabilityCheckerBridge.setBiometricAvailabilityStatusForTesting(
                BiometricStatus.BIOMETRICS_AVAILABLE);
        assertEquals(
                BiometricStatus.BIOMETRICS_AVAILABLE,
                DeviceAuthAvailabilityCheckerBridge.getBiometricAvailabilityStatus());

        DeviceAuthAvailabilityCheckerBridge.setBiometricAvailabilityStatusForTesting(
                BiometricStatus.UNAVAILABLE);
        assertEquals(
                BiometricStatus.UNAVAILABLE,
                DeviceAuthAvailabilityCheckerBridge.getBiometricAvailabilityStatus());
    }
}
