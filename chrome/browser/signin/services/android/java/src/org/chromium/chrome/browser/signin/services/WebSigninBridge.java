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
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.Objects;

/**
 * Used by the web sign-in flow to detect when the flow is completed or failed. Every instance of
 * this class will be destroyed once the callback is run correctly to release native resources but
 * the case where where the callback was not invoked {@link #destroy()} must still be explicitly
 * called.
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
        public WebSigninBridge createWithCoreAccountId(
                Profile profile,
                CoreAccountId accountId,
                Callback<@WebSigninTrackerResult Integer> callback) {
            return new WebSigninBridge(profile, accountId, callback);
        }

        /**
         * Creates a WebSigninBridge object.
         *
         * @param profile The profile to use for the sign-in.
         * @param email The primary account account email used for the sign-in process.
         * @param callback The callback to be notified about sign-in result.
         */
        public WebSigninBridge createWithEmail(
                Profile profile, String email, Callback<@WebSigninTrackerResult Integer> callback) {
            return new WebSigninBridge(profile, email, callback);
        }
    }

    private long mNativeWebSigninBridge;

    /**
     * Notifies the passed {@link Listener} when the sign-in process completes either successfully
     * or with an error. Successful completion means that the primary account is available in
     * cookies.
     *
     * @param account The primary account account used for the sign-in process.
     * @param callback The callback to be notified about sign-in result.
     */
    private WebSigninBridge(
            Profile profile,
            CoreAccountId accountId,
            Callback<@WebSigninTrackerResult Integer> callback) {
        Objects.requireNonNull(accountId);
        Objects.requireNonNull(callback);
        mNativeWebSigninBridge =
                WebSigninBridgeJni.get()
                        .createWithCoreAccountId(
                                profile, accountId, createDestroyCallback(callback));

        assert mNativeWebSigninBridge != 0 : "Couldn't create native WebSigninBridge object!";
    }

    /**
     * Notifies the passed {@link Listener} when the sign-in process completes either successfully
     * or with an error. Successful completion means that the primary account is available in
     * cookies. Should be explicitly destroyed using {@link #destroy()} to release native resources.
     *
     * @param account The primary account account used for the sign-in process.
     * @param callback The callback to be notified about sign-in result.
     */
    private WebSigninBridge(
            Profile profile, String email, Callback<@WebSigninTrackerResult Integer> callback) {
        Objects.requireNonNull(email);
        Objects.requireNonNull(callback);
        mNativeWebSigninBridge =
                WebSigninBridgeJni.get()
                        .createWithEmail(profile, email, createDestroyCallback(callback));
        assert mNativeWebSigninBridge != 0 : "Couldn't create native WebSigninBridge object!";
    }

    /**
     * Creates a wrapped callback that releases the native class and allows it to be correctly
     * destroyed when the native class returns the WebSigninTrackerResult.
     */
    private Callback<@WebSigninTrackerResult Integer> createDestroyCallback(
            Callback<@WebSigninTrackerResult Integer> callback) {
        return result -> {
            callback.onResult(result);
            destroy();
        };
    }

    /** Releases native resources used by this class. */
    public void destroy() {
        if (mNativeWebSigninBridge == 0) return;

        long nativeWebSigninBridge = mNativeWebSigninBridge;
        mNativeWebSigninBridge = 0;
        WebSigninBridgeJni.get().destroy(nativeWebSigninBridge);
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
        long createWithCoreAccountId(
                @JniType("Profile*") Profile profile,
                @JniType("CoreAccountId") CoreAccountId account,
                Callback<@WebSigninTrackerResult Integer> callback);

        long createWithEmail(
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String email,
                Callback<@WebSigninTrackerResult Integer> callback);

        void destroy(long webSigninBridgePtr);
    }
}
