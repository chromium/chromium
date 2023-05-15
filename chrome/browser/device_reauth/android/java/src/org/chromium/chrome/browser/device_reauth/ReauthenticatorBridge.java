// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
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

    private ReauthenticatorBridge(@DeviceAuthRequester int requester) {
        mNativeReauthenticatorBridge = ReauthenticatorBridgeJni.get().create(this, requester);
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
     * Starts reauthentication.
     *
     * @param callback Callback that will be executed once request is done.
     * @param useLastValidAuth A boolean value indicating whether to consider the last but "recent"
     *         validated auth for passing the current authentication request.
     */
    public void reauthenticate(Callback<Boolean> callback, boolean useLastValidAuth) {
        assert mAuthResultCallback == null;
        mAuthResultCallback = callback;
        ReauthenticatorBridgeJni.get().reauthenticate(
                mNativeReauthenticatorBridge, useLastValidAuth);
    }

    /**
     * Create an instance of {@link ReauthenticatorBridge} based on the provided
     * {@link DeviceAuthRequester}.
     * */
    public static ReauthenticatorBridge create(@DeviceAuthRequester int requester) {
        if (sReauthenticatorBridgeForTesting != null) {
            return sReauthenticatorBridgeForTesting;
        }
        return new ReauthenticatorBridge(requester);
    }

    /** For testing only. */
    @VisibleForTesting
    public static void setInstanceForTesting(ReauthenticatorBridge instance) {
        sReauthenticatorBridgeForTesting = instance;
    }

    @CalledByNative
    void onReauthenticationCompleted(boolean authSuccedeed) {
        if (mAuthResultCallback == null) return;
        mAuthResultCallback.onResult(authSuccedeed);
        mAuthResultCallback = null;
    }

    @NativeMethods
    interface Natives {
        long create(ReauthenticatorBridge reauthenticatorBridge, int requester);
        boolean canUseAuthenticationWithBiometric(long nativeReauthenticatorBridge);
        boolean canUseAuthenticationWithBiometricOrScreenLock(long nativeReauthenticatorBridge);
        void reauthenticate(long nativeReauthenticatorBridge, boolean useLastValidAuth);
    }
}
