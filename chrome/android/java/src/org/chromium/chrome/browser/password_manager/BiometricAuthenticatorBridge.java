// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_SUCCESS;

import android.content.Context;
import android.hardware.biometrics.BiometricManager;
import android.os.Build;

import androidx.core.hardware.fingerprint.FingerprintManagerCompat;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.compat.ApiHelperForQ;
import org.chromium.ui.base.WindowAndroid;

class BiometricAuthenticatorBridge {
    private final Context mContext;

    private BiometricAuthenticatorBridge(WindowAndroid windowAndroid) {
        mContext = windowAndroid.getApplicationContext();
    }

    @CalledByNative
    private static BiometricAuthenticatorBridge create(WindowAndroid windowAndroid) {
        return new BiometricAuthenticatorBridge(windowAndroid);
    }

    @CalledByNative
    public @BiometricsAvailability int canAuthenticate() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            BiometricManager biometricManager =
                    ApiHelperForQ.getBiometricManagerSystemService(mContext);
            switch (ApiHelperForQ.canAuthenticate(biometricManager)) {
                case BIOMETRIC_SUCCESS:
                    return BiometricsAvailability.AVAILABLE;
                case BIOMETRIC_ERROR_NO_HARDWARE:
                case BIOMETRIC_ERROR_HW_UNAVAILABLE:
                    return BiometricsAvailability.NO_HARDWARE;
                case BIOMETRIC_ERROR_NONE_ENROLLED:
                    return BiometricsAvailability.NOT_ENROLLED;
                default:
                    return BiometricsAvailability.NO_HARDWARE;
            }
        } else {
            FingerprintManagerCompat fingerprintManager = FingerprintManagerCompat.from(mContext);
            if (!fingerprintManager.isHardwareDetected()) {
                return BiometricsAvailability.NO_HARDWARE;
            } else if (!fingerprintManager.hasEnrolledFingerprints()) {
                return BiometricsAvailability.NOT_ENROLLED;
            } else {
                return BiometricsAvailability.AVAILABLE;
            }
        }
    }
}
