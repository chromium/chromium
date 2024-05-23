// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static androidx.biometric.BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE;
import static androidx.biometric.BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED;
import static androidx.biometric.BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE;
import static androidx.biometric.BiometricManager.BIOMETRIC_ERROR_SECURITY_UPDATE_REQUIRED;
import static androidx.biometric.BiometricManager.BIOMETRIC_SUCCESS;

import android.app.KeyguardManager;
import android.content.Context;

import androidx.biometric.BiometricManager;
import androidx.biometric.BiometricManager.Authenticators;

class AndroidxDeviceAuthenticatorControllerImpl implements DeviceAuthenticatorController {
    Context mContext;
    Delegate mDelegate;

    public AndroidxDeviceAuthenticatorControllerImpl(Context context, Delegate delegate) {
        mContext = context;
        mDelegate = delegate;
    }

    @Override
    public @BiometricsAvailability int canAuthenticateWithBiometric() {
        BiometricManager biometricManager = BiometricManager.from(mContext);
        switch (biometricManager.canAuthenticate(
                Authenticators.BIOMETRIC_STRONG | Authenticators.BIOMETRIC_WEAK)) {
            case BIOMETRIC_SUCCESS:
                return hasScreenLockSetUp()
                        ? BiometricsAvailability.AVAILABLE
                        : BiometricsAvailability.AVAILABLE_NO_FALLBACK;
            case BIOMETRIC_ERROR_NONE_ENROLLED:
                return BiometricsAvailability.NOT_ENROLLED;
            case BIOMETRIC_ERROR_SECURITY_UPDATE_REQUIRED:
                return BiometricsAvailability.SECURITY_UPDATE_REQUIRED;
            case BIOMETRIC_ERROR_NO_HARDWARE:
                return BiometricsAvailability.NO_HARDWARE;
            case BIOMETRIC_ERROR_HW_UNAVAILABLE:
                return BiometricsAvailability.HW_UNAVAILABLE;
            default:
                return BiometricsAvailability.OTHER_ERROR;
        }
    }

    @Override
    public boolean canAuthenticateWithBiometricOrScreenLock() {
        @BiometricsAvailability int availability = canAuthenticateWithBiometric();
        return (mContext != null && availability == BiometricsAvailability.AVAILABLE)
                || hasScreenLockSetUp();
    }

    private boolean hasScreenLockSetUp() {
        return ((KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE))
                .isDeviceSecure();
    }

    @Override
    public void authenticate() {
        // TODO(crbug.com/340437460): Implement.
    }

    @Override
    public void cancel() {
        // TODO(crbug.com/340437460): Implement.
    }
}
