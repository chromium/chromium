// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/**
 * Class handling communication with C++ password store from Java. It forwards
 * messages to and from its C++ counterpart.
 */
public class PasswordStoreBridge {
    @CalledByNative
    private static PasswordStoreCredential createPasswordStoreCredential(
            GURL url, String username, String password) {
        return new PasswordStoreCredential(url, username, password);
    }

    private long mNativePasswordStoreBridge;
    private final PasswordStoreObserver mPasswordStoreObserver;

    /**
     * Observer listening to messages relevant to password store changes.
     */
    public interface PasswordStoreObserver {
        /**
         * Called when the set of password credentials is changed.
         *
         * @param count The total number of stored password credentials.
         */
        void onSavedPasswordsChanged(int count);

        /**
         * Called when a stored credential has been updated.
         *
         * @param credential Credential updated.
         */
        void onEdit(PasswordStoreCredential credential);
    }

    /**
     * Initializes its native counterpart.
     */
    public PasswordStoreBridge(PasswordStoreObserver passwordStoreObserver) {
        mNativePasswordStoreBridge = PasswordStoreBridgeJni.get().init(this);
        mPasswordStoreObserver = passwordStoreObserver;
    }

    @CalledByNative
    private void passwordListAvailable(int count) {
        mPasswordStoreObserver.onSavedPasswordsChanged(count);
    }

    @CalledByNative
    private void onEditCredential(PasswordStoreCredential credential) {
        mPasswordStoreObserver.onEdit(credential);
    }

    @CalledByNative
    private static void insertCredential(PasswordStoreCredential[] credentials, int index, GURL url,
            String username, String password) {
        credentials[index] = new PasswordStoreCredential(url, username, password);
    }

    /**
     * Inserts new credential into the password store.
     */
    public void insertPasswordCredential(PasswordStoreCredential credential) {
        PasswordStoreBridgeJni.get().insertPasswordCredential(
                mNativePasswordStoreBridge, credential);
    }

    /**
     * Updates an existing credential with a new password.
     *
     * @return True if credential was successfully updated, false otherwise.
     */
    public boolean editPassword(PasswordStoreCredential credential, String newPassword) {
        return PasswordStoreBridgeJni.get().editPassword(
                mNativePasswordStoreBridge, credential, newPassword);
    }

    /**
     * Returns the count of stored credentials.
     */
    public int getPasswordStoreCredentialsCount() {
        return PasswordStoreBridgeJni.get().getPasswordStoreCredentialsCount(
                mNativePasswordStoreBridge);
    }

    /**
     * Returns the list of credentials stored in the database.
     */
    public PasswordStoreCredential[] getAllCredentials() {
        PasswordStoreCredential[] credentials =
                new PasswordStoreCredential[getPasswordStoreCredentialsCount()];
        PasswordStoreBridgeJni.get().getAllCredentials(mNativePasswordStoreBridge, credentials);
        return credentials;
    }

    /**
     * Empties the password store.
     */
    public void clearAllPasswords() {
        PasswordStoreBridgeJni.get().clearAllPasswords(mNativePasswordStoreBridge);
    }

    /**
     * Destroys its C++ counterpart.
     */
    public void destroy() {
        if (mNativePasswordStoreBridge != 0) {
            PasswordStoreBridgeJni.get().destroy(mNativePasswordStoreBridge);
            mNativePasswordStoreBridge = 0;
        }
    }

    /**
     * C++ method signatures.
     */
    @NativeMethods
    interface Natives {
        long init(PasswordStoreBridge passwordStoreBridge);
        void insertPasswordCredential(
                long nativePasswordStoreBridge, PasswordStoreCredential credential);
        boolean editPassword(long nativePasswordStoreBridge, PasswordStoreCredential credential,
                String newPassword);
        int getPasswordStoreCredentialsCount(long nativePasswordStoreBridge);
        void getAllCredentials(
                long nativePasswordStoreBridge, PasswordStoreCredential[] credentials);
        void clearAllPasswords(long nativePasswordStoreBridge);
        void destroy(long nativePasswordStoreBridge);
    }
}
