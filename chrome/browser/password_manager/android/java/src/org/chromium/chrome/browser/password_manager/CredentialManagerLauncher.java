// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;
import android.content.Intent;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface for the launcher responsible for opening the Credential Manager. */
public interface CredentialManagerLauncher {
    /**
     * These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. They should be kept in sync with the enum values in enums.xml.
     * TODO(crbug.com/40853413): These error codes are also used by PasswordCheckup, consider moving
     * out of this class.
     */
    @IntDef({
        CredentialManagerError.NO_CONTEXT,
        CredentialManagerError.NO_ACCOUNT_NAME,
        CredentialManagerError.UNCATEGORIZED,
        CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED,
        CredentialManagerError.BACKEND_NOT_AVAILABLE,
        CredentialManagerError.API_EXCEPTION,
        CredentialManagerError.OTHER_API_ERROR,
        CredentialManagerError.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CredentialManagerError {
        // There is no application context.
        int NO_CONTEXT = 0;

        // The provided account name is empty
        int NO_ACCOUNT_NAME = 1;

        // Error encountered after calling the API to fetch the launch intent.
        // Obsolete: this is now split into API_EXCEPTION and OTHER_API_ERROR
        // int API_ERROR = 2;

        // Error is not categorized.
        int UNCATEGORIZED = 3;

        // Operation can not be executed due to unsupported backend version.
        int BACKEND_VERSION_NOT_SUPPORTED = 4;

        // Backend downstream implementation is not available.
        int BACKEND_NOT_AVAILABLE = 5;

        // Recorded when the call failed at the API level, with an exception that is an instance
        // of ApiException.
        int API_EXCEPTION = 6;

        // Recorded when the call failed at the API level, with an exception that is NOT an
        // instance of ApiException. This might signal that there was an exception thrown
        // by the API implementation outside of Chrome.
        int OTHER_API_ERROR = 7;

        int COUNT = 8;
    }

    /** Serves as a general exception for failed requests to the credential manager backend. */
    class CredentialManagerBackendException extends Exception {
        public @CredentialManagerError int errorCode;

        public CredentialManagerBackendException(
                String message, @CredentialManagerError int error) {
            super(message);
            errorCode = error;
        }
    }

    /**
     * Retrieves a pending intent that can be used to launch the credential manager. The intent
     * is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the credential manager
     * @param accountName the account name that is syncing passwords.
     * @param successCallback callback called with the intent if the retrieving was successful.
     * @param failureCallback callback called if the retrieving failed with the encountered error.
     */
    void getAccountCredentialManagerIntent(
            @ManagePasswordsReferrer int referrer,
            String accountName,
            Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback);

    /**
     * Retrieves a pending intent that can be used to launch the credential manager. The intent
     * is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the credential manager
     * @param successCallback callback called with the intent if the retrieving was successful.
     * @param failureCallback callback called if the retrieving failed with the encountered error.
     */
    void getLocalCredentialManagerIntent(
            @ManagePasswordsReferrer int referrer,
            Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback);

    /**
     * Retrieves a pending AccountSettings API intent that can be used to launch the credential
     * manager. Other than using a different GMS API, the main difference between this and
     * CredentialManager intents above is that this can cause an account chooser to be shown before
     * opening the credential manager. The intent is to either be used immediately or discarded.
     *
     * @param accountName the account name that is syncing passwords, or an empty string if there
     *      is no such account.
     * @param completionCallback callback called with the intent if the retrieving was successful,
     *      or null if there was an error.
     */
    void getAccountSettingsIntent(String accountName, Callback<Intent> completionCallback);
}
