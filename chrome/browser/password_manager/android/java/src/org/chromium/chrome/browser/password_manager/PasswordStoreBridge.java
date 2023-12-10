// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
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
    private final ObserverList<PasswordStoreObserver> mObserverList;
    private int mPasswordsCount = -1;

    /** Observer listening to messages relevant to password store changes. */
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

    /** Initializes its native counterpart. */
    public PasswordStoreBridge() {
        mNativePasswordStoreBridge = PasswordStoreBridgeJni.get().init(this);
        mObserverList = new ObserverList<>();
    }

    @CalledByNative
    private void passwordListAvailable(int count) {
        mPasswordsCount = count;
        for (PasswordStoreObserver obs : mObserverList) {
            obs.onSavedPasswordsChanged(count);
        }
    }

    @CalledByNative
    private void onEditCredential(PasswordStoreCredential credential) {
        for (PasswordStoreObserver obs : mObserverList) {
            obs.onEdit(credential);
        }
    }

    @CalledByNative
    private static void insertCredential(
            PasswordStoreCredential[] credentials,
            int index,
            GURL url,
            String username,
            String password) {
        credentials[index] = new PasswordStoreCredential(url, username, password);
    }

    /** Inserts new credential into the password store. */
    @VisibleForTesting
    public void insertPasswordCredential(PasswordStoreCredential credential) {
        PasswordStoreBridgeJni.get()
                .insertPasswordCredentialForTesting(mNativePasswordStoreBridge, credential);
    }

    public void blocklistForTesting(String url) {
        PasswordStoreBridgeJni.get().blocklistForTesting(mNativePasswordStoreBridge, url);
    }

    /**
     * Updates an existing credential with a new password.
     *
     * @return True if credential was successfully updated, false otherwise.
     */
    public boolean editPassword(PasswordStoreCredential credential, String newPassword) {
        return PasswordStoreBridgeJni.get()
                .editPassword(mNativePasswordStoreBridge, credential, newPassword);
    }

    /** Returns the count of stored credentials. */
    public int getPasswordStoreCredentialsCount() {
        return PasswordStoreBridgeJni.get()
                .getPasswordStoreCredentialsCount(mNativePasswordStoreBridge);
    }

    /** Returns the list of credentials stored in the database. */
    public PasswordStoreCredential[] getAllCredentials() {
        PasswordStoreCredential[] credentials =
                new PasswordStoreCredential[getPasswordStoreCredentialsCount()];
        PasswordStoreBridgeJni.get().getAllCredentials(mNativePasswordStoreBridge, credentials);
        return credentials;
    }

    /** Empties the password store. */
    public void clearAllPasswords() {
        PasswordStoreBridgeJni.get().clearAllPasswords(mNativePasswordStoreBridge);
    }

    /** Destroys its C++ counterpart. */
    public void destroy() {
        if (mNativePasswordStoreBridge != 0) {
            PasswordStoreBridgeJni.get().destroy(mNativePasswordStoreBridge);
            mNativePasswordStoreBridge = 0;
        }
    }

    /**
     * Adds a new observer to the list of observers.
     * @param obs An {@link PasswordStoreObserver} implementation instance.
     * @param callImmediatelyIfReady Invokes {@link PasswordStoreObserver#onSavedPasswordsChanged}
     *   on the observer if passwords are already fetched when this is true.
     */
    public void addObserver(PasswordStoreObserver obs, boolean callImmediatelyIfReady) {
        mObserverList.addObserver(obs);
        if (callImmediatelyIfReady && mPasswordsCount != -1) {
            obs.onSavedPasswordsChanged(mPasswordsCount);
        }
    }

    /**
     * Removes a given observer from the observers list.
     * @param obs An {@link PasswordStoreObserver} implementation instance.
     */
    public void removeObserver(PasswordStoreObserver obs) {
        mObserverList.removeObserver(obs);
    }

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        long init(PasswordStoreBridge passwordStoreBridge);

        void insertPasswordCredentialForTesting(
                long nativePasswordStoreBridge, PasswordStoreCredential credential);

        void blocklistForTesting(long nativePasswordStoreBridge, String url);

        boolean editPassword(
                long nativePasswordStoreBridge,
                PasswordStoreCredential credential,
                String newPassword);

        int getPasswordStoreCredentialsCount(long nativePasswordStoreBridge);

        void getAllCredentials(
                long nativePasswordStoreBridge, PasswordStoreCredential[] credentials);

        void clearAllPasswords(long nativePasswordStoreBridge);

        void destroy(long nativePasswordStoreBridge);
    }
}
