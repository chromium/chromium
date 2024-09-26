// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.WindowAndroid;

class DeviceAuthenticatorBridge implements DeviceAuthenticatorController.Delegate {
    private long mNativeDeviceAuthenticator;
    private DeviceAuthenticatorController mController;

    private DeviceAuthenticatorBridge(
            long nativeDeviceAuthenticator, @Nullable FragmentActivity activity) {
        mNativeDeviceAuthenticator = nativeDeviceAuthenticator;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.BIOMETRIC_AUTH_IDENTITY_CHECK)
                && VERSION.SDK_INT >= VERSION_CODES.VANILLA_ICE_CREAM) {
            mController =
                    new MandatoryAuthenticatorControllerImpl(
                            ContextUtils.getApplicationContext(), this);
        } else if (ChromeFeatureList.isEnabled(ChromeFeatureList.DEVICE_AUTHENTICATOR_ANDROIDX)) {
            if (activity == null) return;
            mController = new AndroidxDeviceAuthenticatorControllerImpl(activity, this);
        } else {
            mController =
                    new DeviceAuthenticatorControllerImpl(
                            ContextUtils.getApplicationContext(), this);
        }
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
            long nativeDeviceAuthenticator, FragmentActivity activity) {
        return new DeviceAuthenticatorBridge(nativeDeviceAuthenticator, activity);
    }

    @CalledByNative
    @BiometricsAvailability
    int canAuthenticateWithBiometric() {
        if (mController == null) return BiometricsAvailability.OTHER_ERROR;

        return mController.canAuthenticateWithBiometric();
    }

    /**
     * A general method to check whether we can authenticate either via biometrics or screen lock.
     *
     * <p>True, if either biometrics are enrolled or screen lock is setup, false otherwise.
     */
    @CalledByNative
    boolean canAuthenticateWithBiometricOrScreenLock() {
        if (mController == null) return false;

        return mController.canAuthenticateWithBiometricOrScreenLock();
    }

    @CalledByNative
    void authenticate() {
        assert mController != null
                : "The authentication controller must not be null, call"
                        + " canAuthenticateWithBiometric before this.";

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
        if (mController == null) return;
        mController.cancel();
    }

    @NativeMethods
    interface Natives {
        void onAuthenticationCompleted(long nativeDeviceAuthenticatorBridgeImpl, int result);
    }
}
