// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Class handling the communication with the C++ part of the reauthentication based on device lock.
 * It forwards messages to and from its C++ counterpart and owns it.
 */
public class ReauthenticatorBridge {
    private static ReauthenticatorBridge sReauthenticatorBridgeForTesting;

    private long mNativeReauthenticatorBridge;
    private Callback<Boolean> mAuthResultCallback;

    private ReauthenticatorBridge(@DeviceAuthSource int source) {
        mNativeReauthenticatorBridge = ReauthenticatorBridgeJni.get().create(this, source);
    }

    /**
     * Checks if biometric authentication can be used.
     *
     * @return Whether authentication can be used.
     */
    public boolean canUseAuthenticationWithBiometric() {
        return ReauthenticatorBridgeJni.get().canUseAuthenticationWithBiometric(
                mNativeReauthenticatorBridge);
    }

    /**
     * Checks if biometric or screen lock authentication can be used.
     *
     * @return Whether authentication can be used.
     */
    public boolean canUseAuthenticationWithBiometricOrScreenLock() {
        return ReauthenticatorBridgeJni.get().canUseAuthenticationWithBiometricOrScreenLock(
                mNativeReauthenticatorBridge);
    }

    /**
     * Starts reauthentication. This method implies that the user will need to authenticate again if
     * they want to perform an authenticated action (i.e. the user will be considered not
     * authenticated immediately after the current action finishes).
     *
     * @param callback Callback that will be executed once request is done.
     */
    public void reauthenticate(Callback<Boolean> callback) {
        if (mAuthResultCallback == null) {
            mAuthResultCallback = callback;
            ReauthenticatorBridgeJni.get().reauthenticate(mNativeReauthenticatorBridge);
        }
    }

    /**
     * Create an instance of {@link ReauthenticatorBridge} based on the provided
     * {@link DeviceAuthSource}.
     * */
    public static ReauthenticatorBridge create(@DeviceAuthSource int source) {
        if (sReauthenticatorBridgeForTesting != null) {
            return sReauthenticatorBridgeForTesting;
        }
        return new ReauthenticatorBridge(source);
    }

    /** For testing only. */
    public static void setInstanceForTesting(ReauthenticatorBridge instance) {
        sReauthenticatorBridgeForTesting = instance;
        ResettersForTesting.register(() -> sReauthenticatorBridgeForTesting = null);
    }

    @CalledByNative
    void onReauthenticationCompleted(boolean authSuccedeed) {
        if (mAuthResultCallback == null) return;
        mAuthResultCallback.onResult(authSuccedeed);
        mAuthResultCallback = null;
    }

    @NativeMethods
    interface Natives {
        long create(ReauthenticatorBridge reauthenticatorBridge, int source);
        boolean canUseAuthenticationWithBiometric(long nativeReauthenticatorBridge);
        boolean canUseAuthenticationWithBiometricOrScreenLock(long nativeReauthenticatorBridge);
        void reauthenticate(long nativeReauthenticatorBridge);
    }
}
