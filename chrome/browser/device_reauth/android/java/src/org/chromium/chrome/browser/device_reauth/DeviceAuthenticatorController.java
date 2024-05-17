// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

/** Encapsulates the logic for the user authentication on Android device. */
interface DeviceAuthenticatorController {
    interface Delegate {

        /**
         * Notifies about authentication completion.
         *
         * @param result the result of the authentication (success with biometrics, success with
         *     fallback or failure).
         */
        void onAuthenticationCompleted(@DeviceAuthUIResult int result);
    }

    /**
     * Checks if biometric authentication is available.
     *
     * @return the enum value, which represents either the auth being available or the error type.
     */
    @BiometricsAvailability
    int canAuthenticateWithBiometric();

    /**
     * A general method to check whether we can authenticate either via biometrics or screen lock.
     *
     * <p>True, if either biometrics are enrolled or screen lock is setup, false otherwise.
     */
    boolean canAuthenticateWithBiometricOrScreenLock();

    /**
     * Launches biometric authentication on the device. Remember to call {@link
     * canAuthenticateWithBiometric} before this method.
     */
    void authenticate();

    /** Cancels authentication. */
    void cancel();
}
