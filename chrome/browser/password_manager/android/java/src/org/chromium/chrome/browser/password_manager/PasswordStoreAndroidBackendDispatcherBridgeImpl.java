// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.accounts.Account;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackendReceiverBridgeImpl.JobId;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.signin.AccountUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;

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
        return new PasswordStoreAndroidBackendDispatcherBridgeImpl(backendReceiverBridge,
                PasswordStoreAndroidBackendFactory.getInstance().createBackend());
    }

    @CalledByNative
    static boolean canCreateBackend() {
        return PasswordStoreAndroidBackendFactory.getInstance().canCreateBackend();
    }

    @CalledByNative
    void getAllLogins(@JobId int jobId, String syncingAccount) {
        mBackend.getAllLogins(getAccount(syncingAccount),
                passwords
                -> mBackendReceiverBridge.onCompleteWithLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void getAutofillableLogins(@JobId int jobId, String syncingAccount) {
        mBackend.getAutofillableLogins(getAccount(syncingAccount),
                passwords
                -> mBackendReceiverBridge.onCompleteWithLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void getLoginsForSignonRealm(@JobId int jobId, String signonRealm, String syncingAccount) {
        mBackend.getLoginsForSignonRealm(signonRealm, getAccount(syncingAccount),
                passwords
                -> mBackendReceiverBridge.onCompleteWithLogins(jobId, passwords),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void addLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.addLogin(pwdWithLocalData, getAccount(syncingAccount),
                ()
                        -> mBackendReceiverBridge.onLoginChanged(jobId),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void updateLogin(@JobId int jobId, byte[] pwdWithLocalData, String syncingAccount) {
        mBackend.updateLogin(pwdWithLocalData, getAccount(syncingAccount),
                ()
                        -> mBackendReceiverBridge.onLoginChanged(jobId),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    @CalledByNative
    void removeLogin(@JobId int jobId, byte[] pwdSpecificsData, String syncingAccount) {
        mBackend.removeLogin(pwdSpecificsData, getAccount(syncingAccount),
                ()
                        -> mBackendReceiverBridge.onLoginChanged(jobId),
                exception -> handleAndroidBackendExceptionOnUiThread(jobId, exception));
    }

    private void handleAndroidBackendExceptionOnUiThread(@JobId int jobId, Exception exception) {
        // Error callback could be either triggered
        // - by the GMS Core on the UI thread
        // - by the password store downstream backend on the operation thread
        // |runOrPostTask| ensures callback will always be executed on the UI thread.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> mBackendReceiverBridge.handleAndroidBackendException(jobId, exception));
    }

    private Optional<Account> getAccount(String syncingAccount) {
        if (syncingAccount == null) return Optional.empty();
        return Optional.of(AccountUtils.createAccountFromName(syncingAccount));
    }

    // This method interacts with the UI and should be executed on the UI thread. Native dispatcher
    // bridge however does not have JNIEnv for UI thread and calls this method on background thread.
    // Operation is reposted on the default UI sequence for execution.
    @CalledByNative
    private void showErrorUi() {
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> showErrorUiOnMainThread());
    }

    private void showErrorUiOnMainThread() {
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
}
