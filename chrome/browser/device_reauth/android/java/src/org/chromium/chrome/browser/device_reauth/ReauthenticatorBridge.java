// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Class handling the communication with the C++ part of the reauthentication based on device lock.
 * It forwards messages to and from its C++ counterpart and owns it.
 */
public class ReauthenticatorBridge {
    private static ReauthenticatorBridge sReauthenticatorBridgeForTesting;

    private long mNativeReauthenticatorBridge;
    private Callback<Boolean> mAuthResultCallback;

    private ReauthenticatorBridge(
            Activity activity, Profile profile, @DeviceAuthSource int source) {
        mNativeReauthenticatorBridge =
                ReauthenticatorBridgeJni.get().create(this, activity, profile, source);
    }

    /**
     * Checks biometric auth availability status. It returns one of the following:
     * <li>REQUIRED - biometric is mandatory,
     * <li>BIOMETRICS_AVAILABLE - biometric auth is available but not mandatory,
     * <li>ONLY_LSKF_AVAILABLE - auth with pin or patter is available,
     * <li>UNAVAILABLE - no authentication method is available.
     */
    public @BiometricStatus int getBiometricAvailabilityStatus() {
        if (mNativeReauthenticatorBridge == 0) return BiometricStatus.UNAVAILABLE;

        return ReauthenticatorBridgeJni.get()
                .getBiometricAvailabilityStatus(mNativeReauthenticatorBridge);
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

    /** Deletes the C++ counterpart. */
    public void destroy() {
        ReauthenticatorBridgeJni.get().destroy(mNativeReauthenticatorBridge);
        mNativeReauthenticatorBridge = 0;
    }

    /**
     * Create an instance of {@link ReauthenticatorBridge} based on the provided {@link
     * DeviceAuthSource}.
     *
     * @param activity Used to display the biometric prompt and modal dialogs.
     * @param profile The profile to which the device authenticator service belongs.
     * @param unused_modalDialogManager Used to display error dialogs during mandatory auth steps.
     * @param source The feature invoking the authentication.
     */
    public static ReauthenticatorBridge create(
            Activity activity,
            Profile profile,
            ModalDialogManager unused_modalDialogManager,
            @DeviceAuthSource int source) {
        if (sReauthenticatorBridgeForTesting != null) {
            return sReauthenticatorBridgeForTesting;
        }
        return new ReauthenticatorBridge(activity, profile, source);
    }

    /**
     * Create an instance of {@link ReauthenticatorBridge} based on the provided {@link
     * DeviceAuthSource}.
     *
     * <p>// TODO(crbug.com/370467784) Remove once all callers have switched to the one above.
     */
    public static ReauthenticatorBridge create(
            Activity activity, Profile profile, @DeviceAuthSource int source) {
        return ReauthenticatorBridge.create(activity, profile, null, source);
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
        long create(
                ReauthenticatorBridge reauthenticatorBridge,
                Activity activity,
                @JniType("Profile*") Profile profile,
                int source);

        @BiometricStatus
        int getBiometricAvailabilityStatus(long nativeReauthenticatorBridge);

        void reauthenticate(long nativeReauthenticatorBridge);

        void destroy(long nativeReauthenticatorBridge);
    }
}
