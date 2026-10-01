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
import androidx.biometric.BiometricPrompt;
import androidx.biometric.BiometricPrompt.AuthenticationCallback;
import androidx.biometric.BiometricPrompt.PromptInfo;
import androidx.fragment.app.FragmentActivity;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
class DeviceAuthenticatorController {
    private final @Nullable FragmentActivity mActivity;
    private final Delegate mDelegate;
    private @Nullable BiometricPrompt mBiometricPrompt;

    interface Delegate {

        /**
         * Notifies about authentication completion.
         *
         * @param result the result of the authentication (success with biometrics, success with
         *     fallback or failure).
         */
        void onAuthenticationCompleted(@DeviceAuthUIResult int result);
    }

    public DeviceAuthenticatorController(@Nullable FragmentActivity activity, Delegate delegate) {
        mActivity = activity;
        mDelegate = delegate;
    }

    /**
     * Checks if biometric authentication is available.
     *
     * @return the enum value, which represents either the auth being available or the error type.
     */
    public @BiometricsAvailability int canAuthenticateWithBiometric() {
        BiometricManager biometricManager =
                BiometricManager.from(ContextUtils.getApplicationContext());
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

    /**
     * A general method to check whether we can authenticate either via biometrics or screen lock.
     *
     * @return true, if either biometrics are enrolled or screen lock is setup, false otherwise.
     */
    public boolean canAuthenticateWithBiometricOrScreenLock() {
        @BiometricsAvailability int availability = canAuthenticateWithBiometric();
        return (availability == BiometricsAvailability.AVAILABLE) || hasScreenLockSetUp();
    }

    private boolean hasScreenLockSetUp() {
        KeyguardManager keyguardManager =
                (KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE);
        return keyguardManager != null && keyguardManager.isDeviceSecure();
    }

    /**
     * Launches biometric authentication on the device. {@link canAuthenticateWithBiometric} should
     * be called before this method.
     *
     * <p>Authentication requires an {@link FragmentActivity} to host the prompt. If this controller
     * was created without one, the request fails with {@link
     * DeviceAuthUIResult#FAILED_NO_ACTIVITY}. The delegate is always notified asynchronously, so
     * that callers can safely destroy this object from within {@link
     * Delegate#onAuthenticationCompleted}.
     */
    public void authenticate() {
        if (mActivity == null) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> onAuthenticationCompleted(DeviceAuthUIResult.FAILED_NO_ACTIVITY));
            return;
        }

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
                                    int errorCode, CharSequence errString) {
                                if (errorCode == BiometricPrompt.ERROR_USER_CANCELED) {
                                    onAuthenticationCompleted(DeviceAuthUIResult.CANCELED_BY_USER);
                                    return;
                                }
                                onAuthenticationCompleted(DeviceAuthUIResult.FAILED);
                            }

                            @Override
                            public void onAuthenticationSucceeded(
                                    BiometricPrompt.AuthenticationResult result) {
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

    /** Cancels authentication. */
    public void cancel() {
        if (mBiometricPrompt == null) return;
        mBiometricPrompt.cancelAuthentication();
    }
}
