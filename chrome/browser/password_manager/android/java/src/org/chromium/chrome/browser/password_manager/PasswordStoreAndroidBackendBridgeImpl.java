// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerUI;

import android.accounts.Account;
import android.app.PendingIntent;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.ResolvableApiException;
import com.google.common.base.Optional;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.AccountUtils;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Java-counterpart of the native PasswordStoreAndroidBackendBridgeImpl. It's part of the password
 * store backend that forwards password store operations to a downstream implementation.
 */
class PasswordStoreAndroidBackendBridgeImpl {
    private static final String TAG = "PwdStoreBackend";
    /**
     * Each operation sent to the passwords API will be assigned a JobId. The native side uses
     * this ID to map an API response to the job that invoked it.
     */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface JobId {}

    private final PasswordStoreAndroidBackend mBackend;
    private long mNativeBackendBridge;

    PasswordStoreAndroidBackendBridgeImpl(
            long nativeBackendBridge, PasswordStoreAndroidBackend backend) {
        mNativeBackendBridge = nativeBackendBridge;
        mBackend = backend;
        assert mBackend != null;
    }

    @CalledByNative
    static PasswordStoreAndroidBackendBridgeImpl create(long nativeBackendBridge) {
        return new PasswordStoreAndroidBackendBridgeImpl(nativeBackendBridge,
                PasswordStoreAndroidBackendFactory.getInstance().createBackend());
    }

    @CalledByNative
    static boolean canCreateBackend() {
        return PasswordStoreAndroidBackendFactory.getInstance().canCreateBackend();
    }

    @CalledByNative
    void getAllLogins(@JobId int jobId, String syncingAccount) {
        mBackend.getAllLogins(getAccount(syncingAccount), passwords -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onCompleteWithLogins(
                    mNativeBackendBridge, jobId, passwords);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void getAutofillableLogins(@JobId int jobId, String syncingAccount) {
        mBackend.getAutofillableLogins(getAccount(syncingAccount), passwords -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onCompleteWithLogins(
                    mNativeBackendBridge, jobId, passwords);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void getLoginsForSignonRealm(@JobId int jobId, String signonRealm, String syncingAccount) {
        mBackend.getLoginsForSignonRealm(signonRealm, getAccount(syncingAccount), passwords -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onCompleteWithLogins(
                    mNativeBackendBridge, jobId, passwords);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void addLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.addLogin(pwdWithLocalData, getAccount(syncingAccount), () -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onLoginAdded(
                    mNativeBackendBridge, jobId, pwdWithLocalData);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void updateLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.updateLogin(pwdWithLocalData, getAccount(syncingAccount), () -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onLoginUpdated(
                    mNativeBackendBridge, jobId, pwdWithLocalData);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void removeLogin(@JobId int jobId, byte[] pwdSpecificsData, String syncingAccount) {
        mBackend.removeLogin(pwdSpecificsData, getAccount(syncingAccount), () -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onLoginDeleted(
                    mNativeBackendBridge, jobId, pwdSpecificsData);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    private void handleAndroidBackendException(@JobId int jobId, Exception exception) {
        if (mNativeBackendBridge == 0) return;

        @AndroidBackendErrorType
        int error = AndroidBackendErrorType.UNCATEGORIZED;
        int api_error_code = 0; // '0' means SUCCESS.

        if (exception instanceof PasswordStoreAndroidBackend.BackendException) {
            error = ((PasswordStoreAndroidBackend.BackendException) exception).errorCode;
        }

        if (exception instanceof ApiException) {
            error = AndroidBackendErrorType.EXTERNAL_ERROR;
            api_error_code = ((ApiException) exception).getStatusCode();

            if (usesUnifiedPasswordManagerUI() && exception instanceof ResolvableApiException
                    && api_error_code != ChromeSyncStatusCode.AUTH_ERROR_RESOLVABLE) {
                // Backend error is user-recoverable, launch pending intent to allow the user to
                // resolve it. Resolution for the authentication errors is not launched as
                // user is requested to reauthenticate by Google services and Sync in Chrome.
                ResolvableApiException resolvableApiException = (ResolvableApiException) exception;
                PendingIntent pendingIntent = resolvableApiException.getResolution();
                try {
                    pendingIntent.send();
                } catch (PendingIntent.CanceledException e) {
                    Log.e(TAG, "Can not launch error resolution intent", e);
                }
            }
        }

        PasswordStoreAndroidBackendBridgeImplJni.get().onError(
                mNativeBackendBridge, jobId, error, api_error_code);
    }

    private Optional<Account> getAccount(String syncingAccount) {
        if (syncingAccount == null) return Optional.absent();
        return Optional.of(AccountUtils.createAccountFromName(syncingAccount));
    }

    @CalledByNative
    private void destroy() {
        mNativeBackendBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativePasswordStoreAndroidBackendBridgeImpl,
                @JobId int jobId, byte[] passwords);
        void onLoginAdded(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId,
                byte[] pwdWithLocalData);
        void onLoginUpdated(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId,
                byte[] pwdWithLocalData);
        void onLoginDeleted(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId,
                byte[] pwdSpecificsData);
        void onError(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId,
                int errorType, int apiErrorCode);
    }
}
