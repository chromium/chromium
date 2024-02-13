// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Activity;
import android.content.Context;

import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;

/**
 * This component allows to check for compromised passwords. It provides a settings page which shows
 * the compromised passwords and exposes actions that will help the users to make safer their
 * credentials.
 */
public interface PasswordCheck extends PasswordCheckComponentUi.Delegate {
    /** Observes events and state changes of the password check. */
    interface Observer {
        /**
         * Gets invoked when the compromised credentials are fetched from the disk.
         * After this call, {@link #getCompromisedCredentialsCount} returns a valid value.
         */
        void onCompromisedCredentialsFetchCompleted();

        /**
         * Gets invoked when the saved passwords are fetched from the disk.
         * After this call, {@link #getSavedPasswordsCount} returns a valid value.
         */
        void onSavedPasswordsFetchCompleted();

        /**
         * Gets invoked once the password check stops running.
         * @param status A {@link PasswordCheckUIStatus} enum value.
         */
        void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int status);

        /**
         * Called during a check when a credential has finished being processed.
         * @param alreadyProcessed Number of credentials that the check already processed.
         * @param remainingInQueue Number of credentials that still need to be processed.
         */
        void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue);
    }

    /**
     * Initializes the PasswordCheck UI and launches it.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param passwordCheckReferrer The place which launched the check UI.
     */
    void showUi(Context context, @PasswordCheckReferrer int passwordCheckReferrer);

    /** Cleans up the C++ part, thus removing the compromised credentials from memory. */
    void destroy();

    /**
     * Adds a new observer to the list of observers. If it's already there, does nothing.
     * @param obs An {@link Observer} implementation instance.
     * @param callImmediatelyIfReady Invokes {@link Observer#onCompromisedCredentialsFetchCompleted}
     *   and {@link Observer#onSavedPasswordsFetchCompleted} on the observer if the corresponding
     *   data is already fetched when this is true.
     */
    void addObserver(Observer obs, boolean callImmediatelyIfReady);

    /**
     * Removes a given observer from the observers list if it is there. Otherwise, does nothing.
     * @param obs An {@link Observer} implementation instance.
     */
    void removeObserver(Observer obs);

    /**
     * @return The timestamp of the last completed check.
     */
    long getLastCheckTimestamp();

    /**
     * @return The last known status of the check.
     */
    @PasswordCheckUIStatus
    int getCheckStatus();

    /**
     * @return The latest available number of compromised passwords. If this is invoked before
     * {@link Observer#onCompromisedCredentialsFetchCompleted}, the returned value is likely
     * invalid.
     */
    int getCompromisedCredentialsCount();

    /**
     * @return The latest available compromised passwords. If this is invoked before
     * {@link Observer#onCompromisedCredentialsFetchCompleted}, the returned array is likely
     * incomplete.
     */
    CompromisedCredential[] getCompromisedCredentials();

    /**
     * Update the given credential in the password store.
     * @param credential A {@link CompromisedCredential}.
     * @param newPassword The new password for the credential.
     */
    void updateCredential(CompromisedCredential credential, String newPassword);

    /**
     * @return The latest available number of all saved passwords. If this is invoked before
     * {@link Observer#onSavedPasswordsFetchCompleted}, the returned value is likely invalid.
     */
    int getSavedPasswordsCount();

    /** Launch the password check in the Google Account. */
    void launchCheckupInAccount(Activity activity);

    /** Starts the password check, if one is not running already. */
    void startCheck();

    /** Stops the password check, if one is running. Otherwise, does nothing. */
    void stopCheck();

    /** Checks if user is signed into Chrome account to perform the password check. */
    boolean hasAccountForRequest();
}
