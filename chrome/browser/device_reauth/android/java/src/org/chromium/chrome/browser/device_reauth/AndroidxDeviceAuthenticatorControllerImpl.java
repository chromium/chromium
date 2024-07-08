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

import androidx.annotation.NonNull;
import androidx.biometric.BiometricManager;
import androidx.biometric.BiometricManager.Authenticators;
import androidx.biometric.BiometricPrompt;
import androidx.biometric.BiometricPrompt.AuthenticationCallback;
import androidx.biometric.BiometricPrompt.PromptInfo;
import androidx.fragment.app.FragmentActivity;

class AndroidxDeviceAuthenticatorControllerImpl implements DeviceAuthenticatorController {
    FragmentActivity mActivity;
    Delegate mDelegate;
    private BiometricPrompt mBiometricPrompt;

    public AndroidxDeviceAuthenticatorControllerImpl(FragmentActivity activity, Delegate delegate) {
        mActivity = activity;
        mDelegate = delegate;
    }

    @Override
    public @BiometricsAvailability int canAuthenticateWithBiometric() {
        BiometricManager biometricManager = BiometricManager.from(mActivity);
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
        return (availability == BiometricsAvailability.AVAILABLE) || hasScreenLockSetUp();
    }

    private boolean hasScreenLockSetUp() {
        return ((KeyguardManager) mActivity.getSystemService(Context.KEYGUARD_SERVICE))
                .isDeviceSecure();
    }

    @Override
    public void authenticate() {
        PromptInfo promptInfo =
                new PromptInfo.Builder()
                        .setTitle(
                                mActivity.getString(R.string.password_filling_reauth_prompt_title))
                        .setConfirmationRequired(false)
                        .setAllowedAuthenticators(
                                Authenticators.BIOMETRIC_STRONG
                                        | Authenticators.BIOMETRIC_WEAK
                                        | Authenticators.DEVICE_CREDENTIAL)
                        .build();
        mBiometricPrompt =
                new BiometricPrompt(
                        mActivity,
                        new AuthenticationCallback() {
                            @Override
                            public void onAuthenticationError(
                                    int errorCode, @NonNull CharSequence errString) {
                                if (errorCode == BiometricPrompt.ERROR_USER_CANCELED) {
                                    onAuthenticationCompleted(DeviceAuthUIResult.CANCELED_BY_USER);
                                    return;
                                }
                                onAuthenticationCompleted(DeviceAuthUIResult.FAILED);
                            }

                            @Override
                            public void onAuthenticationSucceeded(
                                    @NonNull BiometricPrompt.AuthenticationResult result) {
                                switch (result.getAuthenticationType()) {
                                    case BiometricPrompt.AUTHENTICATION_RESULT_TYPE_UNKNOWN:
                                        onAuthenticationCompleted(
                                                DeviceAuthUIResult.SUCCESS_WITH_UNKNOWN_METHOD);
                                        break;
                                    case BiometricPrompt.AUTHENTICATION_RESULT_TYPE_BIOMETRIC:
                                        onAuthenticationCompleted(
                                                DeviceAuthUIResult.SUCCESS_WITH_BIOMETRICS);
                                        break;
                                    case BiometricPrompt
                                            .AUTHENTICATION_RESULT_TYPE_DEVICE_CREDENTIAL:
                                        onAuthenticationCompleted(
                                                DeviceAuthUIResult.SUCCESS_WITH_DEVICE_LOCK);
                                        break;
                                    default:
                                        onAuthenticationCompleted(DeviceAuthUIResult.FAILED);
                                        break;
                                }
                            }
                        });
        mBiometricPrompt.authenticate(promptInfo);
    }

    private void onAuthenticationCompleted(@DeviceAuthUIResult int result) {
        mDelegate.onAuthenticationCompleted(result);
    }

    @Override
    public void cancel() {
        if (mBiometricPrompt == null) return;
        mBiometricPrompt.cancelAuthentication();
    }
}
