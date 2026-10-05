// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import android.app.KeyguardManager;
import android.content.Context;

import androidx.biometric.BiometricManager;
import androidx.biometric.BiometricManager.Authenticators;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Provides static helper methods to check biometric and screen lock availability via Android system
 * services using the application context.
 */
@NullMarked
public final class DeviceAuthAvailabilityCheckerBridge {
    private DeviceAuthAvailabilityCheckerBridge() {}

    @CalledByNative
    static @JniType("device_reauth::BiometricsAvailability") @BiometricsAvailability int
            canAuthenticateWithBiometric() {
        BiometricManager biometricManager =
                BiometricManager.from(ContextUtils.getApplicationContext());
        switch (biometricManager.canAuthenticate(
                Authenticators.BIOMETRIC_STRONG | Authenticators.BIOMETRIC_WEAK)) {
            case BiometricManager.BIOMETRIC_SUCCESS:
                return hasScreenLockSetUp()
                        ? BiometricsAvailability.AVAILABLE
                        : BiometricsAvailability.AVAILABLE_NO_FALLBACK;
            case BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED:
                return BiometricsAvailability.NOT_ENROLLED;
            case BiometricManager.BIOMETRIC_ERROR_SECURITY_UPDATE_REQUIRED:
                return BiometricsAvailability.SECURITY_UPDATE_REQUIRED;
            case BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE:
                return BiometricsAvailability.NO_HARDWARE;
            case BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE:
                return BiometricsAvailability.HW_UNAVAILABLE;
            default:
                return BiometricsAvailability.OTHER_ERROR;
        }
    }

    @CalledByNative
    static boolean canAuthenticateWithBiometricOrScreenLock() {
        @BiometricsAvailability int availability = canAuthenticateWithBiometric();
        return (availability == BiometricsAvailability.AVAILABLE) || hasScreenLockSetUp();
    }

    private static boolean hasScreenLockSetUp() {
        KeyguardManager keyguardManager =
                (KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE);
        return keyguardManager != null && keyguardManager.isDeviceSecure();
    }
}
