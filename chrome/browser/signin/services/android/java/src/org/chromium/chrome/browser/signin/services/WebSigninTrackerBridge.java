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
 * Used by the web sign-in flow to detect when the flow is completed or failed. The sign-in isn't
 * tracked until {@link #start()} is called. Every instance of this class will be destroyed once the
 * callback is run correctly to release native resources but the case where the callback was not
 * invoked {@link #destroy()} must still be explicitly called.
 */
@MainThread
@NullMarked
public class WebSigninTrackerBridge {
    /** Factory to create WebSigninTrackerBridge object. */
    public static class Factory {
        /**
         * Creates a WebSigninTrackerBridge object. {@link WebSigninTrackerBridge#start()} must be
         * called to start tracking the sign-in.
         *
         * @param profile The profile to use for the sign-in.
         * @param accountId The primary account used for the sign-in process.
         * @param callback The callback to be notified about sign-in result.
         */
        public WebSigninTrackerBridge createWithCoreAccountId(
                Profile profile,
                CoreAccountId accountId,
                Callback<@WebSigninTrackerResult Integer> callback) {
            return new WebSigninTrackerBridge(profile, accountId, callback);
        }

        /**
         * Creates a WebSigninTrackerBridge object. {@link WebSigninTrackerBridge#start()} must be
         * called to start tracking the sign-in.
         *
         * @param profile The profile to use for the sign-in.
         * @param email The primary account email used for the sign-in process.
         * @param callback The callback to be notified about sign-in result.
         */
        public WebSigninTrackerBridge createWithEmail(
                Profile profile, String email, Callback<@WebSigninTrackerResult Integer> callback) {
            return new WebSigninTrackerBridge(profile, email, callback);
        }
    }

    private long mNativeWebSigninTrackerBridge;

    /**
     * Notifies the passed {@link Listener} when the sign-in process completes either successfully
     * or with an error. Successful completion means that the primary account is available in
     * cookies.
     *
     * @param accountId The primary account used for the sign-in process.
     * @param callback The callback to be notified about sign-in result.
     */
    private WebSigninTrackerBridge(
            Profile profile,
            CoreAccountId accountId,
            Callback<@WebSigninTrackerResult Integer> callback) {
        Objects.requireNonNull(accountId);
        Objects.requireNonNull(callback);
        mNativeWebSigninTrackerBridge =
                WebSigninTrackerBridgeJni.get()
                        .createWithCoreAccountId(
                                profile, accountId, createDestroyCallback(callback));

        assert mNativeWebSigninTrackerBridge != 0
                : "Couldn't create native WebSigninTrackerBridge object!";
    }

    /**
     * Notifies the passed {@link Listener} when the sign-in process completes either successfully
     * or with an error. Successful completion means that the primary account is available in
     * cookies. Should be explicitly destroyed using {@link #destroy()} to release native resources.
     *
     * @param email The primary account email used for the sign-in process.
     * @param callback The callback to be notified about sign-in result.
     */
    private WebSigninTrackerBridge(
            Profile profile, String email, Callback<@WebSigninTrackerResult Integer> callback) {
        Objects.requireNonNull(email);
        Objects.requireNonNull(callback);
        mNativeWebSigninTrackerBridge =
                WebSigninTrackerBridgeJni.get()
                        .createWithEmail(profile, email, createDestroyCallback(callback));
        assert mNativeWebSigninTrackerBridge != 0
                : "Couldn't create native WebSigninTrackerBridge object!";
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

    /**
     * Starts tracking the sign-in. Must be called at most once. If the result is already known
     * (e.g. the account is already in cookies), the callback is invoked synchronously from this
     * method, so callers should store this object before calling it.
     */
    public void start() {
        assert mNativeWebSigninTrackerBridge != 0 : "WebSigninTrackerBridge is already destroyed!";
        WebSigninTrackerBridgeJni.get().start(mNativeWebSigninTrackerBridge);
    }

    /** Releases native resources used by this class. */
    public void destroy() {
        if (mNativeWebSigninTrackerBridge == 0) return;

        long nativeWebSigninTrackerBridge = mNativeWebSigninTrackerBridge;
        mNativeWebSigninTrackerBridge = 0;
        WebSigninTrackerBridgeJni.get().destroy(nativeWebSigninTrackerBridge);
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

        void start(long webSigninTrackerBridgePtr);

        void destroy(long webSigninTrackerBridgePtr);
    }
}
