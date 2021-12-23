// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;

/**
 * Interface to send backend requests to a downstream implementation to fulfill password store
 * jobs. All methods are expected to respond asynchronously to callbacks.
 */
public interface PasswordStoreAndroidBackend {
    /**
     * Serves as a general exception for failed requests to the PasswordStoreAndroidBackend.
     */
    public class BackendException extends Exception {
        public @AndroidBackendErrorType int errorCode;

        public BackendException(String message, @AndroidBackendErrorType int error) {
            super(message);
            errorCode = error;
        }
    }

    /**
     * Subscribes to notifications from the downstream implementation.
     *
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation.
     */
    default void subscribe(Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Unsubscribes from notifications from the downstream implementation.
     *
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     * TODO(crbug.com/1278767): Remove default keyword after downstream implementation.
     */
    default void unsubscribe(Runnable successCallback, Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve all logins.
     *
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     *
     * @deprecated use {@link #getAllLogins(@PasswordStoreOperationTarget int,
            Callback<byte[]>, Callback<Exception>)}.
     */
    @Deprecated
    default void getAllLogins(Callback<byte[]> loginsReply, Callback<Exception> failureCallback) {
        getAllLogins(PasswordStoreOperationTarget.DEFAULT, loginsReply, failureCallback);
    };

    /**
     * Triggers an async list call to retrieve all logins.
     *
     * @param target enum {@link StoreOperationTarget} to decide which storage to use.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    default void getAllLogins(@PasswordStoreOperationTarget int target,
            Callback<byte[]> loginsReply, Callback<Exception> failureCallback){};

    /**
     * Triggers an async list call to retrieve autofillable logins.
     *
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAutofillableLogins(Callback<byte[]> loginsReply, Callback<Exception> failureCallback);

    /**
     * Triggers an async list call to retrieve logins with matching signon realm.
     *
     * @param signonRealm Signon realm string matched by a substring match. The returned results
     * must be validated (e.g matching "sample.com" also returns logins for "not-sample.com").
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getLoginsForSignonRealm(
            String signonRealm, Callback<byte[]> loginsReply, Callback<Exception> failureCallback);

    /**
     * Triggers an async call to add a login to the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData identifying the login to be added.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void addLogin(
            byte[] pwdWithLocalData, Runnable successCallback, Callback<Exception> failureCallback);

    /**
     * Triggers an async call to update a login in the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData identifying the login to be updated.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void updateLogin(
            byte[] pwdWithLocalData, Runnable successCallback, Callback<Exception> failureCallback);

    /**
     * Triggers an async call to remove a login from store.
     *
     * @param pwdSpecificsData Serialized PasswordSpecificsData identifying the login to be deleted.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     *
     * * @deprecated use use {@link #removeLogin(byte[], @PasswordStoreOperationTarget int,
            Runnable, Callback<Exception>)}.
     */
    @Deprecated
    default void removeLogin(byte[] pwdSpecificsData, Runnable successCallback,
            Callback<Exception> failureCallback) {
        removeLogin(pwdSpecificsData, PasswordStoreOperationTarget.DEFAULT, successCallback,
                failureCallback);
    };

    /**
     * Triggers an async call to remove a login from store.
     *
     * @param pwdSpecificsData Serialized PasswordSpecificsData identifying the login to be deleted.
     * @param target enum {@link StoreOperationTarget} to decide which storage to use.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    default void removeLogin(byte[] pwdSpecificsData, @PasswordStoreOperationTarget int target,
            Runnable successCallback, Callback<Exception> failureCallback){};
}
