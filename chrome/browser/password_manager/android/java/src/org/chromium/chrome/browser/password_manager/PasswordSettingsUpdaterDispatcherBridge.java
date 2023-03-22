// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnBackgroundThread;
import static org.chromium.chrome.browser.password_manager.PasswordManagerSetting.AUTO_SIGN_IN;
import static org.chromium.chrome.browser.password_manager.PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS;

import android.accounts.Account;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.signin.AccountUtils;

import java.util.Optional;

/**
 * Java-counterpart of the native PasswordSettingsUpdaterAndroidDispatcherBridge. It forwards
 * passwords settings updates from native to the downstream implementation. Response callbacks
 * are forwarded by a separate PasswordSettingsUpdaterReceiverBridge.
 */
@JNINamespace("password_manager")
public class PasswordSettingsUpdaterDispatcherBridge {
    private final PasswordSettingsAccessor mSettingsAccessor;
    private final PasswordSettingsUpdaterReceiverBridge mReceiverBridge;

    PasswordSettingsUpdaterDispatcherBridge(
            PasswordSettingsUpdaterReceiverBridge settingsUpdaterReceiverBridge,
            PasswordSettingsAccessor settingsAccessor) {
        assertOnBackgroundThread();
        assert settingsUpdaterReceiverBridge != null;
        assert settingsAccessor != null;
        mReceiverBridge = settingsUpdaterReceiverBridge;
        mSettingsAccessor = settingsAccessor;
    }

    @CalledByNative
    static PasswordSettingsUpdaterDispatcherBridge create(
            PasswordSettingsUpdaterReceiverBridge settingsUpdaterReceiverBridge) {
        return new PasswordSettingsUpdaterDispatcherBridge(settingsUpdaterReceiverBridge,
                PasswordSettingsAccessorFactoryImpl.getOrCreate().createAccessor());
    }

    @CalledByNative
    static boolean canCreateAccessor() {
        return PasswordSettingsAccessorFactoryImpl.getOrCreate().canCreateAccessor();
    }

    @CalledByNative
    void getSettingValue(String account, @PasswordManagerSetting int setting) {
        assertOnBackgroundThread();
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        setting, PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);
        switch (setting) {
            case OFFER_TO_SAVE_PASSWORDS:
                mSettingsAccessor.getOfferToSavePasswords(getAccount(account),
                        offerToSavePasswords
                        -> mReceiverBridge.onSettingValueFetched(
                                OFFER_TO_SAVE_PASSWORDS, offerToSavePasswords, metricsRecorder),
                        exception
                        -> handleFetchingExceptionOnUiThread(
                                OFFER_TO_SAVE_PASSWORDS, exception, metricsRecorder));
                break;
            case AUTO_SIGN_IN:
                mSettingsAccessor.getAutoSignIn(getAccount(account),
                        autoSignIn
                        -> mReceiverBridge.onSettingValueFetched(
                                AUTO_SIGN_IN, autoSignIn, metricsRecorder),
                        exception
                        -> handleFetchingExceptionOnUiThread(
                                AUTO_SIGN_IN, exception, metricsRecorder));
                break;
            default:
                assert false : "All settings need to be handled.";
        }
    }

    @CalledByNative
    void setSettingValue(String account, @PasswordManagerSetting int setting, boolean value) {
        assertOnBackgroundThread();
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        setting, PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);
        switch (setting) {
            case OFFER_TO_SAVE_PASSWORDS:
                mSettingsAccessor.setOfferToSavePasswords(value, getAccount(account),
                        unused
                        -> mReceiverBridge.onSettingValueSet(
                                OFFER_TO_SAVE_PASSWORDS, metricsRecorder),
                        exception
                        -> handleSettingExceptionOnUiThread(
                                OFFER_TO_SAVE_PASSWORDS, exception, metricsRecorder));
                break;
            case AUTO_SIGN_IN:
                mSettingsAccessor.setAutoSignIn(value, getAccount(account),
                        unused
                        -> mReceiverBridge.onSettingValueSet(AUTO_SIGN_IN, metricsRecorder),
                        exception
                        -> handleSettingExceptionOnUiThread(
                                AUTO_SIGN_IN, exception, metricsRecorder));
                break;
            default:
                assert false : "All settings need to be handled.";
        }
    }

    private void handleFetchingExceptionOnUiThread(@PasswordManagerSetting int setting,
            Exception exception, PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        // Error callback could be either triggered
        // - by the GMS Core on the UI thread
        // - by the downstream backend on the operation thread if preconditions are not met
        // |runOrPostTask| ensures callback will always be executed on the UI thread.
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT,
                () -> mReceiverBridge.handleFetchingException(setting, exception, metricsRecorder));
    }

    private void handleSettingExceptionOnUiThread(@PasswordManagerSetting int setting,
            Exception exception, PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        // Error callback could be either triggered
        // - by the GMS Core on the UI thread
        // - by the downstream backend on the operation thread if preconditions are not met
        // |runOrPostTask| ensures callback will always be executed on the UI thread.
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT,
                () -> mReceiverBridge.handleSettingException(setting, exception, metricsRecorder));
    }

    private Optional<Account> getAccount(String syncingAccount) {
        if (syncingAccount == null) return Optional.empty();
        return Optional.of(AccountUtils.createAccountFromName(syncingAccount));
    }
}
