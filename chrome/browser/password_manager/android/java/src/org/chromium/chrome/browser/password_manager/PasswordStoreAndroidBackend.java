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
     * Triggers an async list call to retrieve all logins.
     *
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAllLogins(Callback<byte[]> loginsReply, Callback<Exception> failureCallback);

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
     */
    void removeLogin(
            byte[] pwdSpecificsData, Runnable successCallback, Callback<Exception> failureCallback);
}
