// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static android.hardware.biometrics.BiometricManager.BIOMETRIC_SUCCESS;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.CancellationSignal;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.concurrent.Executor;

/**
 * The controller used for user authentication (either with screen lock or biometrics) for devices
 * with Android V or higher.
 */
class MandatoryAuthenticatorControllerImpl extends DeviceAuthenticatorControllerImpl {
    /** The bit is used to request for mandatory biometrics. */
    public static final int AUTHENTICATOR_MANDATORY_BIOMETRICS = 1 << 16;

    /** Mandatory biometrics is not in effect. */
    public static final int BIOMETRIC_ERROR_MANDATORY_NOT_ACTIVE = 20;

    /** Mandatory biometrics is not enabled for apps. */
    public static final int BIOMETRIC_ERROR_NOT_ENABLED_FOR_APPS = 21;

    /** The operation was canceled because the API is locked out due to too many attempts. */
    public static final int BIOMETRIC_ERROR_LOCKOUT = 7;

    /**
     * The operation was canceled because {@link #BIOMETRIC_ERROR_LOCKOUT} occurred too many times.
     * Biometric authentication is disabled until the user unlocks with strong authentication
     * (PIN/Pattern/Password)
     */
    public static final int BIOMETRIC_ERROR_LOCKOUT_PERMANENT = 9;

    private Context mContext;
    private BiometricPrompt mBiometricPrompt;

    public MandatoryAuthenticatorControllerImpl(Context context, Delegate delegate) {
        super(context, delegate);
        mContext = context;
    }

    @Override
    @RequiresApi(VERSION_CODES.VANILLA_ICE_CREAM)
    @SuppressLint("WrongConstant")
    public @BiometricsAvailability int canAuthenticateWithBiometric() {
        BiometricManager manager =
                (BiometricManager) mContext.getSystemService(Context.BIOMETRIC_SERVICE);
        if (manager == null) {
            return BiometricsAvailability.OTHER_ERROR;
        }
        try {
            switch (manager.canAuthenticate(AUTHENTICATOR_MANDATORY_BIOMETRICS)) {
                case BIOMETRIC_SUCCESS:
                case BIOMETRIC_ERROR_LOCKOUT:
                    return BiometricsAvailability.REQUIRED;
                case BIOMETRIC_ERROR_MANDATORY_NOT_ACTIVE:
                case BIOMETRIC_ERROR_NOT_ENABLED_FOR_APPS:
                    // Fallback to the original way of checking biometric availability.
                    return super.canAuthenticateWithBiometric();
                    // The list of cases above  should be the full list of constant that
                    // `canAuthenticate` returns, but still adding default case for this to compile.
                default:
                    // With AUTHENTICATOR_MANDATORY_BIOMETRICS bit in place errors should result in
                    // calling `authenticate`.
                    return BiometricsAvailability.REQUIRED_BUT_HAS_ERROR;
            }
        } catch (SecurityException e) {
            // The build is not V-QPR1+, so Identity Check is not in effect.
            return super.canAuthenticateWithBiometric();
        }
    }

    @Override
    @RequiresApi(VERSION_CODES.VANILLA_ICE_CREAM)
    public boolean canAuthenticateWithBiometricOrScreenLock() {
        @BiometricsAvailability int canAuthenticateWithBiometric = canAuthenticateWithBiometric();
        if (canAuthenticateWithBiometric == BiometricsAvailability.REQUIRED
                || canAuthenticateWithBiometric == BiometricsAvailability.REQUIRED_BUT_HAS_ERROR) {
            // Returning true here, because if mandatory authentication is enabled, then
            // `authenticate` must be called (and error displayed in case of
            // REQUIRED_BUT_HAS_ERROR).
            return true;
        }
        return super.canAuthenticateWithBiometricOrScreenLock();
    }

    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @Override
    public void authenticate() {
        int availability = canAuthenticateWithBiometric();
        if (availability == BiometricsAvailability.REQUIRED_BUT_HAS_ERROR) {
            // TODO (crbug.com/367683407): trigger error UI offering to disable the mandatory auth.
            onAuthenticationCompleted(DeviceAuthUIResult.FAILED);
            return;
        }
        mBiometricPrompt = getBiometricPrompt(availability);
        mCancellationSignal = new CancellationSignal();
        Executor callbackExecutor = (r) -> PostTask.postTask(TaskTraits.UI_DEFAULT, r);

        try {
            mBiometricPrompt.authenticate(
                    mCancellationSignal,
                    callbackExecutor,
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationError(
                                int errorCode, @NonNull CharSequence errString) {
                            super.onAuthenticationError(errorCode, errString);
                            if (errorCode == BIOMETRIC_ERROR_LOCKOUT_PERMANENT) {
                                // TODO (crbug.com/367683201): trigger lockout dialog.
                                onAuthenticationCompleted(DeviceAuthUIResult.LOCKOUT);
                                return;
                            }
                            MandatoryAuthenticatorControllerImpl.this.onAuthenticationError(
                                    errorCode);
                        }

                        @Override
                        public void onAuthenticationSucceeded(
                                @NonNull BiometricPrompt.AuthenticationResult result) {
                            super.onAuthenticationSucceeded(result);
                            MandatoryAuthenticatorControllerImpl.this.onAuthenticationSucceeded(
                                    result);
                        }
                    });
        } catch (SecurityException e) {
            // The build is not V-QPR1+, so Identity Check is not in effect.
            super.authenticate();
        }
    }

    @RequiresApi(VERSION_CODES.VANILLA_ICE_CREAM)
    @SuppressLint("WrongConstant") // AUTHENTICATOR_MANDATORY_BIOMETRICS is not publicly available
    private BiometricPrompt getBiometricPrompt(@BiometricsAvailability int availability) {
        BiometricPrompt.Builder promptBuilder =
                new BiometricPrompt.Builder(mContext)
                        .setTitle(
                                mContext.getResources()
                                        .getString(R.string.password_filling_reauth_prompt_title));
        if (availability == BiometricsAvailability.REQUIRED) {
            promptBuilder.setAllowedAuthenticators(AUTHENTICATOR_MANDATORY_BIOMETRICS);
        }
        promptBuilder.setDeviceCredentialAllowed(true);
        promptBuilder.setConfirmationRequired(false);
        // TODO(crbug.com/368545705): Customize authentication prompt UI.
        return promptBuilder.build();
    }
}
