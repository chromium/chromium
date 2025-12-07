// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for the helper responsible for Password Checkup operations. */
@NullMarked
public interface PasswordCheckupClientHelper {
    /** Thrown when isPasswordManagerAvailable() is false upon the backend call. */
    class PasswordManagerUnavailableException extends Exception {
        public PasswordManagerUnavailableException() {
            super("Password manager is unavailable");
        }
    }

    /**
     * Retrieves a pending intent that can be used to launch the Password Checkup UI in the
     * credential manager. The intent is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the password checkup UI
     * @param accountName the account name that is syncing passwords. If it's empty, local account
     *     will be used.
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the encountered error.
     */
    void getPasswordCheckupIntent(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback);

    /**
     * Asynchronously runs Password Checkup and stores the result in PasswordSpecifics then saves it
     * to the ChromeSync module.
     *
     * @param referrer the place that requested to start a check.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *     account will be used.
     * @param successCallback callback called with Password Check started successful
     * @param failureCallback callback called if encountered an error.
     */
    void runPasswordCheckupInBackground(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<@Nullable Void> successCallback,
            Callback<Exception> failureCallback);

    /**
     * Asynchronously returns the number of breached credentials for the provided account.
     *
     * @param referrer the place that requested number of breached credentials.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *     account will be used.
     * @param successCallback callback called with the number of breached passwords.
     * @param failureCallback callback called if encountered an error.
     */
    void getBreachedCredentialsCount(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback);

    /**
     * Asynchronously returns the number of weak credentials for the provided account.
     *
     * @param referrer the place that requested number of weak credentials.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *     account will be used.
     * @param successCallback callback called with the number of weak passwords.
     * @param failureCallback callback called if encountered an error.
     */
    void getWeakCredentialsCount(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback);

    /**
     * Asynchronously returns the number of reused credentials for the provided account.
     *
     * @param referrer the place that requested number of reused credentials.
     * @param accountName the account name that is syncing passwords. If no value was provided local
     *     account will be used.
     * @param successCallback callback called with the number of reused passwords.
     * @param failureCallback callback called if encountered an error.
     */
    void getReusedCredentialsCount(
            @PasswordCheckReferrer int referrer,
            @Nullable String accountName,
            Callback<Integer> successCallback,
            Callback<Exception> failureCallback);
}
