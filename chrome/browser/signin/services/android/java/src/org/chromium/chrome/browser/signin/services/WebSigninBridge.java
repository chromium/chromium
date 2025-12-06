// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;

import java.util.Objects;

/**
 * Used by the web sign-in flow to detect when the flow is completed or failed. Every instance of
 * this class should be explicitly destroyed using {@link #destroy()} to correctly release native
 * resources.
 */
@MainThread
@NullMarked
public class WebSigninBridge {
    /** Factory to create WebSigninBridge object. */
    public static class Factory {
        /**
         * Creates a WebSigninBridge object.
         *
         * @param profile The profile to use for the sign-in.
         * @param account The primary account account used for the sign-in process.
         * @param callback The callback to be notified about sign-in result.
         */
        public WebSigninBridge create(
                Profile profile,
                CoreAccountInfo account,
                Callback<@WebSigninTrackerResult Integer> callback) {
            return new WebSigninBridge(profile, account, callback);
        }
    }

    private long mNativeWebSigninBridge;

    /**
     * Notifies the passed {@link Listener} when the sign-in process completes either successfully
     * or with an error. Successful completion means that the primary account is available in
     * cookies. Should be explicitly destroyed using {@link #destroy()} to release native resources.
     *
     * @param account The primary account account used for the sign-in process.
     * @param callback The callback to be notified about sign-in result.
     */
    private WebSigninBridge(
            Profile profile,
            CoreAccountInfo account,
            Callback<@WebSigninTrackerResult Integer> callback) {
        Objects.requireNonNull(account);
        Objects.requireNonNull(callback);
        mNativeWebSigninBridge = WebSigninBridgeJni.get().create(profile, account, callback);
        assert mNativeWebSigninBridge != 0 : "Couldn't create native WebSigninBridge object!";
    }

    /** Releases native resources used by this class. */
    public void destroy() {
        WebSigninBridgeJni.get().destroy(mNativeWebSigninBridge);
        mNativeWebSigninBridge = 0;
    }

    @VisibleForTesting
    @CalledByNative
    static void onSigninResult(
            Callback<@WebSigninTrackerResult Integer> callback,
            @WebSigninTrackerResult int result) {
        callback.onResult(result);
    }

    @NativeMethods
    interface Natives {
        long create(
                @JniType("Profile*") Profile profile,
                @JniType("CoreAccountInfo") CoreAccountInfo account,
                Callback<@WebSigninTrackerResult Integer> callback);

        void destroy(long webSigninBridgePtr);
    }
}
