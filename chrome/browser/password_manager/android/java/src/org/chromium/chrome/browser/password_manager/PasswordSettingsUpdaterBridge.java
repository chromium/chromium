// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerSetting.AUTO_SIGN_IN;
import static org.chromium.chrome.browser.password_manager.PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS;

import android.accounts.Account;

import com.google.android.gms.common.api.ResolvableApiException;
import com.google.common.base.Optional;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.AccountUtils;

/**
 * Java-counterpart of the native PasswordSettingsUpdaterAndroidBridgeImpl. It forwards passwords
 * settings updates from native to the downstream implementation and vice-versa.
 */
public class PasswordSettingsUpdaterBridge {
    private final PasswordSettingsAccessor mSettingsAccessor;
    private long mNativeSettingsUpdaterBridge;

    PasswordSettingsUpdaterBridge(
            long nativeSettingsUpdaterBridge, PasswordSettingsAccessor settingsAccessor) {
        assert settingsAccessor != null;
        mNativeSettingsUpdaterBridge = nativeSettingsUpdaterBridge;
        mSettingsAccessor = settingsAccessor;
    }

    @CalledByNative
    static PasswordSettingsUpdaterBridge create(long nativeSettingsUpdaterBridge) {
        return new PasswordSettingsUpdaterBridge(nativeSettingsUpdaterBridge,
                new PasswordSettingsAccessorFactoryImpl().createAccessor());
    }

    @CalledByNative
    static boolean canCreateAccessor() {
        return new PasswordSettingsAccessorFactoryImpl().canCreateAccessor();
    }

    @CalledByNative
    void getSettingValue(String account, @PasswordManagerSetting int setting) {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        setting, PasswordSettingsUpdaterMetricsRecorder.GET_VALUE_FUNCTION_SUFFIX);
        switch (setting) {
            case OFFER_TO_SAVE_PASSWORDS:
                mSettingsAccessor.getOfferToSavePasswords(getAccount(account),
                        offerToSavePasswords
                        -> onSettingValueFetched(
                                OFFER_TO_SAVE_PASSWORDS, offerToSavePasswords, metricsRecorder),
                        exception
                        -> handleFetchingException(
                                OFFER_TO_SAVE_PASSWORDS, exception, metricsRecorder));
                break;
            case AUTO_SIGN_IN:
                mSettingsAccessor.getAutoSignIn(getAccount(account),
                        autoSignIn
                        -> onSettingValueFetched(AUTO_SIGN_IN, autoSignIn, metricsRecorder),
                        exception
                        -> handleFetchingException(AUTO_SIGN_IN, exception, metricsRecorder));
                break;
            default:
                assert false : "All settings need to be handled.";
        }
    }

    @CalledByNative
    void setSettingValue(String account, @PasswordManagerSetting int setting, boolean value) {
        PasswordSettingsUpdaterMetricsRecorder metricsRecorder =
                new PasswordSettingsUpdaterMetricsRecorder(
                        setting, PasswordSettingsUpdaterMetricsRecorder.SET_VALUE_FUNCTION_SUFFIX);
        switch (setting) {
            case OFFER_TO_SAVE_PASSWORDS:
                mSettingsAccessor.setOfferToSavePasswords(value, getAccount(account),
                        unused
                        -> onSettingValueSet(OFFER_TO_SAVE_PASSWORDS, metricsRecorder),
                        exception
                        -> handleSettingException(
                                OFFER_TO_SAVE_PASSWORDS, exception, metricsRecorder));
                break;
            case AUTO_SIGN_IN:
                mSettingsAccessor.setAutoSignIn(value, getAccount(account),
                        unused
                        -> onSettingValueSet(AUTO_SIGN_IN, metricsRecorder),
                        exception
                        -> handleSettingException(AUTO_SIGN_IN, exception, metricsRecorder));
                break;
            default:
                assert false : "All settings need to be handled.";
        }
    }

    private void onSettingValueFetched(@PasswordManagerSetting int setting,
            Optional<Boolean> settingValue,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        metricsRecorder.recordMetrics(null);
        if (mNativeSettingsUpdaterBridge == 0) return;
        if (settingValue.isPresent()) {
            PasswordSettingsUpdaterBridgeJni.get().onSettingValueFetched(
                    mNativeSettingsUpdaterBridge, setting, settingValue.get());
            return;
        }
        PasswordSettingsUpdaterBridgeJni.get().onSettingValueAbsent(
                mNativeSettingsUpdaterBridge, setting);
    }

    private void handleFetchingException(@PasswordManagerSetting int setting, Exception exception,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        metricsRecorder.recordMetrics(exception);
        if (mNativeSettingsUpdaterBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);

        if (exception instanceof ResolvableApiException) {
            PasswordManagerAndroidBackendUtil.handleResolvableApiException(
                    (ResolvableApiException) exception);
        }

        PasswordSettingsUpdaterBridgeJni.get().onSettingFetchingError(
                mNativeSettingsUpdaterBridge, setting, error, apiErrorCode);
    }

    private void onSettingValueSet(@PasswordManagerSetting int setting,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        metricsRecorder.recordMetrics(null);
        if (mNativeSettingsUpdaterBridge == 0) return;
        PasswordSettingsUpdaterBridgeJni.get().onSuccessfulSettingChange(
                mNativeSettingsUpdaterBridge, setting);
    }

    private void handleSettingException(@PasswordManagerSetting int setting, Exception exception,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        metricsRecorder.recordMetrics(exception);
        if (mNativeSettingsUpdaterBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);

        if (exception instanceof ResolvableApiException) {
            PasswordManagerAndroidBackendUtil.handleResolvableApiException(
                    (ResolvableApiException) exception);
        }

        PasswordSettingsUpdaterBridgeJni.get().onFailedSettingChange(
                mNativeSettingsUpdaterBridge, setting, error, apiErrorCode);
    }

    @CalledByNative
    private void destroy() {
        mNativeSettingsUpdaterBridge = 0;
    }

    private Optional<Account> getAccount(String syncingAccount) {
        if (syncingAccount == null) return Optional.absent();
        return Optional.of(AccountUtils.createAccountFromName(syncingAccount));
    }

    @NativeMethods
    interface Natives {
        void onSettingValueFetched(long nativePasswordSettingsUpdaterAndroidBridgeImpl, int setting,
                boolean offerToSavePasswordsEnabled);
        void onSettingValueAbsent(long nativePasswordSettingsUpdaterAndroidBridgeImpl, int setting);
        void onSettingFetchingError(long nativePasswordSettingsUpdaterAndroidBridgeImpl,
                int setting, int error, int apiErrorCode);
        void onSuccessfulSettingChange(
                long nativePasswordSettingsUpdaterAndroidBridgeImpl, int setting);
        void onFailedSettingChange(long nativePasswordSettingsUpdaterAndroidBridgeImpl, int setting,
                int error, int apiErrorCode);
    }
}
