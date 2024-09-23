// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;

/**
 * Class handling the communication with the C++ part of the password check feature. It forwards
 * messages to and from its C++ counterpart.
 */
class PasswordCheckBridge {
    private long mNativePasswordCheckBridge;
    private final PasswordCheckObserver mPasswordCheckObserver;

    /** Observer listening to all messages relevant to the password check. */
    interface PasswordCheckObserver {
        /**
         * Called when the compromised credentials found in a previous check are read from disk.
         * @param count The number of compromised credentials that were found in a previous check.
         */
        void onCompromisedCredentialsFetched(int count);

        /**
         * Called when the saved passwords are read from disk.
         * @param count The total number of saved passwords.
         */
        void onSavedPasswordsFetched(int count);

        /**
         * Called when the password check status changes, e.g. from idle to running.
         * @param status The current status of the password check.
         */
        void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int status);

        /**
         * Called during a check when a credential has finished being processed.
         * @param alreadyProcessed Number of credentials that the check already processed.
         * @param remainingInQueue Number of credentials that still need to be processed.
         */
        void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue);
    }

    PasswordCheckBridge(PasswordCheckObserver passwordCheckObserver) {
        // Initialized its native counterpart. This will also start fetching the compromised
        // credentials stored in the database by the last check.
        mNativePasswordCheckBridge = PasswordCheckBridgeJni.get().create(this);
        mPasswordCheckObserver = passwordCheckObserver;
    }

    @CalledByNative
    void onCompromisedCredentialsFetched(int count) {
        mPasswordCheckObserver.onCompromisedCredentialsFetched(count);
    }

    @CalledByNative
    void onSavedPasswordsFetched(int count) {
        mPasswordCheckObserver.onSavedPasswordsFetched(count);
    }

    @CalledByNative
    void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int state) {
        mPasswordCheckObserver.onPasswordCheckStatusChanged(state);
    }

    @CalledByNative
    void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue) {
        mPasswordCheckObserver.onPasswordCheckProgressChanged(alreadyProcessed, remainingInQueue);
    }

    @CalledByNative
    private static void insertCredential(
            CompromisedCredential[] credentials,
            int index,
            String signonRealm,
            GURL associatedUrl,
            String username,
            String displayOrigin,
            String displayUsername,
            String password,
            String passwordChangeUrl,
            String associatedApp,
            long creationTime,
            long lastUsedTime,
            boolean leaked,
            boolean phished) {
        credentials[index] =
                new CompromisedCredential(
                        signonRealm,
                        associatedUrl,
                        username,
                        displayOrigin,
                        displayUsername,
                        password,
                        passwordChangeUrl,
                        associatedApp,
                        creationTime,
                        lastUsedTime,
                        leaked,
                        phished);
    }

    /** Starts the password check. */
    void startCheck() {
        PasswordCheckBridgeJni.get().startCheck(mNativePasswordCheckBridge);
    }

    /** Stops the password check. */
    void stopCheck() {
        PasswordCheckBridgeJni.get().stopCheck(mNativePasswordCheckBridge);
    }

    /**
     * @return The timestamp of the last completed check.
     */
    long getLastCheckTimestamp() {
        return PasswordCheckBridgeJni.get().getLastCheckTimestamp(mNativePasswordCheckBridge);
    }

    /**
     * This can return 0 if the compromised credentials haven't been fetched from the database yet.
     * @return The number of compromised credentials found in the last run password check.
     */
    int getCompromisedCredentialsCount() {
        return PasswordCheckBridgeJni.get()
                .getCompromisedCredentialsCount(mNativePasswordCheckBridge);
    }

    /**
     * This can return 0 if the saved passwords haven't been fetched from the database yet.
     * @return The total number of saved passwords.
     */
    int getSavedPasswordsCount() {
        return PasswordCheckBridgeJni.get().getSavedPasswordsCount(mNativePasswordCheckBridge);
    }

    /**
     * Returns the list of compromised credentials that are stored in the database.
     * @param credentials array to be populated with the compromised credentials.
     */
    void getCompromisedCredentials(CompromisedCredential[] credentials) {
        PasswordCheckBridgeJni.get()
                .getCompromisedCredentials(mNativePasswordCheckBridge, credentials);
    }

    /** Launch the password check in the Google Account. */
    void launchCheckupInAccount(Activity activity) {
        PasswordCheckBridgeJni.get().launchCheckupInAccount(mNativePasswordCheckBridge, activity);
    }

    void updateCredential(CompromisedCredential credential, String newPassword) {
        PasswordCheckBridgeJni.get()
                .updateCredential(mNativePasswordCheckBridge, credential, newPassword);
    }

    void onEditCredential(CompromisedCredential credential, Context context) {
        PasswordCheckBridgeJni.get()
                .onEditCredential(mNativePasswordCheckBridge, credential, context);
    }

    void removeCredential(CompromisedCredential credential) {
        PasswordCheckBridgeJni.get().removeCredential(mNativePasswordCheckBridge, credential);
    }

    boolean hasAccountForRequest() {
        return PasswordCheckBridgeJni.get().hasAccountForRequest(mNativePasswordCheckBridge);
    }

    /** Destroys its C++ counterpart. */
    void destroy() {
        if (mNativePasswordCheckBridge != 0) {
            PasswordCheckBridgeJni.get().destroy(mNativePasswordCheckBridge);
            mNativePasswordCheckBridge = 0;
        }
    }

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        long create(PasswordCheckBridge passwordCheckBridge);

        void startCheck(long nativePasswordCheckBridge);

        void stopCheck(long nativePasswordCheckBridge);

        long getLastCheckTimestamp(long nativePasswordCheckBridge);

        int getCompromisedCredentialsCount(long nativePasswordCheckBridge);

        int getSavedPasswordsCount(long nativePasswordCheckBridge);

        void getCompromisedCredentials(
                long nativePasswordCheckBridge, CompromisedCredential[] credentials);

        void launchCheckupInAccount(long nativePasswordCheckBridge, Activity activity);

        void updateCredential(
                long nativePasswordCheckBridge,
                CompromisedCredential credential,
                String newPassword);

        void onEditCredential(
                long nativePasswordCheckBridge, CompromisedCredential credential, Context context);

        void removeCredential(long nativePasswordCheckBridge, CompromisedCredential credentials);

        boolean hasAccountForRequest(long nativePasswordCheckBridge);

        void destroy(long nativePasswordCheckBridge);
    }
}
