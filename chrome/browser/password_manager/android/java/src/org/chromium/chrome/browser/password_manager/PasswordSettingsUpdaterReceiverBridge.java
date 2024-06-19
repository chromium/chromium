// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.util.Optional;

/**
 * Java-counterpart of the native PasswordSettingsUpdaterAndroidReceiverBridge. It forwards
 * passwords settings update callbacks from the downstream java implementation to native.
 */
@JNINamespace("password_manager")
public class PasswordSettingsUpdaterReceiverBridge {
    private long mNativeReceiverBridge;

    PasswordSettingsUpdaterReceiverBridge(long nativeReceiverBridge) {
        mNativeReceiverBridge = nativeReceiverBridge;
    }

    @CalledByNative
    static PasswordSettingsUpdaterReceiverBridge create(long nativeReceiverBridge) {
        assertOnUiThread();
        return new PasswordSettingsUpdaterReceiverBridge(nativeReceiverBridge);
    }

    void onSettingValueFetched(
            @PasswordManagerSetting int setting,
            Optional<Boolean> settingValue,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        assertOnUiThread();
        metricsRecorder.recordMetrics(null);
        if (mNativeReceiverBridge == 0) return;

        if (settingValue.isPresent()) {
            PasswordSettingsUpdaterReceiverBridgeJni.get()
                    .onSettingValueFetched(mNativeReceiverBridge, setting, settingValue.get());
            return;
        }
        PasswordSettingsUpdaterReceiverBridgeJni.get()
                .onSettingValueAbsent(mNativeReceiverBridge, setting);
    }

    void handleFetchingException(
            @PasswordManagerSetting int setting,
            Exception exception,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        assertOnUiThread();
        metricsRecorder.recordMetrics(exception);
        if (mNativeReceiverBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);

        PasswordSettingsUpdaterReceiverBridgeJni.get()
                .onSettingFetchingError(mNativeReceiverBridge, setting, error, apiErrorCode);
    }

    void onSettingValueSet(
            @PasswordManagerSetting int setting,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        assertOnUiThread();
        metricsRecorder.recordMetrics(null);
        if (mNativeReceiverBridge == 0) return;

        PasswordSettingsUpdaterReceiverBridgeJni.get()
                .onSuccessfulSettingChange(mNativeReceiverBridge, setting);
    }

    void handleSettingException(
            @PasswordManagerSetting int setting,
            Exception exception,
            PasswordSettingsUpdaterMetricsRecorder metricsRecorder) {
        assertOnUiThread();
        metricsRecorder.recordMetrics(exception);
        if (mNativeReceiverBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);

        PasswordSettingsUpdaterReceiverBridgeJni.get()
                .onFailedSettingChange(mNativeReceiverBridge, setting, error, apiErrorCode);
    }

    void destroyForTesting() {
        mNativeReceiverBridge = 0;
    }

    @CalledByNative
    private void destroy() {
        assertOnUiThread();
        mNativeReceiverBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onSettingValueFetched(
                long nativePasswordSettingsUpdaterAndroidReceiverBridgeImpl,
                int setting,
                boolean offerToSavePasswordsEnabled);

        void onSettingValueAbsent(
                long nativePasswordSettingsUpdaterAndroidReceiverBridgeImpl, int setting);

        void onSettingFetchingError(
                long nativePasswordSettingsUpdaterAndroidReceiverBridgeImpl,
                int setting,
                int error,
                int apiErrorCode);

        void onSuccessfulSettingChange(
                long nativePasswordSettingsUpdaterAndroidReceiverBridgeImpl, int setting);

        void onFailedSettingChange(
                long nativePasswordSettingsUpdaterAndroidReceiverBridgeImpl,
                int setting,
                int error,
                int apiErrorCode);
    }
}
