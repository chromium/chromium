// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import com.google.common.base.Optional;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;

/**
 * Interface for the helper responsible for Password Checkup operations.
 */
public interface PasswordCheckupClientHelper {
    /**
     * Serves as a general exception for failed requests to the password checkup backend.
     */
    class PasswordCheckBackendException extends Exception {
        public @CredentialManagerError int errorCode;

        public PasswordCheckBackendException(String message, @CredentialManagerError int error) {
            super(message);
            errorCode = error;
        }
    }

    /**
     * Retrieves a pending intent that can be used to launch the Password Checkup UI in the
     * credential manager. The intent is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the password checkup UI
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the encountered error.
     *      The error should be a value from {@link CredentialManagerError}.
     */
    @Deprecated
    default void getPasswordCheckupPendingIntent(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<PendingIntent> successCallback,
            Callback<Integer> failureCallback) {}

    /**
     * Asynchronously runs Password Checkup and stores the result in PasswordSpecifics then saves it
     * to the ChromeSync module.
     *
     * @param referrer the place that requested to start a check.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with Password Check started successful
     * @param failureCallback callback called if encountered an error.
     *      The error should be a value from {@link CredentialManagerError}.
     */
    @Deprecated
    default void runPasswordCheckup(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Void> successCallback,
            Callback<Integer> failureCallback) {}

    /**
     * Asynchronously returns the number of breached credentials for the provided account.
     *
     * @param referrer the place that requested number of breached credentials.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with the number of breached passwords.
     * @param failureCallback callback called if encountered an error.
     *      The error should be a value from {@link CredentialManagerError}.
     */
    @Deprecated
    default void getNumberOfBreachedCredentials(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Integer> successCallback,
            Callback<Integer> failureCallback) {}

    /**
     * Retrieves a pending intent that can be used to launch the Password Checkup UI in the
     * credential manager. The intent is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the password checkup UI
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the encountered error.
     */
    default void getPasswordCheckupIntent(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {}

    /**
     * Asynchronously runs Password Checkup and stores the result in PasswordSpecifics then saves it
     * to the ChromeSync module.
     *
     * @param referrer the place that requested to start a check.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with Password Check started successful
     * @param failureCallback callback called if encountered an error.
     */
    default void runPasswordCheckupInBackground(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {}

    /**
     * Asynchronously returns the number of breached credentials for the provided account.
     *
     * @param referrer the place that requested number of breached credentials.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *         account will be used.
     * @param successCallback callback called with the number of breached passwords.
     * @param failureCallback callback called if encountered an error.
     */
    default void getBreachedCredentialsCount(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {}
}
