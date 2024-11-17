// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;

import org.chromium.base.Callback;

import java.util.Date;
import java.util.Optional;

/**
 * Interface to send backend requests to a downstream implementation to fulfill password store
 * jobs. All methods are expected to respond asynchronously to callbacks.
 */
public interface PasswordStoreAndroidBackend {
    /** Serves as a general exception for failed requests to the PasswordStoreAndroidBackend. */
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
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *     account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *     org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAllLogins(
            Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async list call to retrieve all logins withing given time frame.
     *
     * @param createdAfter Filters for passwords that were created after this date.
     * @param createdBefore Filters for passwords that were created before this date.
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *         account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAllLoginsBetween(
            Date createdAfter,
            Date createdBefore,
            Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async list call to retrieve all logins with branding info.
     *
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *     account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *     org.chromium.components.password_manager.core.browser.proto.ListPasswordsWithUiInfoResult
     *     } data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAllLoginsWithBrandingInfo(
            Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async list call to retrieve autofillable logins.
     *
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *     account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *     org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAutofillableLogins(
            Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async list call to retrieve logins with matching signon realm.
     *
     * @param signonRealm Signon realm string matched by a substring match. The returned results
     * must be validated (e.g matching "sample.com" also returns logins for "not-sample.com").
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *         account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult}
     * data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getLoginsForSignonRealm(
            String signonRealm,
            Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async list call to retrieve affiliated logins with matching signon realm. This
     * includes credential sharing affiliation and grouping affiliation.
     *
     * @param signonRealm Signon realm string matched by a substring match. The returned results
     *     must be validated (e.g matching "sample.com" also returns logins for "not-sample.com").
     * @param syncingAccount Account used to sync passwords. If the syncingAccount is empty local
     *     account will be used.
     * @param loginsReply Callback that is called on success with serialized {@link
     *     org.chromium.components.password_manager.core.browser.proto.ListAffiliatedPasswordsResult}
     *     data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAffiliatedLoginsForSignonRealm(
            String signonRealm,
            Optional<Account> syncingAccount,
            Callback<byte[]> loginsReply,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async call to add a login to the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData to be added.
     * @param syncingAccount Account used to sync passwords. If Nullopt was provided local account
     *         will be used.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void addLogin(
            byte[] pwdWithLocalData,
            Optional<Account> syncingAccount,
            Runnable successCallback,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async call to update a login in the store.
     *
     * @param pwdWithLocalData Serialized PasswordWithLocalData replacing a login with the same
     *         unique key.
     * @param syncingAccount Account used to sync passwords. If Nullopt was provided local account
     *         will be used.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void updateLogin(
            byte[] pwdWithLocalData,
            Optional<Account> syncingAccount,
            Runnable successCallback,
            Callback<Exception> failureCallback);

    /**
     * Triggers an async call to remove a login from store.
     *
     * @param pwdSpecificsData Serialized PasswordSpecificsData identifying the login to be deleted.
     * @param syncingAccount Account used to sync passwords. If Nullopt was provided local account
     *         will be used.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void removeLogin(
            byte[] pwdSpecificsData,
            Optional<Account> syncingAccount,
            Runnable successCallback,
            Callback<Exception> failureCallback);
}
