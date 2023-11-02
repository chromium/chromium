// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_HW_UNAVAILABLE;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_ERROR_SECURITY_UPDATE_REQUIRED;
import static android.hardware.biometrics.BiometricManager.BIOMETRIC_SUCCESS;

import android.app.KeyguardManager;
import android.content.Context;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.Build;
import android.os.CancellationSignal;
import android.support.annotation.NonNull;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.Executor;

class BiometricAuthenticatorBridge {
    private CancellationSignal mCancellationSignal;
    private final Context mContext;
    private long mNativeBiometricAuthenticator;
    private BiometricPrompt mBiometricPrompt;

    private BiometricAuthenticatorBridge(long nativeBiometricAuthenticator) {
        mNativeBiometricAuthenticator = nativeBiometricAuthenticator;
        mContext = ContextUtils.getApplicationContext();
        mNativeBiometricAuthenticator = nativeBiometricAuthenticator;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
            BiometricPrompt.Builder promptBuilder = new BiometricPrompt.Builder(mContext).setTitle(
                    mContext.getResources().getString(
                            R.string.password_filling_reauth_prompt_title));
            promptBuilder.setDeviceCredentialAllowed(true);
            promptBuilder.setConfirmationRequired(false);
            mBiometricPrompt = promptBuilder.build();
        }
    }

    @CalledByNative
    private static BiometricAuthenticatorBridge create(long nativeBiometricAuthenticator) {
        return new BiometricAuthenticatorBridge(nativeBiometricAuthenticator);
    }

    @CalledByNative
    @BiometricsAvailability
    int canAuthenticateWithBiometric() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return BiometricsAvailability.ANDROID_VERSION_NOT_SUPPORTED;
        }
        BiometricManager biometricManager = mContext.getSystemService(BiometricManager.class);
        switch (biometricManager.canAuthenticate()) {
            case BIOMETRIC_SUCCESS:
                return hasScreenLockSetUp() ? BiometricsAvailability.AVAILABLE
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

    /**
     * A general method to check whether we can authenticate either via biometrics or screen lock.
     *
     * True, if either biometrics are enrolled or screen lock is setup, false otherwise.
     */
    @CalledByNative
    boolean canAuthenticateWithBiometricOrScreenLock() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return false;
        }

        @BiometricsAvailability
        int availability = canAuthenticateWithBiometric();
        return (availability == BiometricsAvailability.AVAILABLE) || hasScreenLockSetUp();
    }

    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.P)
    void authenticate() {
        if (mBiometricPrompt == null) {
            return;
        }
        mCancellationSignal = new CancellationSignal();
        Executor callbackExecutor = (r) -> PostTask.postTask(UiThreadTaskTraits.DEFAULT, r);

        mBiometricPrompt.authenticate(mCancellationSignal, callbackExecutor,
                new BiometricPrompt.AuthenticationCallback() {
                    @Override
                    public void onAuthenticationError(
                            int errorCode, @NonNull CharSequence errString) {
                        super.onAuthenticationError(errorCode, errString);
                        if (errorCode == BiometricPrompt.BIOMETRIC_ERROR_USER_CANCELED) {
                            onAuthenticationCompleted(BiometricAuthUIResult.CANCELED_BY_USER);
                            return;
                        }
                        onAuthenticationCompleted(BiometricAuthUIResult.FAILED);
                    }

                    @Override
                    public void onAuthenticationSucceeded(
                            @NonNull BiometricPrompt.AuthenticationResult result) {
                        super.onAuthenticationSucceeded(result);
                        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.R) {
                            onAuthenticationCompleted(
                                    BiometricAuthUIResult.SUCCESS_WITH_UNKNOWN_METHOD);
                            return;
                        }

                        if (result.getAuthenticationType()
                                == BiometricPrompt.AUTHENTICATION_RESULT_TYPE_BIOMETRIC) {
                            onAuthenticationCompleted(
                                    BiometricAuthUIResult.SUCCESS_WITH_BIOMETRICS);
                            return;
                        }
                        onAuthenticationCompleted(BiometricAuthUIResult.SUCCESS_WITH_DEVICE_LOCK);
                    }
                });
    }

    void onAuthenticationCompleted(@BiometricAuthUIResult int result) {
        mCancellationSignal = null;
        if (mNativeBiometricAuthenticator != 0) {
            BiometricAuthenticatorBridgeJni.get().onAuthenticationCompleted(
                    mNativeBiometricAuthenticator, result);
        }
    }

    @CalledByNative
    void destroy() {
        mNativeBiometricAuthenticator = 0;
        cancel();
    }

    @CalledByNative
    void cancel() {
        if (mCancellationSignal != null) {
            mCancellationSignal.cancel();
        }
    }

    private boolean hasScreenLockSetUp() {
        return ((KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE))
                .isKeyguardSecure();
    }

    @NativeMethods
    interface Natives {
        void onAuthenticationCompleted(long nativeBiometricAuthenticatorBridgeImpl, int result);
    }
}
