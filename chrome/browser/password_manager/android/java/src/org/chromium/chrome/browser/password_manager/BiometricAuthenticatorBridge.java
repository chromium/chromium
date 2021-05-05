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
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForQ;
import org.chromium.ui.base.WindowAndroid;

class BiometricAuthenticatorBridge {
    private final Context mContext;
    private long mNativeBiometricAuthenticator;

    private BiometricAuthenticatorBridge(
            long nativeBiometricAuthenticator, WindowAndroid windowAndroid) {
        mContext = windowAndroid.getApplicationContext();
        mNativeBiometricAuthenticator = nativeBiometricAuthenticator;
    }

    @CalledByNative
    private static BiometricAuthenticatorBridge create(
            long nativeBiometricAuthenticator, WindowAndroid windowAndroid) {
        return new BiometricAuthenticatorBridge(nativeBiometricAuthenticator, windowAndroid);
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

    @CalledByNative
    void authenticate() {
        // TODO(crbug.com/1031483): Trigger a biometric prompt.
        onAuthenticationCompleted(true);
    }

    @CalledByNative
    void cancel() {
        mNativeBiometricAuthenticator = 0;
        // TODO(crbug.com/1031483): Cancel the reauth if one is in progress.
    }

    void onAuthenticationCompleted(boolean success) {
        if (mNativeBiometricAuthenticator != 0) {
            BiometricAuthenticatorBridgeJni.get().onAuthenticationCompleted(
                    mNativeBiometricAuthenticator, success);
        }
    }

    @NativeMethods
    interface Natives {
        void onAuthenticationCompleted(long nativeBiometricAuthenticatorAndroid, boolean success);
    }
}
