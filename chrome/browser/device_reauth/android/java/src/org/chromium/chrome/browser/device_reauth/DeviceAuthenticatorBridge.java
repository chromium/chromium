// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import androidx.fragment.app.FragmentActivity;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

@NullMarked
class DeviceAuthenticatorBridge implements DeviceAuthenticatorController.Delegate {
    private long mNativeDeviceAuthenticator;
    private final DeviceAuthenticatorController mController;

    private DeviceAuthenticatorBridge(
            long nativeDeviceAuthenticator, @Nullable FragmentActivity activity) {
        mNativeDeviceAuthenticator = nativeDeviceAuthenticator;
        mController = new DeviceAuthenticatorController(activity, this);
    }

    @CalledByNative
    private static DeviceAuthenticatorBridge createForWindow(
            long nativeDeviceAuthenticator, @Nullable WindowAndroid window) {
        FragmentActivity activity =
                (window == null || window.getActivity().get() == null)
                        ? null
                        : (FragmentActivity) window.getActivity().get();
        return new DeviceAuthenticatorBridge(nativeDeviceAuthenticator, activity);
    }

    @CalledByNative
    private static DeviceAuthenticatorBridge createForActivity(
            long nativeDeviceAuthenticator, @Nullable FragmentActivity activity) {
        return new DeviceAuthenticatorBridge(nativeDeviceAuthenticator, activity);
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
