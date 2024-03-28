// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackendReceiverBridgeImpl.JobId;
import org.chromium.components.signin.AccountUtils;

import java.util.Optional;

/**
 * Java-counterpart of the native PasswordStoreAndroidBackendDispatcherBridgeImpl. It's part of the
 * password store backend that forwards password store operations to a downstream implementation.
 */
@JNINamespace("password_manager")
class PasswordStoreAndroidBackendDispatcherBridgeImpl {
    private final PasswordStoreAndroidBackend mBackend;
    private final PasswordStoreAndroidBackendReceiverBridgeImpl mBackendReceiverBridge;

    PasswordStoreAndroidBackendDispatcherBridgeImpl(
            PasswordStoreAndroidBackendReceiverBridgeImpl backendReceiverBridge,
            PasswordStoreAndroidBackend backend) {
        mBackendReceiverBridge = backendReceiverBridge;
        mBackend = backend;
        assert mBackend != null;
    }

    @CalledByNative
    static PasswordStoreAndroidBackendDispatcherBridgeImpl create(
            PasswordStoreAndroidBackendReceiverBridgeImpl backendReceiverBridge) {
        return new PasswordStoreAndroidBackendDispatcherBridgeImpl(
                backendReceiverBridge,
                PasswordStoreAndroidBackendFactory.getInstance().createBackend());
    }

    @CalledByNative
    void getAllLogins(@JobId int jobId, String syncingAccount) {
        mBackend.getAllLogins(
                getAccount(syncingAccount),
                passwords -> mBackendReceiverBridge.onCompleteWithLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void getAllLoginsWithBrandingInfo(@JobId int jobId, String syncingAccount) {
        mBackend.getAllLoginsWithBrandingInfo(
                getAccount(syncingAccount),
                passwords -> mBackendReceiverBridge.onCompleteWithBrandedLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void getAutofillableLogins(@JobId int jobId, String syncingAccount) {
        mBackend.getAutofillableLogins(
                getAccount(syncingAccount),
                passwords -> mBackendReceiverBridge.onCompleteWithLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void getLoginsForSignonRealm(@JobId int jobId, String signonRealm, String syncingAccount) {
        mBackend.getLoginsForSignonRealm(
                signonRealm,
                getAccount(syncingAccount),
                passwords -> mBackendReceiverBridge.onCompleteWithLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void getAffiliatedLoginsForSignonRealm(
            @JobId int jobId, String signonRealm, String syncingAccount) {
        mBackend.getAffiliatedLoginsForSignonRealm(
                signonRealm,
                getAccount(syncingAccount),
                passwords ->
                        mBackendReceiverBridge.onCompleteWithAffiliatedLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void addLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.addLogin(
                pwdWithLocalData,
                getAccount(syncingAccount),
                () -> mBackendReceiverBridge.onLoginChanged(jobId),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void updateLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.updateLogin(
                pwdWithLocalData,
                getAccount(syncingAccount),
                () -> mBackendReceiverBridge.onLoginChanged(jobId),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void removeLogin(@JobId int jobId, byte[] pwdSpecificsData, String syncingAccount) {
        mBackend.removeLogin(
                pwdSpecificsData,
                getAccount(syncingAccount),
                () -> mBackendReceiverBridge.onLoginChanged(jobId),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    private void handleAndroidBackendExceptionOnUiThread(@JobId int jobId, Exception exception) {
        // Error callback could be either triggered
        // - by the GMS Core on the UI thread
        // - by the password store downstream backend on the operation thread
        // |runOrPostTask| ensures callback will always be executed on the UI thread.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mBackendReceiverBridge.handleAndroidBackendException(jobId, exception));
    }

    private Optional<Account> getAccount(String syncingAccount) {
        if (syncingAccount == null) return Optional.empty();
        return Optional.of(AccountUtils.createAccountFromName(syncingAccount));
    }
}
