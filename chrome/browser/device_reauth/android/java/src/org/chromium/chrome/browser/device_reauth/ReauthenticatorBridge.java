// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * Class handling the communication with the C++ part of the reauthentication based on device lock.
 * It forwards messages to and from its C++ counterpart and owns it.
 */
public class ReauthenticatorBridge {
    private long mNativeReauthenticatorBridge;
    private Callback<Boolean> mAuthResultCallback;

    public ReauthenticatorBridge(@BiometricAuthRequester int requester) {
        mNativeReauthenticatorBridge = ReauthenticatorBridgeJni.get().create(this, requester);
    }

    /**
     * Checks if authentication can be used. Note. Check is specific to the biometric
     * authentication.
     *
     * @return Whether authentication can be used.
     */
    public boolean canUseAuthentication() {
        return ReauthenticatorBridgeJni.get().canUseAuthentication(mNativeReauthenticatorBridge);
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

    @CalledByNative
    void onReauthenticationCompleted(boolean authSuccedeed) {
        if (mAuthResultCallback == null) return;
        mAuthResultCallback.onResult(authSuccedeed);
        mAuthResultCallback = null;
    }

    @NativeMethods
    interface Natives {
        long create(ReauthenticatorBridge reauthenticatorBridge, int requester);
        boolean canUseAuthentication(long nativeReauthenticatorBridge);
        void reauthenticate(long nativeReauthenticatorBridge, boolean useLastValidAuth);
    }
}
