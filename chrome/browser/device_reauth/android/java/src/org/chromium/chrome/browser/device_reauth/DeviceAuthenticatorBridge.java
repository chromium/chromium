// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

class DeviceAuthenticatorBridge implements DeviceAuthenticatorController.Delegate {
    private final Context mContext;
    private long mNativeDeviceAuthenticator;
    private DeviceAuthenticatorController mController;

    private DeviceAuthenticatorBridge(long nativeDeviceAuthenticator) {
        mContext = ContextUtils.getApplicationContext();
        mNativeDeviceAuthenticator = nativeDeviceAuthenticator;
        mController =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DEVICE_AUTHENTICATOR_ANDROIDX)
                        ? new AndroidxDeviceAuthenticatorControllerImpl()
                        : new DeviceAuthenticatorControllerImpl(mContext, this);
    }

    @CalledByNative
    private static DeviceAuthenticatorBridge create(long nativeDeviceAuthenticator) {
        return new DeviceAuthenticatorBridge(nativeDeviceAuthenticator);
    }

    @CalledByNative
    @BiometricsAvailability
    int canAuthenticateWithBiometric() {
        return mController.canAuthenticateWithBiometric();
    }

    /**
     * A general method to check whether we can authenticate either via biometrics or screen lock.
     *
     * <p>True, if either biometrics are enrolled or screen lock is setup, false otherwise.
     */
    @CalledByNative
    boolean canAuthenticateWithBiometricOrScreenLock() {
        return mController.canAuthenticateWithBiometricOrScreenLock();
    }

    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.P)
    void authenticate() {
        mController.authenticate();
    }

    @Override
    public void onAuthenticationCompleted(@DeviceAuthUIResult int result) {
        if (mNativeDeviceAuthenticator != 0) {
            DeviceAuthenticatorBridgeJni.get()
                    .onAuthenticationCompleted(mNativeDeviceAuthenticator, result);
        }
    }

    @CalledByNative
    void destroy() {
        mNativeDeviceAuthenticator = 0;
        cancel();
    }

    @CalledByNative
    void cancel() {
        mController.cancel();
    }

    @NativeMethods
    interface Natives {
        void onAuthenticationCompleted(long nativeDeviceAuthenticatorBridgeImpl, int result);
    }
}
