// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;
import android.content.Context;

import com.google.common.base.Optional;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
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
            PasswordStoreAndroidBackendBridgeImplJni.get().onLoginChanged(
                    mNativeBackendBridge, jobId);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void updateLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.updateLogin(pwdWithLocalData, getAccount(syncingAccount), () -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onLoginChanged(
                    mNativeBackendBridge, jobId);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    @CalledByNative
    void removeLogin(@JobId int jobId, byte[] pwdSpecificsData, String syncingAccount) {
        mBackend.removeLogin(pwdSpecificsData, getAccount(syncingAccount), () -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onLoginChanged(
                    mNativeBackendBridge, jobId);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    private void handleAndroidBackendException(@JobId int jobId, Exception exception) {
        if (mNativeBackendBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
        Integer connectionResultCode =
                PasswordManagerAndroidBackendUtil.getConnectionResultCode(exception);

        PasswordStoreAndroidBackendBridgeImplJni.get().onError(mNativeBackendBridge, jobId, error,
                apiErrorCode, connectionResultCode != null,
                connectionResultCode == null ? -1 : connectionResultCode.intValue());
    }

    private Optional<Account> getAccount(String syncingAccount) {
        if (syncingAccount == null) return Optional.absent();
        return Optional.of(AccountUtils.createAccountFromName(syncingAccount));
    }

    @CalledByNative
    private void showErrorUi() {
        Context context = ContextUtils.getApplicationContext();
        // The context can sometimes be null in tests.
        if (context == null) return;
        String title = context.getString(R.string.upm_error_notification_title);
        String contents = context.getString(R.string.upm_error_notification_contents);
        NotificationManagerProxy notificationManager = new NotificationManagerProxyImpl(context);
        NotificationWrapperBuilder notificationWrapperBuilder =
                NotificationWrapperBuilderFactory
                        .createNotificationWrapperBuilder(ChannelId.BROWSER,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.UPM_ERROR,
                                        null, NotificationConstants.NOTIFICATION_ID_UPM))
                        .setAutoCancel(false)
                        .setContentTitle(title)
                        .setContentText(contents)
                        .setSmallIcon(PasswordManagerResourceProviderFactory.create()
                                              .getPasswordManagerIcon())
                        .setTicker(contents)
                        .setLocalOnly(true);
        NotificationWrapper notification =
                notificationWrapperBuilder.buildWithBigTextStyle(contents);
        notificationManager.notify(notification);
    }

    @CalledByNative
    private void destroy() {
        mNativeBackendBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativePasswordStoreAndroidBackendBridgeImpl,
                @JobId int jobId, byte[] passwords);
        void onLoginChanged(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId);
        void onError(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId,
                int errorType, int apiErrorCode, boolean hasConnectionResult,
                int connectionResultStatusCode);
    }
}
